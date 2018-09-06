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

#include <blpwtk2_webscene.h>

#include <blpwtk2_stringref.h>
#include <blpwtk2_webframeimpl.h>
#include <blpwtk2_webviewclient.h>
#include <blpwtk2_contextmenuparams.h>
#include <blpwtk2_profileimpl.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_stringref.h>
#include <blpwtk2_webframeimpl.h>
#include <blpwtk2_webviewdelegate.h>
#include <blpwtk2_blob.h>
#include <blpwtk2_rendererutil.h>

#include <base/logging.h>
#include <content/renderer/render_thread_impl.h>
#include <content/renderer/render_view_impl.h>
#include <content/renderer/renderer_blink_platform_impl.h>
#include <content/public/renderer/render_view.h>
#include <third_party/skia/include/core/SkDocument.h>
#include <third_party/skia/include/core/SkStream.h>
#include <third_party/WebKit/public/platform/WebData.h>
#include <third_party/WebKit/public/platform/WebURLRequest.h>
#include <third_party/WebKit/public/web/WebFrameWidget.h>
#include <third_party/WebKit/public/web/WebLocalFrame.h>
#include <third_party/WebKit/public/web/WebView.h>
#include <cc/trees/proxy_main.h>

#include <dwmapi.h>
#include <windows.h>
#include <unordered_map>
#include <unordered_set>

#define GetAValue(argb)      (LOBYTE((argb)>>24))

namespace blpwtk2 {

class WebSceneFrame : public WebFrame {
private:

    std::unique_ptr<
        blink::WebLocalFrame,
        CloseDeleter<blink::WebLocalFrame>> d_webFrame;
    WebFrameImpl d_impl;

public:
    explicit WebSceneFrame(
        std::unique_ptr<
            blink::WebLocalFrame,
            CloseDeleter<blink::WebLocalFrame>> webFrame);

    // WebFrame overrides:
    v8::Local<v8::Context> mainWorldScriptContext() const override;
    v8::Isolate* scriptIsolate() const override;
    void setContentSettingsDelegate(WebContentSettingsDelegate *contentSettingsDelegate) override;

    blink::WebLocalFrame *webFrame() { return d_webFrame.get(); }
};

WebSceneFrame::WebSceneFrame(
    std::unique_ptr<
        blink::WebLocalFrame,
        CloseDeleter<blink::WebLocalFrame>> webFrame)
: d_webFrame(std::move(webFrame))
, d_impl(d_webFrame.get())
{
}

v8::Local<v8::Context> WebSceneFrame::mainWorldScriptContext() const
{
    return d_impl.mainWorldScriptContext();
}

v8::Isolate* WebSceneFrame::scriptIsolate() const
{
    return d_impl.scriptIsolate();
}

void WebSceneFrame::setContentSettingsDelegate(
    WebContentSettingsDelegate *contentSettingsDelegate)
{
    return d_impl.setContentSettingsDelegate(contentSettingsDelegate);
}

                      // --------------
                      // class WebScene
                      // --------------

WebScene::WebScene(WebViewDelegate *delegate, const blpwtk2::StringRef& html)
    : d_delegate(delegate)
{
    d_webView.reset(
        blink::WebView::Create(
            this,
            blink::kWebPageVisibilityStateHidden));
    d_webView->SetDisplayMode(blink::kWebDisplayModeBrowser);

    std::unique_ptr<
        blink::WebLocalFrame,
        CloseDeleter<blink::WebLocalFrame>> webFrame(
            blink::WebLocalFrame::CreateMainFrame(
                d_webView.get(),
                this,
                this));

    d_webFrameWidget.reset(
        blink::WebFrameWidget::Create(
            this,
            webFrame.get()));
    d_webFrameWidget->SetVisibilityState(blink::kWebPageVisibilityStateHidden);

    d_mainFrame.reset(
        new WebSceneFrame(std::move(webFrame)));

    if (!html.isEmpty()) {
        d_mainFrame->webFrame()->LoadHTMLString(
            blink::WebData(html.data(), html.length()),
            blink::WebURL());
    }

    if (d_delegate) {
        d_delegate->created(this);
    }
}

WebScene::~WebScene()
{
    LOG(INFO) << "Destroying WebScene";
}

void WebScene::destroy()
{
    DCHECK(Statics::isInApplicationMainThread());

    // Schedule a deletion of this WebScene.  The reason we don't delete
    // the object right here right now is because there may be a callback
    // that is already scheduled and the callback requires the existence of
    // the WebView.
    d_delegate = nullptr;
    base::MessageLoop::current()->task_runner()->DeleteSoon(FROM_HERE, this);
}

WebFrame *WebScene::mainFrame()
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    return d_mainFrame.get();
}

