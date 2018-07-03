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
#include <cc/surfaces/frame_sink_id.h>
#include <gpu/ipc/common/surface_handle.h>
#include <ui/gfx/geometry/size.h>

namespace base {

class SingleThreadTaskRunner;

} // close namespace base

namespace cc {

class CompositorFrameSink;
class Display;
class RendererSettings;
class SharedBitmapManager;
class SurfaceManager;

} // close namespace cc

namespace content {

class ContextProviderCommandBuffer;

} // close namespace content

namespace gpu {

class GpuChannelHost;
class GpuMemoryBufferManager;

} // close namespace gpu

namespace ui {

class CompositorVSyncManager;

} // close namespace ui

namespace blpwtk2 {

class CompositorFrameSink;
class RenderCompositor;
class SoftwareBackingManager;

class RenderCompositorContext {
  public:

    static RenderCompositorContext* GetInstance();
    static void Terminate();

    std::unique_ptr<RenderCompositor> CreateCompositor(gpu::SurfaceHandle gpu_surface_handle);
    base::Optional<std::unique_ptr<cc::CompositorFrameSink>> CreateCompositorFrameSink(int routing_id);

  private:

    friend struct base::DefaultLazyInstanceTraits<RenderCompositorContext>;
    friend class RenderCompositor;

    RenderCompositorContext();
    ~RenderCompositorContext();

    void Destruct();
    scoped_refptr<gpu::GpuChannelHost> EstablishPrivilegedGpuChannel();
    std::unique_ptr<cc::CompositorFrameSink> CreateUncorrelatedCompositorFrameSink();

    std::map<int, RenderCompositor *> d_compositors_by_routing_id;

    // Constructed on main thread.
    // Initialized, accessed, and destructed on compositor thread.
    struct Details {
        cc::SharedBitmapManager *d_shared_bitmap_manager = nullptr;
        gpu::GpuMemoryBufferManager *d_gpu_memory_buffer_manager = nullptr;

        std::unique_ptr<cc::RendererSettings> d_renderer_settings;

        std::unique_ptr<cc::SurfaceManager> d_surface_manager;
        uint32_t d_next_frame_sink_id = 1u;

        std::unique_ptr<SoftwareBackingManager> d_software_backing_manager;
        scoped_refptr<content::ContextProviderCommandBuffer> d_worker_context_provider;

        void ConstructImpl(
            cc::SharedBitmapManager *shared_bitmap_manager,
            gpu::GpuMemoryBufferManager *gpu_memory_buffer_manager);
        void EstablishPrivilegedGpuChannelImpl(scoped_refptr<gpu::GpuChannelHost> gpu_channel);
        void CreateUncorrelatedCompositorFrameSinkImpl(
            std::unique_ptr<CompositorFrameSink> *result,
            base::WaitableEvent *event);
    };

    std::unique_ptr<Details> d_details;
};

class RenderCompositor {
  public:

    ~RenderCompositor();

    void SetVisible(bool visible);
    void Resize(const gfx::Size& size);
    void Correlate(int routing_id);
    std::unique_ptr<cc::CompositorFrameSink> CreateCompositorFrameSink();

  private:

    friend class RenderCompositorContext;

    RenderCompositor(RenderCompositorContext *context, gpu::SurfaceHandle gpu_surface_handle);

    RenderCompositorContext                             *d_context;
    gpu::SurfaceHandle                                   d_gpu_surface_handle;
    int                                                  d_routing_id = 0;

    // Constructed on main thread.
    // Initialized, accessed, and destructed on compositor thread.
    struct Details {
        ~Details();

        bool                                      d_visible = false;
        gfx::Size                                 d_size;
        cc::FrameSinkId                           d_frame_sink_id;
        scoped_refptr<ui::CompositorVSyncManager> d_vsync_manager;
        std::unique_ptr<cc::Display>              d_display;
        base::WeakPtr<CompositorFrameSink>        d_output_surface;

        void ConstructImpl(
            scoped_refptr<gpu::GpuChannelHost> gpu_channel,
            scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
            RenderCompositorContext::Details *context,
            gpu::SurfaceHandle gpu_surface_handle);
        void SetVisibleImpl(bool visible);
        void ResizeImpl(const gfx::Size& size);
        void CreateCompositorFrameSinkImpl(
            scoped_refptr<gpu::GpuChannelHost> gpu_channel,
            scoped_refptr<base::SingleThreadTaskRunner> compositor_task_runner,
            RenderCompositorContext::Details *context,
            gpu::SurfaceHandle gpu_surface_handle,
            std::unique_ptr<CompositorFrameSink> *result,
            base::WaitableEvent *event);
    };

    std::unique_ptr<Details> d_details;
};

} // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RENDERCOMPOSITOR_H
