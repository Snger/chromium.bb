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

#include <blpwtk2_renderwebview.h>

#include <blpwtk2_webviewclient.h>
#include <blpwtk2_contextmenuparams.h>
#include <blpwtk2_profileimpl.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_stringref.h>
#include <blpwtk2_webframeimpl.h>
#include <blpwtk2_webviewdelegate.h>
#include <blpwtk2_blob.h>
#include <blpwtk2_rendererutil.h>

#include <content/renderer/render_view_impl.h>
#include <content/public/renderer/render_view.h>
#include <third_party/WebKit/public/web/WebLocalFrame.h>
#include <third_party/WebKit/public/web/WebView.h>
#include <ui/base/win/lock_state.h>

#include <dwmapi.h>
#include <windows.h>
#include <unordered_map>
#include <unordered_set>

#define GetAValue(argb)      (LOBYTE((argb)>>24))

namespace blpwtk2 {

                        // -------------------
                        // class RenderWebView
                        // -------------------

RenderWebView::RenderWebView(WebViewDelegate          *delegate,
                             ProfileImpl              *profile,
                             const WebViewProperties&  properties)
    : d_client(nullptr)
    , d_delegate(delegate)
    , d_profile(profile)
    , d_renderViewRoutingId(0)
    , d_gotRenderViewInfo(false)
    , d_pendingLoadStatus(false)
    , d_isMainFrameAccessible(false)
    , d_pendingDestroy(false)
    , d_properties(properties)
{
    d_profile->incrementWebViewCount();

    d_hwnd.reset(CreateWindowEx(
        0,
        GetWindowClass(),
        L"blpwtk2-RenderWebView",
        WS_OVERLAPPED | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        1,
        1,
        NULL,
        NULL,
        NULL,
        NULL));
    DCHECK(d_hwnd.is_valid());

    SetWindowLongPtr(d_hwnd.get(), GWLP_USERDATA, reinterpret_cast<LONG>(this));

    SetWindowLong(
        d_hwnd.get(),
        GWL_STYLE,
        GetWindowLong(d_hwnd.get(), GWL_STYLE) & ~WS_CAPTION);

    d_has_parent = false;

    windows_session_change_observer_.reset(new views::WindowsSessionChangeObserver(
        base::Bind(&RenderWebView::OnSessionChange,
                   base::Unretained(this))
    ));
}

LPCTSTR RenderWebView::GetWindowClass()
{
    static const LPCTSTR s_className    = L"blpwtk2-RenderWebView";
    static ATOM          s_atom         = 0;

    if (s_atom == 0) {
        static WNDCLASSEX s_class = {};

        s_class.cbSize        = sizeof(WNDCLASSEX);
        s_class.style         = 0;
        s_class.lpfnWndProc   = WindowProcedure;
        s_class.cbClsExtra    = 0;
        s_class.cbWndExtra    = 0;
        s_class.hInstance     = GetModuleHandle(NULL);
        s_class.hIcon         = NULL;
        s_class.hCursor       = NULL;
        s_class.hbrBackground = NULL;
        s_class.lpszMenuName  = NULL;
        s_class.lpszClassName = s_className;
        s_class.hIconSm       = NULL;

        s_atom = RegisterClassEx(&s_class);
        DCHECK(s_atom);
    }

    return s_className;
}

LRESULT CALLBACK RenderWebView::WindowProcedure(HWND      hWnd,
                                                UINT      uMsg,
                                                WPARAM    wParam,
                                                LPARAM    lParam)
{
    RenderWebView *that = reinterpret_cast<RenderWebView *>(
        GetWindowLongPtr(hWnd, GWLP_USERDATA)
    );

    // GWL_USERDATA hasn't been set to anything yet:
    if (!that) {
        return DefWindowProc(hWnd, uMsg, wParam, lParam);
    }
    else {
        // Otherwise:
        DCHECK(that->d_hwnd == hWnd);

        return that->windowProcedure(uMsg, wParam, lParam);
    }
}

LRESULT RenderWebView::windowProcedure(UINT   uMsg,
                                       WPARAM wParam,
                                       LPARAM lParam)
{
    switch (uMsg) {
    case WM_NCDESTROY: {
        d_hwnd.release();
    } return 0;
    case WM_WINDOWPOSCHANGED: {
        auto windowpos = reinterpret_cast<WINDOWPOS *>(lParam);

        if (windowpos->flags & (SWP_SHOWWINDOW | SWP_HIDEWINDOW)) {
            d_visible = (windowpos->flags & SWP_SHOWWINDOW)?
                true : false;
        }

        if (!(windowpos->flags & SWP_NOSIZE)) {
            gfx::Size size(windowpos->cx, windowpos->cy);
        }
    } return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(d_hwnd.get(), &ps);

        EndPaint(d_hwnd.get(), &ps);
    } return 0;
    case WM_ERASEBKGND:
        return 1;
    default:
        break;
    }

