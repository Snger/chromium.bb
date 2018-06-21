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

#ifndef INCLUDED_BLPWTK2_JSWIDGET_H
#define INCLUDED_BLPWTK2_JSWIDGET_H

#include <blpwtk2_config.h>

#include <third_party/WebKit/public/web/WebElement.h>
#include <third_party/WebKit/public/web/WebPlugin.h>

namespace blink {
class WebDOMEvent;
class WebLocalFrame;
}  // close namespace blink

namespace blpwtk2 {

// This is a WebPlugin implementation that is created whenever there is an
// object element with "application/x-bloomberg-jswidget" mime type.  All it
// does is raise custom events on the DOM element whenever certain plugin
// callbacks are invoked.
class JsWidget : public blink::WebPlugin {
  public:
    explicit JsWidget(blink::WebLocalFrame* frame);
    virtual ~JsWidget();

    void dispatchEvent(const blink::WebDOMEvent& event);

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
    void didReceiveData(const char* data, int dataLength) override {}
    void didFinishLoading() override {}
    void didFailLoading(const blink::WebURLError&) override {}
    void updateAllLifecyclePhases() override {}
    void addedToParent() override;
    void removedFromParent() override;

  private:
    blink::WebPluginContainer* d_container;    
    blink::WebLocalFrame* d_frame;
    bool d_hasParent;
    bool d_pendingVisible; // Whether to make visible when added to a parent

    DISALLOW_COPY_AND_ASSIGN(JsWidget);
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_JSWIDGET_H
