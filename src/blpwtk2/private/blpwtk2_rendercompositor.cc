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

#include <blpwtk2_profileimpl.h>

#include <algorithm>
#include <map>
#include <memory>

#include <base/debug/alias.h>
#include <base/lazy_instance.h>
#include <base/memory/ref_counted.h>
#include <base/message_loop/message_loop.h>
#include <cc/trees/layer_tree_frame_sink_client.h>
#include <components/viz/common/display/renderer_settings.h>
#include <components/viz/common/frame_sinks/begin_frame_source.h>
#include <components/viz/common/frame_sinks/delay_based_time_source.h>
#include <components/viz/common/surfaces/frame_sink_id.h>
#include <components/viz/common/surfaces/parent_local_surface_id_allocator.h>
#include <components/viz/host/host_frame_sink_client.h>
#include <components/viz/host/host_frame_sink_manager.h>
#include <components/viz/host/renderer_settings_creation.h>
#include <components/viz/service/display/display.h>
#include <components/viz/service/display_embedder/compositor_overlay_candidate_validator.h>
#include <components/viz/service/display_embedder/output_device_backing.h>
#include <components/viz/service/display_embedder/server_shared_bitmap_manager.h>
#include <components/viz/service/display_embedder/software_output_device_win.h>
#include <components/viz/service/display/display_scheduler.h>
#include <components/viz/service/frame_sinks/direct_layer_tree_frame_sink.h>
#include <components/viz/service/frame_sinks/frame_sink_manager_impl.h>
#include <content/browser/compositor/gpu_browser_compositor_output_surface.h>
#include <content/browser/compositor/software_browser_compositor_output_surface.h>
#include <content/common/gpu_stream_constants.h>
#include <content/common/render_frame_metadata.mojom.h>
#include <content/renderer/render_thread_impl.h>
#include <services/viz/public/interfaces/compositing/compositor_frame_sink.mojom.h>
#include <services/ws/public/cpp/gpu/context_provider_command_buffer.h>
#include <ui/compositor/compositor_vsync_manager.h>