    return DefWindowProc(d_hwnd.get(), uMsg, wParam, lParam);
}

RenderWebView::~RenderWebView()
{
    LOG(INFO) << "Destroying RenderWebView, routingId=" << d_renderViewRoutingId;
    d_profile->decrementWebViewCount();

    if (d_client) {
        auto client = d_client;
        d_client = nullptr;
        client->releaseHost();
    }
}

void RenderWebView::ForceRedrawWindow(int attempts) {
    if (ui::IsWorkstationLocked()) {
        // Presents will continue to fail as long as the input desktop is
        // unavailable.
        if (--attempts <= 0)
            return;
        base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
            FROM_HERE, base::Bind(&RenderWebView::ForceRedrawWindow,
                                    base::Unretained(this), attempts),
            base::TimeDelta::FromMilliseconds(500));
        return;
    }
    InvalidateRect(d_hwnd.get(), NULL, FALSE);
}

void RenderWebView::OnSessionChange(WPARAM status_code) {
    // Direct3D presents are ignored while the screen is locked, so force the
    // window to be redrawn on unlock.
    if (status_code == WTS_SESSION_UNLOCK)
        ForceRedrawWindow(10);
}

void RenderWebView::destroy()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_pendingDestroy);

    if (d_hwnd.is_valid()) {
        d_hwnd.reset();
    }

    // Schedule a deletion of this RenderWebView.  The reason we don't delete
    // the object right here right now is because there may be a callback
    // that is already scheduled and the callback requires the existence of
    // the WebView.
    d_pendingDestroy = true;
    base::MessageLoop::current()->task_runner()->DeleteSoon(FROM_HERE, this);
}

WebFrame *RenderWebView::mainFrame()
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_isMainFrameAccessible)
        << "You should wait for didFinishLoad";
    DCHECK(d_gotRenderViewInfo);

    if (!d_mainFrame.get()) {
        content::RenderView *rv =
            content::RenderView::FromRoutingID(d_renderViewRoutingId);
        DCHECK(rv);

        blink::WebFrame *webFrame = rv->GetWebView()->MainFrame();
        d_mainFrame.reset(new WebFrameImpl(webFrame));
    }

    return d_mainFrame.get();
}

int RenderWebView::loadUrl(const StringRef& url)
{
    DCHECK(Statics::isInApplicationMainThread());
    if (d_pendingLoadStatus) {
        return EBUSY;
    }

    d_pendingLoadStatus = true;
    d_url = std::string(url.data(), url.length());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", loadUrl=" << d_url;
    d_client->loadUrl(d_url);
    return 0;
}

void RenderWebView::loadInspector(unsigned int pid, int routingId)
{
    DCHECK(Statics::isInApplicationMainThread());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId
              << ", loading inspector for " << routingId;

    d_client->proxy()->loadInspector(pid, routingId);
}

void RenderWebView::inspectElementAt(const POINT& point)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->inspectElementAt(point.x, point.y);
}

int RenderWebView::goBack()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (d_pendingLoadStatus) {
        return EBUSY;
    }

    d_pendingLoadStatus = true;
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", goBack()";
    d_client->goBack();
    return 0;
}

int RenderWebView::goForward()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (d_pendingLoadStatus) {
        return EBUSY;
    }

    d_pendingLoadStatus = true;
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", goForward()";
    d_client->goForward();
    return 0;
}

int RenderWebView::reload()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (d_pendingLoadStatus) {
        return EBUSY;
    }

    d_pendingLoadStatus = true;
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", reload()";
    d_client->reload();
    return 0;
}

void RenderWebView::stop()
{
    DCHECK(Statics::isInApplicationMainThread());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", stop";
    d_client->proxy()->stop();
}

void RenderWebView::show()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_hwnd.is_valid());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", show";

    if (d_shown) {
        return;
    }

    d_shown = true;

    if (d_has_parent) {
        SetWindowPos(
            d_hwnd.get(),
            0,
            0, 0, 0, 0,
            SWP_SHOWWINDOW |
            SWP_NOMOVE | SWP_NOSIZE |
            SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
    }
}

