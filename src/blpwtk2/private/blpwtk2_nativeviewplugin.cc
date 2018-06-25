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

#include <blpwtk2_nativeviewplugin.h>

#include <base/bind.h>
#include <base/logging.h>
#include <base/message_loop/message_loop.h>
#include <base/strings/string_number_conversions.h>
#include <base/win/win_util.h>
#include <third_party/WebKit/public/web/WebLocalFrame.h>
#include <third_party/WebKit/public/web/WebPluginContainer.h>
#include <third_party/WebKit/public/web/WebPluginParams.h>
#include <third_party/WebKit/public/web/WebScriptBindings.h>
#include <v8/include/v8.h>

namespace blpwtk2 {

NativeViewPlugin::NativeViewPlugin(blink::WebLocalFrame* frame, const blink::WebPluginParams& params)
: d_params(params)
, d_container(nullptr)
, d_frame(frame)
, d_nativeView(0)
{
    for (std::size_t
            i = 0,
            n = std::min(
                params.attributeNames.size(),
                params.attributeValues.size());
         i < n;
         ++i) {
        const auto& name  = params.attributeNames[i];
        const auto& value = params.attributeValues[i];

        if (name == "nativeview") {
            uint64_t nativeView = 0;
            if (base::StringToUint64(value.utf8(), &nativeView)) {
                d_nativeView = reinterpret_cast<HWND>(nativeView);
            }
            else {
                d_nativeView = 0;
            }
        }
    }

}

NativeViewPlugin::~NativeViewPlugin()
{
}

// blink::WebPlugin overrides

bool NativeViewPlugin::initialize(blink::WebPluginContainer* container)
{
    d_container = container;

    return true;
}

void NativeViewPlugin::destroy()
{
    if (d_container) {
        base::MessageLoop::current()->task_runner()->DeleteSoon(
            FROM_HERE, this);

        d_container = nullptr;
    }
}

blink::WebPluginContainer* NativeViewPlugin::container() const
{
    return d_container;
}

void NativeViewPlugin::updateGeometry(
    const blink::WebRect& windowRect, const blink::WebRect& clipRect,
    const blink::WebRect& unobscuredRect, const blink::WebVector<blink::WebRect>& cutOutsRects,
    bool isVisible)
{
    if (!d_nativeView) {
        return;
    }

    blink::WebScriptBindings::runUserAgentScript(
        base::Bind(
            &NativeViewPlugin::updateGeometryImpl,
            base::Unretained(this),
            windowRect, clipRect, unobscuredRect, cutOutsRects, isVisible));
}

void NativeViewPlugin::updateGeometryImpl(
    const blink::WebRect& windowRect, const blink::WebRect& clipRect,
    const blink::WebRect& unobscuredRect, const blink::WebVector<blink::WebRect>& cutOutsRects,
    bool isVisible)
{
    ::SetWindowPos(
        d_nativeView, 0,
        windowRect.x, windowRect.y, windowRect.width, windowRect.height,
        SWP_NOZORDER | SWP_NOACTIVATE |
        (isVisible? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
}

void NativeViewPlugin::updateVisibility(bool isVisible)
{
    if (!d_nativeView) {
        return;
    }

    blink::WebScriptBindings::runUserAgentScript(
        base::Bind(
            &NativeViewPlugin::updateVisibilityImpl,
            base::Unretained(this),
            isVisible));
}

void NativeViewPlugin::updateVisibilityImpl(bool isVisible)
{
    ::SetWindowPos(
        d_nativeView, 0,
        0, 0, 0, 0,
        SWP_NOZORDER | SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE |
        (isVisible? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
}

void NativeViewPlugin::didReceiveData(const char* data, int dataLength)
{
    uint64_t nativeView = 0;
    if (base::StringToUint64(std::string(data, dataLength), &nativeView)) {
        d_nativeView = reinterpret_cast<HWND>(nativeView);
    }
    else {
        d_nativeView = 0;
    }
}

}  // close namespace blpwtk2
