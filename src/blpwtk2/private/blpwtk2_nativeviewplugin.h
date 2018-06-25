/* * Copyright (C) 2017 Bloomberg Finance L.P.
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

#ifndef INCLUDED_BLPWTK2_NATIVEVIEWPLUGIN_H
#define INCLUDED_BLPWTK2_NATIVEVIEWPLUGIN_H

#include <blpwtk2_config.h>

#include <third_party/WebKit/public/web/WebPlugin.h>
#include <third_party/WebKit/public/web/WebPluginParams.h>

namespace blink {
class WebDOMEvent;
class WebLocalFrame;
struct WebPluginParams;
}  // close namespace blink

namespace blpwtk2 {

// This is a WebPlugin implementation that is created whenever there is an
// object element with "application/x-bloomberg-nativeview" mime type. It
// sets the geometry and visibility of an HWND in response to notification
// from blink.
class NativeViewPlugin : public blink::WebPlugin {
  public:
    explicit NativeViewPlugin(blink::WebLocalFrame* frame, const blink::WebPluginParams& params);
    virtual ~NativeViewPlugin();

    // blink::WebPlugin overrides
    bool initialize(blink::WebPluginContainer*) override;
    void destroy() override;
    blink::WebPluginContainer* container() const override;
    void paint(blink::WebCanvas*, const blink::WebRect&) override {}
    void updateGeometry(
        const blink::WebRect& windowRect, const blink::WebRect& clipRect,
        const blink::WebRect& unobscuredRect, const blink::WebVector<blink::WebRect>& cutOutsRects,
        bool isVisible) override;
    void updateFocus(bool, blink::WebFocusType) override {}
    void updateVisibility(bool isVisible) override;
    blink::WebInputEventResult handleInputEvent(const blink::WebInputEvent&, blink::WebCursorInfo&) override { return blink::WebInputEventResult::NotHandled; }
    void didReceiveResponse(const blink::WebURLResponse&) override {}
    void didReceiveData(const char* data, int dataLength) override;
    void didFinishLoading() override {}
    void didFailLoading(const blink::WebURLError&) override {}
    void updateAllLifecyclePhases() override {}

  private:
    void updateGeometryImpl(
        const blink::WebRect& windowRect, const blink::WebRect& clipRect,
        const blink::WebRect& unobscuredRect, const blink::WebVector<blink::WebRect>& cutOutsRects,
        bool isVisible);
    void updateVisibilityImpl(bool isVisible);

    blink::WebPluginParams d_params;

    blink::WebPluginContainer* d_container;
    blink::WebLocalFrame* d_frame;

    NativeView d_nativeView;

    DISALLOW_COPY_AND_ASSIGN(NativeViewPlugin);
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_NATIVEVIEWPLUGIN_H