void RenderWebView::hide()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_hwnd.is_valid());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", hide";

    if (!d_shown) {
        return;
    }

    d_shown = false;

    if (d_has_parent) {
        SetWindowPos(
            d_hwnd.get(),
            0,
            0, 0, 0, 0,
            SWP_HIDEWINDOW |
            SWP_NOMOVE | SWP_NOSIZE |
            SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
    }
}

void RenderWebView::setParent(NativeView parent)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_hwnd.is_valid());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId
              << ", setParent=" << (void*)parent;

    auto shown = d_shown;

    // The window is losing its parent:
    if (!parent && d_has_parent) {
        if (shown) {
            SetWindowPos(
                d_hwnd.get(),
                0,
                0, 0, 0, 0,
                SWP_HIDEWINDOW |
                SWP_NOMOVE | SWP_NOSIZE |
                SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
        }
    }
    else if (parent && !d_has_parent) {
        SetWindowLong(
            d_hwnd.get(),
            GWL_STYLE,
            (GetWindowLong(d_hwnd.get(), GWL_STYLE) & ~WS_OVERLAPPED) | WS_CHILD);
    }

    SetParent(d_hwnd.get(), parent);

    // The window is gaining a parent:
    if (parent && !d_has_parent) {
        if (shown) {
            SetWindowPos(
                d_hwnd.get(),
                0,
                0, 0, 0, 0,
                SWP_SHOWWINDOW |
                SWP_NOMOVE | SWP_NOSIZE |
                SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
        }
    }
    else if (!parent && d_has_parent) {
        SetWindowLong(
            d_hwnd.get(),
            GWL_STYLE,
            (GetWindowLong(d_hwnd.get(), GWL_STYLE) & ~WS_CHILD) | WS_OVERLAPPED);
    }

    d_has_parent = !!parent;
}

void RenderWebView::move(int left, int top, int width, int height)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(0 <= width);
    DCHECK(0 <= height);
    DCHECK(d_hwnd.is_valid());

    SetWindowPos(
        d_hwnd.get(),
        0,
        left, top, width, height,
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER);
}

void RenderWebView::cutSelection()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->cutSelection();
}

void RenderWebView::copySelection()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->copySelection();
}

void RenderWebView::paste()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->paste();
}

void RenderWebView::deleteSelection()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->deleteSelection();
}

void RenderWebView::enableNCHitTest(bool enabled)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->enableNCHitTest(enabled);
}

void RenderWebView::onNCHitTestResult(int x, int y, int result)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->ncHitTestResult(x, y, result);
}

void RenderWebView::performCustomContextMenuAction(int actionId)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->performCustomContextMenuAction(actionId);
}

void RenderWebView::find(const StringRef& text, bool matchCase, bool forward)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->find(std::string(text.data(), text.size()), matchCase, forward);
}

void RenderWebView::stopFind(bool preserveSelection)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->stopFind(preserveSelection);
}

void RenderWebView::replaceMisspelledRange(const StringRef& text)
{
    DCHECK(Statics::isInApplicationMainThread());
    std::string stext(text.data(), text.length());
    d_client->proxy()->replaceMisspelledRange(stext);
}

void RenderWebView::rootWindowPositionChanged()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->rootWindowPositionChanged();
}

void RenderWebView::rootWindowSettingsChanged()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->rootWindowSettingsChanged();
}

void RenderWebView::handleInputEvents(const InputEvent *events, size_t eventsCount)
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_isMainFrameAccessible)
        << "You should wait for didFinishLoad";
    DCHECK(d_gotRenderViewInfo);

    content::RenderWidget *rw =
        content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);
    DCHECK(rw);

    RendererUtil::handleInputEvents(rw, events, eventsCount);
}

void RenderWebView::setDelegate(WebViewDelegate *delegate)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_delegate = delegate;
}

int RenderWebView::getRoutingId() const
{
    return d_renderViewRoutingId;
}

void RenderWebView::setBackgroundColor(NativeColor color)
{
    int red = GetRValue(color);
    int green = GetGValue(color);
    int blue = GetBValue(color);
    int alpha = GetAValue(color);

    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_isMainFrameAccessible) << "You should wait for didFinishLoad";
    DCHECK(d_gotRenderViewInfo);

    d_client->proxy()->setBackgroundColor(red, green, blue);

    content::RenderView* rv = content::RenderView::FromRoutingID(d_renderViewRoutingId);
    blink::WebFrameWidget* frameWidget = rv->GetWebFrameWidget();
    frameWidget->SetBaseBackgroundColor(
        SkColorSetARGB(alpha, red, green, blue));
}

