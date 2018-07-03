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

#include <base/lazy_instance.h>
#include <base/memory/shared_memory.h>
#include <cc/output/output_surface.h>
#include <cc/output/output_surface_frame.h>
#include <cc/output/renderer_settings.h>
#include <cc/output/software_output_device.h>
#include <cc/output/texture_mailbox_deleter.h>
#include <cc/scheduler/begin_frame_source.h>
#include <cc/scheduler/delay_based_time_source.h>
#include <cc/surfaces/direct_compositor_frame_sink.h>
#include <cc/surfaces/display.h>
#include <cc/surfaces/display_scheduler.h>
#include <cc/surfaces/surface_id_allocator.h>
#include <cc/surfaces/surface_manager.h>
#include <content/child/child_gpu_memory_buffer_manager.h>
#include <content/child/child_shared_bitmap_manager.h>
#include <content/child/thread_safe_sender.h>
#include <content/common/gpu/client/command_buffer_metrics.h>
#include <content/common/gpu/client/context_provider_command_buffer.h>
#include <content/renderer/render_thread_impl.h>
#include <gpu/command_buffer/client/context_support.h>
#include <gpu/command_buffer/client/gles2_interface.h>
#include <gpu/command_buffer/client/gpu_memory_buffer_manager.h>
#include <gpu/ipc/client/command_buffer_proxy_impl.h>
#include <skia/ext/platform_canvas.h>
#include <ui/compositor/compositor_vsync_manager.h>
#include <ui/gfx/vsync_provider.h>

#include <algorithm>
#include <numeric>
#include <set>

namespace blpwtk2 {

namespace {

static base::LazyInstance<RenderCompositorContext> s_instance = LAZY_INSTANCE_INITIALIZER;

} // close namespace

class OutputSurfaceForDisplay : public cc::OutputSurface,
                                private ui::CompositorVSyncManager::Observer {
  public:

    ~OutputSurfaceForDisplay() override {
        d_vsync_manager->RemoveObserver(this);
    }

    // cc::OutputSurface overrides:
    void BindToClient(cc::OutputSurfaceClient* client) override {
        d_client = client;

        d_vsync_manager->AddObserver(this);
    }

    cc::OverlayCandidateValidator* GetOverlayCandidateValidator() const override {
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
            interval = cc::BeginFrameArgs::DefaultInterval();
        }

        d_begin_frame_source->OnUpdateVSyncParameters(timebase, interval);
    }

  protected:

    OutputSurfaceForDisplay(
        scoped_refptr<cc::ContextProvider>         context_provider,
        scoped_refptr<ui::CompositorVSyncManager>  vsync_manager,
        cc::SyntheticBeginFrameSource             *begin_frame_source)
    : cc::OutputSurface(context_provider)
    , d_vsync_manager(vsync_manager)
    , d_begin_frame_source(begin_frame_source) {
    }

    OutputSurfaceForDisplay(
        std::unique_ptr<cc::SoftwareOutputDevice>  software_device,
        scoped_refptr<ui::CompositorVSyncManager>  vsync_manager,
        cc::SyntheticBeginFrameSource             *begin_frame_source)
    : cc::OutputSurface(std::move(software_device))
    , d_vsync_manager(vsync_manager)
    , d_begin_frame_source(begin_frame_source) {
    }

    void UpdateVSyncParameters(
        base::TimeTicks timebase,
        base::TimeDelta interval) {
        d_vsync_manager->UpdateVSyncParameters(timebase, interval);
    }

  protected:

    cc::OutputSurfaceClient* d_client = nullptr;

  private:

    scoped_refptr<ui::CompositorVSyncManager> d_vsync_manager;

    cc::SyntheticBeginFrameSource *d_begin_frame_source;
};

// GpuOutputSurfaceForDisplay
//
// This is the cc::OutputSurface that is passed to the cc::Display. It is
// fairly similar to content::GpuBrowserCompositorOutputSurface.
class GpuOutputSurfaceForDisplay : public OutputSurfaceForDisplay {
  public:

    GpuOutputSurfaceForDisplay(
        scoped_refptr<content::ContextProviderCommandBuffer>  context_provider,
        scoped_refptr<ui::CompositorVSyncManager>             vsync_manager,
        cc::SyntheticBeginFrameSource                        *begin_frame_source)
    : OutputSurfaceForDisplay(context_provider,
                              vsync_manager, begin_frame_source) {
    }

    // cc::OutputSurface overrides:
    void BindToClient(cc::OutputSurfaceClient* client) override {
        OutputSurfaceForDisplay::BindToClient(client);

        command_buffer_proxy()->SetSwapBuffersCompletionCallback(
            base::Bind(
                &GpuOutputSurfaceForDisplay::OnGpuSwapBuffersCompleted,
                base::Unretained(this)));

        command_buffer_proxy()->SetUpdateVSyncParametersCallback(
            base::Bind(
                &GpuOutputSurfaceForDisplay::OnGpuUpdateVSyncParameters,
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
                 bool has_alpha) override {
        context_provider()->ContextGL()->ResizeCHROMIUM(
            size.width(), size.height(), device_scale_factor, has_alpha);
    }

    void SwapBuffers(cc::OutputSurfaceFrame frame) override {
        command_buffer_proxy()->SetLatencyInfo(frame.latency_info);

        if (frame.sub_buffer_rect == gfx::Rect(frame.size)) {
            context_provider()->ContextSupport()->Swap();
        }
        else {
            context_provider()->ContextSupport()->PartialSwapBuffers(
                frame.sub_buffer_rect);
        }
    }

    bool IsDisplayedAsOverlayPlane() const override {
        return false;
    }

    unsigned GetOverlayTextureId() const override {
        return 0;
    }

    bool SurfaceIsSuspendForRecycle() const override {
        return false;
    }

    uint32_t GetFramebufferCopyTextureFormat() override {
        return provider_command_buffer()->GetCopyTextureInternalFormat();
    }

  private:

    content::ContextProviderCommandBuffer* provider_command_buffer() {
        return static_cast<content::ContextProviderCommandBuffer*>(context_provider_.get());
    }

    gpu::CommandBufferProxyImpl* command_buffer_proxy() {
        auto command_buffer_proxy = provider_command_buffer()->GetCommandBufferProxy();
        DCHECK(command_buffer_proxy);

        return command_buffer_proxy;
    }

    void OnGpuSwapBuffersCompleted(
        const std::vector<ui::LatencyInfo>& latency_info,
        gfx::SwapResult result,
        const gpu::GpuProcessHostedCALayerTreeParamsMac* params_mac) {
        d_client->DidReceiveSwapBuffersAck();
    }

    void OnGpuUpdateVSyncParameters(
        base::TimeTicks timebase,
        base::TimeDelta interval) {
        UpdateVSyncParameters(timebase, interval);
    }
};

class SoftwareBackingManagerClient {
  public:

    virtual gfx::Size GetSize() const = 0;
    virtual void ReleaseBacking() = 0;
};

class SoftwareBackingManager {
  public:

    void Register(SoftwareBackingManagerClient *device) {
        DCHECK(d_devices.find(device) == d_devices.end());
        d_devices.insert(device);
    }

    void Unregister(SoftwareBackingManagerClient *device) {
        DCHECK(d_devices.find(device) != d_devices.end());
        d_devices.erase(device);

        ResetBacking();
    }

    base::SharedMemory* GetBacking(const gfx::Size& size) {
        if (d_backing) {
            return d_backing.get();
        }

        std::size_t size_in_bytes = 0;
        if (!cc::SharedBitmap::SizeInBytes(size, &size_in_bytes)) {
            return nullptr;
        }

        auto required_size_in_bytes = ComputeRequiredSizeInBytes();
        if (size_in_bytes > required_size_in_bytes) {
            return nullptr;
        }

        d_backing_size_in_bytes = required_size_in_bytes;
        d_backing.reset(
            new base::SharedMemory());
        base::debug::Alias(&required_size_in_bytes);

        auto success = d_backing->CreateAnonymous(d_backing_size_in_bytes);
        CHECK(success);

        return d_backing.get();
    }

    void ResetBacking() {
        if (ComputeRequiredSizeInBytes() == d_backing_size_in_bytes) {
            return;
        }

        std::for_each(
            d_devices.begin(), d_devices.end(),
            [](SoftwareBackingManagerClient *device) -> void {
                device->ReleaseBacking();
            });

        d_backing.reset();
        d_backing_size_in_bytes = 0;
    }

  private:

    // Computes a byte size that satisfies all devices:
    std::size_t ComputeRequiredSizeInBytes() {
        constexpr std::size_t kMinSizeInBytes = 1;

        return std::accumulate(
            d_devices.begin(), d_devices.end(),
            kMinSizeInBytes,
            [](std::size_t required_size_in_bytes, SoftwareBackingManagerClient *device) -> std::size_t {
                constexpr std::size_t kMaxSizeInBytes = 4 * (16384 * 8192);

                std::size_t size_in_bytes = 0;

                if (!cc::SharedBitmap::SizeInBytes(
                        device->GetSize(),
                        &size_in_bytes)) {
					return required_size_in_bytes;
                }

                if (size_in_bytes > kMaxSizeInBytes) {
					return required_size_in_bytes;
                }

                return std::max(required_size_in_bytes, size_in_bytes);
            });
    }

    std::set<SoftwareBackingManagerClient *> d_devices;
    std::unique_ptr<base::SharedMemory> d_backing;
    std::size_t d_backing_size_in_bytes = 0;
};

class HwndSoftwareOutputDevice : public cc::SoftwareOutputDevice,
                                 private SoftwareBackingManagerClient {
  public:

    HwndSoftwareOutputDevice(SoftwareBackingManager& backing_manager, HWND hwnd)
    : d_backing_manager(backing_manager)
    , d_hwnd(hwnd) {
        d_backing_manager.Register(this);
    }

    ~HwndSoftwareOutputDevice() override {
        d_backing_manager.Unregister(this);
    }

    // cc::SoftwareOutputDevice overrides:
    void Resize(const gfx::Size& size,
                float scale_factor) override {
        if (viewport_pixel_size_ != size) {
            viewport_pixel_size_ = size;

            d_backing_manager.ResetBacking();
            d_contents.reset();
        }
    }

    SkCanvas *BeginPaint(const gfx::Rect& damage) override {
        damage_rect_ = damage;

        if (!d_contents) {
            auto memory = d_backing_manager.GetBacking(viewport_pixel_size_);
            if (memory) {
                d_contents = sk_sp<SkCanvas>(
                    skia::CreatePlatformCanvas(
                        viewport_pixel_size_.width(), viewport_pixel_size_.height(),
                        true,
                        memory->handle().GetHandle(), skia::CRASH_ON_FAILURE));
            }
        }

        return d_contents.get();
    }

    void EndPaint() override {
        if (!d_contents) {
            return;
        }

        gfx::Rect dest_rect = damage_rect_;
        dest_rect.Intersect(gfx::Rect(viewport_pixel_size_));
        if (dest_rect.IsEmpty()) {
            return;
        }

        auto dc = GetDC(d_hwnd);
        auto dest_RECT = dest_rect.ToRECT();
        skia::DrawToNativeContext(
            d_contents.get(),
            dc,
            dest_rect.x(), dest_rect.y(),
            &dest_RECT);

        ReleaseDC(d_hwnd, dc);
    }

    // SoftwareBackingManagerClient overrides:
    gfx::Size GetSize() const override {
        return viewport_pixel_size_;
    }

    void ReleaseBacking() override {
        d_contents.reset();
    }

  private:

    SoftwareBackingManager& d_backing_manager;
    HWND                    d_hwnd;

    sk_sp<SkCanvas>         d_contents;
};

class SoftwareOutputSurfaceForDisplay : public OutputSurfaceForDisplay,
                                        public base::SupportsWeakPtr<SoftwareOutputSurfaceForDisplay> {
  public:
    SoftwareOutputSurfaceForDisplay(
        std::unique_ptr<cc::SoftwareOutputDevice>    software_device,
        scoped_refptr<ui::CompositorVSyncManager>    vsync_manager,
        cc::SyntheticBeginFrameSource               *begin_frame_source,
        scoped_refptr<base::SingleThreadTaskRunner>  task_runner)
    : OutputSurfaceForDisplay(std::move(software_device),
                              vsync_manager, begin_frame_source)
    , d_task_runner(task_runner) {
    }

    // cc::OutputSurface overrides:
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
        bool has_alpha) override {
      software_device()->Resize(size, device_scale_factor);
    }

    void SwapBuffers(cc::OutputSurfaceFrame frame) override {
        gfx::VSyncProvider* vsync_provider = software_device()->GetVSyncProvider();
        if (vsync_provider) {
            vsync_provider->GetVSyncParameters(
                base::Bind(
                    &SoftwareOutputSurfaceForDisplay::OnSoftwareUpdateVSyncParameters,
                    AsWeakPtr()));
        }
        d_task_runner->PostTask(
            FROM_HERE,
                base::Bind(&SoftwareOutputSurfaceForDisplay::SwapBuffersImpl,
                    AsWeakPtr()));
    }

    bool IsDisplayedAsOverlayPlane() const override {
        return false;
    }

    unsigned GetOverlayTextureId() const override {
        return 0;
    }

    bool SurfaceIsSuspendForRecycle() const override {
        return false;
    }

    uint32_t GetFramebufferCopyTextureFormat() override {
        NOTREACHED();
        return 0;
    }

  private:

    void OnSoftwareUpdateVSyncParameters(
        base::TimeTicks timebase,
        base::TimeDelta interval) {
        UpdateVSyncParameters(timebase, interval);
    }

    void SwapBuffersImpl() {
        d_client->DidReceiveSwapBuffersAck();
    }

    scoped_refptr<base::SingleThreadTaskRunner> d_task_runner;
};

class CompositorFrameSink : public cc::CompositorFrameSink,
                            public base::SupportsWeakPtr<CompositorFrameSink> {
  public:

    CompositorFrameSink(
        std::unique_ptr<cc::CompositorFrameSink>  delegate,
        scoped_refptr<cc::ContextProvider>        context_provider,
        scoped_refptr<cc::ContextProvider>        worker_context_provider,
        gpu::GpuMemoryBufferManager              *gpu_memory_buffer_manager,
        cc::SharedBitmapManager                  *shared_bitmap_manager);
    ~CompositorFrameSink() override;

    // cc::CompositorFrameSink overrides:
    bool BindToClient(cc::CompositorFrameSinkClient* client) override;
    void DetachFromClient() override;
    void SubmitCompositorFrame(cc::CompositorFrame frame) override;

    void OnRenderCompositorDestroyed();

  private:

    std::unique_ptr<cc::CompositorFrameSink>  d_delegate;
};

CompositorFrameSink::CompositorFrameSink(
    std::unique_ptr<cc::CompositorFrameSink>  delegate,
    scoped_refptr<cc::ContextProvider>  context_provider,
    scoped_refptr<cc::ContextProvider>  worker_context_provider,
    gpu::GpuMemoryBufferManager        *gpu_memory_buffer_manager,
    cc::SharedBitmapManager            *shared_bitmap_manager)
: cc::CompositorFrameSink(context_provider, worker_context_provider, gpu_memory_buffer_manager, shared_bitmap_manager)
, d_delegate(std::move(delegate))
{
}

CompositorFrameSink::~CompositorFrameSink()
{
}

bool CompositorFrameSink::BindToClient(cc::CompositorFrameSinkClient* client)
{
    if (!cc::CompositorFrameSink::BindToClient(client)) {
        return false;
    }

    if (d_delegate) {
        return d_delegate->BindToClient(client);
    }

    return true;
}

void CompositorFrameSink::DetachFromClient()
{
    if (d_delegate) {
        d_delegate->DetachFromClient();
    }

    cc::CompositorFrameSink::DetachFromClient();
}

void CompositorFrameSink::SubmitCompositorFrame(cc::CompositorFrame frame)
{
    if (!d_delegate) {
        return;
    }

    d_delegate->SubmitCompositorFrame(std::move(frame));
}

void CompositorFrameSink::OnRenderCompositorDestroyed()
{
    if (d_delegate && d_delegate->HasClient()) {
        d_delegate->DetachFromClient();
    }

    d_delegate.reset();
}

RenderCompositorContext::RenderCompositorContext()
{
    auto render_thread = content::RenderThreadImpl::current();

    d_details.reset(new Details());

    render_thread->compositor_task_runner()->
        PostTask(
            FROM_HERE,
            base::Bind(&RenderCompositorContext::Details::ConstructImpl,
                base::Unretained(d_details.get()),
                render_thread->GetSharedBitmapManager(),
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
    if (s_instance == nullptr) {
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

std::unique_ptr<cc::CompositorFrameSink> RenderCompositorContext::CreateUncorrelatedCompositorFrameSink()
{
    base::WaitableEvent event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    std::unique_ptr<CompositorFrameSink> result;

    content::RenderThreadImpl::current()->compositor_task_runner()->
        PostTask(
            FROM_HERE,
            base::Bind(&RenderCompositorContext::Details::CreateUncorrelatedCompositorFrameSinkImpl,
                base::Unretained(d_details.get()),
                &result,
                &event));

    event.Wait();

    return result;
}

void RenderCompositorContext::Details::ConstructImpl(
    cc::SharedBitmapManager *shared_bitmap_manager,
    gpu::GpuMemoryBufferManager *gpu_memory_buffer_manager)
{
    d_shared_bitmap_manager = shared_bitmap_manager;
    d_gpu_memory_buffer_manager = gpu_memory_buffer_manager;

    d_renderer_settings.reset(new cc::RendererSettings());
    d_renderer_settings->partial_swap_enabled = true;
    d_renderer_settings->finish_rendering_on_resize = true;

    d_surface_manager.reset(new cc::SurfaceManager());

    d_software_backing_manager.reset(new SoftwareBackingManager());
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

        gpu::gles2::ContextCreationAttribHelper attributes;
        attributes.alpha_size                      = -1;
        attributes.depth_size                      = 0;
        attributes.stencil_size                    = 0;
        attributes.samples                         = 0;
        attributes.sample_buffers                  = 0;
        attributes.bind_generates_resource         = false;
        attributes.lose_context_when_out_of_memory = true;

        d_worker_context_provider = new content::ContextProviderCommandBuffer(
            gpu_channel,
            gpu::GPU_STREAM_DEFAULT,
            gpu::GpuStreamPriority::NORMAL,
            gpu::kNullSurfaceHandle,
            GURL("chrome://gpu/RenderCompositorContext::EstablishPrivilegedGpuChannel"),
            automatic_flushes,
            support_locking,
            gpu::SharedMemoryLimits(),
            attributes,
            nullptr,
            content::command_buffer_metrics::RENDER_WORKER_CONTEXT);

        if (!d_worker_context_provider->BindToCurrentThread()) {
            d_worker_context_provider = nullptr;
        }
    }
}

void RenderCompositorContext::Details::CreateUncorrelatedCompositorFrameSinkImpl(
    std::unique_ptr<CompositorFrameSink> *result,
    base::WaitableEvent *event)
{
    auto compositor_output_surface =
        std::make_unique<CompositorFrameSink>(
            nullptr,
            nullptr,
            nullptr,
            d_gpu_memory_buffer_manager,
            d_shared_bitmap_manager);

    *result = std::move(compositor_output_surface);

    event->Signal();
}

std::unique_ptr<RenderCompositor> RenderCompositorContext::CreateCompositor(gpu::SurfaceHandle gpu_surface_handle)
{
    return std::unique_ptr<RenderCompositor>(
        new RenderCompositor(this, gpu_surface_handle));
}

base::Optional<std::unique_ptr<cc::CompositorFrameSink>> RenderCompositorContext::CreateCompositorFrameSink(int routing_id)
{
    auto it = d_compositors_by_routing_id.find(routing_id);
    if (it == d_compositors_by_routing_id.end()) {
        return base::make_optional(
            CreateUncorrelatedCompositorFrameSink());
    }

    return base::make_optional(
        it->second->CreateCompositorFrameSink());
}

RenderCompositor::RenderCompositor(
    RenderCompositorContext *context, gpu::SurfaceHandle gpu_surface_handle)
: d_context(context)
, d_gpu_surface_handle(gpu_surface_handle)
{
    auto gpu_channel =
        context->EstablishPrivilegedGpuChannel();

    d_details.reset(new Details());

    content::RenderThreadImpl::current()->compositor_task_runner()->
        PostTask(
            FROM_HERE,
            base::Bind(&RenderCompositor::Details::ConstructImpl,
                base::Unretained(d_details.get()),
                std::move(gpu_channel),
                content::RenderThreadImpl::current()->compositor_task_runner(),
                context->d_details.get(),
                gpu_surface_handle));
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

void RenderCompositor::Resize(const gfx::Size& size)
{
    content::RenderThreadImpl::current()->compositor_task_runner()->
        PostTask(
            FROM_HERE,
            base::Bind(&RenderCompositor::Details::ResizeImpl,
                base::Unretained(d_details.get()),
                size));
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

std::unique_ptr<cc::CompositorFrameSink> RenderCompositor::CreateCompositorFrameSink()
{
    base::WaitableEvent event(
        base::WaitableEvent::ResetPolicy::AUTOMATIC,
        base::WaitableEvent::InitialState::NOT_SIGNALED);

    auto gpu_channel = d_context->EstablishPrivilegedGpuChannel();

    std::unique_ptr<CompositorFrameSink> result;

    content::RenderThreadImpl::current()->compositor_task_runner()->
        PostTask(
            FROM_HERE,
            base::Bind(&RenderCompositor::Details::CreateCompositorFrameSinkImpl,
                base::Unretained(d_details.get()),
                std::move(gpu_channel),
                content::RenderThreadImpl::current()->compositor_task_runner(),
                d_context->d_details.get(),
                d_gpu_surface_handle,
                &result,
                &event));

    event.Wait();

    return result;
}

RenderCompositor::Details::~Details()
{
    if (d_output_surface) {
        d_output_surface->OnRenderCompositorDestroyed();
    }
}

void RenderCompositor::Details::ConstructImpl(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    RenderCompositorContext::Details *context,
    gpu::SurfaceHandle gpu_surface_handle)
{
    d_frame_sink_id = cc::FrameSinkId(0, context->d_next_frame_sink_id++);

    d_vsync_manager = new ui::CompositorVSyncManager();

    auto worker_context_provider = context->d_worker_context_provider;
}

void RenderCompositor::Details::SetVisibleImpl(bool visible)
{
    d_visible = visible;

    if (d_display) {
        d_display->SetVisible(d_visible);
    }
}

void RenderCompositor::Details::ResizeImpl(const gfx::Size& size)
{
    d_size = size;

    if (d_display) {
        d_display->Resize(d_size);
    }
}

void RenderCompositor::Details::CreateCompositorFrameSinkImpl(
    scoped_refptr<gpu::GpuChannelHost> gpu_channel,
    scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
    RenderCompositorContext::Details *context,
    gpu::SurfaceHandle gpu_surface_handle,
    std::unique_ptr<CompositorFrameSink> *result,
    base::WaitableEvent *event)
{
    if (d_output_surface) {
        d_output_surface->OnRenderCompositorDestroyed();
    }

    auto worker_context_provider = context->d_worker_context_provider;

    scoped_refptr<content::ContextProviderCommandBuffer> context_provider;

    if (worker_context_provider) {
        constexpr bool automatic_flushes = false;
        constexpr bool support_locking   = false;

        gpu::gles2::ContextCreationAttribHelper attributes;
        attributes.alpha_size                      = -1;
        attributes.depth_size                      = 0;
        attributes.stencil_size                    = 0;
        attributes.samples                         = 0;
        attributes.sample_buffers                  = 0;
        attributes.bind_generates_resource         = false;
        attributes.lose_context_when_out_of_memory = true;

        context_provider = new content::ContextProviderCommandBuffer(
            gpu_channel,
            gpu::GPU_STREAM_DEFAULT,
            gpu::GpuStreamPriority::NORMAL,
            gpu_surface_handle,
            GURL("chrome://gpu/RenderCompositorContext::CreateCompositor"),
            automatic_flushes,
            support_locking,
            gpu::SharedMemoryLimits(),
            attributes,
            worker_context_provider.get(),
            content::command_buffer_metrics::DISPLAY_COMPOSITOR_ONSCREEN_CONTEXT);

        if (!context_provider->BindToCurrentThread()) {
            context_provider = nullptr;
        }
    }

    // cc::BeginFrameSource:
    std::unique_ptr<cc::SyntheticBeginFrameSource> begin_frame_source;

    if (!context->d_renderer_settings->disable_display_vsync) {
        begin_frame_source =
            std::move(
                std::make_unique<cc::DelayBasedBeginFrameSource>(
                    std::make_unique<cc::DelayBasedTimeSource>(
                        compositor_task_runner.get())));
    }
    else {
        begin_frame_source =
            std::move(
                std::make_unique<cc::BackToBackBeginFrameSource>(
                    std::make_unique<cc::DelayBasedTimeSource>(
                        compositor_task_runner.get())));
    }

    // cc::OutputSurface for the display:
    std::unique_ptr<cc::OutputSurface> display_output_surface;

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
                std::move(
                    std::make_unique<HwndSoftwareOutputDevice>(
                        *context->d_software_backing_manager,
                        gpu_surface_handle)),
                d_vsync_manager,
                begin_frame_source.get(),
                compositor_task_runner);
    }

    // cc::DisplayScheduler:
    auto display_scheduler =
        std::make_unique<cc::DisplayScheduler>(
            begin_frame_source.get(),
            compositor_task_runner.get(),
            display_output_surface->capabilities().max_frames_pending);

    // cc::TextureMailboxDeleter:
    auto texture_mailbox_deleter =
        std::make_unique<cc::TextureMailboxDeleter>(
            compositor_task_runner.get());

    // cc::Display:
    d_display.reset(
        new cc::Display(
            context->d_shared_bitmap_manager,
            context->d_gpu_memory_buffer_manager,
            *context->d_renderer_settings,
            d_frame_sink_id,
            std::move(begin_frame_source),
            std::move(display_output_surface),
            std::move(display_scheduler),
            std::move(texture_mailbox_deleter)));

    // cc::OutputSurface:
    auto frame_sink =
        std::make_unique<cc::DirectCompositorFrameSink>(
            d_frame_sink_id,
            context->d_surface_manager.get(),
            d_display.get(),
            context_provider,
            worker_context_provider,
            context->d_gpu_memory_buffer_manager,
            context->d_shared_bitmap_manager);

    scoped_refptr<content::ContextProviderCommandBuffer> compositor_surface_context_provider;

    if (worker_context_provider) {
        constexpr bool automatic_flushes = false;
        constexpr bool support_locking = false;

        gpu::gles2::ContextCreationAttribHelper attributes;
        attributes.alpha_size                      = -1;
        attributes.depth_size                      = 0;
        attributes.stencil_size                    = 0;
        attributes.samples                         = 0;
        attributes.sample_buffers                  = 0;
        attributes.bind_generates_resource         = false;
        attributes.lose_context_when_out_of_memory = true;

        compositor_surface_context_provider =
            new content::ContextProviderCommandBuffer(
                gpu_channel,
                gpu::GPU_STREAM_DEFAULT,
                gpu::GpuStreamPriority::NORMAL,
                gpu::kNullSurfaceHandle,
                GURL("chrome://gpu/RenderCompositor::CreateCompositorFrameSink"),
                automatic_flushes,
                support_locking,
                gpu::SharedMemoryLimits::ForMailboxContext(),
                attributes,
                worker_context_provider.get(),
                content::command_buffer_metrics::RENDER_COMPOSITOR_CONTEXT);

        if (!compositor_surface_context_provider->BindToCurrentThread()) {
            compositor_surface_context_provider = nullptr;
        }
    }

    auto compositor_output_surface =
        std::make_unique<CompositorFrameSink>(
            std::move(frame_sink),
            compositor_surface_context_provider,
            worker_context_provider,
            context->d_gpu_memory_buffer_manager,
            context->d_shared_bitmap_manager);

    d_output_surface = compositor_output_surface->AsWeakPtr();

    d_display->SetVisible(d_visible);
    d_display->Resize(d_size);

    *result = std::move(compositor_output_surface);

    event->Signal();
}

} // close namespace blpwtk2
