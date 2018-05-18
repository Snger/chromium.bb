/*
 * Copyright (C) 2015 Bloomberg Finance L.P.
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
#include <vector>

#include <blpwtk2_rendererutil.h>
#include <blpwtk2_blob.h>
#include <blpwtk2_statics.h>

#include <base/logging.h>  // for DCHECK

#include <content/renderer/render_widget.h>
#include <content/public/browser/native_web_keyboard_event.h>
#include <ui/events/event.h>

#include <third_party/WebKit/public/web/WebView.h>
#include <third_party/WebKit/public/web/WebFrame.h>
#include <skia/ext/platform_canvas.h>
#include <third_party/skia/include/core/SkDocument.h>
#include <third_party/skia/include/core/SkStream.h>
#include <pdf/pdf.h>
#include <ui/gfx/geometry/size.h>
#include <ui/events/blink/web_input_event.h>
#include <ui/aura/window.h>
#include <ui/aura/client/screen_position_client.h>
#include <components/printing/renderer/print_web_view_helper.h>
#include <v8.h>

namespace blpwtk2 {

// This function is copied from
// content/browser/renderer_host/renderer_widget_host_view_event_handler.cc
gfx::Point GetScreenLocationFromEvent(const ui::LocatedEvent& event)
{
    aura::Window* root =
        static_cast<aura::Window*>(event.target())->GetRootWindow();
    aura::client::ScreenPositionClient *spc =
        aura::client::GetScreenPositionClient(root);
    if (!spc) {
        return event.root_location();
    }

    gfx::Point screen_location(event.root_location());
    spc->ConvertPointToScreen(root, &screen_location);
    return screen_location;
}


void RendererUtil::handleInputEvents(content::RenderWidget *rw, const WebView::InputEvent *events, size_t eventsCount)
{
    for (size_t i=0; i < eventsCount; ++i) {
        const WebView::InputEvent *event = events + i;
        MSG msg = {
            event->hwnd,
            event->message,
            event->wparam,
            event->lparam,
            GetMessageTime()
        };

        switch (event->message) {
        case WM_SYSKEYDOWN:
        case WM_KEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYUP:
        case WM_IME_CHAR:
        case WM_SYSCHAR:
        case WM_CHAR: {
            ui::KeyEvent uiKeyboardEvent(msg);
            content::NativeWebKeyboardEvent blinkKeyboardEvent(&uiKeyboardEvent);

            blinkKeyboardEvent.modifiers &= ~(
                    blink::WebInputEvent::ShiftKey |
                    blink::WebInputEvent::ControlKey |
                    blink::WebInputEvent::AltKey |
                    blink::WebInputEvent::MetaKey |
                    blink::WebInputEvent::IsAutoRepeat |
                    blink::WebInputEvent::IsKeyPad |
                    blink::WebInputEvent::IsLeft |
                    blink::WebInputEvent::IsRight |
                    blink::WebInputEvent::NumLockOn |
                    blink::WebInputEvent::CapsLockOn
                );

            if (event->shiftKey)
                blinkKeyboardEvent.modifiers |= blink::WebInputEvent::ShiftKey;

            if (event->controlKey)
                blinkKeyboardEvent.modifiers |= blink::WebInputEvent::ControlKey;

            if (event->altKey)
                blinkKeyboardEvent.modifiers |= blink::WebInputEvent::AltKey;

            if (event->metaKey)
                blinkKeyboardEvent.modifiers |= blink::WebInputEvent::MetaKey;

            if (event->isAutoRepeat)
                blinkKeyboardEvent.modifiers |= blink::WebInputEvent::IsAutoRepeat;

            if (event->isKeyPad)
                blinkKeyboardEvent.modifiers |= blink::WebInputEvent::IsKeyPad;

            if (event->isLeft)
                blinkKeyboardEvent.modifiers |= blink::WebInputEvent::IsLeft;

            if (event->isRight)
                blinkKeyboardEvent.modifiers |= blink::WebInputEvent::IsRight;

            if (event->numLockOn)
                blinkKeyboardEvent.modifiers |= blink::WebInputEvent::NumLockOn;

            if (event->capsLockOn)
                blinkKeyboardEvent.modifiers |= blink::WebInputEvent::CapsLockOn;

            rw->bbHandleInputEvent(blinkKeyboardEvent);
        } break;

        case WM_MOUSEMOVE:
        case WM_MOUSELEAVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP: {
            ui::MouseEvent uiMouseEvent(msg);
            blink::WebMouseEvent blinkMouseEvent = ui::MakeWebMouseEvent(
                    uiMouseEvent,
                    base::Bind(&GetScreenLocationFromEvent));
            rw->bbHandleInputEvent(blinkMouseEvent);
        } break;

        case WM_MOUSEWHEEL: {
            ui::MouseWheelEvent uiMouseWheelEvent(msg);
            blink::WebMouseWheelEvent blinkMouseWheelEvent =
                ui::MakeWebMouseWheelEvent(uiMouseWheelEvent,
                                           base::Bind(&GetScreenLocationFromEvent));
            rw->bbHandleInputEvent(blinkMouseWheelEvent);
        } break;
        }
    }
}

// hunk separator

String RendererUtil::printToPDF(
    content::RenderView* renderView, const std::string& propertyName)
{
    blpwtk2::String returnVal;
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);

    for (auto* frame = renderView->GetWebView()->mainFrame();
         frame;
         frame = frame->traverseNext()) {

        v8::Local<v8::Context> jsContext = frame->mainWorldScriptContext();
        v8::Local<v8::Object> winObject = jsContext->Global();

        if (winObject->Has(v8::String::NewFromUtf8(isolate, propertyName.c_str()))) {
            std::vector<char> buffer =
                printing::PrintWebViewHelper::Get(renderView->GetMainRenderFrame())->PrintToPDF(
                    frame->toWebLocalFrame());

            returnVal.assign(buffer.data(), buffer.size());
            break;
        }
    }

    return returnVal;
}

}  // close namespace blpwtk2

// vim: ts=4 et