int WebScene::loadUrl(const StringRef& url)
{
    d_urlRequest.reset(
        new blink::WebURLRequest(GURL(url.data())));
    d_urlRequest->SetCheckForBrowserSideNavigation(false);

    d_mainFrame->webFrame()->Load(*d_urlRequest);

    return 0;
}

#if defined(BLPWTK2_FEATURE_DWM)
void WebScene::rootWindowCompositionChanged()
{
    NOTIMPLEMENTED();
}
#endif

void WebScene::loadInspector(unsigned int pid, int routingId)
{
    NOTIMPLEMENTED();
}

void WebScene::inspectElementAt(const POINT& point)
{
    NOTIMPLEMENTED();
}

#if defined(BLPWTK2_FEATURE_SCREENPRINT)
void WebScene::drawContentsToBlob(Blob *blob, const DrawParams& params)
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(blob);

    auto webFrame = d_mainFrame->webFrame();

    int srcWidth = params.srcRegion.right - params.srcRegion.left;
    int srcHeight = params.srcRegion.bottom - params.srcRegion.top;

    if (params.rendererType == WebView::DrawParams::RendererType::PDF) {
        SkDynamicMemoryWStream& pdf_stream = blob->makeSkStream();
        {
            sk_sp<SkDocument> document(
                    SkDocument::MakePDF(&pdf_stream,
                                        params.dpi,
                                        SkDocument::PDFMetadata(),
                                        nullptr,
                                        false).release());

            SkCanvas *canvas = document->beginPage(params.destWidth, params.destHeight);
            DCHECK(canvas);
            canvas->scale(params.destWidth / srcWidth, params.destHeight / srcHeight);

            webFrame->DrawInCanvas(blink::WebRect(params.srcRegion.left, params.srcRegion.top, srcWidth, srcHeight),
                                   blink::WebString::FromUTF8(params.styleClass.data(), params.styleClass.length()),
                                   *canvas);
            canvas->flush();
            document->endPage();
        }
    }
    else if (params.rendererType == WebView::DrawParams::RendererType::Bitmap) {
        SkBitmap& bitmap = blob->makeSkBitmap();
        bitmap.allocN32Pixels(params.destWidth + 0.5, params.destHeight + 0.5);

        SkCanvas canvas(bitmap);
        canvas.scale(params.destWidth / srcWidth, params.destHeight / srcHeight);

        webFrame->DrawInCanvas(blink::WebRect(params.srcRegion.left, params.srcRegion.top, srcWidth, srcHeight),
                               blink::WebString::FromUTF8(params.styleClass.data(), params.styleClass.length()),
                               canvas);

        canvas.flush();
    }
}
#endif

int WebScene::goBack()
{
    NOTIMPLEMENTED();
    return 0;
}

int WebScene::goForward()
{
    NOTIMPLEMENTED();
    return 0;
}

int WebScene::reload()
{
    NOTIMPLEMENTED();
    return 0;
}

void WebScene::stop()
{
    NOTIMPLEMENTED();
}

#if defined(BLPWTK2_FEATURE_FOCUS)
void WebScene::takeKeyboardFocus()
{
    NOTIMPLEMENTED();
}

void WebScene::setLogicalFocus(bool focused)
{
    NOTIMPLEMENTED();
}
#endif

void WebScene::show()
{
    NOTIMPLEMENTED();
}

void WebScene::hide()
{
    NOTIMPLEMENTED();
}

void WebScene::setParent(NativeView parent)
{
    NOTIMPLEMENTED();
}

void WebScene::move(int left, int top, int width, int height)
{
    NOTIMPLEMENTED();
}

void WebScene::cutSelection()
{
    NOTIMPLEMENTED();
}

void WebScene::copySelection()
{
    NOTIMPLEMENTED();
}

void WebScene::paste()
{
    NOTIMPLEMENTED();
}

void WebScene::deleteSelection()
{
    NOTIMPLEMENTED();
}

