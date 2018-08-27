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

#include <blpwtk2_rendererutil.h>
#include <blpwtk2_blob.h>
#include <blpwtk2_statics.h>

#include <base/logging.h>  // for DCHECK
#include <base/optional.h>

#include <content/renderer/render_widget.h>
#include <content/public/browser/native_web_keyboard_event.h>
#include <ui/events/event.h>

#include <content/public/renderer/render_view.h>
#include <third_party/WebKit/public/web/WebView.h>
#include <third_party/WebKit/public/web/WebFrame.h>
#include <third_party/WebKit/public/web/WebWidget.h>
#include <skia/ext/platform_canvas.h>
#include <third_party/skia/include/core/SkDocument.h>
#include <third_party/skia/include/core/SkStream.h>
#include <pdf/pdf.h>
#include <ui/gfx/geometry/size.h>
#include <ui/events/blink/web_input_event.h>
#include <ui/aura/window.h>
#include <ui/aura/client/screen_position_client.h>

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

namespace {

base::Optional<blink::WebInputEvent> CreateWebInputEvent(
    const WebView::InputEvent *event)
{
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
      constexpr int masktOutModifiers =
          ~(blink::WebInputEvent::kShiftKey |
            blink::WebInputEvent::kControlKey |
            blink::WebInputEvent::kAltKey |
            blink::WebInputEvent::kMetaKey |
            blink::WebInputEvent::kIsAutoRepeat |
            blink::WebInputEvent::kIsKeyPad |
            blink::WebInputEvent::kIsLeft |
            blink::WebInputEvent::kIsRight |
            blink::WebInputEvent::kNumLockOn |
            blink::WebInputEvent::kCapsLockOn);
      ui::KeyEvent uiKeyboardEvent(msg);
      int modifiers = uiKeyboardEvent.flags() & masktOutModifiers;

      if (event->shiftKey)
        modifiers |= blink::WebInputEvent::kShiftKey;

      if (event->controlKey)
        modifiers |= blink::WebInputEvent::kControlKey;

      if (event->altKey)
        modifiers |= blink::WebInputEvent::kAltKey;

      if (event->metaKey)
        modifiers |= blink::WebInputEvent::kMetaKey;

      if (event->isAutoRepeat)
        modifiers |= blink::WebInputEvent::kIsAutoRepeat;

      if (event->isKeyPad)
        modifiers |= blink::WebInputEvent::kIsKeyPad;

      if (event->isLeft)
        modifiers |= blink::WebInputEvent::kIsLeft;

      if (event->isRight)
        modifiers |= blink::WebInputEvent::kIsRight;

      if (event->numLockOn)
        modifiers |= blink::WebInputEvent::kNumLockOn;

      if (event->capsLockOn)
        modifiers |= blink::WebInputEvent::kCapsLockOn;

      uiKeyboardEvent.set_flags(modifiers);
      content::NativeWebKeyboardEvent blinkKeyboardEvent(uiKeyboardEvent);
      return base::Optional<blink::WebInputEvent>(blinkKeyboardEvent);
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
        return base::Optional<blink::WebInputEvent>(blinkMouseEvent);
    } break;

    case WM_MOUSEWHEEL: {
        ui::MouseWheelEvent uiMouseWheelEvent(msg);
        blink::WebMouseWheelEvent blinkMouseWheelEvent =
            ui::MakeWebMouseWheelEvent(uiMouseWheelEvent,
                                       base::Bind(&GetScreenLocationFromEvent));
        return base::Optional<blink::WebInputEvent>(blinkMouseWheelEvent);
    } break;

    default:
      return base::Optional<blink::WebInputEvent>();
    }
}

}

void RendererUtil::handleInputEvents(content::RenderWidget *rw, const WebView::InputEvent *events, size_t eventsCount)
{
    for (size_t i=0; i < eventsCount; ++i) {
        auto webInputEvent = CreateWebInputEvent(events + i);

        if (!webInputEvent) {
          continue;
        }

        rw->bbHandleInputEvent(*webInputEvent);
    }
}

void RendererUtil::handleInputEvents(blink::WebWidget *webWidget, const WebView::InputEvent *events, size_t eventsCount)
{
    for (size_t i=0; i < eventsCount; ++i) {
        auto webInputEvent = CreateWebInputEvent(events + i);

        if (!webInputEvent) {
          continue;
        }

        webWidget->HandleInputEvent(
            blink::WebCoalescedInputEvent(*webInputEvent));
    }
}

// hunk separator

}  // close namespace blpwtk2

// vim: ts=4 et