namespace blpwtk2 {

RenderFrameSinkProvider::~RenderFrameSinkProvider()
{
}

RenderCompositor::~RenderCompositor()
{
}

class RenderCompositorImpl;

class RenderFrameSinkProviderImpl : public RenderFrameSinkProvider,
                                    private content::mojom::FrameSinkProvider {
  private:

    friend RenderCompositorImpl;

    mojo::Binding<content::mojom::FrameSinkProvider> d_binding;
    std::map<int, RenderCompositorImpl *> d_compositors_by_widget_id;

    scoped_refptr<base::SingleThreadTaskRunner> d_task_runner;

    gpu::GpuMemoryBufferManager *d_gpu_memory_buffer_manager = nullptr;
    std::unique_ptr<viz::SharedBitmapManager> d_shared_bitmap_manager;
    std::unique_ptr<viz::FrameSinkManagerImpl> d_frame_sink_manager;
    std::unique_ptr<viz::HostFrameSinkManager> d_host_frame_sink_manager;
    std::unique_ptr<viz::RendererSettings> d_renderer_settings;
    std::unique_ptr<viz::OutputDeviceBacking> d_software_output_device_backing;

    uint32_t d_next_frame_sink_id = 1u;

    scoped_refptr<gpu::GpuChannelHost> d_gpu_channel;
    scoped_refptr<ws::ContextProviderCommandBuffer> d_worker_context_provider;

  public:

    RenderFrameSinkProviderImpl();
    ~RenderFrameSinkProviderImpl() override;

    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager() { return d_gpu_memory_buffer_manager; }
    viz::SharedBitmapManager* shared_bitmap_manager() { return d_shared_bitmap_manager.get(); }
    viz::OutputDeviceBacking* software_output_device_backing() { return d_software_output_device_backing.get(); }

    // RenderFrameSinkProvider overrides:
    void Bind(content::mojom::FrameSinkProviderRequest request) override;
    void Unbind() override;

    std::unique_ptr<RenderCompositor> CreateCompositor(
        int32_t widget_id,
        gpu::SurfaceHandle gpu_surface_handle,
        blpwtk2::ProfileImpl *profile) override;

  private:

    // content::mojom::FrameSinkProvider overrides:
    void CreateForWidget(
        int32_t widget_id,
        viz::mojom::CompositorFrameSinkRequest compositor_frame_sink_request,
        viz::mojom::CompositorFrameSinkClientPtr compositor_frame_sink_client) override;
    void RegisterRenderFrameMetadataObserver(
        int32_t widget_id,
        content::mojom::RenderFrameMetadataObserverClientRequest render_frame_metadata_observer_client_request,
        content::mojom::RenderFrameMetadataObserverPtr observer) override;
};

class RenderCompositorImpl : public RenderCompositor,
                             private viz::mojom::CompositorFrameSink,
                             private viz::HostFrameSinkClient,
                             private cc::LayerTreeFrameSinkClient,
                             private viz::BeginFrameObserver {
private:

    mojo::Binding<viz::mojom::CompositorFrameSink> d_binding;
    RenderFrameSinkProviderImpl& d_frame_sink_provider;
    int32_t d_widget_id;
    viz::FrameSinkId d_frame_sink_id;
    gpu::SurfaceHandle d_gpu_surface_handle;
    blpwtk2::ProfileImpl *d_profile;
    scoped_refptr<ui::CompositorVSyncManager> d_vsync_manager;
    std::unique_ptr<viz::ParentLocalSurfaceIdAllocator> d_local_surface_id_allocator;

    viz::mojom::CompositorFrameSinkClientPtr d_client;

    std::unique_ptr<viz::SyntheticBeginFrameSource> d_begin_frame_source;
    std::unique_ptr<viz::Display> d_display;
    std::unique_ptr<cc::LayerTreeFrameSink> d_layer_tree_frame_sink;

    bool d_visible = false;
    gfx::Size d_size;

    viz::BeginFrameSource *d_delegated_begin_frame_source = nullptr;
    viz::BeginFrameArgs d_last_begin_frame_args;
    bool d_client_needs_begin_frame = false;
    bool d_added_frame_observer = false;
    bool d_client_wants_animate_only_begin_frames = false;
    std::vector<viz::ReturnedResource> d_resources_to_reclaim;

public:

    RenderCompositorImpl(
        RenderFrameSinkProviderImpl &,
        int32_t widget_id,
        const viz::FrameSinkId& frame_sink_id,
        gpu::SurfaceHandle gpu_surface_handle,
        blpwtk2::ProfileImpl *profile);
    ~RenderCompositorImpl() override;

    void CreateFrameSink(
        viz::mojom::CompositorFrameSinkRequest compositor_frame_sink_request,
        viz::mojom::CompositorFrameSinkClientPtr compositor_frame_sink_client);

    // RenderCompositor overrides:
    viz::LocalSurfaceId GetLocalSurfaceId() override;
    void SetVisible(bool visible) override;
    void Resize(const gfx::Size& size) override;

private:

    void UpdateNeedsBeginFrameSource();
    void UpdateVSyncParameters(base::TimeTicks timebase, base::TimeDelta interval);

    // viz::mojom::CompositorFrameSink overrides:
    void SetNeedsBeginFrame(bool needs_begin_frame) override;
    void SetWantsAnimateOnlyBeginFrames() override;
    void SubmitCompositorFrame(
        const viz::LocalSurfaceId& local_surface_id,
        viz::CompositorFrame frame,
        base::Optional<viz::HitTestRegionList> hit_test_region_list,
        uint64_t submit_time) override;
    void SubmitCompositorFrameSync(
        const viz::LocalSurfaceId& local_surface_id,
        viz::CompositorFrame frame,
        base::Optional<viz::HitTestRegionList> hit_test_region_list,
        uint64_t submit_time,
        const SubmitCompositorFrameSyncCallback callback) override;
    void DidNotProduceFrame(const viz::BeginFrameAck& ack) override;
    void DidAllocateSharedBitmap(mojo::ScopedSharedBufferHandle buffer,
                                 const viz::SharedBitmapId& id) override;
    void DidDeleteSharedBitmap(const viz::SharedBitmapId& id) override;

    // viz::HostFrameSinkClient overrides:
    void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override {}
    void OnFrameTokenChanged(uint32_t frame_token) override {}

    // cc::LayerTreeFrameSinkClient overrides:
    void SetBeginFrameSource(viz::BeginFrameSource* source) override;
    base::Optional<viz::HitTestRegionList> BuildHitTestData() override { return base::Optional<viz::HitTestRegionList>(); }
    void ReclaimResources(const std::vector<viz::ReturnedResource>& resources) override;
    void SetTreeActivationCallback(const base::Closure& callback) override {}
    void DidReceiveCompositorFrameAck() override;
    void DidPresentCompositorFrame(
        uint32_t presentation_token,
        const gfx::PresentationFeedback& feedback) override;
    void DidLoseLayerTreeFrameSink() override {}
    void OnDraw(const gfx::Transform& transform,
                const gfx::Rect& viewport,
                bool resourceless_software_draw,
                bool skip_draw) override {}
    void SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) override {}
    void SetExternalTilePriorityConstraints(
        const gfx::Rect& viewport_rect,
        const gfx::Transform& transform) override {}

