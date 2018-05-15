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

#ifndef INCLUDED_BLPWTK2_RENDERWEBVIEW_H
#define INCLUDED_BLPWTK2_RENDERWEBVIEW_H

#include <blpwtk2.h>
#include <blpwtk2_config.h>

#include <blpwtk2_scopedhwnd.h>
#include <blpwtk2_webview.h>
#include <blpwtk2_webviewclientdelegate.h>
#include <blpwtk2_webviewproperties.h>

#include <content/public/renderer/render_view_observer.h>
#include <ipc/ipc_listener.h>
#include <ui/gfx/geometry/size.h>
#include <ui/views/win/windows_session_change_observer.h>

struct ViewHostMsg_UpdateRect_Params;

namespace gfx {
class Point;
}  // close namespace gfx

namespace views {
class WindowsSessionChangeObserver;
}  // close namespace views

namespace blpwtk2 {

class WebFrameImpl;
class ProfileImpl;
struct WebViewProperties;

                        // ===================
                        // class RenderWebView
                        // ===================

class RenderWebView final : public WebView
                          , public WebViewClientDelegate
                          , private IPC::Listener
{
    class RenderViewObserver : public content::RenderViewObserver {
      private:

        RenderWebView *d_renderWebView;

      public:

        RenderViewObserver(
            content::RenderView *renderView, RenderWebView *renderWebView);

        void OnDestruct() override;
    };

    // DATA
    WebViewClient *d_client;
    WebViewDelegate *d_delegate;

    ProfileImpl *d_profile;
    int d_renderViewRoutingId, d_mainFrameRoutingId;
    bool d_gotRenderViewInfo;
    bool d_pendingLoadStatus;
    bool d_isMainFrameAccessible;
    bool d_pendingDestroy;
    std::string d_url;
    std::unique_ptr<WebFrameImpl> d_mainFrame;

    blpwtk2::WebViewProperties d_properties;

    ScopedHWND d_hwnd;

    bool d_has_parent = false;
    bool d_shown = false, d_visible = false;
    gfx::Size d_size;

    // Manages observation of Windows Session Change messages.
    std::unique_ptr<views::WindowsSessionChangeObserver>
        windows_session_change_observer_;

    static LPCTSTR GetWindowClass();
    static LRESULT CALLBACK WindowProcedure(HWND   hWnd,
                                            UINT   uMsg,
                                            WPARAM wParam,
                                            LPARAM lParam);
    LRESULT windowProcedure(UINT   uMsg,
                            WPARAM wParam,
                            LPARAM lParam);

    bool dispatchToRenderViewImpl(const IPC::Message& message);
    void OnRenderViewDestruct();
    void updateVisibility();
    void updateSize();

    bool dispatchToRenderViewImpl(const IPC::Message& message);
    void OnRenderViewDestruct();
    void updateVisibility();
    void updateSize();

    // blpwtk2::WebView overrides
    void destroy() override;
    WebFrame *mainFrame() override;
    int loadUrl(const StringRef& url) override;
    void loadInspector(unsigned int pid, int routingId) override;
    void inspectElementAt(const POINT& point) override;
    int goBack() override;
    int goForward() override;
    int reload() override;
    void stop() override;
    void show() override;
    void hide() override;
    void setParent(NativeView parent) override;
    void move(int left, int top, int width, int height) override;
    void cutSelection() override;
    void copySelection() override;
    void paste() override;
    void deleteSelection() override;
    void enableNCHitTest(bool enabled) override;
    void onNCHitTestResult(int x, int y, int result) override;
    void performCustomContextMenuAction(int actionId) override;
    void find(const StringRef& text, bool matchCase, bool forward) override;
    void stopFind(bool preserveSelection) override;
    void replaceMisspelledRange(const StringRef& text) override;
    void rootWindowPositionChanged() override;
    void rootWindowSettingsChanged() override;

    void handleInputEvents(const InputEvent *events,
                           size_t            eventsCount) override;
    void setDelegate(WebViewDelegate *delegate) override;
    int getRoutingId() const override;
    void setBackgroundColor(NativeColor color) override;
    void setRegion(NativeRegion region) override;
    void clearTooltip() override;
    v8::MaybeLocal<v8::Value> callFunction(
            v8::Local<v8::Function>  func,
            v8::Local<v8::Value>     recv,
            int                      argc,
            v8::Local<v8::Value>    *argv) override;
#if defined(BLPWTK2_FEATURE_PRINTPDF)
    String printToPDF(const StringRef& propertyName) override;
#endif

    // IPC::Listener overrides
    bool OnMessageReceived(const IPC::Message& message) override;

    // IPC message handlers:
    void OnDetach();
    void OnUpdateRect(const ViewHostMsg_UpdateRect_Params& params);

    DISALLOW_COPY_AND_ASSIGN(RenderWebView);

  public:
    explicit RenderWebView(WebViewDelegate          *delegate,
                           ProfileImpl              *profile,
                           const WebViewProperties&  properties);
    ~RenderWebView();

    // blpwtk2::WebViewClientDelegate overrides
    void setClient(WebViewClient *client) override;
    void ncHitTest() override;
    void ncDragBegin(int hitTestCode, const gfx::Point& point) override;
    void ncDragMove(const gfx::Point& point) override;
    void ncDragEnd(const gfx::Point& point) override;
    void ncDoubleClick(const gfx::Point& point) override;
    void focused() override;
    void blurred() override;
    void showContextMenu(const ContextMenuParams& params) override;
    void findReply(int  numberOfMatches,
                   int  activeMatchOrdinal,
                   bool finalUpdate) override;
    void preResize(const gfx::Size& size) override;
    void notifyRoutingId(int id) override;
    void onLoadStatus(int status) override;
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RENDERWEBVIEW_H

// vim: ts=4 et