void WebScene::enableNCHitTest(bool enabled)
{
    NOTIMPLEMENTED();
}

void WebScene::onNCHitTestResult(int x, int y, int result)
{
    NOTIMPLEMENTED();
}

void WebScene::performCustomContextMenuAction(int actionId)
{
    NOTIMPLEMENTED();
}

void WebScene::find(const StringRef& text, bool matchCase, bool forward)
{
    NOTIMPLEMENTED();
}

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
void WebScene::enableAltDragRubberbanding(bool enabled)
{
    NOTIMPLEMENTED();
}

bool WebScene::forceStartRubberbanding(int x, int y)
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    return d_webView->ForceStartRubberbanding(x, y);
}

bool WebScene::isRubberbanding() const
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    return d_webView->IsRubberbanding();
}

void WebScene::abortRubberbanding()
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    d_webView->AbortRubberbanding();
}

String WebScene::getTextInRubberband(const NativeRect& rect)
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    blink::WebRect webRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    std::string str = d_webView->GetTextInRubberband(webRect).Utf8();
    return String(str.data(), str.size());
}
#endif

void WebScene::stopFind(bool preserveSelection)
{
    NOTIMPLEMENTED();
}

void WebScene::replaceMisspelledRange(const StringRef& text)
{
    NOTIMPLEMENTED();
}

void WebScene::rootWindowPositionChanged()
{
    NOTIMPLEMENTED();
}

void WebScene::rootWindowSettingsChanged()
{
    NOTIMPLEMENTED();
}

void WebScene::handleInputEvents(const InputEvent *events, size_t eventsCount)
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());

    RendererUtil::handleInputEvents(
        d_webFrameWidget.get(), events, eventsCount);
}

void WebScene::setDelegate(WebViewDelegate *delegate)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_delegate = delegate;
}

int WebScene::getRoutingId() const
{
    return 0;
}

void WebScene::setBackgroundColor(NativeColor color)
{
    int red = GetRValue(color);
    int green = GetGValue(color);
    int blue = GetBValue(color);
    int alpha = GetAValue(color);

    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());

    d_webFrameWidget->SetBaseBackgroundColor(
        SkColorSetARGB(alpha, red, green, blue));
}

void WebScene::setRegion(NativeRegion region)
{
    NOTIMPLEMENTED();
}

void WebScene::clearTooltip()
{
    NOTIMPLEMENTED();
}

v8::MaybeLocal<v8::Value> WebScene::callFunction(
        v8::Local<v8::Function>  func,
        v8::Local<v8::Value>     recv,
        int                      argc,
        v8::Local<v8::Value>    *argv)
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());

    v8::Local<v8::Value> result =
        d_mainFrame->webFrame()->CallFunctionEvenIfScriptDisabled(
            func, recv, argc, argv);

    return v8::MaybeLocal<v8::Value>(result);
}

#if defined(BLPWTK2_FEATURE_PRINTPDF)
String WebScene::printToPDF(const StringRef& propertyName)
{
    NOTIMPLEMENTED();
    return String();
}
#endif

// blink::InterfaceRegistry overrides
void WebScene::AddInterface(const char* name,
                            const blink::InterfaceFactory& factory) {
    d_interface_registry.AddInterface(name, factory);
}

// blink::WebFrameClient overrides
void WebScene::DidFailLoad(const blink::WebURLError&, blink::WebHistoryCommitType)
{
    std::unique_ptr<blink::WebURLRequest> urlRequest = std::move(d_urlRequest);

    if (d_delegate) {
        d_delegate->didFailLoad(
            this, fromWebString(urlRequest->Url().GetString()));
    }
}

void WebScene::DidFinishLoad()
{
    std::unique_ptr<blink::WebURLRequest> urlRequest = std::move(d_urlRequest);

    if (d_delegate) {
        d_delegate->didFinishLoad(
            this, fromWebString(urlRequest->Url().GetString()));
    }
}

std::unique_ptr<blink::WebURLLoader> WebScene::CreateURLLoader(
    const blink::WebURLRequest& request,
    base::SingleThreadTaskRunner *task_runner)
{
    return content::RenderThreadImpl::current()->
        blink_platform_impl()->
            CreateURLLoader(request, task_runner);
}

}  // close namespace blpwtk2

// vim: ts=4 et