    // viz::BeginFrameObserver implementation:
    void OnBeginFrame(const viz::BeginFrameArgs& args) override;
    const viz::BeginFrameArgs& LastUsedBeginFrameArgs() const override;
    void OnBeginFrameSourcePausedChanged(bool paused) override;
    bool WantsAnimateOnlyBeginFrames() const override;
};

RenderFrameSinkProviderImpl::RenderFrameSinkProviderImpl()
: d_binding(this)
, d_task_runner(base::MessageLoop::current()->task_runner())
, d_gpu_memory_buffer_manager(content::RenderThreadImpl::current()->GetGpuMemoryBufferManager())
, d_shared_bitmap_manager(
    new viz::ServerSharedBitmapManager())
, d_frame_sink_manager(
    new viz::FrameSinkManagerImpl(
        d_shared_bitmap_manager.get()))
, d_host_frame_sink_manager(
    new viz::HostFrameSinkManager())
, d_renderer_settings(
    new viz::RendererSettings(viz::CreateRendererSettings()))
, d_software_output_device_backing(
    new viz::OutputDeviceBacking())
{
    d_host_frame_sink_manager->SetLocalManager(d_frame_sink_manager.get());
    d_frame_sink_manager->SetLocalClient(d_host_frame_sink_manager.get());
}

RenderFrameSinkProviderImpl::~RenderFrameSinkProviderImpl()
{
}

void RenderFrameSinkProviderImpl::Bind(content::mojom::FrameSinkProviderRequest request)
{
    d_binding.Bind(std::move(request), d_task_runner);
}

void RenderFrameSinkProviderImpl::Unbind()
{
    d_binding.Close();
}

std::unique_ptr<RenderCompositor> RenderFrameSinkProviderImpl::CreateCompositor(
    int32_t widget_id,
    gpu::SurfaceHandle gpu_surface_handle,
    blpwtk2::ProfileImpl *profile)
{
    auto it = d_compositors_by_widget_id.find(widget_id);
    DCHECK(it == d_compositors_by_widget_id.end());

    return std::make_unique<RenderCompositorImpl>(
        *this, widget_id,
        viz::FrameSinkId(0, d_next_frame_sink_id++),
        gpu_surface_handle, profile);
}

void RenderFrameSinkProviderImpl::CreateForWidget(
    int32_t widget_id,
    viz::mojom::CompositorFrameSinkRequest compositor_frame_sink_request,
    viz::mojom::CompositorFrameSinkClientPtr compositor_frame_sink_client)
{
    auto it = d_compositors_by_widget_id.find(widget_id);
    if (it == d_compositors_by_widget_id.end()) {
        return;
    }

    auto gpu_channel =
        content::RenderThreadImpl::current()->EstablishGpuChannelSync();

    if ((!gpu_channel || gpu_channel != d_gpu_channel) && d_worker_context_provider) {
        d_worker_context_provider = nullptr;
    }

    d_gpu_channel = gpu_channel;

    if (d_gpu_channel && !d_worker_context_provider) {
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

        d_worker_context_provider = new ws::ContextProviderCommandBuffer(
            d_gpu_channel,
            d_gpu_memory_buffer_manager,
            content::kGpuStreamIdDefault,
            content::kGpuStreamPriorityUI,
            gpu::kNullSurfaceHandle,
            GURL("chrome://gpu/RenderCompositorContext::EstablishGpuChannel"),
            automatic_flushes,
            support_locking,
            support_grcontext,
            gpu::SharedMemoryLimits(),
            attributes,
            ws::command_buffer_metrics::ContextType::RENDER_WORKER);

        if (d_worker_context_provider->BindToCurrentThread()
                != gpu::ContextResult::kSuccess) {
            d_worker_context_provider = nullptr;
        }
    }

    it->second->CreateFrameSink(
        std::move(compositor_frame_sink_request), std::move(compositor_frame_sink_client));
}