void RenderWebView::setRegion(NativeRegion region)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->applyRegion(region);
}

void RenderWebView::clearTooltip()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->clearTooltip();
}

v8::MaybeLocal<v8::Value> RenderWebView::callFunction(
        v8::Local<v8::Function>  func,
        v8::Local<v8::Value>     recv,
        int                      argc,
        v8::Local<v8::Value>    *argv)
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_isMainFrameAccessible)
        << "You should wait for didFinishLoad";
    DCHECK(d_gotRenderViewInfo);

    content::RenderView *rv = content::RenderView::FromRoutingID(d_renderViewRoutingId);
    blink::WebFrame *webFrame = rv->GetWebView()->MainFrame();
    DCHECK(webFrame->IsWebLocalFrame());
    blink::WebLocalFrame* localWebFrame = webFrame->ToWebLocalFrame();

    v8::Local<v8::Value> result =
        localWebFrame->CallFunctionEvenIfScriptDisabled(func, recv, argc, argv);

    return v8::MaybeLocal<v8::Value>(result);
}

#if defined(BLPWTK2_FEATURE_PRINTPDF)
String RenderWebView::printToPDF(const StringRef& propertyName)
{
    auto* rv = content::RenderView::FromRoutingID(d_renderViewRoutingId);
    return RendererUtil::printToPDF(rv, propertyName.toStdString());
}
#endif

// blpwtk2::WebViewClientDelegate overrides
void RenderWebView::setClient(WebViewClient *client)
{
    d_client = client;
}

void RenderWebView::ncHitTest()
{
    d_delegate->requestNCHitTest(this);
    // Note: The embedder is expected to call WebView::onNCHitTestResult
}

void RenderWebView::ncDragBegin(int hitTestCode, const gfx::Point& point)
{
    POINT winPoint = { point.x(), point.y() };
    d_delegate->ncDragBegin(this, hitTestCode, winPoint);
}

void RenderWebView::ncDragMove(const gfx::Point& point)
{
    POINT winPoint = { point.x(), point.y() };
    d_delegate->ncDragMove(this, winPoint);
}

void RenderWebView::ncDragEnd(const gfx::Point& point)
{
    POINT winPoint = { point.x(), point.y() };
    d_delegate->ncDragEnd(this, winPoint);
}

void RenderWebView::ncDoubleClick(const gfx::Point& point)
{
    POINT winPoint = { point.x(), point.y() };
    d_delegate->ncDoubleClick(this, winPoint);
}

void RenderWebView::focused()
{
    d_delegate->focused(this);
}

void RenderWebView::blurred()
{
    d_delegate->blurred(this);
}

void RenderWebView::showContextMenu(const ContextMenuParams& params)
{
    d_delegate->showContextMenu(this, params);
}

void RenderWebView::findReply(int  numberOfMatches,
                             int  activeMatchOrdinal,
                             bool finalUpdate)
{
    d_delegate->findState(
            this, numberOfMatches, activeMatchOrdinal, finalUpdate);
}

void RenderWebView::preResize(const gfx::Size& size)
{
}

void RenderWebView::notifyRoutingId(int id)
{
    if (d_pendingDestroy) {
        LOG(INFO) << "WebView destroyed before we got a reference to a RenderView";
        return;
    }

    content::RenderView *rv =
        content::RenderView::FromRoutingID(id);

    if (!rv) {
        // The RenderView has not been created yet.  Keep reposting this task
        // until the RenderView is available.
        base::MessageLoop::current()->task_runner()->PostTask(
            FROM_HERE,
            base::Bind(&RenderWebView::notifyRoutingId,
                       base::Unretained(this),
                       id));
        return;
    }

    d_gotRenderViewInfo = true;

    d_renderViewRoutingId = id;
    LOG(INFO) << "routingId=" << id;
}

void RenderWebView::onLoadStatus(int status)
{
    d_pendingLoadStatus = false;

    if (0 == status) {
        LOG(INFO) << "routingId=" << d_renderViewRoutingId
                  << ", didFinishLoad url=" << d_url;

        // wait until we receive this
        // notification before we make the
        // mainFrame accessible
        d_isMainFrameAccessible = true;

        if (d_delegate) {
            d_delegate->didFinishLoad(this, StringRef(d_url));
        }
    }
    else {
        LOG(INFO) << "routingId=" << d_renderViewRoutingId
                  << ", didFailLoad url=" << d_url;

        if (d_delegate) {
            d_delegate->didFailLoad(this, StringRef(d_url));
        }
    }
}

}  // close namespace blpwtk2

// vim: ts=4 et


