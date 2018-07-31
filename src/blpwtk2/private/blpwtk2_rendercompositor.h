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

#ifndef INCLUDED_BLPWTK2_RENDERCOMPOSITOR_H
#define INCLUDED_BLPWTK2_RENDERCOMPOSITOR_H

#include <map>
#include <memory>

#include <base/lazy_instance.h>
#include <base/optional.h>
#include <base/memory/ref_counted.h>
#include <base/memory/weak_ptr.h>
#include <base/synchronization/waitable_event.h>
#include <components/viz/common/surfaces/frame_sink_id.h>
#include <components/viz/host/host_frame_sink_client.h>
#include <gpu/ipc/common/surface_handle.h>
#include <ui/gfx/geometry/size.h>

namespace base {

class SingleThreadTaskRunner;

} // close namespace base

namespace cc {

class LayerTreeFrameSink;

} // close namespace cc

namespace gpu {

class GpuMemoryBufferManager;

} // close namespace gpu

namespace ui {

class CompositorVSyncManager;
class ContextProviderCommandBuffer;

} // close namespace ui

namespace viz {

class BeginFrameSource;
class Display;
class FrameSinkManagerImpl;
class HostFrameSinkManager;
class RendererSettings;
class SharedBitmapManager;

} // close namespace viz

namespace blpwtk2 {

class LayerTreeFrameSink;
class RenderCompositor;
class SoftwareBackingManager;

class RenderCompositorContext {
  public:

    static RenderCompositorContext* GetInstance();
    static void Terminate();

    std::unique_ptr<RenderCompositor> CreateCompositor(gpu::SurfaceHandle gpu_surface_handle);

    bool RequestNewLayerTreeFrameSink(
        bool use_software, int routing_id,
        const base::Callback<void(std::unique_ptr<cc::LayerTreeFrameSink>)>& callback);

  private:

    friend struct base::LazyInstanceTraitsBase<RenderCompositorContext>;
    friend class RenderCompositor;

    RenderCompositorContext();
    ~RenderCompositorContext();

    void Destruct();

    scoped_refptr<gpu::GpuChannelHost> EstablishPrivilegedGpuChannel();
    void RequestUncorrelatedNewLayerTreeFrameSink(
        const base::Callback<void(std::unique_ptr<cc::LayerTreeFrameSink>)>& callback);

    std::map<int, RenderCompositor *> d_compositors_by_routing_id;

    // Constructed on main thread.
    // Initialized, accessed, and destructed on compositor thread.
    struct Details {
        viz::SharedBitmapManager *d_shared_bitmap_manager = nullptr;
        gpu::GpuMemoryBufferManager *d_gpu_memory_buffer_manager = nullptr;

        std::unique_ptr<viz::RendererSettings> d_renderer_settings;
        bool d_disable_display_vsync = false;
        bool d_wait_for_all_pipeline_stages_before_draw = false;

        std::unique_ptr<viz::FrameSinkManagerImpl> d_frame_sink_manager;
        std::unique_ptr<viz::HostFrameSinkManager> d_host_frame_sink_manager;
        uint32_t d_next_frame_sink_id = 1u;

        std::unique_ptr<SoftwareBackingManager> d_software_backing_manager;
        scoped_refptr<ui::ContextProviderCommandBuffer> d_worker_context_provider;

        void ConstructImpl(
            viz::SharedBitmapManager *shared_bitmap_manager,
            gpu::GpuMemoryBufferManager *gpu_memory_buffer_manager);

        void EstablishPrivilegedGpuChannelImpl(scoped_refptr<gpu::GpuChannelHost> gpu_channel);
        void RequestUncorrelatedNewLayerTreeFrameSinkImpl(
            const base::Callback<void(std::unique_ptr<cc::LayerTreeFrameSink>)>& callback);
    };

    std::unique_ptr<Details> d_details;
};

class RenderCompositor {
  public:

    ~RenderCompositor();

    void SetVisible(bool visible);
    void DisableSwapUntilResize();
    void Resize(const gfx::Size& size);
    void Correlate(int routing_id);
    void RequestNewLayerTreeFrameSink(
        bool use_software,
        const base::Callback<void(std::unique_ptr<cc::LayerTreeFrameSink>)>& callback);

  private:

    friend class RenderCompositorContext;

    RenderCompositor(RenderCompositorContext *context, gpu::SurfaceHandle gpu_surface_handle);

    RenderCompositorContext                             *d_context;
    gpu::SurfaceHandle                                   d_gpu_surface_handle;
    int                                                  d_routing_id = 0;

    // Constructed on main thread.
    // Initialized, accessed, and destructed on compositor thread.
    struct Details : private viz::HostFrameSinkClient {
        ~Details();

        RenderCompositorContext::Details          *d_context = nullptr;
        bool                                       d_visible = false;
        gfx::Size                                  d_size;
        viz::FrameSinkId                           d_frame_sink_id;
        scoped_refptr<ui::CompositorVSyncManager>  d_vsync_manager;
        std::unique_ptr<viz::BeginFrameSource>     d_begin_frame_source;
        std::unique_ptr<viz::Display>              d_display;
        base::WeakPtr<LayerTreeFrameSink>          d_layer_tree_frame_sink;

        // viz::HostFrameSinkClient overrides:
        void OnFirstSurfaceActivation(const viz::SurfaceInfo& surface_info) override;

        void ConstructImpl(
            RenderCompositorContext::Details *context);
        void SetVisibleImpl(bool visible);
        void ResizeImpl(const gfx::Size& size, base::WaitableEvent *event);
        void RequestNewLayerTreeFrameSinkImpl(
            scoped_refptr<gpu::GpuChannelHost> gpu_channel,
            scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
            RenderCompositorContext::Details *context,
            gpu::SurfaceHandle gpu_surface_handle,
            const base::Callback<void(std::unique_ptr<cc::LayerTreeFrameSink>)>& callback);
    };

    std::unique_ptr<Details> d_details;
};

} // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RENDERCOMPOSITOR_H