void RenderFrameSinkProviderImpl::RegisterRenderFrameMetadataObserver(
    int32_t widget_id,
    content::mojom::RenderFrameMetadataObserverClientRequest render_frame_metadata_observer_client_request,
    content::mojom::RenderFrameMetadataObserverPtr observer)
{
}

RenderCompositorImpl::RenderCompositorImpl(
    RenderFrameSinkProviderImpl& frame_sink_provider,
    int32_t widget_id,
    const viz::FrameSinkId& frame_sink_id,
    gpu::SurfaceHandle gpu_surface_handle,
    blpwtk2::ProfileImpl *profile)
: d_binding(this)
, d_frame_sink_provider(frame_sink_provider)
, d_widget_id(widget_id)
, d_frame_sink_id(frame_sink_id)
, d_gpu_surface_handle(gpu_surface_handle)
, d_profile(profile)
, d_vsync_manager(new ui::CompositorVSyncManager())
, d_local_surface_id_allocator(new viz::ParentLocalSurfaceIdAllocator())
{
    d_frame_sink_provider.d_compositors_by_widget_id.insert(
        std::make_pair(d_widget_id, this));

    d_frame_sink_provider.d_host_frame_sink_manager->RegisterFrameSinkId(
        d_frame_sink_id, this);
}

RenderCompositorImpl::~RenderCompositorImpl()
{
    d_profile->unregisterNativeViewForComposition(d_gpu_surface_handle);

    auto it = d_frame_sink_provider.d_compositors_by_widget_id.find(d_widget_id);
    DCHECK(it != d_frame_sink_provider.d_compositors_by_widget_id.end());

    d_frame_sink_provider.d_compositors_by_widget_id.erase(it);

    d_frame_sink_provider.d_host_frame_sink_manager->InvalidateFrameSinkId(
        d_frame_sink_id);

    if (d_begin_frame_source) {
        d_frame_sink_provider.d_frame_sink_manager->
            UnregisterBeginFrameSource(d_begin_frame_source.get());
    }

    if (d_layer_tree_frame_sink) {
        d_layer_tree_frame_sink->DetachFromClient();
    }
}

