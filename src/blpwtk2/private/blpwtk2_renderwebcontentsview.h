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

#ifndef INCLUDED_BLPWTK2_RENDERWEBCONTENTSVIEW_H
#define INCLUDED_BLPWTK2_RENDERWEBCONTENTSVIEW_H

#include <content/browser/renderer_host/render_view_host_delegate_view.h>
#include <content/public/browser/web_contents_view.h>

namespace blpwtk2 {

class RenderWebContentsView : public content::WebContentsView,
                              public content::RenderViewHostDelegateView {
  public:

    RenderWebContentsView();
    ~RenderWebContentsView() override;

    gfx::NativeView GetNativeView() const override;
    gfx::NativeView GetContentNativeView() const override;
    gfx::NativeWindow GetTopLevelNativeWindow() const override;
    void GetScreenInfo(content::ScreenInfo* screen_info) const override;
    void GetContainerBounds(gfx::Rect* out) const override;
    void SizeContents(const gfx::Size& size) override;
    void Focus() override;
    void SetInitialFocus() override;
    void StoreFocus() override;
    void RestoreFocus() override;
    content::DropData* GetDropData() const override;
    gfx::Rect GetViewBounds() const override;
    void CreateView(
      const gfx::Size& initial_size, gfx::NativeView context) override;
    content::RenderWidgetHostViewBase* CreateViewForWidget(
      content::RenderWidgetHost* render_widget_host, bool is_guest_view_hack) override;
    content::RenderWidgetHostViewBase* CreateViewForPopupWidget(
      content::RenderWidgetHost* render_widget_host) override;
    void SetPageTitle(const base::string16& title) override;
    void RenderViewCreated(content::RenderViewHost* host) override;
    void RenderViewSwappedIn(content::RenderViewHost* host) override;
    void SetOverscrollControllerEnabled(bool enabled) override;
};

} // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RENDERWEBCONTENTSVIEW_H
