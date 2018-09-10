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
#include <base/optional.h>

#include <content/renderer/render_widget.h>
#include <content/public/browser/native_web_keyboard_event.h>
#include <ui/events/event.h>

#include <content/public/renderer/render_view.h>
#include <third_party/WebKit/public/web/WebView.h>
#include <third_party/WebKit/public/web/WebFrame.h>
#include <third_party/WebKit/public/web/WebLocalFrame.h>
#include <third_party/WebKit/public/web/WebWidget.h>
#include <skia/ext/platform_canvas.h>
#include <third_party/skia/include/core/SkDocument.h>
#include <third_party/skia/include/core/SkStream.h>
#include <pdf/pdf.h>
#include <ui/gfx/geometry/size.h>
#include <ui/events/blink/web_input_event.h>
#include <ui/aura/window.h>
#include <ui/aura/client/screen_position_client.h>
#include <components/printing/renderer/print_render_frame_helper.h>
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

namespace {

base::Optional<content::NativeWebKeyboardEvent> CreateKeyboardEvent(
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
      ui::KeyEvent uiKeyboardEvent(msg);
      content::NativeWebKeyboardEvent blinkKeyboardEvent(&uiKeyboardEvent);

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

      int modifiers = blinkKeyboardEvent.GetModifiers() & masktOutModifiers;

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

      blinkKeyboardEvent.SetModifiers(modifiers);
      return base::Optional<content::NativeWebKeyboardEvent>(blinkKeyboardEvent);
    } break;

    default:
      return base::Optional<content::NativeWebKeyboardEvent>();
    }
}

base::Optional<blink::WebMouseEvent> CreateWebMouseEvent(
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
        return base::Optional<blink::WebMouseEvent>(blinkMouseEvent);
    } break;

    default:
      return base::Optional<blink::WebMouseEvent>();
    }
}


base::Optional<blink::WebMouseWheelEvent> CreateMouseWheelEvent(
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
    case WM_MOUSEWHEEL: {
        ui::MouseWheelEvent uiMouseWheelEvent(msg);
        blink::WebMouseWheelEvent blinkMouseWheelEvent =
            ui::MakeWebMouseWheelEvent(uiMouseWheelEvent,
                                       base::Bind(&GetScreenLocationFromEvent));
        return base::Optional<blink::WebMouseWheelEvent>(blinkMouseWheelEvent);
    } break;

    default:
      return base::Optional<blink::WebMouseWheelEvent>();
    }
}

}

void RendererUtil::handleInputEvents(content::RenderWidget *rw, const WebView::InputEvent *events, size_t eventsCount)
{
    for (size_t i=0; i < eventsCount; ++i) {
        auto keyboardEvent = CreateKeyboardEvent(events + i);
        if (keyboardEvent) {
            rw->bbHandleInputEvent(*keyboardEvent);
            continue;
        }

        auto mouseEvent = CreateWebMouseEvent(events + i);
        if (mouseEvent) {
            rw->bbHandleInputEvent(*mouseEvent);
            continue;
        }
        
        auto wheelEvent = CreateMouseWheelEvent(events + i);
        if (wheelEvent) {
            rw->bbHandleInputEvent(*wheelEvent);
            continue;
        }
    }
}

void RendererUtil::handleInputEvents(blink::WebWidget *webWidget, const WebView::InputEvent *events, size_t eventsCount)
{
    for (size_t i=0; i < eventsCount; ++i) {
        auto keyboardEvent = CreateKeyboardEvent(events + i);
        if (keyboardEvent) {
            webWidget->HandleInputEvent(
                blink::WebCoalescedInputEvent(*keyboardEvent));
            continue;
        }

        auto mouseEvent = CreateWebMouseEvent(events + i);
        if (mouseEvent) {
            webWidget->HandleInputEvent(
                blink::WebCoalescedInputEvent(*mouseEvent));
            continue;
        }
        
        auto wheelEvent = CreateMouseWheelEvent(events + i);
        if (wheelEvent) {
            webWidget->HandleInputEvent(
                blink::WebCoalescedInputEvent(*wheelEvent));
            continue;
        }
    }
}

// hunk separator
void RendererUtil::drawContentsToBlob(content::RenderView        *rv,
                                      Blob                       *blob,
                                      const WebView::DrawParams&  params)
{
    blink::WebFrame* webFrame = rv->GetWebView()->MainFrame();
    DCHECK(webFrame->IsWebLocalFrame());

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

String RendererUtil::printToPDF(
    content::RenderView* renderView, const std::string& propertyName)
{
    blpwtk2::String returnVal;
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::HandleScope handleScope(isolate);

    for (auto* frame = renderView->GetWebView()->MainFrame(); frame;
         frame = frame->TraverseNext()) {
      if (auto* local_frame = frame->ToWebLocalFrame()) {
        v8::Local<v8::Context> jsContext =
            local_frame->MainWorldScriptContext();
        v8::Local<v8::Object> winObject = jsContext->Global();

        if (winObject->Has(
                v8::String::NewFromUtf8(isolate, propertyName.c_str()))) {
          std::vector<char> buffer = printing::PrintRenderFrameHelper::Get(
                                         renderView->GetMainRenderFrame())
                                         ->PrintToPDF(local_frame);
          returnVal.assign(buffer.data(), buffer.size());
          break;
        }
      }
    }

    return returnVal;
}

}  // close namespace blpwtk2

// vim: ts=4 et