void RenderCompositorImpl::CreateFrameSink(
    viz::mojom::CompositorFrameSinkRequest compositor_frame_sink_request,
    viz::mojom::CompositorFrameSinkClientPtr compositor_frame_sink_client)
{
    d_profile->unregisterNativeViewForComposition(d_gpu_surface_handle);

    auto task_runner = d_frame_sink_provider.d_task_runner;

    if (d_binding.is_bound()) {
        d_binding.Close();
    }

    d_binding.Bind(std::move(compositor_frame_sink_request), task_runner);

    d_client = std::move(compositor_frame_sink_client);

    if (d_frame_sink_provider.d_gpu_channel) {
        d_profile->registerNativeViewForComposition(d_gpu_surface_handle);
    }

    // Setup the begin frame source:
    if (d_begin_frame_source) {
        d_frame_sink_provider.d_frame_sink_manager->
            UnregisterBeginFrameSource(d_begin_frame_source.get());
    }

    constexpr bool disable_display_vsync = false;

    if (disable_display_vsync) {
        d_begin_frame_source =
            std::make_unique<viz::BackToBackBeginFrameSource>(
                std::make_unique<viz::DelayBasedTimeSource>(
                    task_runner.get()));
    }
    else {
        d_begin_frame_source =
            std::make_unique<viz::DelayBasedBeginFrameSource>(
                std::make_unique<viz::DelayBasedTimeSource>(
                    task_runner.get()),
                    viz::BeginFrameSource::kNotRestartableId);
    }

    d_frame_sink_provider.d_frame_sink_manager->
        RegisterBeginFrameSource(d_begin_frame_source.get(), d_frame_sink_id);

    // The GPU context provider:
    auto worker_context_provider = d_frame_sink_provider.d_worker_context_provider;

    scoped_refptr<ws::ContextProviderCommandBuffer> context_provider;

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

        context_provider = new ws::ContextProviderCommandBuffer(
            d_frame_sink_provider.d_gpu_channel,
            d_frame_sink_provider.d_gpu_memory_buffer_manager,
            content::kGpuStreamIdDefault,
            content::kGpuStreamPriorityUI,
            d_gpu_surface_handle,
            GURL("chrome://gpu/RenderCompositorImpl::CreateFrameSink"),
            automatic_flushes,
            support_locking,
            support_grcontext,
            gpu::SharedMemoryLimits(),
            attributes,
            ws::command_buffer_metrics::ContextType::BROWSER_COMPOSITOR);

        if (context_provider->BindToCurrentThread()
                != gpu::ContextResult::kSuccess) {
            context_provider = nullptr;
            worker_context_provider = nullptr;
        }
    }

    // viz::OutputSurface for the display:
    content::BrowserCompositorOutputSurface::UpdateVSyncParametersCallback
        update_vsync_parameters_callback =
            base::Bind(
                &RenderCompositorImpl::UpdateVSyncParameters,
                base::Unretained(this));

    std::unique_ptr<viz::OutputSurface> display_output_surface;

    if (context_provider && worker_context_provider) {
        display_output_surface =
            std::make_unique<content::GpuBrowserCompositorOutputSurface>(
                context_provider,
                update_vsync_parameters_callback,
                nullptr);
    }
    else {
        display_output_surface =
            std::make_unique<content::SoftwareBrowserCompositorOutputSurface>(
                viz::CreateSoftwareOutputDeviceWinBrowser(
                    d_gpu_surface_handle,
                    d_frame_sink_provider.software_output_device_backing()),
                update_vsync_parameters_callback);
    }

    // viz::DisplayScheduler:
    constexpr bool wait_for_all_pipeline_stages_before_draw = false;

    auto display_scheduler =
        std::make_unique<viz::DisplayScheduler>(
            d_begin_frame_source.get(),
            task_runner.get(),
            display_output_surface->capabilities().max_frames_pending,
            wait_for_all_pipeline_stages_before_draw);

    // viz::Display:
    d_display =
        std::make_unique<viz::Display>(
            d_frame_sink_provider.d_shared_bitmap_manager.get(),
            *d_frame_sink_provider.d_renderer_settings,
            d_frame_sink_id,
            std::move(display_output_surface),
            std::move(display_scheduler),
            task_runner);

    // The frame sink:
    if (d_layer_tree_frame_sink) {
        d_layer_tree_frame_sink->DetachFromClient();
    }

    d_display->Resize(d_size);
    d_display->SetOutputIsSecure(true);

    d_display->SetVisible(d_visible);

    d_layer_tree_frame_sink =
        std::make_unique<viz::DirectLayerTreeFrameSink>(
            d_frame_sink_id,
            d_frame_sink_provider.d_host_frame_sink_manager.get(),
            d_frame_sink_provider.d_frame_sink_manager.get(),
            d_display.get(), nullptr,
            context_provider,
            worker_context_provider,
            task_runner,
            d_frame_sink_provider.d_gpu_memory_buffer_manager,
            false);

    d_layer_tree_frame_sink->BindToClient(this);
}

viz::LocalSurfaceId RenderCompositorImpl::GetLocalSurfaceId()
{
    return d_local_surface_id_allocator->GetCurrentLocalSurfaceId();
}

void RenderCompositorImpl::SetVisible(bool visible)
{
    d_visible = visible;

    if (d_display) {
        d_display->SetVisible(d_visible);
    }
}

void RenderCompositorImpl::Resize(const gfx::Size& size)
{
    if (d_size == size) {
        return;
    }

    d_size = size;

    d_local_surface_id_allocator->GenerateId();

    if (d_display) {
        d_display->Resize(d_size);
    }
}

// viz::mojom::CompositorFrameSink overrides:
void RenderCompositorImpl::SetNeedsBeginFrame(bool needs_begin_frame)
{
    d_client_needs_begin_frame = needs_begin_frame;
    UpdateNeedsBeginFrameSource();
}

void RenderCompositorImpl::SetWantsAnimateOnlyBeginFrames()
{
    d_client_wants_animate_only_begin_frames = true;
}

void RenderCompositorImpl::SubmitCompositorFrame(
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame,
    base::Optional<viz::HitTestRegionList> hit_test_region_list,
    uint64_t submit_time)
{
    d_resources_to_reclaim = viz::TransferableResource::ReturnResources(frame.resource_list);

    d_layer_tree_frame_sink->SubmitCompositorFrame(std::move(frame));
}

