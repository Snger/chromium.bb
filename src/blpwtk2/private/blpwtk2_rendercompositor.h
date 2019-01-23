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

#ifndef INCLUDED_BLPWTK2_RENDERCOMPOSITOR2_H
#define INCLUDED_BLPWTK2_RENDERCOMPOSITOR2_H

#include <content/common/frame_sink_provider.mojom.h>
#include <gpu/ipc/common/surface_handle.h>
#include <mojo/public/cpp/bindings/binding.h>
#include <ui/gfx/geometry/size.h>

namespace blpwtk2 {

class ProfileImpl;
class RenderCompositor;

class RenderFrameSinkProvider {
  public:

    static RenderFrameSinkProvider* GetInstance();
    static void Terminate();

    virtual ~RenderFrameSinkProvider();

    virtual void Bind(content::mojom::FrameSinkProviderRequest request) = 0;
    virtual void Unbind() = 0;

    virtual std::unique_ptr<RenderCompositor> CreateCompositor(
        int32_t widget_id,
        gpu::SurfaceHandle gpu_surface_handle,
        blpwtk2::ProfileImpl *profile) = 0;
};

class RenderCompositor {
  public:

    virtual ~RenderCompositor();

    virtual viz::LocalSurfaceId GetLocalSurfaceId() = 0;

    virtual void SetVisible(bool visible) = 0;
    virtual void Resize(const gfx::Size& size) = 0;
};

} // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RENDERCOMPOSITOR_H
