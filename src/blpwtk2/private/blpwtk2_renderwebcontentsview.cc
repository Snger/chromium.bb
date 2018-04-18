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

#include <blpwtk2_renderwebcontentsview.h>

#include <base/logging.h>
#include <content/browser/renderer_host/render_widget_host_impl.h>
#include <content/browser/renderer_host/render_widget_host_view_base.h>

namespace blpwtk2 {

class RenderWidgetHostView : public content::RenderWidgetHostViewBase {
  public:

    RenderWidgetHostView(content::RenderWidgetHost *render_widget_host);
    ~RenderWidgetHostView() override;

    // content::RenderWidgetHostView overrides:
    void InitAsChild(gfx::NativeView parent_view) override;
    void SetSize(const gfx::Size& size) override;
    void SetBounds(const gfx::Rect& rect) override;
    gfx::NativeView GetNativeView() const override;
    gfx::NativeViewAccessible GetNativeViewAccessible() override;
    void Focus() override;
    bool HasFocus() const override;
    bool IsSurfaceAvailableForCopy() const override;
    void Show() override;
    void Hide() override;
    bool IsShowing() override;
    gfx::Rect GetViewBounds() const override;
    void SetBackgroundColor(SkColor color) override;
    SkColor background_color() const override;
    bool LockMouse() override;
    void UnlockMouse() override;
    void SetNeedsBeginFrames(bool needs_begin_frames) override;
    void SetWantsAnimateOnlyBeginFrames() override;
    void TakeFallbackContentFrom(content::RenderWidgetHostView* view) override;

    // content::RenderWidgetHostViewBase overrides:
    gfx::Size GetRequestedRendererSize() const override;
    void DidCreateNewRendererCompositorFrameSink(
        viz::mojom::CompositorFrameSinkClient*
            renderer_compositor_frame_sink) override;
    void SubmitCompositorFrame(
        const viz::LocalSurfaceId& local_surface_id,
        viz::CompositorFrame frame,
        viz::mojom::HitTestRegionListPtr hit_test_region_list) override;
    void ClearCompositorFrame() override;
    viz::SurfaceId GetCurrentSurfaceId() const override;
    void InitAsPopup(
        content::RenderWidgetHostView* parent_host_view,
        const gfx::Rect& bounds) override;
    void InitAsFullscreen(content::RenderWidgetHostView* reference_host_view) override;
    void UpdateCursor(const content::WebCursor& cursor) override;
    void SetIsLoading(bool is_loading) override;
    void RenderProcessGone(
        base::TerminationStatus status,
        int error_code) override;
    void Destroy() override;
    void SetTooltipText(
        const base::string16& tooltip_text) override;
    gfx::Vector2d GetOffsetFromRootSurface() override;
    void SelectionBoundsChanged(
        const ViewHostMsg_SelectionBounds_Params& params) override;
    gfx::Rect GetBoundsInRootWindow() override;
    void ImeCompositionRangeChanged(
        const gfx::Range& range,
        const std::vector<gfx::Rect>& character_bounds) override;
};


RenderWidgetHostView::RenderWidgetHostView(content::RenderWidgetHost *render_widget_host)
: content::RenderWidgetHostViewBase(render_widget_host)
{
    content::RenderWidgetHostImpl::From(render_widget_host)->SetView(this);
}

RenderWidgetHostView::~RenderWidgetHostView()
{
}

void RenderWidgetHostView::InitAsChild(gfx::NativeView parent_view)
{
    NOTREACHED();
}

void RenderWidgetHostView::SetSize(const gfx::Size& size)
{
}

void RenderWidgetHostView::SetBounds(const gfx::Rect& rect)
{
    NOTREACHED();
}

gfx::NativeView RenderWidgetHostView::GetNativeView() const
{
    return nullptr;
}

gfx::NativeViewAccessible RenderWidgetHostView::GetNativeViewAccessible()
{
    NOTREACHED();
    return nullptr;
}

void RenderWidgetHostView::Focus()
{
    NOTREACHED();
}

bool RenderWidgetHostView::HasFocus() const
{
    NOTREACHED();
    return false;
}

bool RenderWidgetHostView::IsSurfaceAvailableForCopy() const
{
    NOTREACHED();
    return false;
}

void RenderWidgetHostView::Show()
{
    NOTREACHED();
}

void RenderWidgetHostView::Hide()
{
    NOTREACHED();
}

bool RenderWidgetHostView::IsShowing()
{
    NOTREACHED();
    return false;
}

gfx::Rect RenderWidgetHostView::GetViewBounds() const
{
    return gfx::Rect(0, 0, 1, 1);
}

void RenderWidgetHostView::SetBackgroundColor(SkColor color)
{
    NOTREACHED();
}

SkColor RenderWidgetHostView::background_color() const
{
    NOTREACHED();
    return SK_ColorBLACK;
}

bool RenderWidgetHostView::LockMouse()
{
    NOTREACHED();
    return false;
}

void RenderWidgetHostView::UnlockMouse()
{
    NOTREACHED();
}

void RenderWidgetHostView::SetNeedsBeginFrames(bool needs_begin_frames)
{
}

void RenderWidgetHostView::SetWantsAnimateOnlyBeginFrames()
{
}


void RenderWidgetHostView::TakeFallbackContentFrom(
    content::RenderWidgetHostView* view)
{
    NOTREACHED();
}

// content::RenderWidgetHostViewBase overrides:
gfx::Size RenderWidgetHostView::GetRequestedRendererSize() const
{
    return gfx::Size(0, 0);
}

void RenderWidgetHostView::DidCreateNewRendererCompositorFrameSink(
    viz::mojom::CompositorFrameSinkClient* renderer_compositor_frame_sink)
{
}

void RenderWidgetHostView::SubmitCompositorFrame(
    const viz::LocalSurfaceId& local_surface_id,
    viz::CompositorFrame frame,
    viz::mojom::HitTestRegionListPtr hit_test_region_list)
{
    NOTREACHED();
}

void RenderWidgetHostView::ClearCompositorFrame()
{
}

viz::SurfaceId RenderWidgetHostView::GetCurrentSurfaceId() const
{
    NOTREACHED();
    return viz::SurfaceId();
}

void RenderWidgetHostView::InitAsPopup(
    content::RenderWidgetHostView* parent_host_view,
    const gfx::Rect& bounds)
{
    NOTREACHED();
}

void RenderWidgetHostView::InitAsFullscreen(content::RenderWidgetHostView* reference_host_view)
{
    NOTREACHED();
}

void RenderWidgetHostView::UpdateCursor(const content::WebCursor& cursor)
{
    NOTREACHED();
}

void RenderWidgetHostView::SetIsLoading(bool is_loading)
{
}

void RenderWidgetHostView::RenderProcessGone(
    base::TerminationStatus status,
    int error_code)
{
    NOTREACHED();
}

void RenderWidgetHostView::Destroy()
{
}

void RenderWidgetHostView::SetTooltipText(
    const base::string16& tooltip_text)
{
    NOTREACHED();
}

gfx::Vector2d RenderWidgetHostView::GetOffsetFromRootSurface()
{
    NOTREACHED();
    return gfx::Vector2d(0, 0);
}

void RenderWidgetHostView::SelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params)
{
}