void RenderCompositorImpl::SubmitCompositorFrameSync(
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame,
    base::Optional<viz::HitTestRegionList> hit_test_region_list,
    uint64_t submit_time,
    const SubmitCompositorFrameSyncCallback callback)
{
    NOTIMPLEMENTED();
}

void RenderCompositorImpl::DidNotProduceFrame(const viz::BeginFrameAck& ack)
{
    d_layer_tree_frame_sink->DidNotProduceFrame(ack);
}

void RenderCompositorImpl::DidAllocateSharedBitmap(
    mojo::ScopedSharedBufferHandle buffer,
    const viz::SharedBitmapId& id)
{
    d_layer_tree_frame_sink->DidAllocateSharedBitmap(std::move(buffer), id);
}

void RenderCompositorImpl::DidDeleteSharedBitmap(const viz::SharedBitmapId& id)
{
    d_layer_tree_frame_sink->DidDeleteSharedBitmap(id);
}

void RenderCompositorImpl::UpdateNeedsBeginFrameSource()
{
    if (!d_delegated_begin_frame_source) {
        return;
    }

    // We require a begin frame if there's a callback pending, or if the client
    // requested it.
    bool needs_begin_frame =
        d_client_needs_begin_frame;

    if (needs_begin_frame == d_added_frame_observer) {
        return;
    }

    if (needs_begin_frame) {
        d_delegated_begin_frame_source->AddObserver(this);
        d_added_frame_observer = true;
    }
    else {
        d_delegated_begin_frame_source->RemoveObserver(this);
        d_added_frame_observer = false;
    }
}

void RenderCompositorImpl::UpdateVSyncParameters(base::TimeTicks timebase, base::TimeDelta interval)
{
    if (d_begin_frame_source) {
        d_begin_frame_source->OnUpdateVSyncParameters(timebase, interval);
    }

    d_vsync_manager->UpdateVSyncParameters(timebase, interval);
}

// cc::LayerTreeFrameSinkClient overrides:
void RenderCompositorImpl::SetBeginFrameSource(viz::BeginFrameSource* source)
{
    if (d_delegated_begin_frame_source && d_added_frame_observer) {
        d_delegated_begin_frame_source->RemoveObserver(this);
        d_added_frame_observer = false;
    }

    d_delegated_begin_frame_source = source;

    UpdateNeedsBeginFrameSource();
}

void RenderCompositorImpl::ReclaimResources(const std::vector<viz::ReturnedResource>& resources)
{
}

void RenderCompositorImpl::DidReceiveCompositorFrameAck()
{
    if (d_client) {
        d_client->DidReceiveCompositorFrameAck(
            std::move(d_resources_to_reclaim));
    }
}

void RenderCompositorImpl::DidPresentCompositorFrame(
    uint32_t presentation_token,
    const gfx::PresentationFeedback& feedback)
{
    if (d_client) {
        d_client->DidPresentCompositorFrame(presentation_token, feedback);
    }
}

void RenderCompositorImpl::OnBeginFrame(const viz::BeginFrameArgs& args)
{
    d_last_begin_frame_args = args;

    if (d_client && d_client_needs_begin_frame) {
        d_client->OnBeginFrame(args);
    }
}

const viz::BeginFrameArgs& RenderCompositorImpl::LastUsedBeginFrameArgs() const
{
    return d_last_begin_frame_args;
}

void RenderCompositorImpl::OnBeginFrameSourcePausedChanged(bool paused)
{
    if (d_client) {
        d_client->OnBeginFramePausedChanged(paused);
    }
}

bool RenderCompositorImpl::WantsAnimateOnlyBeginFrames() const
{
    return d_client_wants_animate_only_begin_frames;
}

//
namespace {

static base::LazyInstance<RenderFrameSinkProviderImpl>::DestructorAtExit
    s_render_frame_sink_provider_instance = LAZY_INSTANCE_INITIALIZER;

} // close namespace

RenderFrameSinkProvider* RenderFrameSinkProvider::GetInstance()
{
    return s_render_frame_sink_provider_instance.Pointer();
}

void RenderFrameSinkProvider::Terminate()
{
    if (!s_render_frame_sink_provider_instance.IsCreated()) {
        return;
    }
}

} // close namespace blpwtk2
