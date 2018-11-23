/*
 * Copyright (C) 2018 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <blpwtk2_rendercompositor.h>

#include <base/debug/alias.h>
#include <base/lazy_instance.h>
#include <components/viz/common/display/renderer_settings.h>
#include <components/viz/common/frame_sinks/begin_frame_source.h>
#include <components/viz/common/frame_sinks/delay_based_time_source.h>
#include <components/viz/common/resources/resource_sizes.h>
#include <components/viz/common/surfaces/parent_local_surface_id_allocator.h>
#include <components/viz/host/host_frame_sink_manager.h>
#include <components/viz/service/display/display.h>
#include <components/viz/service/display/output_surface.h>
#include <components/viz/service/display/output_surface_frame.h>
#include <components/viz/service/display_embedder/server_shared_bitmap_manager.h>
#include <components/viz/service/display_embedder/software_output_device_win.h>
#include <components/viz/service/display/display_scheduler.h>
#include <components/viz/service/frame_sinks/direct_layer_tree_frame_sink.h>
#include <components/viz/service/frame_sinks/frame_sink_manager_impl.h>
#include <content/common/gpu_stream_constants.h>
#include <content/renderer/render_thread_impl.h>
#include <gpu/command_buffer/client/context_support.h>
#include <gpu/command_buffer/client/gles2_interface.h>
#include <gpu/command_buffer/common/swap_buffers_complete_params.h>
#include <gpu/ipc/client/command_buffer_proxy_impl.h>
#include <skia/ext/platform_canvas.h>
#include <skia/ext/skia_utils_win.h>
#include <services/ui/public/cpp/gpu/context_provider_command_buffer.h>
#include <ui/compositor/compositor_vsync_manager.h>
#include <ui/gfx/vsync_provider.h>
#include <ui/gl/gl_utils.h>

#include <algorithm>
#include <numeric>
#include <set>

namespace blpwtk2 {

namespace {

static base::LazyInstance<RenderCompositorContext>::DestructorAtExit
    s_instance = LAZY_INSTANCE_INITIALIZER;

} // close namespace

class OutputSurfaceForDisplay : public viz::OutputSurface,
                                private ui::CompositorVSyncManager::Observer {
  public:

    ~OutputSurfaceForDisplay() override {
        d_vsync_manager->RemoveObserver(this);
    }

    // viz::OutputSurface overrides:
    void BindToClient(viz::OutputSurfaceClient* client) override {
        d_client = client;

        d_vsync_manager->AddObserver(this);
    }

    viz::OverlayCandidateValidator* GetOverlayCandidateValidator() const override {
        return nullptr;
    }

    bool HasExternalStencilTest() const override {
        return false;
    }

    void ApplyExternalStencil() override {
    }

    // ui::CompositorVSyncManager::Observer overrides:
    void OnUpdateVSyncParameters(base::TimeTicks timebase,
                                 base::TimeDelta interval) override {
        if (interval.is_zero()) {
            interval = viz::BeginFrameArgs::DefaultInterval();
        }

        d_begin_frame_source->OnUpdateVSyncParameters(timebase, interval);
    }

  protected:

    OutputSurfaceForDisplay(
        scoped_refptr<viz::ContextProvider>        context_provider,
        scoped_refptr<ui::CompositorVSyncManager>  vsync_manager,
        viz::SyntheticBeginFrameSource            *begin_frame_source)
    : viz::OutputSurface(context_provider)
    , d_vsync_manager(vsync_manager)
    , d_begin_frame_source(begin_frame_source) {
    }

    OutputSurfaceForDisplay(
        std::unique_ptr<viz::SoftwareOutputDevice> software_device,
        scoped_refptr<ui::CompositorVSyncManager>  vsync_manager,
        viz::SyntheticBeginFrameSource            *begin_frame_source)
    : viz::OutputSurface(std::move(software_device))
    , d_vsync_manager(vsync_manager)
    , d_begin_frame_source(begin_frame_source) {
    }

    void UpdateVSyncParameters(
        base::TimeTicks timebase,
        base::TimeDelta interval) {
        d_vsync_manager->UpdateVSyncParameters(timebase, interval);
    }

  protected:

    viz::OutputSurfaceClient* d_client = nullptr;

  private:

    scoped_refptr<ui::CompositorVSyncManager> d_vsync_manager;

    viz::SyntheticBeginFrameSource *d_begin_frame_source;
};

// GpuOutputSurfaceForDisplay
//
// This is the viz::OutputSurface that is passed to the cc::Display. It is
// fairly similar to content::GpuBrowserCompositorOutputSurface.
class GpuOutputSurfaceForDisplay : public OutputSurfaceForDisplay {
  public:

    GpuOutputSurfaceForDisplay(
        scoped_refptr<ui::ContextProviderCommandBuffer>  context_provider,
        scoped_refptr<ui::CompositorVSyncManager>        vsync_manager,
        viz::SyntheticBeginFrameSource                  *begin_frame_source)
    : OutputSurfaceForDisplay(context_provider,
                              vsync_manager, begin_frame_source) {
    }

    // viz::OutputSurface overrides:
    void BindToClient(viz::OutputSurfaceClient* client) override {
        OutputSurfaceForDisplay::BindToClient(client);

        command_buffer_proxy()->SetSwapBuffersCompletionCallback(
            base::Bind(
                &GpuOutputSurfaceForDisplay::OnGpuSwapBuffersCompleted,
                base::Unretained(this)));

        command_buffer_proxy()->SetUpdateVSyncParametersCallback(
            base::Bind(
                &GpuOutputSurfaceForDisplay::OnGpuUpdateVSyncParameters,
                base::Unretained(this)));

        command_buffer_proxy()->SetPresentationCallback(
            base::Bind(&GpuOutputSurfaceForDisplay::OnPresentation,
                base::Unretained(this)));

        if (capabilities_.uses_default_gl_framebuffer) {
            capabilities_.flipped_output_surface =
                context_provider()->ContextCapabilities().flips_vertically;
        }
    }

    void EnsureBackbuffer() override {
    }

    void DiscardBackbuffer() override {
        context_provider()->ContextGL()->DiscardBackbufferCHROMIUM();
    }

    void BindFramebuffer() override {
        context_provider()->ContextGL()->BindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    void Reshape(const gfx::Size& size,
                 float device_scale_factor,
                 const gfx::ColorSpace& color_space,
                 bool has_alpha,
                 bool use_stencil) override {
        d_size = size;
        d_has_set_draw_rectangle_since_last_resize = false;

        context_provider()->ContextGL()->ResizeCHROMIUM(
            size.width(), size.height(), device_scale_factor,
            gl::GetGLColorSpace(color_space), has_alpha);
    }

    void SwapBuffers(viz::OutputSurfaceFrame frame) override {
        d_set_draw_rectangle_for_frame = false;

        if (frame.sub_buffer_rect) {
            context_provider()->ContextSupport()->PartialSwapBuffers(
                *frame.sub_buffer_rect);
        }
        else if (!frame.content_bounds.empty()) {
            context_provider()->ContextSupport()->SwapWithBounds(
                frame.content_bounds);
        }
        else {
            context_provider()->ContextSupport()->Swap();
        }
    }

    bool IsDisplayedAsOverlayPlane() const override {
        return false;
    }

    unsigned GetOverlayTextureId() const override {
        return 0;
    }

    gfx::BufferFormat GetOverlayBufferFormat() const override {
        return gfx::BufferFormat::RGBX_8888;
    }

    bool SurfaceIsSuspendForRecycle() const override {
        return false;
    }

    uint32_t GetFramebufferCopyTextureFormat() override {
        return provider_command_buffer()->GetCopyTextureInternalFormat();
    }

    void SetDrawRectangle(const gfx::Rect& rect) override {
        if (d_set_draw_rectangle_for_frame) {
            return;
        }

        DCHECK(gfx::Rect(d_size).Contains(rect));
        DCHECK(d_has_set_draw_rectangle_since_last_resize ||
               (gfx::Rect(d_size) == rect));

        d_set_draw_rectangle_for_frame = true;
        d_has_set_draw_rectangle_since_last_resize = true;
        context_provider()->ContextGL()->SetDrawRectangleCHROMIUM(
            rect.x(), rect.y(), rect.width(), rect.height());
    }

  private:

    ui::ContextProviderCommandBuffer* provider_command_buffer() {
        return static_cast<ui::ContextProviderCommandBuffer*>(context_provider_.get());
    }

    gpu::CommandBufferProxyImpl* command_buffer_proxy() {
        gpu::CommandBufferProxyImpl *command_buffer_proxy =
            provider_command_buffer()->GetCommandBufferProxy();
        DCHECK(command_buffer_proxy);

        return command_buffer_proxy;
    }

    void OnGpuSwapBuffersCompleted(
      const gpu::SwapBuffersCompleteParams& params) {
        if (!params.ca_layer_params.is_empty) {
            d_client->DidReceiveCALayerParams(params.ca_layer_params);
        }

        if (!params.texture_in_use_responses.empty()) {
            d_client->DidReceiveTextureInUseResponses(params.texture_in_use_responses);
        }

        d_client->DidReceiveSwapBuffersAck(params.swap_response.swap_id);
    }

    void OnGpuUpdateVSyncParameters(
        base::TimeTicks timebase,
        base::TimeDelta interval) {
        UpdateVSyncParameters(timebase, interval);
    }

    void OnPresentation(uint64_t swap_id,
        const gfx::PresentationFeedback& feedback) {
        d_client->DidReceivePresentationFeedback(swap_id, feedback);
    }

    bool d_set_draw_rectangle_for_frame = false;
    bool d_has_set_draw_rectangle_since_last_resize = false;
    gfx::Size d_size;
};

class SoftwareOutputSurfaceForDisplay : public OutputSurfaceForDisplay,
                                        public base::SupportsWeakPtr<SoftwareOutputSurfaceForDisplay> {
  public:
    SoftwareOutputSurfaceForDisplay(
        std::unique_ptr<viz::SoftwareOutputDevice>   software_device,
        scoped_refptr<ui::CompositorVSyncManager>    vsync_manager,
        viz::SyntheticBeginFrameSource              *begin_frame_source,
        scoped_refptr<base::SingleThreadTaskRunner>  task_runner)
    : OutputSurfaceForDisplay(std::move(software_device),
                              vsync_manager, begin_frame_source)
    , d_task_runner(task_runner) {
    }

    // viz::OutputSurface overrides:
    void EnsureBackbuffer() override {
        software_device()->EnsureBackbuffer();
    }

    void DiscardBackbuffer() override {
        software_device()->DiscardBackbuffer();
    }

    void BindFramebuffer() override {
        NOTREACHED();
    }

    void Reshape(
        const gfx::Size& size,
        float device_scale_factor,
        const gfx::ColorSpace& color_space,
        bool has_alpha,
        bool use_stencil) override {
      software_device()->Resize(size, device_scale_factor);
    }

    void SwapBuffers(viz::OutputSurfaceFrame frame) override {
        gfx::VSyncProvider* vsync_provider = software_device()->GetVSyncProvider();
        if (vsync_provider) {
            vsync_provider->GetVSyncParameters(
                base::Bind(
                    &SoftwareOutputSurfaceForDisplay::OnSoftwareUpdateVSyncParameters,
                    AsWeakPtr()));
        }

        ++d_swap_id;
        d_task_runner->PostTask(
            FROM_HERE,
                base::Bind(&SoftwareOutputSurfaceForDisplay::SwapBuffersImpl,
                    AsWeakPtr(), d_swap_id));
    }

    bool IsDisplayedAsOverlayPlane() const override {
        return false;
    }

    unsigned GetOverlayTextureId() const override {
        return 0;
    }

    gfx::BufferFormat GetOverlayBufferFormat() const override {
        return gfx::BufferFormat::RGBX_8888;
    }

    bool SurfaceIsSuspendForRecycle() const override {
        return false;
    }

    uint32_t GetFramebufferCopyTextureFormat() override {
        NOTREACHED();
        return 0;
    }

    void SetDrawRectangle(const gfx::Rect& rect) override {
        NOTREACHED();
    }

  private:

    void OnSoftwareUpdateVSyncParameters(
        base::TimeTicks timebase,
        base::TimeDelta interval) {
        d_refresh_interval = interval;
        UpdateVSyncParameters(timebase, interval);
    }

    void SwapBuffersImpl(uint64_t swap_id) {
        d_client->DidReceiveSwapBuffersAck(swap_id);
        d_client->DidReceivePresentationFeedback(
            swap_id,
            gfx::PresentationFeedback(
                base::TimeTicks::Now(), d_refresh_interval, 0u));
    }

    scoped_refptr<base::SingleThreadTaskRunner> d_task_runner;
    uint64_t d_swap_id = 0;
    base::TimeDelta d_refresh_interval;
};

class LayerTreeFrameSink : public cc::LayerTreeFrameSink,
                            public base::SupportsWeakPtr<LayerTreeFrameSink> {
  public:

    LayerTreeFrameSink(
        std::unique_ptr<cc::LayerTreeFrameSink>      delegate,
        scoped_refptr<viz::ContextProvider>          context_provider,
        scoped_refptr<viz::RasterContextProvider>    worker_context_provider,
        scoped_refptr<base::SingleThreadTaskRunner>  compositor_task_runner,
        gpu::GpuMemoryBufferManager                 *gpu_memory_buffer_manager,
        viz::SharedBitmapManager                    *shared_bitmap_manager);
    ~LayerTreeFrameSink() override;

    // cc::LayerTreeFrameSink overrides:
    bool BindToClient(cc::LayerTreeFrameSinkClient* client) override;
    void DetachFromClient() override;
    void SetLocalSurfaceId(const viz::LocalSurfaceId& local_surface_id) override;
    void SubmitCompositorFrame(viz::CompositorFrame frame) override;
    void DidNotProduceFrame(const viz::BeginFrameAck& ack) override;
    void DidAllocateSharedBitmap(mojo::ScopedSharedBufferHandle buffer,
                               const viz::SharedBitmapId& id) override;
    void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override;

    void OnRenderCompositorDestroyed();

  private:

    std::unique_ptr<cc::LayerTreeFrameSink>  d_delegate;
};

LayerTreeFrameSink::LayerTreeFrameSink(
    std::unique_ptr<cc::LayerTreeFrameSink>      delegate,
    scoped_refptr<viz::ContextProvider>          context_provider,
    scoped_refptr<viz::RasterContextProvider>    worker_context_provider,
    scoped_refptr<base::SingleThreadTaskRunner>  compositor_task_runner,
    gpu::GpuMemoryBufferManager                 *gpu_memory_buffer_manager,
    viz::SharedBitmapManager                    *shared_bitmap_manager)
: cc::LayerTreeFrameSink(
    context_provider, worker_context_provider,
    compositor_task_runner,
    gpu_memory_buffer_manager, shared_bitmap_manager)
, d_delegate(std::move(delegate))
{
}

LayerTreeFrameSink::~LayerTreeFrameSink()
{
}

bool LayerTreeFrameSink::BindToClient(cc::LayerTreeFrameSinkClient* client)
{
    if (!d_delegate) {
        return false;
    }

    if (!cc::LayerTreeFrameSink::BindToClient(client)) {
        return false;
    }

    return d_delegate->BindToClient(client);
}

void LayerTreeFrameSink::DetachFromClient()
{
    if (!d_delegate) {
        return;
    }

    d_delegate->DetachFromClient();
    cc::LayerTreeFrameSink::DetachFromClient();
}

void LayerTreeFrameSink::SetLocalSurfaceId(const viz::LocalSurfaceId& local_surface_id)
{
    if (!d_delegate) {
        return;
    }

    d_delegate->SetLocalSurfaceId(local_surface_id);
}

void LayerTreeFrameSink::SubmitCompositorFrame(viz::CompositorFrame frame)
{
    if (!d_delegate) {
        return;
    }

    d_delegate->SubmitCompositorFrame(std::move(frame));
}

void LayerTreeFrameSink::DidNotProduceFrame(const viz::BeginFrameAck& ack)
{
    if (!d_delegate) {
        return;
    }

    d_delegate->DidNotProduceFrame(ack);
}

void LayerTreeFrameSink::DidAllocateSharedBitmap(
    mojo::ScopedSharedBufferHandle buffer,
    const viz::SharedBitmapId& id)
{
    if (!d_delegate) {
        return;
    }

    d_delegate->DidAllocateSharedBitmap(std::move(buffer), id);
}

void LayerTreeFrameSink::DidDeleteSharedBitmap(
    const viz::SharedBitmapId& id)
{
    if (!d_delegate) {
        return;
    }

    d_delegate->DidDeleteSharedBitmap(id);
}


void LayerTreeFrameSink::OnRenderCompositorDestroyed()
{
    if (d_delegate && d_delegate->HasClient()) {
        d_delegate->DetachFromClient();
    }

    d_delegate.reset();
}

RenderCompositorContext::Details::Details()
{
}

RenderCompositorContext::Details::~Details()
{
}

RenderCompositorContext::RenderCompositorContext()
{
    content::RenderThreadImpl *render_thread =
        content::RenderThreadImpl::current();

    d_details.reset(new Details());

    render_thread->compositor_task_runner()->
        PostTask(
            FROM_HERE,
            base::Bind(&RenderCompositorContext::Details::ConstructImpl,
                base::Unretained(d_details.get()),
                viz::ServerSharedBitmapManager::current(),
                render_thread->GetGpuMemoryBufferManager()));
}

RenderCompositorContext::~RenderCompositorContext()
{
}

RenderCompositorContext* RenderCompositorContext::GetInstance()
{
    return s_instance.Pointer();
}

void RenderCompositorContext::Terminate()
{
    if (!s_instance.IsCreated()) {
        return;
    }

    s_instance.Pointer()->Destruct();
}

void RenderCompositorContext::Destruct()
{
    content::RenderThreadImpl::current()->compositor_task_runner()->
        DeleteSoon(
            FROM_HERE,
            d_details.release());
}

scoped_refptr<gpu::GpuChannelHost> RenderCompositorContext::EstablishPrivilegedGpuChannel()
{
    auto gpu_channel =
        content::RenderThreadImpl::current()->EstablishPrivilegedGpuChannelSync();

    content::RenderThreadImpl::current()->compositor_task_runner()->
        PostTask(
            FROM_HERE,
            base::Bind(&RenderCompositorContext::Details::EstablishPrivilegedGpuChannelImpl,
                base::Unretained(d_details.get()),
                gpu_channel));

    return gpu_channel;
}

void RenderCompositorContext::RequestUncorrelatedNewLayerTreeFrameSink(
    const base::Callback<void(std::unique_ptr<cc::LayerTreeFrameSink>)>& callback)
{
    content::RenderThreadImpl::current()->compositor_task_runner()->
        PostTask(
            FROM_HERE,
            base::Bind(&RenderCompositorContext::Details::RequestUncorrelatedNewLayerTreeFrameSinkImpl,
                base::Unretained(d_details.get()),
                callback));
}

void RenderCompositorContext::Details::ConstructImpl(
    viz::SharedBitmapManager *shared_bitmap_manager,
    gpu::GpuMemoryBufferManager *gpu_memory_buffer_manager)
{
    d_shared_bitmap_manager = shared_bitmap_manager;
    d_gpu_memory_buffer_manager = gpu_memory_buffer_manager;

    d_renderer_settings.reset(new viz::RendererSettings());
    d_renderer_settings->partial_swap_enabled = true;
    d_renderer_settings->finish_rendering_on_resize = true;

    d_frame_sink_manager.reset(
        new viz::FrameSinkManagerImpl());

    d_host_frame_sink_manager.reset(
        new viz::HostFrameSinkManager());

    d_host_frame_sink_manager->SetLocalManager(d_frame_sink_manager.get());
    d_frame_sink_manager->SetLocalClient(d_host_frame_sink_manager.get());

    d_software_backing_manager.reset(
        new viz::OutputDeviceBacking());
}

void RenderCompositorContext::Details::EstablishPrivilegedGpuChannelImpl(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel)
{
    if (!gpu_channel && d_worker_context_provider) {
        d_worker_context_provider = nullptr;
    }

    if (gpu_channel && !d_worker_context_provider) {
        constexpr bool automatic_flushes = false;
        constexpr bool support_locking   = true;
        constexpr bool support_gles2_interface = true;
        constexpr bool support_raster_interface = true;
        constexpr bool support_grcontext = true;

        gpu::ContextCreationAttribs attributes;
        attributes.alpha_size                      = -1;
        attributes.depth_size                      = 0;
        attributes.stencil_size                    = 0;
        attributes.samples                         = 0;
        attributes.sample_buffers                  = 0;
        attributes.bind_generates_resource         = false;
        attributes.lose_context_when_out_of_memory = true;
        attributes.buffer_preserved                = false;
        attributes.enable_gles2_interface          = support_gles2_interface;
        attributes.enable_raster_interface         = support_raster_interface;

        d_worker_context_provider = new ui::ContextProviderCommandBuffer(
            gpu_channel,
            d_gpu_memory_buffer_manager,
            content::kGpuStreamIdDefault,
            content::kGpuStreamPriorityUI,
            gpu::kNullSurfaceHandle,
            GURL("chrome://gpu/RenderCompositorContext::EstablishPrivilegedGpuChannel"),
            automatic_flushes,
            support_locking,
            support_grcontext,
            gpu::SharedMemoryLimits(),
            attributes,
            nullptr,
            ui::command_buffer_metrics::RENDER_WORKER_CONTEXT);

        if (d_worker_context_provider->BindToCurrentThread()
                != gpu::ContextResult::kSuccess) {
            d_worker_context_provider = nullptr;
        }
    }
}

void RenderCompositorContext::Details::RequestUncorrelatedNewLayerTreeFrameSinkImpl(
    const base::Callback<void(std::unique_ptr<cc::LayerTreeFrameSink>)>& callback)
{
    auto layer_tree_frame_sink =
        std::make_unique<LayerTreeFrameSink>(
            nullptr,
            nullptr,
            nullptr,
            nullptr,
            d_gpu_memory_buffer_manager,
            d_shared_bitmap_manager);

    callback.Run(std::move(layer_tree_frame_sink));
}

std::unique_ptr<RenderCompositor> RenderCompositorContext::CreateCompositor(gpu::SurfaceHandle gpu_surface_handle)
{
    return std::unique_ptr<RenderCompositor>(
        new RenderCompositor(this, gpu_surface_handle));
}

bool RenderCompositorContext::RequestNewLayerTreeFrameSink(
    bool use_software, int routing_id,
    const base::Callback<void(std::unique_ptr<cc::LayerTreeFrameSink>)>& callback)
{
    auto it = d_compositors_by_routing_id.find(routing_id);
    if (it == d_compositors_by_routing_id.end()) {
        RequestUncorrelatedNewLayerTreeFrameSink(callback);
        return true;
    }

    it->second->RequestNewLayerTreeFrameSink(use_software, callback);

    return true;
}

RenderCompositor::RenderCompositor(
    RenderCompositorContext *context, gpu::SurfaceHandle gpu_surface_handle)
: d_context(context)
, d_gpu_surface_handle(gpu_surface_handle)
{
    d_details.reset(new Details());

    d_local_surface_id_allocator =
        std::make_unique<viz::ParentLocalSurfaceIdAllocator>();
    d_local_surface_id = d_local_surface_id_allocator->GenerateId();

    content::RenderThreadImpl::current()->compositor_task_runner()->
        PostTask(
            FROM_HERE,
            base::Bind(&RenderCompositor::Details::ConstructImpl,
                base::Unretained(d_details.get()),
                context->d_details.get()));
}

RenderCompositor::~RenderCompositor()
{
    if (d_routing_id) {
        auto it = d_context->d_compositors_by_routing_id.find(d_routing_id);
        if (it != d_context->d_compositors_by_routing_id.end()) {
            d_context->d_compositors_by_routing_id.erase(it);
        }
    }

    content::RenderThreadImpl::current()->compositor_task_runner()->
        DeleteSoon(
            FROM_HERE,
            d_details.release());
}

void RenderCompositor::SetVisible(bool visible)
{
    content::RenderThreadImpl::current()->compositor_task_runner()->
        PostTask(
            FROM_HERE,
            base::Bind(&RenderCompositor::Details::SetVisibleImpl,
                base::Unretained(d_details.get()),
                visible));
}

void RenderCompositor::DisableSwapUntilResize()
{
    d_local_surface_id = d_local_surface_id_allocator->GenerateId();

    base::WaitableEvent event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    content::RenderThreadImpl::current()->compositor_task_runner()->
        PostTask(
            FROM_HERE,
            base::Bind(&RenderCompositor::Details::ResizeImpl,
                base::Unretained(d_details.get()),
                gfx::Size(0, 0),
                d_local_surface_id,
                &event));

    event.Wait();
}

void RenderCompositor::Resize(const gfx::Size& size)
{
    d_local_surface_id = d_local_surface_id_allocator->GenerateId();

    base::WaitableEvent event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    content::RenderThreadImpl::current()->compositor_task_runner()->
        PostTask(
            FROM_HERE,
            base::Bind(&RenderCompositor::Details::ResizeImpl,
                base::Unretained(d_details.get()),
                size,
                d_local_surface_id,
                &event));

    event.Wait();
}

void RenderCompositor::Correlate(int routing_id)
{
    if (d_routing_id) {
        auto it = d_context->d_compositors_by_routing_id.find(d_routing_id);
        if (it != d_context->d_compositors_by_routing_id.end()) {
            d_context->d_compositors_by_routing_id.erase(it);
        }
    }

    d_routing_id = routing_id;

    if (d_routing_id) {
        d_context->d_compositors_by_routing_id.insert(std::make_pair(d_routing_id, this));
    }
}

void RenderCompositor::RequestNewLayerTreeFrameSink(
    bool use_software,
    const base::Callback<void(std::unique_ptr<cc::LayerTreeFrameSink>)>& callback)
{
    d_local_surface_id = d_local_surface_id_allocator->GenerateId();

    scoped_refptr<gpu::GpuChannelHost> gpu_channel =
        !use_software ? d_context->EstablishPrivilegedGpuChannel() :
                        nullptr;

    content::RenderThreadImpl::current()->compositor_task_runner()->
        PostTask(
            FROM_HERE,
            base::Bind(&RenderCompositor::Details::RequestNewLayerTreeFrameSinkImpl,
                base::Unretained(d_details.get()),
                gpu_channel,
                content::RenderThreadImpl::current()->compositor_task_runner(),
                d_context->d_details.get(),
                d_gpu_surface_handle,
                d_local_surface_id,
                callback));
}

const viz::LocalSurfaceId& RenderCompositor::GetLocalSurfaceId()
{
    if (!d_local_surface_id.is_valid()) {
        d_local_surface_id = d_local_surface_id_allocator->GenerateId();
    }

    return d_local_surface_id;
}

RenderCompositor::Details::Details()
{
}

RenderCompositor::Details::~Details()
{
    if (d_layer_tree_frame_sink) {
        d_layer_tree_frame_sink->OnRenderCompositorDestroyed();
    }

    d_context->d_host_frame_sink_manager->InvalidateFrameSinkId(d_frame_sink_id);
}

void RenderCompositor::Details::OnFirstSurfaceActivation(
    const viz::SurfaceInfo& surface_info) {
}

void RenderCompositor::Details::OnFrameTokenChanged(uint32_t frame_token)
{
}

void RenderCompositor::Details::ConstructImpl(
    RenderCompositorContext::Details *context)
{
    d_context = context;

    d_frame_sink_id = viz::FrameSinkId(0, context->d_next_frame_sink_id++);

    d_context->d_host_frame_sink_manager->RegisterFrameSinkId(d_frame_sink_id, this);

    d_vsync_manager = new ui::CompositorVSyncManager();
}

void RenderCompositor::Details::SetVisibleImpl(bool visible)
{
    d_visible = visible;

    if (d_display) {
        d_display->SetVisible(d_visible);
    }
}

void RenderCompositor::Details::ResizeImpl(
    const gfx::Size& size,
    const viz::LocalSurfaceId& local_surface_id,
    base::WaitableEvent *event)
{
    d_size = size;

    if (d_display) {
        d_display->Resize(d_size);
    }

    if (d_layer_tree_frame_sink) {
        d_layer_tree_frame_sink->SetLocalSurfaceId(local_surface_id);
    }

    event->Signal();
}

void RenderCompositor::Details::RequestNewLayerTreeFrameSinkImpl(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    RenderCompositorContext::Details *context,
    gpu::SurfaceHandle gpu_surface_handle,
    const viz::LocalSurfaceId& local_surface_id,
    const base::Callback<void(std::unique_ptr<cc::LayerTreeFrameSink>)>& callback)
{
    if (d_layer_tree_frame_sink) {
        d_layer_tree_frame_sink->OnRenderCompositorDestroyed();
    }

    auto worker_context_provider = context->d_worker_context_provider;

    scoped_refptr<ui::ContextProviderCommandBuffer> context_provider;

    if (worker_context_provider) {
        constexpr bool automatic_flushes = false;
        constexpr bool support_locking   = false;
        constexpr bool support_gles2_interface = true;
        constexpr bool support_raster_interface = false;
        constexpr bool support_grcontext = true;

        gpu::ContextCreationAttribs attributes;
        attributes.alpha_size                      = -1;
        attributes.depth_size                      = 0;
        attributes.stencil_size                    = 0;
        attributes.samples                         = 0;
        attributes.sample_buffers                  = 0;
        attributes.bind_generates_resource         = false;
        attributes.lose_context_when_out_of_memory = true;
        attributes.buffer_preserved                = false;
        attributes.enable_gles2_interface          = support_gles2_interface;
        attributes.enable_raster_interface         = support_raster_interface;

        context_provider = new ui::ContextProviderCommandBuffer(
            gpu_channel,
            context->d_gpu_memory_buffer_manager,
            content::kGpuStreamIdDefault,
            content::kGpuStreamPriorityUI,
            gpu_surface_handle,
            GURL("chrome://gpu/RenderCompositorContext::CreateCompositor"),
            automatic_flushes,
            support_locking,
            support_grcontext,
            gpu::SharedMemoryLimits(),
            attributes,
            worker_context_provider.get(),
            ui::command_buffer_metrics::DISPLAY_COMPOSITOR_ONSCREEN_CONTEXT);

        if (context_provider->BindToCurrentThread()
                != gpu::ContextResult::kSuccess) {
            context_provider = nullptr;
            worker_context_provider = nullptr;
        }
    }

    // viz::BeginFrameSource:
    std::unique_ptr<viz::SyntheticBeginFrameSource> begin_frame_source;

    if (!context->d_disable_display_vsync) {
        begin_frame_source =
            std::make_unique<viz::DelayBasedBeginFrameSource>(
                std::make_unique<viz::DelayBasedTimeSource>(
                    compositor_task_runner.get()),
                    viz::BeginFrameSource::kNotRestartableId);
    }
    else {
        begin_frame_source =
            std::make_unique<viz::BackToBackBeginFrameSource>(
                std::make_unique<viz::DelayBasedTimeSource>(
                    compositor_task_runner.get()));
    }

    // viz::OutputSurface for the display:
    std::unique_ptr<viz::OutputSurface> display_output_surface;

    if (context_provider && worker_context_provider) {
        display_output_surface =
            std::make_unique<GpuOutputSurfaceForDisplay>(
                context_provider,
                d_vsync_manager,
                begin_frame_source.get());
    }
    else {
        display_output_surface =
            std::make_unique<SoftwareOutputSurfaceForDisplay>(
                std::make_unique<viz::SoftwareOutputDeviceWin>(
                    context->d_software_backing_manager.get(),
                    gpu_surface_handle),
                d_vsync_manager,
                begin_frame_source.get(),
                compositor_task_runner);
    }

    // viz::DisplayScheduler:
    auto display_scheduler =
        std::make_unique<viz::DisplayScheduler>(
            begin_frame_source.get(),
            compositor_task_runner.get(),
            display_output_surface->capabilities().max_frames_pending,
            context->d_wait_for_all_pipeline_stages_before_draw);

    // viz::Display:
    d_display =
        std::make_unique<viz::Display>(
            context->d_shared_bitmap_manager,
            *context->d_renderer_settings,
            d_frame_sink_id,
            std::move(display_output_surface),
            std::move(display_scheduler),
            compositor_task_runner);

    // viz::BeginFrameSource registration:
    if (d_begin_frame_source) {
        context->d_frame_sink_manager->UnregisterBeginFrameSource(
            d_begin_frame_source.get());
    }

    d_begin_frame_source = std::move(begin_frame_source);

    context->d_frame_sink_manager->RegisterBeginFrameSource(
        d_begin_frame_source.get(), d_frame_sink_id);

    // viz::LayerTreeFrameSink:
    auto frame_sink =
        std::make_unique<viz::DirectLayerTreeFrameSink>(
            d_frame_sink_id,
            context->d_host_frame_sink_manager.get(),
            context->d_frame_sink_manager.get(),
            d_display.get(), nullptr,
            context_provider,
            worker_context_provider,
            compositor_task_runner,
            context->d_gpu_memory_buffer_manager,
            context->d_shared_bitmap_manager,
            false);

    scoped_refptr<ui::ContextProviderCommandBuffer> compositor_surface_context_provider;

    if (worker_context_provider) {
        constexpr bool automatic_flushes = false;
        constexpr bool support_locking = false;
        constexpr bool support_gles2_interface = true;
        constexpr bool support_raster_interface = false;
        constexpr bool support_grcontext = false;

        gpu::ContextCreationAttribs attributes;
        attributes.alpha_size                      = -1;
        attributes.depth_size                      = 0;
        attributes.stencil_size                    = 0;
        attributes.samples                         = 0;
        attributes.sample_buffers                  = 0;
        attributes.bind_generates_resource         = false;
        attributes.lose_context_when_out_of_memory = true;
        attributes.buffer_preserved                = false;
        attributes.enable_gles2_interface          = support_gles2_interface;
        attributes.enable_raster_interface         = support_raster_interface;

        compositor_surface_context_provider =
            new ui::ContextProviderCommandBuffer(
                gpu_channel,
                context->d_gpu_memory_buffer_manager,
                content::kGpuStreamIdDefault,
                content::kGpuStreamPriorityUI,
                gpu::kNullSurfaceHandle,
                GURL("chrome://gpu/RenderCompositor::CreateCompositorFrameSink"),
                automatic_flushes,
                support_locking,
                support_grcontext,
                gpu::SharedMemoryLimits::ForMailboxContext(),
                attributes,
                worker_context_provider.get(),
                ui::command_buffer_metrics::RENDER_COMPOSITOR_CONTEXT);

        if (compositor_surface_context_provider->BindToCurrentThread()
                != gpu::ContextResult::kSuccess) {
            compositor_surface_context_provider = nullptr;
        }
    }

    auto layer_tree_frame_sink =
        std::make_unique<LayerTreeFrameSink>(
            std::move(frame_sink),
            compositor_surface_context_provider,
            worker_context_provider,
            compositor_task_runner,
            context->d_gpu_memory_buffer_manager,
            context->d_shared_bitmap_manager);

    d_layer_tree_frame_sink = layer_tree_frame_sink->AsWeakPtr();
    d_layer_tree_frame_sink->SetLocalSurfaceId(local_surface_id);

    d_display->SetVisible(d_visible);
    d_display->Resize(d_size);
    d_display->SetOutputIsSecure(true);

    callback.Run(std::move(layer_tree_frame_sink));
}

} // close namespace blpwtk2