gfx::Rect RenderWidgetHostView::GetBoundsInRootWindow()
{
    return gfx::Rect(0, 0, 0, 0);
}

void RenderWidgetHostView::ImeCompositionRangeChanged(
    const gfx::Range& range,
    const std::vector<gfx::Rect>& character_bounds)
{
    NOTREACHED();
}

//
RenderWebContentsView::RenderWebContentsView()
{
}

RenderWebContentsView::~RenderWebContentsView()
{
}

gfx::NativeView RenderWebContentsView::GetNativeView() const
{
    NOTREACHED();
    return nullptr;
}

gfx::NativeView RenderWebContentsView::GetContentNativeView() const
{
    NOTREACHED();
    return nullptr;
}

gfx::NativeWindow RenderWebContentsView::GetTopLevelNativeWindow() const
{
    NOTREACHED();
    return nullptr;
}

void RenderWebContentsView::GetContainerBounds(gfx::Rect* out) const
{
    *out = gfx::Rect(0, 0, 0, 0);
}

void RenderWebContentsView::SizeContents(const gfx::Size& size)
{
    NOTREACHED();
}

void RenderWebContentsView::Focus()
{
    NOTREACHED();
}

void RenderWebContentsView::SetInitialFocus()
{
    NOTREACHED();
}

void RenderWebContentsView::StoreFocus()
{
    NOTREACHED();
}

void RenderWebContentsView::RestoreFocus()
{
    NOTREACHED();
}

void RenderWebContentsView::FocusThroughTabTraversal(bool reverse)
{
    NOTREACHED();
}

content::DropData* RenderWebContentsView::GetDropData() const
{
    NOTREACHED();
    return nullptr;
}

gfx::Rect RenderWebContentsView::GetViewBounds() const
{
    NOTREACHED();
    return gfx::Rect(0, 0, 0, 0);
}

void RenderWebContentsView::CreateView(
    const gfx::Size& initial_size, gfx::NativeView context)
{
}

content::RenderWidgetHostViewBase* RenderWebContentsView::CreateViewForWidget(
    content::RenderWidgetHost* render_widget_host, bool is_guest_view_hack)
{
    return new RenderWidgetHostView(render_widget_host);
}

content::RenderWidgetHostViewBase* RenderWebContentsView::CreateViewForPopupWidget(
    content::RenderWidgetHost* render_widget_host)
{
    NOTREACHED();
    return nullptr;
}

void RenderWebContentsView::SetPageTitle(const base::string16& title)
{
}

void RenderWebContentsView::RenderViewCreated(content::RenderViewHost* host)
{
}

void RenderWebContentsView::RenderViewSwappedIn(content::RenderViewHost* host)
{
}

void RenderWebContentsView::SetOverscrollControllerEnabled(bool enabled)
{
}

} // close namespace blpwtk2
