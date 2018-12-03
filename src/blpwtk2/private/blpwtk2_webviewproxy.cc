/*
 * Copyright (C) 2014 Bloomberg Finance L.P.
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

#include <blpwtk2_webviewproxy.h>

#include <blpwtk2_webviewclient.h>
#include <blpwtk2_contextmenuparams.h>
#include <blpwtk2_profileimpl.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_stringref.h>
#include <blpwtk2_webframeimpl.h>
#include <blpwtk2_webviewdelegate.h>
#include <blpwtk2_blob.h>
#include <blpwtk2_rendererutil.h>

#include <cc/trees/proxy_main.h>
#include <content/renderer/render_view_impl.h>
#include <content/public/renderer/render_view.h>
#include <third_party/blink/public/web/web_local_frame.h>
#include <third_party/blink/public/web/web_view.h>

#include <dwmapi.h>
#include <windows.h>
#include <unordered_map>
#include <unordered_set>

namespace {

const int DEFAULT_DPI_X = 96;

float getScreenScaleFactor()
{
    static float scale_x = -1;

    if (scale_x < 0) {
        HWND desktop_window = ::GetDesktopWindow();
        HDC screen_dc = ::GetDC(desktop_window);
        if (screen_dc == NULL) {
            return 1.0;
        }
        int dpi_x = ::GetDeviceCaps(screen_dc, LOGPIXELSX);
        ::ReleaseDC(desktop_window, screen_dc);
        scale_x = (float)dpi_x / DEFAULT_DPI_X;

        if (scale_x <= 1.25) {
            // From WebKit: Force 125% and below to 100% scale. We do this to
            // maintain previous (non-DPI-aware) behavior where only the font
            // size was boosted.
            scale_x = 1.0;
        }
    }

    return scale_x;  // Windows zooms are always symmetric
}

bool disableResizeOptimization()
{
    static bool scale_read = false;
    static bool resizeOptimizationDisabled = false;
    static long lastCallMS = 0;
    SYSTEMTIME st;
    ::GetSystemTime(&st);
    long time = st.wHour * 3600000 + st.wMinute * 60000 + st.wSecond * 1000 + st.wMilliseconds;
    bool hasBeenFullSecond = time < lastCallMS || time - lastCallMS > 1000;

    // To workaround a very rare case where a webview is initially sized
    // incorrectly, we only apply the resize optimization when the last resize
    // operation occured less than a second ago.  This allows the optimization
    // to be used for user-driven interactive resize sessions.
    lastCallMS = time;

    if (!scale_read) {
        HKEY userKey;
        if (ERROR_SUCCESS != ::RegOpenCurrentUser(KEY_QUERY_VALUE, &userKey)) {
            return false;
        }

        HKEY dwmKey;
        long result = ::RegOpenKeyExW(userKey,
                                      L"Software\\Microsoft\\Windows\\DWM",
                                      0,
                                      KEY_QUERY_VALUE,
                                      &dwmKey);

        ::RegCloseKey(userKey);

        if (ERROR_SUCCESS != result) {
            return false;
        }

        scale_read = true;

        unsigned long dpiScaling;
        unsigned long size = sizeof(dpiScaling);
        result = ::RegQueryValueExW(dwmKey,
                                    L"UseDpiScaling",
                                    NULL,
                                    NULL,
                                    reinterpret_cast<unsigned char*>(&dpiScaling),
                                    &size);

        unsigned long compPolicy;
        long result2 = ::RegQueryValueExW(
                                     dwmKey,
                                     L"CompositionPolicy",
                                     NULL,
                                     NULL,
                                     reinterpret_cast<unsigned char*>(&compPolicy),
                                     &size);

        ::RegCloseKey(dwmKey);

        BOOL compEnabled;
        HRESULT result3 = ::DwmIsCompositionEnabled(&compEnabled);

        if (ERROR_SUCCESS != result || ERROR_SUCCESS != result2 || !SUCCEEDED(result3)) {
            resizeOptimizationDisabled = false;
            return false;
        }

        resizeOptimizationDisabled = !dpiScaling || compPolicy || !compEnabled ? true : false;
    }

    return hasBeenFullSecond || blpwtk2::Statics::inProcessResizeOptimizationDisabled || (resizeOptimizationDisabled && getScreenScaleFactor() > 1.0);
}

}  // close anonymous namespace

#define GetAValue(argb)      (LOBYTE((argb)>>24))

namespace {
                        // =========================
                        // class PerformanceProfiler
                        // =========================

class PerformanceProfiler final : public cc::Profiler {
    typedef std::unordered_map<int, blpwtk2::WebViewDelegate *> DelegateMap;
    typedef std::unordered_set<int> ProfileSet;

    bool d_isProfilerSet;
    DelegateMap d_delegateMap;
    ProfileSet d_activeProfiles;

  public:
    PerformanceProfiler();

    void setDelegate(int routingId, blpwtk2::WebViewDelegate *delegate);
    void beginProfile(int routingId) override;
    void endProfile(int routingId) override;
};

                        // -------------------------
                        // class PerformanceProfiler
                        // -------------------------

PerformanceProfiler::PerformanceProfiler()
    : d_isProfilerSet(false)
{
}

void PerformanceProfiler::setDelegate(int routingId, blpwtk2::WebViewDelegate *delegate)
{
    // Remove any existing delegate associated with this routing id
    DelegateMap::iterator iter = d_delegateMap.find(routingId);
    if (iter != d_delegateMap.end()) {
        if (d_activeProfiles.find(routingId) != d_activeProfiles.end()) {
            endProfile(routingId);
        }
        d_delegateMap.erase(iter);
    }

    if (delegate) {
        d_delegateMap[routingId] = delegate;

        if (!d_isProfilerSet) {
            d_isProfilerSet = true;
            cc::ProxyMain::SetProfiler(this);
        }
    }
}

void PerformanceProfiler::beginProfile(int routingId)
{
    DelegateMap::iterator iter = d_delegateMap.find(routingId);
    if (iter == d_delegateMap.end()) {
        return;
    }

    d_activeProfiles.insert(routingId);
    iter->second->startPerformanceTiming();
}

void PerformanceProfiler::endProfile(int routingId)
{
    DelegateMap::iterator iter = d_delegateMap.find(routingId);
    if (iter == d_delegateMap.end()) {
        return;
    }

    d_activeProfiles.erase(routingId);
    iter->second->stopPerformanceTiming();
}

PerformanceProfiler s_profiler;
}

namespace blpwtk2 {

                        // ------------------
                        // class WebViewProxy
                        // ------------------

WebViewProxy::WebViewProxy(WebViewDelegate *delegate, ProfileImpl *profile)
    : d_client(nullptr)
    , d_delegate(delegate)
    , d_profile(profile)
    , d_renderViewRoutingId(0)
    , d_gotRenderViewInfo(false)
    , d_pendingLoadStatus(false)
    , d_isMainFrameAccessible(false)
    , d_pendingDestroy(false)
{
    d_profile->incrementWebViewCount();
}

WebViewProxy::~WebViewProxy()
{
    LOG(INFO) << "Destroying WebViewProxy, routingId=" << d_renderViewRoutingId;
    d_profile->decrementWebViewCount();

    if (d_client) {
        WebViewClient* client = d_client;
        d_client = nullptr;
        client->releaseHost();
    }
}

void WebViewProxy::destroy()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_pendingDestroy);
    s_profiler.setDelegate(d_renderViewRoutingId, nullptr);

    // Schedule a deletion of this WebViewProxy.  The reason we don't delete
    // the object right here right now is because there may be a callback
    // that is already scheduled and the callback requires the existence of
    // the WebView.
    d_pendingDestroy = true;
    d_delegate = nullptr;
    base::MessageLoop::current()->task_runner()->DeleteSoon(FROM_HERE, this);
}

WebFrame *WebViewProxy::mainFrame()
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

int WebViewProxy::loadUrl(const StringRef& url)
{
    DCHECK(Statics::isInApplicationMainThread());
    if (d_pendingLoadStatus) {
        return EBUSY;
    }

    d_pendingLoadStatus = true;
    d_url = std::string(url.data(), url.length());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", loadUrl=" << d_url;
    d_mainFrame.reset();
    d_client->loadUrl(d_url);
    return 0;
}

void WebViewProxy::rootWindowCompositionChanged()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->rootWindowCompositionChanged();
}

void WebViewProxy::loadInspector(unsigned int pid, int routingId)
{
    DCHECK(Statics::isInApplicationMainThread());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId
              << ", loading inspector for " << routingId;

    d_client->proxy()->loadInspector(pid, routingId);
}

void WebViewProxy::inspectElementAt(const POINT& point)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->inspectElementAt(point.x, point.y);
}

int WebViewProxy::goBack()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (d_pendingLoadStatus) {
        return EBUSY;
    }

    d_pendingLoadStatus = true;
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", goBack()";
    d_mainFrame.reset();
    d_client->goBack();
    return 0;
}

int WebViewProxy::goForward()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (d_pendingLoadStatus) {
        return EBUSY;
    }

    d_pendingLoadStatus = true;
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", goForward()";
    d_mainFrame.reset();
    d_client->goForward();
    return 0;
}

int WebViewProxy::reload()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (d_pendingLoadStatus) {
        return EBUSY;
    }

    d_pendingLoadStatus = true;
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", reload()";
    d_mainFrame.reset();
    d_client->reload();
    return 0;
}

void WebViewProxy::stop()
{
    DCHECK(Statics::isInApplicationMainThread());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", stop";
    d_client->proxy()->stop();
}

void WebViewProxy::takeKeyboardFocus()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->takeKeyboardFocus();
}

void WebViewProxy::setLogicalFocus(bool focused)
{
    DCHECK(Statics::isInApplicationMainThread());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId
              << ", setLogicalFocus " << (focused ? "true" : "false");

    if (d_gotRenderViewInfo) {
        // If we have the renderer in-process, then set the logical focus
        // immediately so that handleInputEvents will work as expected.
        content::RenderViewImpl *rv =
            content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);
        DCHECK(rv);
        rv->SetFocus(focused);
    }

    // Send the message, which will update the browser-side aura::Window focus
    // state.
    d_client->proxy()->setLogicalFocus(focused); 
}

void WebViewProxy::show()
{
    DCHECK(Statics::isInApplicationMainThread());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", show";
    d_client->proxy()->show();
}

void WebViewProxy::hide()
{
    DCHECK(Statics::isInApplicationMainThread());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", hide";
    d_client->proxy()->hide();
}

void WebViewProxy::setParent(NativeView parent)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->setParent(parent);
}

void WebViewProxy::move(int left, int top, int width, int height)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->move(gfx::Rect(left, top, width, height));
}

void WebViewProxy::cutSelection()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->cutSelection();
}

void WebViewProxy::copySelection()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->copySelection();
}

void WebViewProxy::paste()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->paste();
}

void WebViewProxy::deleteSelection()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->deleteSelection();
}

void WebViewProxy::enableNCHitTest(bool enabled)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->enableNCHitTest(enabled);
}

void WebViewProxy::onNCHitTestResult(int x, int y, int result)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->ncHitTestResult(x, y, result);
}

void WebViewProxy::performCustomContextMenuAction(int actionId)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->performCustomContextMenuAction(actionId);
}

void WebViewProxy::find(const StringRef& text, bool matchCase, bool forward)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->find(std::string(text.data(), text.size()), matchCase, forward);
}

void WebViewProxy::stopFind(bool preserveSelection)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->stopFind(preserveSelection);
}

void WebViewProxy::replaceMisspelledRange(const StringRef& text)
{
    DCHECK(Statics::isInApplicationMainThread());
    std::string stext(text.data(), text.length());
    d_client->proxy()->replaceMisspelledRange(stext);
}

void WebViewProxy::rootWindowPositionChanged()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->rootWindowPositionChanged();
}

void WebViewProxy::rootWindowSettingsChanged()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->rootWindowSettingsChanged();
}

void WebViewProxy::handleInputEvents(const InputEvent *events, size_t eventsCount)
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

void WebViewProxy::setDelegate(WebViewDelegate *delegate)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_delegate = delegate;

    s_profiler.setDelegate(d_renderViewRoutingId, d_delegate);
}

int WebViewProxy::getRoutingId() const
{
    return d_renderViewRoutingId;
}

void WebViewProxy::setBackgroundColor(NativeColor color)
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

void WebViewProxy::setRegion(NativeRegion region)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->applyRegion(region);
}

void WebViewProxy::clearTooltip()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->clearTooltip();
}

v8::MaybeLocal<v8::Value> WebViewProxy::callFunction(
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

    return localWebFrame->CallFunctionEvenIfScriptDisabled(func, recv, argc, argv);
}

String WebViewProxy::printToPDF(const StringRef& propertyName)
{
    content::RenderView *rv = content::RenderView::FromRoutingID(d_renderViewRoutingId);
    return RendererUtil::printToPDF(rv, propertyName.toStdString());
}

// blpwtk2::WebViewClientDelegate overrides
void WebViewProxy::setClient(WebViewClient *client)
{
    d_client = client;
}

void WebViewProxy::ncHitTest()
{
    if (d_delegate) {
        d_delegate->requestNCHitTest(this);
        // Note: The embedder is expected to call WebView::onNCHitTestResult
    }
    else {
        onNCHitTestResult(0, 0, HTNOWHERE);
    }
}

void WebViewProxy::ncDragBegin(int hitTestCode, const gfx::Point& point)
{
    if (d_delegate) {
        POINT winPoint = { point.x(), point.y() };
        d_delegate->ncDragBegin(this, hitTestCode, winPoint);
    }
}

void WebViewProxy::ncDragMove(const gfx::Point& point)
{
    if (d_delegate) {
        POINT winPoint = { point.x(), point.y() };
        d_delegate->ncDragMove(this, winPoint);
    }
}

void WebViewProxy::ncDragEnd(const gfx::Point& point)
{
    if (d_delegate) {
        POINT winPoint = { point.x(), point.y() };
        d_delegate->ncDragEnd(this, winPoint);
    }
}

void WebViewProxy::ncDoubleClick(const gfx::Point& point)
{
    if (d_delegate) {
        POINT winPoint = { point.x(), point.y() };
        d_delegate->ncDoubleClick(this, winPoint);
    }
}

void WebViewProxy::focused()
{
    if (d_delegate) {
        d_delegate->focused(this);
    }
}

void WebViewProxy::blurred()
{
    if (d_delegate) {
        d_delegate->blurred(this);
    }
}

void WebViewProxy::showContextMenu(const ContextMenuParams& params)
{
    if (d_delegate) {
        d_delegate->showContextMenu(this, params);
    }
}

void WebViewProxy::findReply(int  numberOfMatches,
                             int  activeMatchOrdinal,
                             bool finalUpdate)
{
    if (d_delegate) {
        d_delegate->findState(
                this, numberOfMatches, activeMatchOrdinal, finalUpdate);
    }
}

void WebViewProxy::preResize(const gfx::Size& size)
{
    if (d_gotRenderViewInfo && !size.IsEmpty() && !disableResizeOptimization()) {
        // If we have renderer info (only happens if we are in-process), we can
        // start resizing the RenderView while we are in the main thread.  This
        // is to avoid a round-trip delay waiting for the resize to get to the
        // browser thread, and it sending a ViewMsg_Resize back to this thread.
        // We disable this optimization in XP-style DPI scaling.
        content::RenderView* rv = content::RenderView::FromRoutingID(d_renderViewRoutingId);
        DCHECK(rv);
        rv->SetSize(size);
    }
}

void WebViewProxy::notifyRoutingId(int id)
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
            base::Bind(&WebViewProxy::notifyRoutingId,
                       base::Unretained(this),
                       id));
        return;
    }

    d_gotRenderViewInfo = true;
    s_profiler.setDelegate(d_renderViewRoutingId, nullptr);
    s_profiler.setDelegate(id, d_delegate);

    d_renderViewRoutingId = id;
    LOG(INFO) << "routingId=" << id;
}

void WebViewProxy::onLoadStatus(int status)
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

void WebViewProxy::devToolsAgentHostAttached()
{
    if (d_delegate) {
        d_delegate->devToolsAgentHostAttached(this);
    }
}

void WebViewProxy::devToolsAgentHostDetached()
{
    if (d_delegate) {
        d_delegate->devToolsAgentHostDetached(this);
    }
}

}  // close namespace blpwtk2

// vim: ts=4 et


