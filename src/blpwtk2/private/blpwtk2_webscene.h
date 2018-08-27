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

#ifndef INCLUDED_BLPWTK2_WEBSCENE_H
#define INCLUDED_BLPWTK2_WEBSCENE_H

#include <blpwtk2_config.h>
#include <blpwtk2_webview.h>

#include <base/macros.h>
#include <services/service_manager/public/cpp/binder_registry.h>
#include <third_party/WebKit/public/platform/InterfaceRegistry.h>
#include <third_party/WebKit/public/web/WebFrameClient.h>
#include <third_party/WebKit/public/web/WebViewClient.h>

namespace blink {
class WebView;
class WebLocalFrame;
class WebFrameWidget;
class WebURLRequest;
}  // close namespace blink

namespace gfx {
class Point;
}  // close namespace gfx

namespace blpwtk2 {

class StringRef;

template<typename T>
struct CloseDeleter {
    void operator()(T *thing) { if (thing) thing->Close(); }
};

class WebSceneFrame;

                      // ==============
                      // class WebScene
                      // ==============

class WebScene final : public WebView
                     , private blink::InterfaceRegistry
                     , private blink::WebViewClient
                     , private blink::WebFrameClient
{
    // DATA
    service_manager::BinderRegistry d_interface_registry;

    WebViewDelegate *d_delegate;

    std::unique_ptr<
        blink::WebView,
        CloseDeleter<blink::WebView>> d_webView;
    std::unique_ptr<
        blink::WebFrameWidget,
        CloseDeleter<blink::WebFrameWidget>> d_webFrameWidget;

    std::unique_ptr<blink::WebURLRequest> d_urlRequest;
    std::unique_ptr<WebSceneFrame> d_mainFrame;

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
#if defined(BLPWTK2_FEATURE_FOCUS)
    void takeKeyboardFocus() override;
    void setLogicalFocus(bool focused) override;
#endif
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
#if defined(BLPWTK2_FEATURE_RUBBERBAND)
    void enableAltDragRubberbanding(bool enabled) override;
    bool forceStartRubberbanding(int x, int y) override;
    bool isRubberbanding() const override;
    void abortRubberbanding() override;
    String getTextInRubberband(const NativeRect&) override;
#endif
    void find(const StringRef& text, bool matchCase, bool forward) override;
    void stopFind(bool preserveSelection) override;
    void replaceMisspelledRange(const StringRef& text) override;
    void rootWindowPositionChanged() override;
    void rootWindowSettingsChanged() override;
    void handleInputEvents(const InputEvent *events,
                           size_t            eventsCount) override;
    void setDelegate(WebViewDelegate *delegate) override;
#if defined(BLPWTK2_FEATURE_SCREENPRINT)
    void drawContentsToBlob(Blob *blob, const DrawParams& params) override;
#endif
    int getRoutingId() const override;
    void setBackgroundColor(NativeColor color) override;
    void setRegion(NativeRegion region) override;
    void clearTooltip() override;
#if defined(BLPWTK2_FEATURE_DWM)
    void rootWindowCompositionChanged() override;
#endif
    v8::MaybeLocal<v8::Value> callFunction(
            v8::Local<v8::Function>  func,
            v8::Local<v8::Value>     recv,
            int                      argc,
            v8::Local<v8::Value>    *argv) override;
#if defined(BLPWTK2_FEATURE_PRINTPDF)
    String printToPDF(const StringRef& propertyName) override;
#endif

    // blink::InterfaceRegistry overrides:
    void AddInterface(const char* name,
                      const blink::InterfaceFactory& factory) override;

    // blink::WebFrameClient overrides:
    void DidFailLoad(const blink::WebURLError&, blink::WebHistoryCommitType) override;
    void DidFinishLoad() override;
    std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
        const blink::WebURLRequest& request,
        base::SingleThreadTaskRunner *task_runner) override;

    DISALLOW_COPY_AND_ASSIGN(WebScene);

  public:
    explicit WebScene(
        WebViewDelegate *delegate,
        const blpwtk2::StringRef& html);
    ~WebScene();
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_WEBSCENE_H

// vim: ts=4 et

