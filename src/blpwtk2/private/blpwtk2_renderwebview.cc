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
#include <blpwtk2_dragdrop.h>
#include <blpwtk2_profileimpl.h>
#include <blpwtk2_rendercompositor.h>
#include <blpwtk2_rendermessagedelegate.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_stringref.h>
#include <blpwtk2_webframeimpl.h>
#include <blpwtk2_webviewdelegate.h>
#include <blpwtk2_blob.h>
#include <blpwtk2_rendererutil.h>

#include <base/message_loop/message_loop.h>
#include <base/win/scoped_gdi_object.h>
#include <content/browser/renderer_host/display_util.h>
#include <content/common/frame_messages.h>
#include <content/common/drag_messages.h>
#include <content/common/input_messages.h>
#include <content/common/view_messages.h>
#include <content/renderer/render_thread_impl.h>
#include <content/renderer/render_view_impl.h>
#include <content/renderer/render_widget.h>
#include <content/public/renderer/render_view.h>
#include <third_party/blink/public/web/web_local_frame.h>
#include <third_party/blink/public/web/web_view.h>
#include <ui/base/win/lock_state.h>
#include <ui/base/cursor/cursor_loader.h>
#include <ui/base/win/lock_state.h>
#include <ui/events/blink/web_input_event.h>
#include "ui/events/blink/web_input_event_traits.h"
#include <ui/base/ime/input_method.h>
#include <ui/base/ime/input_method_factory.h>
#include <ui/base/win/mouse_wheel_util.h>
#include <ui/events/event.h>
#include <ui/events/event_utils.h>
#include <ui/latency/latency_info.h>
#include <ui/gfx/icon_util.h>
#include <ui/display/display.h>
#include <ui/display/screen.h>
#include <ui/views/corewm/tooltip_win.h>
#include <v8/include/v8.h>

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
#include <ui/base/win/rubberband_windows.h>
#endif

#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>
#include <unordered_map>
#include <unordered_set>

#define GetAValue(argb)      (LOBYTE((argb)>>24))

namespace {

gfx::PointF GetScreenLocationFromEvent(const ui::LocatedEvent& event)
{
    return event.root_location_f();
}

void GetNativeViewScreenInfo(content::ScreenInfo* screen_info,
                             HWND hwnd) {
    display::Screen* screen = display::Screen::GetScreen();
    if (!screen) {
        *screen_info = content::ScreenInfo();
        return;
    }

    HMONITOR monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

    MONITORINFO monitor_info = { sizeof(MONITORINFO) };
    GetMonitorInfo(monitor, &monitor_info);

    content::DisplayUtil::DisplayToScreenInfo(
        screen_info,
        screen->GetDisplayMatching(
            gfx::Rect(monitor_info.rcMonitor)));
}

const int kDelayForTooltipUpdateInMs = 500;
const int kDefaultTooltipShownTimeoutMs = 10000;
constexpr base::TimeDelta kDefaultMouseWheelLatchingTransaction =
    base::TimeDelta::FromMilliseconds(500);
const double kWheelLatchingSlopRegion = 10.0;

}

namespace blpwtk2 {

                        // -------------------
                        // class RenderWebView
                        // -------------------

RenderWebView::RenderWebView(WebViewDelegate          *delegate,
                             ProfileImpl              *profile,
                             const WebViewProperties&  properties)
    : d_proxy(new WebViewProxy(this, profile))
    , d_delegate(delegate)
    , d_profile(profile)
#if defined(BLPWTK2_FEATURE_FOCUS) || defined(BLPWTK2_FEATURE_REROUTEMOUSEWHEEL)
    , d_properties(properties)
#endif
    , d_cursor_loader(ui::CursorLoader::Create())
    , d_current_platform_cursor(LoadCursor(NULL, IDC_ARROW))
    , d_inputRouterImpl(new content::InputRouterImpl(
        this, this, this, content::InputRouter::Config()))
    , d_renderViewRoutingId(0)
    , d_renderWidgetRoutingId(0)
    , d_mainFrameRoutingId(0)
    , d_gotRenderViewInfo(false)
    , d_pendingDestroy(false)
{
    initialize();
}

RenderWebView::RenderWebView(ProfileImpl              *profile,
                             int                       routingId,
                             const gfx::Rect&          initialRect)
    : d_proxy(nullptr)
    , d_delegate(nullptr)
    , d_profile(profile)
#if defined(BLPWTK2_FEATURE_FOCUS) || defined(BLPWTK2_FEATURE_REROUTEMOUSEWHEEL)
    , d_properties({ false, false, false, false, false, false })
#endif
    , d_cursor_loader(ui::CursorLoader::Create())
    , d_current_platform_cursor(LoadCursor(NULL, IDC_ARROW))
    , d_inputRouterImpl(new content::InputRouterImpl(
        this, this, this, content::InputRouter::Config()))
    , d_renderViewRoutingId(0)
    , d_renderWidgetRoutingId(0)
    , d_mainFrameRoutingId(0)
    , d_gotRenderViewInfo(false)
    , d_pendingDestroy(false)
{
    initialize();

    SetWindowLong(
        d_hwnd.get(), GWL_STYLE,
        GetWindowLong(d_hwnd.get(), GWL_STYLE) | WS_POPUP);

    //
    d_gotRenderViewInfo = true;

    d_renderWidgetRoutingId = routingId;

    RenderMessageDelegate::GetInstance()->AddRoute(
        d_renderWidgetRoutingId, this);

    content::mojom::WidgetInputHandlerHostPtr input_handler_host_ptr;
    auto widgetInputHandlerHostRequest = mojo::MakeRequest(&input_handler_host_ptr);

    content::RenderWidget::FromRoutingID(routingId)->
        SetupWidgetInputHandler(
            mojo::MakeRequest(&d_widgetInputHandler), std::move(input_handler_host_ptr));

    d_inputRouterImpl->BindHost(
        std::move(widgetInputHandlerHostRequest), true);

    d_compositor = RenderCompositorFactory::GetInstance()->CreateCompositor(
        d_renderWidgetRoutingId, d_hwnd.get(), d_profile);

    updateVisibility();
    updateSize();
    updateFocus();
#if defined(BLPWTK2_FEATURE_RUBBERBAND)
    updateAltDragRubberBanding();
#endif

    //
    d_shown = true;

    SetWindowPos(
        d_hwnd.get(),
        0,
        initialRect.x(),     initialRect.y(),
        initialRect.width(), initialRect.height(),
        SWP_SHOWWINDOW | SWP_FRAMECHANGED |
        SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

RenderWebView::~RenderWebView()
{
    LOG(INFO) << "Destroying RenderWebView, routingId=" << d_renderViewRoutingId;

    if (d_gotRenderViewInfo) {
        d_compositor.reset();

        if (d_mainFrameRoutingId != 0) {
            RenderMessageDelegate::GetInstance()->RemoveRoute(
                d_mainFrameRoutingId);
            d_mainFrameRoutingId = 0;
        }

        RenderMessageDelegate::GetInstance()->RemoveRoute(
            d_renderWidgetRoutingId);
        d_renderWidgetRoutingId = 0;

        if (d_renderViewRoutingId != 0) {
            RenderMessageDelegate::GetInstance()->RemoveRoute(
                d_renderViewRoutingId);
            d_renderViewRoutingId = 0;
        }

        d_gotRenderViewInfo = false;
    }
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
        if (d_compositor) {
            d_compositor->SetVisible(false);
        }

        #pragma clang diagnostic push
        #pragma clang diagnostic ignored "-Wunused"

        HWND leaked_hwnd = d_hwnd.release();

        #pragma clang diagnostic pop
    } return 0;
    case WM_WINDOWPOSCHANGING: {
        WINDOWPOS *windowpos = reinterpret_cast<WINDOWPOS *>(lParam);

        gfx::Size size(windowpos->cx, windowpos->cy);

        if (d_compositor &&
            ((size != d_size && !(windowpos->flags & SWP_NOSIZE)) ||
             windowpos->flags & SWP_FRAMECHANGED)) {
            d_compositor->Resize(gfx::Size());
        }
    } break;
    case WM_WINDOWPOSCHANGED: {
        WINDOWPOS *windowpos = reinterpret_cast<WINDOWPOS *>(lParam);

        if (windowpos->flags & (SWP_SHOWWINDOW | SWP_HIDEWINDOW)) {
            d_visible = (windowpos->flags & SWP_SHOWWINDOW)?
                true : false;

            updateVisibility();
        }

        gfx::Size size(windowpos->cx, windowpos->cy);

        if ((size != d_size && !(windowpos->flags & SWP_NOSIZE)) ||
            windowpos->flags & SWP_FRAMECHANGED) {
            d_size = size;

            updateSize();
        }
    } return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        BeginPaint(d_hwnd.get(), &ps);

        if (d_gotRenderViewInfo) {
            content::RenderWidget::FromRoutingID(d_renderWidgetRoutingId)->
                Redraw();
        }

        EndPaint(d_hwnd.get(), &ps);
    } return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_NCHITTEST: {
        if (d_nc_hit_test_enabled && d_delegate) {
            d_nc_hit_test_result = HTCLIENT;
            d_delegate->requestNCHitTest(this);
            return d_nc_hit_test_result;
        }
    } break;
    case WM_MOUSEMOVE:
    case WM_MOUSELEAVE:
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_MBUTTONDBLCLK:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_RBUTTONDBLCLK:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MOUSEWHEEL:
    case WM_MOUSEHWHEEL:
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_CHAR:
    case WM_SYSCHAR:
    case WM_IME_CHAR:
    case WM_IME_COMPOSITION:
    case WM_IME_ENDCOMPOSITION:
    case WM_IME_REQUEST:
    case WM_IME_NOTIFY:
    case WM_IME_SETCONTEXT:
    case WM_IME_STARTCOMPOSITION: {
        MSG msg;
        msg.hwnd    = d_hwnd.get();
        msg.message = uMsg;
        msg.wParam  = wParam;
        msg.lParam  = lParam;
        msg.time    = GetMessageTime();

        auto pt     = GetMessagePos();
        msg.pt.x    = GET_X_LPARAM(pt);
        msg.pt.y    = GET_Y_LPARAM(pt);

        switch (uMsg) {
        // Mouse:
        case WM_MOUSEMOVE:
        case WM_MOUSELEAVE:
        case WM_LBUTTONDBLCLK:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_MBUTTONDBLCLK:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP: {
            auto event =
                ui::MakeWebMouseEvent(
                    ui::MouseEvent(msg),
                    base::Bind(&GetScreenLocationFromEvent));

            // Mouse enter/leave:
            switch (uMsg) {
            case WM_MOUSEMOVE: {
                if (!d_mouse_entered) {
                    TRACKMOUSEEVENT track_mouse_event = {
                        sizeof(TRACKMOUSEEVENT),
                        TME_LEAVE,
                        d_hwnd.get(),
                        0
                    };

                    if (TrackMouseEvent(&track_mouse_event)) {
                        d_mouse_entered = true;

                        d_mouse_screen_position.SetPoint(
                            event.PositionInScreen().x,
                            event.PositionInScreen().y);
                    }
                }
            } break;
            case WM_MOUSELEAVE: {
                d_mouse_entered = false;

                d_mouse_screen_position.SetPoint(
                    event.PositionInScreen().x,
                    event.PositionInScreen().y);
            } break;
            case WM_LBUTTONDOWN:
            case WM_MBUTTONDOWN:
            case WM_RBUTTONDOWN: {
                d_mouse_pressed = true;

                d_tooltip_text_at_mouse_press = d_last_tooltip_text;
                d_tooltip->Hide();

#if defined(BLPWTK2_FEATURE_FOCUS)
                // Focus on mouse button down:
                if (d_properties.takeKeyboardFocusOnMouseDown) {
                    SetFocus(d_hwnd.get());
                }
#endif

                // Capture on mouse button down:
                SetCapture(d_hwnd.get());
            } break;
            // Capture on mouse button up:
            case WM_LBUTTONUP:
            case WM_MBUTTONUP:
            case WM_RBUTTONUP: {
                d_mouse_pressed = false;

                ReleaseCapture();
            } break;
            }

            event.movement_x = event.PositionInScreen().x - d_mouse_screen_position.x();
            event.movement_y = event.PositionInScreen().y - d_mouse_screen_position.y();

            d_mouse_screen_position.SetPoint(
                event.PositionInScreen().x,
                event.PositionInScreen().y);

            if (d_mouse_locked) {
                event.SetPositionInWidget(
                    d_unlocked_mouse_webview_position.x(),
                    d_unlocked_mouse_webview_position.y());
                event.SetPositionInScreen(
                    d_unlocked_mouse_screen_position.x(),
                    d_unlocked_mouse_screen_position.y());
            }
            else {
                d_unlocked_mouse_webview_position.SetPoint(
                    event.PositionInWidget().x, event.PositionInWidget().y);
                d_unlocked_mouse_screen_position.SetPoint(
                    event.PositionInScreen().x, event.PositionInScreen().y);
            }

            d_inputRouterImpl->SendMouseEvent(
                content::MouseEventWithLatencyInfo(
                    event,
                    ui::LatencyInfo()));

            return 0;
        } break;
        // Mousewheel:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL: {
            if (d_tooltip->IsVisible()) {
                d_tooltip->Hide();
            }

#if defined(BLPWTK2_FEATURE_REROUTEMOUSEWHEEL)
            if (ui::RerouteMouseWheel(
                d_hwnd.get(),
                wParam, lParam,
                d_properties.rerouteMouseWheelToAnyRelatedWindow)) {
                return 0;
            }
#else
            if (ui::RerouteMouseWheel(
                d_hwnd.get(),
                wParam, lParam)) {
                return 0;
            }
#endif

            auto event =
                ui::MakeWebMouseWheelEvent(
                    ui::MouseWheelEvent(msg),
                    base::Bind(&GetScreenLocationFromEvent));

            gfx::Vector2dF location(event.PositionInWidget().x, event.PositionInWidget().y);

            event.has_synthetic_phase = true;

            const auto is_within_slop_region =
                (location - d_first_wheel_location).LengthSquared() <
                (kWheelLatchingSlopRegion*kWheelLatchingSlopRegion);
            const auto has_different_modifiers =
                event.GetModifiers() != d_initial_wheel_event.GetModifiers();
            const auto consistent_x_direction =
                (event.delta_x == 0 && d_initial_wheel_event.delta_x == 0) ||
                event.delta_x * d_initial_wheel_event.delta_x > 0;
            const auto consistent_y_direction =
                (event.delta_y == 0 && d_initial_wheel_event.delta_y == 0) ||
                event.delta_y * d_initial_wheel_event.delta_y > 0;

            if (is_within_slop_region ||
                has_different_modifiers ||
                (d_first_scroll_update_ack_state == FirstScrollUpdateAckState::kNotConsumed && (!consistent_x_direction || !consistent_y_direction))) {
                if (d_mouse_wheel_end_dispatch_timer.IsRunning()) {
                    d_mouse_wheel_end_dispatch_timer.FireNow();
                }
            }

            if (!d_mouse_wheel_end_dispatch_timer.IsRunning()) {
                event.phase = blink::WebMouseWheelEvent::kPhaseBegan;

                d_first_wheel_location = location;
                d_initial_wheel_event = event;
                d_first_scroll_update_ack_state = FirstScrollUpdateAckState::kNotArrived;

                d_mouse_wheel_end_dispatch_timer.Start(
                    FROM_HERE,
                    kDefaultMouseWheelLatchingTransaction,
                    base::BindOnce(
                        &RenderWebView::onQueueWheelEventWithPhaseEnded,
                        base::Unretained(this)));
            }
            else {
                event.phase =
                    (event.delta_x || event.delta_y)                ?
                        blink::WebMouseWheelEvent::kPhaseChanged    :
                        blink::WebMouseWheelEvent::kPhaseStationary;

                d_mouse_wheel_end_dispatch_timer.Reset();
            }

            d_last_mouse_wheel_event = event;

            d_inputRouterImpl->SendWheelEvent(
                content::MouseWheelEventWithLatencyInfo(
                    event,
                    ui::LatencyInfo()));

            return 0;
        } break;
        // Keyboard:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP: {
            if (d_tooltip_shown_timer.IsRunning()) {
                d_tooltip_shown_timer.Stop();
                hideTooltip();
            }

            ui::KeyEvent event(msg);

            #pragma clang diagnostic push
            #pragma clang diagnostic ignored "-Wunused"

            d_input_method->DispatchKeyEvent(&event);

            #pragma clang diagnostic pop

            if (event.handled()) {
                return 0;
            }
        } break;
        // Input method keyboard:
        case WM_CHAR:
        case WM_SYSCHAR:
        case WM_IME_CHAR:
        case WM_IME_COMPOSITION:
        case WM_IME_ENDCOMPOSITION:
        case WM_IME_REQUEST:
        case WM_IME_NOTIFY:
        case WM_IME_SETCONTEXT:
        case WM_IME_STARTCOMPOSITION: {
            LRESULT result = 0;
            auto handled = d_input_method->OnUntranslatedIMEMessage(msg, &result);

            if (handled) {
                return result;
            }
        } break;
        }
    } break;
    case WM_MOUSEACTIVATE: {
        if (GetWindowLong(d_hwnd.get(), GWL_EXSTYLE) & WS_EX_NOACTIVATE) {
            return MA_NOACTIVATE;
        }
    } break;
    case WM_SETCURSOR: {
        wchar_t *cursor = IDC_ARROW;

        switch (LOWORD(lParam)) {
        case HTSIZE:
            cursor = IDC_SIZENWSE;
            break;
        case HTLEFT:
        case HTRIGHT:
            cursor = IDC_SIZEWE;
            break;
        case HTTOP:
        case HTBOTTOM:
            cursor = IDC_SIZENS;
            break;
        case HTTOPLEFT:
        case HTBOTTOMRIGHT:
        case HTOBJECT:
            cursor = IDC_SIZENWSE;
            break;
        case HTTOPRIGHT:
        case HTBOTTOMLEFT:
            cursor = IDC_SIZENESW;
            break;
        case HTCLIENT:
            d_is_cursor_overridden = false;
            setPlatformCursor(d_current_platform_cursor);
            return 1;
        case LOWORD(HTERROR):
            return 0;
        default:
            break;
        }

        d_is_cursor_overridden = true;
        SetCursor(
            LoadCursor(NULL, cursor));
    } return 1;
    case WM_SETFOCUS: {
        d_input_method->SetFocusedTextInputClient(this);
        d_input_method->OnFocus();

        if (d_delegate) {
            d_delegate->focused(this);
        }

        d_focused = true;

        updateFocus();
    } return 0;
    case WM_KILLFOCUS: {
        d_input_method->SetFocusedTextInputClient(nullptr);
        d_input_method->OnBlur();

        if (d_delegate) {
            d_delegate->blurred(this);
        }

        d_focused = false;

        updateFocus();
    } return 0;
    case WM_NOTIFY: {
        LPARAM l_result = 0;
        if (d_tooltip && static_cast<views::corewm::TooltipWin *>(d_tooltip.get())->
                HandleNotify(
                    wParam, reinterpret_cast<NMHDR*>(lParam), &l_result)) {
            return 1;
        }
    } return 0;
    default:
        break;
    }

    return DefWindowProc(d_hwnd.get(), uMsg, wParam, lParam);
}

void RenderWebView::initialize()
{
    d_hwnd.reset(CreateWindowEx(
#if defined(BLPWTK2_FEATURE_FOCUS)
        d_properties.activateWindowOnMouseDown?
            0 : WS_EX_NOACTIVATE,
#else
        0,
#endif
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

    RECT rect;
    GetWindowRect(d_hwnd.get(), &rect);
    d_size = gfx::Rect(rect).size();

    d_input_method = ui::CreateInputMethod(this, d_hwnd.get());

    d_dragDrop = new DragDrop(d_hwnd.get(), this);

    d_windows_session_change_observer =
        std::make_unique<views::WindowsSessionChangeObserver>(
            base::Bind(&RenderWebView::OnSessionChange, base::Unretained(this)));

    d_tooltip = std::make_unique<views::corewm::TooltipWin>(d_hwnd.get());
}

void RenderWebView::finishNotifyRoutingId(int id)
{
    if (d_gotRenderViewInfo) {
        return;
    }

    if (d_pendingDestroy) {
        LOG(INFO) << "WebView destroyed before we got a reference to a RenderView";
        return;
    }

    content::RenderViewImpl *rv =
        content::RenderViewImpl::FromRoutingID(id);

    if (!rv) {
        // The RenderView has not been created yet.  Keep reposting this task
        // until the RenderView is available.
        base::MessageLoop::current()->task_runner()->PostTask(
            FROM_HERE,
            base::Bind(&RenderWebView::finishNotifyRoutingId,
                       base::Unretained(this),
                       id));
        return;
    }

    d_gotRenderViewInfo = true;

    d_renderViewRoutingId = id;
    LOG(INFO) << "routingId=" << id;

    d_renderWidgetRoutingId = rv->GetWidget()->routing_id();

    d_mainFrameRoutingId = rv->GetMainRenderFrame()->GetRoutingID();

    RenderMessageDelegate::GetInstance()->AddRoute(
        d_renderViewRoutingId, this);
    RenderMessageDelegate::GetInstance()->AddRoute(
        d_renderWidgetRoutingId, this);
    RenderMessageDelegate::GetInstance()->AddRoute(
        d_mainFrameRoutingId, this);

    content::mojom::WidgetInputHandlerHostPtr input_handler_host_ptr;
    auto widgetInputHandlerHostRequest = mojo::MakeRequest(&input_handler_host_ptr);

    rv->GetWidget()->SetupWidgetInputHandler(
        mojo::MakeRequest(&d_widgetInputHandler), std::move(input_handler_host_ptr));

    d_inputRouterImpl->BindHost(
        std::move(widgetInputHandlerHostRequest), true);

    d_compositor = RenderCompositorFactory::GetInstance()->CreateCompositor(
        d_renderWidgetRoutingId, d_hwnd.get(), d_profile);

    updateVisibility();
    updateSize();
    updateFocus();
#if defined(BLPWTK2_FEATURE_RUBBERBAND)
    updateAltDragRubberBanding();
#endif
}

bool RenderWebView::dispatchToRenderViewImpl(const IPC::Message& message)
{
    content::RenderView *rv =
        content::RenderView::FromRoutingID(d_renderViewRoutingId);

    if (rv) {
        blink::WebFrame *webFrame = rv->GetWebView()->MainFrame();

        v8::Isolate* isolate = webFrame->ScriptIsolate();
        v8::Isolate::Scope isolateScope(isolate);

        v8::HandleScope handleScope(isolate);

        v8::Context::Scope contextScope(
            webFrame->ToWebLocalFrame()->MainWorldScriptContext());

        return static_cast<IPC::Listener *>(
            content::RenderThreadImpl::current())
                ->OnMessageReceived(message);
    }
    else {
        return static_cast<IPC::Listener *>(
            content::RenderThreadImpl::current())
                ->OnMessageReceived(message);
    }
}

void RenderWebView::ForceRedrawWindow(int attempts)
{
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

void RenderWebView::OnSessionChange(WPARAM status_code)
{
    // Direct3D presents are ignored while the screen is locked, so force the
    // window to be redrawn on unlock.
    if (status_code == WTS_SESSION_UNLOCK)
        ForceRedrawWindow(10);
}

void RenderWebView::updateVisibility()
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    d_compositor->SetVisible(d_visible);

    if (d_visible) {
        dispatchToRenderViewImpl(
            ViewMsg_WasShown(d_renderWidgetRoutingId,
                true, base::TimeTicks::Now()));
    }
    else {
        dispatchToRenderViewImpl(
            ViewMsg_WasHidden(d_renderWidgetRoutingId));

        d_tooltip->Hide();
    }
}

void RenderWebView::updateFocus()
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    if (d_focused) {
        d_widgetInputHandler->SetFocus(d_focused);

        dispatchToRenderViewImpl(
            ViewMsg_SetActive(d_renderWidgetRoutingId,
                d_focused));
    }
    else {
        dispatchToRenderViewImpl(
            ViewMsg_SetActive(d_renderWidgetRoutingId,
                d_focused));

        d_widgetInputHandler->SetFocus(d_focused);
    }
}

void RenderWebView::setPlatformCursor(HCURSOR cursor)
{
    if (d_is_cursor_overridden) {
        d_current_platform_cursor = cursor;
        return;
    }

    if (cursor) {
        d_previous_platform_cursor = SetCursor(cursor);
        d_current_platform_cursor = cursor;
    }
    else if (d_previous_platform_cursor) {
        SetCursor(d_previous_platform_cursor);
        d_previous_platform_cursor = NULL;
    }
}

void RenderWebView::updateSize()
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    d_compositor->Resize(d_size);

    content::VisualProperties params = {};
    params.new_size = d_size;
    params.compositor_viewport_pixel_size = d_size;
    params.visible_viewport_size = d_size;
    params.display_mode = blink::kWebDisplayModeBrowser;
    params.local_surface_id = d_compositor->GetLocalSurfaceId();
    GetNativeViewScreenInfo(&params.screen_info, d_hwnd.get());

    dispatchToRenderViewImpl(
        ViewMsg_SynchronizeVisualProperties(d_renderWidgetRoutingId,
            params));
}

void RenderWebView::sendScreenRects()
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    RECT view_screen_rect;
    GetWindowRect(d_hwnd.get(), &view_screen_rect);

    dispatchToRenderViewImpl(
        ViewMsg_UpdateScreenRects(d_renderWidgetRoutingId,
            gfx::Rect(view_screen_rect),
            gfx::Rect(view_screen_rect)));
}

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
void RenderWebView::updateAltDragRubberBanding()
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    dispatchToRenderViewImpl(
        ViewMsg_EnableAltDragRubberbanding(d_renderViewRoutingId,
            d_enableAltDragRubberBanding));
}
#endif

void RenderWebView::onQueueWheelEventWithPhaseEnded()
{
    d_last_mouse_wheel_event.SetTimeStamp(ui::EventTimeForNow());
    d_last_mouse_wheel_event.delta_x = 0;
    d_last_mouse_wheel_event.delta_y = 0;
    d_last_mouse_wheel_event.wheel_ticks_x = 0;
    d_last_mouse_wheel_event.wheel_ticks_y = 0;
    d_last_mouse_wheel_event.dispatch_type = blink::WebInputEvent::DispatchType::kEventNonBlocking;

    d_last_mouse_wheel_event.phase = blink::WebMouseWheelEvent::kPhaseEnded;
    d_inputRouterImpl->SendWheelEvent(
        content::MouseWheelEventWithLatencyInfo(
            d_last_mouse_wheel_event,
            ui::LatencyInfo()));
}

void RenderWebView::showTooltip()
{
    POINT location;
    GetCursorPos(&location);

    d_tooltip->SetText(nullptr, d_last_tooltip_text, gfx::Point(location));
    d_tooltip->Show();

    if (kDefaultTooltipShownTimeoutMs > 0) {
        d_tooltip_shown_timer.Start(
            FROM_HERE,
            base::TimeDelta::FromMilliseconds(kDefaultTooltipShownTimeoutMs),
            this,
            &RenderWebView::hideTooltip);
    }
}

void RenderWebView::hideTooltip()
{
    d_tooltip->Hide();
}

void RenderWebView::updateTooltip()
{
    if (d_mouse_pressed) {
        if (d_tooltip_text_at_mouse_press == d_last_tooltip_text) {
            d_tooltip->Hide();
            return;
        }
    }

    if (d_tooltip_text != d_last_tooltip_text || !d_tooltip->IsVisible()) {
        d_tooltip_shown_timer.Stop();
        d_last_tooltip_text = d_tooltip_text;

        if (d_last_tooltip_text.empty()) {
            d_tooltip->Hide();
            d_tooltip_defer_timer.Stop();
        }
        else {
            if (d_tooltip_defer_timer.IsRunning()) {
                d_tooltip_defer_timer.Reset();
            }
            else {
                d_tooltip_defer_timer.Start(
                    FROM_HERE,
                    base::TimeDelta::FromMilliseconds(kDelayForTooltipUpdateInMs),
                    this,
                    &RenderWebView::showTooltip);
            }
        }
    }
}

void RenderWebView::destroy()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_pendingDestroy);

    if (d_proxy) {
        d_proxy->destroy();
    }

    d_pendingDestroy = true;
    d_delegate = nullptr;

    base::MessageLoop::current()->task_runner()->DeleteSoon(FROM_HERE, this);
}

WebFrame *RenderWebView::mainFrame()
{
    return d_proxy->mainFrame();
}

int RenderWebView::loadUrl(const StringRef& url)
{
    return d_proxy->loadUrl(url);
}

#if defined(BLPWTK2_FEATURE_DWM)
void RenderWebView::rootWindowCompositionChanged()
{
    d_proxy->rootWindowCompositionChanged();
}
#endif

void RenderWebView::loadInspector(unsigned int pid, int routingId)
{
    d_proxy->loadInspector(pid, routingId);
}

void RenderWebView::inspectElementAt(const POINT& point)
{
    d_proxy->inspectElementAt(point);
}

#if defined(BLPWTK2_FEATURE_SCREENPRINT)
void RenderWebView::drawContentsToBlob(Blob *blob, const DrawParams& params)
{
    d_proxy->drawContentsToBlob(blob, params);
}
#endif

int RenderWebView::goBack()
{
    return d_proxy->goBack();
}

int RenderWebView::goForward()
{
    return d_proxy->goForward();
}

int RenderWebView::reload()
{
    return d_proxy->reload();
}

void RenderWebView::stop()
{
    d_proxy->stop();
}

#if defined(BLPWTK2_FEATURE_FOCUS)
void RenderWebView::takeKeyboardFocus()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_hwnd.is_valid());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", takeKeyboardFocus";

    SetFocus(d_hwnd.get());
}

void RenderWebView::setLogicalFocus(bool focused)
{
    if (d_gotRenderViewInfo) {
        // If we have the renderer in-process, then set the logical focus
        // immediately so that handleInputEvents will work as expected.
        content::RenderViewImpl *rv =
            content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);
        DCHECK(rv);
        rv->SetFocus(focused);
    }
}
#endif

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
    d_proxy->cutSelection();
}

void RenderWebView::copySelection()
{
    d_proxy->copySelection();
}

void RenderWebView::paste()
{
    d_proxy->paste();
}

void RenderWebView::deleteSelection()
{
    d_proxy->deleteSelection();
}

void RenderWebView::enableNCHitTest(bool enabled)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_nc_hit_test_enabled = enabled;
}

void RenderWebView::onNCHitTestResult(int x, int y, int result)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_nc_hit_test_result = result;
}

void RenderWebView::performCustomContextMenuAction(int actionId)
{
    d_proxy->performCustomContextMenuAction(actionId);
}

void RenderWebView::find(const StringRef& text, bool matchCase, bool forward)
{
    d_proxy->find(text, matchCase, forward);
}

void RenderWebView::stopFind(bool preserveSelection)
{
    d_proxy->stopFind(preserveSelection);
}

void RenderWebView::replaceMisspelledRange(const StringRef& text)
{
    d_proxy->replaceMisspelledRange(text);
}

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
void RenderWebView::enableAltDragRubberbanding(bool enabled)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_enableAltDragRubberBanding = enabled;
    updateAltDragRubberBanding();
}

bool RenderWebView::forceStartRubberbanding(int x, int y)
{
    return d_proxy->forceStartRubberbanding(x, y);
}

bool RenderWebView::isRubberbanding() const
{
    return d_proxy->isRubberbanding();
}

void RenderWebView::abortRubberbanding()
{
    d_proxy->abortRubberbanding();
}

String RenderWebView::getTextInRubberband(const NativeRect& rect)
{
    return d_proxy->getTextInRubberband(rect);
}
#endif

void RenderWebView::rootWindowPositionChanged()
{
    sendScreenRects();
}

void RenderWebView::rootWindowSettingsChanged()
{
    sendScreenRects();
}

void RenderWebView::handleInputEvents(const InputEvent *events, size_t eventsCount)
{
    d_proxy->handleInputEvents(events, eventsCount);
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
    d_proxy->setBackgroundColor(color);
}

void RenderWebView::setRegion(NativeRegion region)
{
    DCHECK(Statics::isInApplicationMainThread());

    SetWindowRgn(d_hwnd.get(),
                 region,
                 IsWindowVisible(d_hwnd.get()));
}

void RenderWebView::activateKeyboardLayout(unsigned int hkl)
{
    d_proxy->activateKeyboardLayout(hkl);
}

void RenderWebView::clearTooltip()
{
    d_proxy->clearTooltip();
}

v8::MaybeLocal<v8::Value> RenderWebView::callFunction(
        v8::Local<v8::Function>  func,
        v8::Local<v8::Value>     recv,
        int                      argc,
        v8::Local<v8::Value>    *argv)
{
    return d_proxy->callFunction(func, recv, argc, argv);
}

#if defined(BLPWTK2_FEATURE_PRINTPDF)
String RenderWebView::printToPDF(const StringRef& propertyName)
{
    return d_proxy->printToPDF(propertyName);
}
#endif

#if defined(BLPWTK2_FEATURE_FASTRESIZE)
void RenderWebView::disableResizeOptimization()
{
    d_proxy->disableResizeOptimization();
}
#endif

void RenderWebView::setSecurityToken(v8::Isolate *isolate,
                                     v8::Local<v8::Value> token)
{
    d_proxy->setSecurityToken(isolate, token);
}

// blpwtk2::WebViewClientDelegate overrides
void RenderWebView::setClient(WebViewClient *client)
{
    static_cast<WebViewProxy *>(d_proxy)->setClient(client);
}

void RenderWebView::ncHitTest()
{
    NOTREACHED();
}

void RenderWebView::ncDragBegin(int hitTestCode, const gfx::Point& point)
{
    NOTREACHED();
}

void RenderWebView::ncDragMove(const gfx::Point& point)
{
    NOTREACHED();
}

void RenderWebView::ncDragEnd(const gfx::Point& point)
{
    NOTREACHED();
}

void RenderWebView::ncDoubleClick(const gfx::Point& point)
{
    NOTREACHED();
}

void RenderWebView::focused()
{
    NOTREACHED();
}

void RenderWebView::blurred()
{
    NOTREACHED();
}

void RenderWebView::showContextMenu(const ContextMenuParams& params)
{
    static_cast<WebViewProxy *>(d_proxy)->showContextMenu(params);
}

void RenderWebView::findReply(int  numberOfMatches,
                              int  activeMatchOrdinal,
                              bool finalUpdate)
{
    static_cast<WebViewProxy *>(d_proxy)->findReply(numberOfMatches, activeMatchOrdinal, finalUpdate);
}

void RenderWebView::preResize(const gfx::Size& size)
{
    static_cast<WebViewProxy *>(d_proxy)->preResize(size);
}

void RenderWebView::notifyRoutingId(int id)
{
    static_cast<WebViewProxy *>(d_proxy)->notifyRoutingId(id);

    finishNotifyRoutingId(id);
}

void RenderWebView::onLoadStatus(int status)
{
    static_cast<WebViewProxy *>(d_proxy)->onLoadStatus(status);
}

#if defined(BLPWTK2_FEATURE_DEVTOOLSINTEGRATION)
void RenderWebView::devToolsAgentHostAttached()
{
    static_cast<WebViewProxy *>(d_proxy)->devToolsAgentHostAttached();
}

void RenderWebView::devToolsAgentHostDetached()
{
    static_cast<WebViewProxy *>(d_proxy)->devToolsAgentHostDetached();
}
#endif

void RenderWebView::didFinishLoadForFrame(int              routingId,
                                          const StringRef& url)
{
    static_cast<WebViewProxy *>(d_proxy)->didFinishLoadForFrame(routingId, url);
}

void RenderWebView::didFailLoadForFrame(int              routingId,
                                        const StringRef& url)
{
    static_cast<WebViewProxy *>(d_proxy)->didFailLoadForFrame(routingId, url);
}

// WebViewDelegate overrides
void RenderWebView::created(WebView *source)
{
    DCHECK(source == d_proxy);
}

void RenderWebView::didFinishLoad(WebView *source, const StringRef& url)
{
    if (d_delegate) {
        d_delegate->didFinishLoad(this, url);
    }
}

void RenderWebView::didFailLoad(WebView *source, const StringRef& url)
{
    if (d_delegate) {
        d_delegate->didFinishLoad(this, url);
    }
}

void RenderWebView::focused(WebView *source)
{
    NOTREACHED();
}

void RenderWebView::blurred(WebView *source)
{
    NOTREACHED();
}

void RenderWebView::showContextMenu(
        WebView *source, const ContextMenuParams& params)
{
    if (d_delegate) {
        d_delegate->showContextMenu(this, params);
    }
}

void RenderWebView::requestNCHitTest(WebView *source)
{
    NOTREACHED();
}

void RenderWebView::ncDragBegin(WebView      *source,
                                  int           hitTestCode,
                                  const POINT&  startPoint)
{
    NOTREACHED();
}

void RenderWebView::ncDragMove(WebView *source, const POINT& movePoint)
{
    NOTREACHED();
}

void RenderWebView::ncDragEnd(WebView *source, const POINT& endPoint)
{
    NOTREACHED();
}

void RenderWebView::ncDoubleClick(WebView *source, const POINT& point)
{
    NOTREACHED();
}

void RenderWebView::findState(WebView *source,
                                int      numberOfMatches,
                                int      activeMatchOrdinal,
                                bool     finalUpdate)
{
    // The RenderWebView is only used when the embedder lives in another
    // process.  Instead of filtering out all but the latest response in
    // this process, we ship all the responses to the process running the
    // WebViewClientImpl (by using findStateWithReqId) and let it filter out
    // all but the latest response.
    NOTREACHED() << "findState should come in via findStateWithReqId";
}

void RenderWebView::devToolsAgentHostAttached(WebView *source)
{
    if (d_delegate) {
        d_delegate->devToolsAgentHostAttached(this);
    }
}

void RenderWebView::devToolsAgentHostDetached(WebView *source)
{
    if (d_delegate) {
        d_delegate->devToolsAgentHostDetached(this);
    }
}

// IPC::Listener overrides
bool RenderWebView::OnMessageReceived(const IPC::Message& message)
{
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(RenderWebView, message)
        IPC_MESSAGE_HANDLER(DragHostMsg_StartDragging,
            OnStartDragging)
        IPC_MESSAGE_HANDLER(DragHostMsg_UpdateDragCursor,
            OnUpdateDragCursor)
        IPC_MESSAGE_HANDLER(FrameHostMsg_Detach,
            OnDetach)
        IPC_MESSAGE_HANDLER(FrameHostMsg_SelectionChanged,
            OnSelectionChanged)
        IPC_MESSAGE_HANDLER(ViewHostMsg_Close,
            OnClose)
        IPC_MESSAGE_HANDLER(ViewHostMsg_HasTouchEventHandlers,
            OnHasTouchEventHandlers)
        IPC_MESSAGE_HANDLER(ViewHostMsg_LockMouse,
            OnLockMouse)
        IPC_MESSAGE_HANDLER(ViewHostMsg_SetCursor,
            OnSetCursor)
        IPC_MESSAGE_HANDLER(ViewHostMsg_SetTooltipText,
            OnSetTooltipText)
        IPC_MESSAGE_HANDLER(ViewHostMsg_SelectionBoundsChanged,
            OnSelectionBoundsChanged)
        IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget,
            OnShowWidget)
        IPC_MESSAGE_HANDLER(ViewHostMsg_UnlockMouse,
            OnUnlockMouse)
        IPC_MESSAGE_HANDLER(ViewHostMsg_TextInputStateChanged,
            OnTextInputStateChanged)
#if defined(BLPWTK2_FEATURE_RUBBERBAND)
        IPC_MESSAGE_HANDLER(ViewHostMsg_HideRubberbandRect,
            OnHideRubberbandRect)
        IPC_MESSAGE_HANDLER(ViewHostMsg_SetRubberbandRect,
            OnSetRubberbandRect)
#endif
        IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()

    return handled;
}

// ui::internal::InputMethodDelegate overrides:
ui::EventDispatchDetails RenderWebView::DispatchKeyEventPostIME(
    ui::KeyEvent* key_event)
{
    if (!key_event->handled()) {
        d_inputRouterImpl->SendKeyboardEvent(
            content::NativeWebKeyboardEventWithLatencyInfo(
                content::NativeWebKeyboardEvent(*key_event),
                ui::LatencyInfo()));
    }

    return ui::EventDispatchDetails();
}

// ui::TextInputClient overrides:
void RenderWebView::SetCompositionText(const ui::CompositionText& composition)
{
    d_widgetInputHandler->
        ImeSetComposition(
            composition.text,
            composition.ime_text_spans,
            gfx::Range::InvalidRange(),
            composition.selection.end(), composition.selection.end());

    d_has_composition_text = !composition.text.empty();
}

void RenderWebView::ConfirmCompositionText()
{
    if (d_has_composition_text) {
        d_widgetInputHandler->
            ImeFinishComposingText(false);
    }

    d_has_composition_text = false;
}

void RenderWebView::ClearCompositionText()
{
    if (d_has_composition_text) {
        d_widgetInputHandler->
            ImeSetComposition(
                base::string16(),
                {},
                gfx::Range::InvalidRange(),
                0, 0);
    }

    d_has_composition_text = false;
}

void RenderWebView::InsertText(const base::string16& text)
{
    if (!text.empty()) {
        d_widgetInputHandler->
            ImeCommitText(
                text,
                {},
                gfx::Range::InvalidRange(),
                0,
                {});
    }
    else {
        d_widgetInputHandler->
            ImeFinishComposingText(
                false);
    }

    d_has_composition_text = false;
}

void RenderWebView::InsertChar(const ui::KeyEvent& event)
{
    d_inputRouterImpl->SendKeyboardEvent(
        content::NativeWebKeyboardEventWithLatencyInfo(
            content::NativeWebKeyboardEvent(event),
            ui::LatencyInfo()));
}

ui::TextInputType RenderWebView::GetTextInputType() const
{
    return d_text_input_state.type;
}

ui::TextInputMode RenderWebView::GetTextInputMode() const
{
    return d_text_input_state.mode;
}

base::i18n::TextDirection RenderWebView::GetTextDirection() const
{
    NOTIMPLEMENTED();
    return base::i18n::UNKNOWN_DIRECTION;
}

int RenderWebView::GetTextInputFlags() const
{
    return d_text_input_state.flags;
}

bool RenderWebView::CanComposeInline() const
{
    return d_text_input_state.can_compose_inline;
}

gfx::Rect RenderWebView::GetCaretBounds() const
{
    auto bounds = gfx::RectBetweenSelectionBounds(
        d_selection_anchor, d_selection_focus).ToRECT();

    MapWindowPoints(
        d_hwnd.get(),
        NULL,
        (LPPOINT)&bounds,
        2);

    return gfx::Rect(bounds);
}

bool RenderWebView::GetCompositionCharacterBounds(
    uint32_t index,
    gfx::Rect* rect) const
{
    if (index >= d_composition_character_bounds.size()) {
        return false;
    }

    auto bounds = d_composition_character_bounds[index].ToRECT();

    MapWindowPoints(
        d_hwnd.get(),
        NULL,
        (LPPOINT)&bounds,
        2);

    *rect = gfx::Rect(bounds);

    return true;
}

bool RenderWebView::HasCompositionText() const
{
    return d_has_composition_text;
}

ui::TextInputClient::FocusReason RenderWebView::GetFocusReason() const
{
    return ui::TextInputClient::FOCUS_REASON_NONE;
}

bool RenderWebView::GetTextRange(gfx::Range* range) const
{
    range->set_start(d_selection_text_offset);
    range->set_end(d_selection_text_offset + d_selection_text.length());
    return true;
}

bool RenderWebView::GetCompositionTextRange(gfx::Range* range) const
{
    NOTIMPLEMENTED();
    return false;
}

bool RenderWebView::GetSelectionRange(gfx::Range* range) const
{
    range->set_start(d_selection_range.start());
    range->set_end(d_selection_range.end());
    return true;
}

bool RenderWebView::SetSelectionRange(const gfx::Range& range)
{
    NOTIMPLEMENTED();
    return false;
}

bool RenderWebView::DeleteRange(const gfx::Range& range)
{
    NOTIMPLEMENTED();
    return false;
}

bool RenderWebView::GetTextFromRange(
    const gfx::Range& range,
    base::string16* text) const
{
    gfx::Range selection_text_range(
        d_selection_text_offset,
        d_selection_text_offset + d_selection_text.length());

    if (!selection_text_range.Contains(range)) {
        text->clear();
        return false;
    }

    if (selection_text_range.EqualsIgnoringDirection(range)) {
        *text = d_selection_text;
    }
    else {
        *text = d_selection_text.substr(
            range.GetMin() - d_selection_text_offset,
            range.length());
    }

    return true;
}

void RenderWebView::OnInputMethodChanged()
{
}

bool RenderWebView::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction)
{
    dispatchToRenderViewImpl(
        ViewMsg_SetTextDirection(d_renderWidgetRoutingId,
            direction == base::i18n::RIGHT_TO_LEFT?
                blink::kWebTextDirectionRightToLeft :
                blink::kWebTextDirectionLeftToRight));

    return true;
}

void RenderWebView::ExtendSelectionAndDelete(size_t before, size_t after)
{
    //TODO
}

void RenderWebView::EnsureCaretNotInRect(const gfx::Rect& rect)
{
    //TODO
}

bool RenderWebView::IsTextEditCommandEnabled(ui::TextEditCommand command) const
{
    return false;
}

void RenderWebView::SetTextEditCommandForNextKeyEvent(ui::TextEditCommand command)
{
}

ukm::SourceId RenderWebView::GetClientSourceForMetrics() const
{
    return ukm::SourceId();
}

bool RenderWebView::ShouldDoLearning()
{
    return false;
}

// DragDropDelegate overrides:
void RenderWebView::DragTargetEnter(
    const std::vector<content::DropData::Metadata>& drag_data_metadata,
    const gfx::PointF& client_pt,
    const gfx::PointF& screen_pt,
    blink::WebDragOperationsMask ops_allowed,
    int key_modifiers)
{
    dispatchToRenderViewImpl(
        DragMsg_TargetDragEnter(d_renderViewRoutingId,
            drag_data_metadata,
            client_pt, screen_pt,
            ops_allowed,
            key_modifiers));
}

void RenderWebView::DragTargetOver(
    const gfx::PointF& client_pt,
    const gfx::PointF& screen_pt,
    blink::WebDragOperationsMask ops_allowed,
    int key_modifiers)
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    dispatchToRenderViewImpl(
        DragMsg_TargetDragOver(d_renderWidgetRoutingId,
            client_pt, screen_pt,
            ops_allowed,
            key_modifiers));
}

void RenderWebView::DragTargetLeave()
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    dispatchToRenderViewImpl(
        DragMsg_TargetDragLeave(d_renderWidgetRoutingId,
            gfx::PointF(), gfx::PointF()));
}

void RenderWebView::DragTargetDrop(
    const content::DropData& drop_data,
    const gfx::PointF& client_pt,
    const gfx::PointF& screen_pt,
    int key_modifiers)
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    dispatchToRenderViewImpl(
        DragMsg_TargetDrop(d_renderWidgetRoutingId,
            drop_data,
            client_pt, screen_pt,
            key_modifiers));
}

void RenderWebView::DragSourceEnded(
    const gfx::PointF& client_pt,
    const gfx::PointF& screen_pt,
    blink::WebDragOperation drag_operation)
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    dispatchToRenderViewImpl(
        DragMsg_SourceEnded(d_renderWidgetRoutingId,
            client_pt, screen_pt,
            drag_operation));
}

void RenderWebView::DragSourceSystemEnded()
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    dispatchToRenderViewImpl(
        DragMsg_SourceSystemDragEnded(d_renderWidgetRoutingId));
}

// content::InputRouterClient overrides:
content::InputEventAckState RenderWebView::FilterInputEvent(
    const blink::WebInputEvent& input_event,
    const ui::LatencyInfo& latency_info)
{
    return content::INPUT_EVENT_ACK_STATE_NOT_CONSUMED;
}

void RenderWebView::OnHasTouchEventHandlers(bool has_handlers)
{
}

void RenderWebView::ForwardGestureEventWithLatencyInfo(
    const blink::WebGestureEvent& gesture_event,
    const ui::LatencyInfo& latency_info)
{
    d_inputRouterImpl->SendGestureEvent(
        content::GestureEventWithLatencyInfo(
            gesture_event,
            latency_info));
}

bool RenderWebView::IsWheelScrollInProgress()
{
    return false;
}

// content::InputRouterImplClient overrides:
content::mojom::WidgetInputHandler* RenderWebView::GetWidgetInputHandler()
{
    return d_widgetInputHandler.get();
}

void RenderWebView::OnImeCancelComposition()
{
    d_input_method->CancelComposition(this);
    d_has_composition_text = false;
}

void RenderWebView::OnImeCompositionRangeChanged(
        const gfx::Range& range,
        const std::vector<gfx::Rect>& character_bounds)
{
    d_composition_character_bounds = character_bounds;
}

// content::InputDispositionHandler overrides:
void RenderWebView::OnGestureEventAck(
    const content::GestureEventWithLatencyInfo& event,
    content::InputEventAckSource ack_source,
    content::InputEventAckState ack_result)
{
    if (event.event.GetType() == blink::WebInputEvent::kGestureScrollUpdate) {
        if (d_first_scroll_update_ack_state == FirstScrollUpdateAckState::kNotArrived) {
            d_first_scroll_update_ack_state =
                (ack_result == content::INPUT_EVENT_ACK_STATE_CONSUMED) ?
                FirstScrollUpdateAckState::kConsumed :
                FirstScrollUpdateAckState::kNotConsumed;
        }
    }
}

// content::FlingControllerSchedulerClient overrides:
bool RenderWebView::NeedsBeginFrameForFlingProgress()
{
    return false;
}

// Message handlers
void RenderWebView::OnClose()
{
    this->destroy();
}

void RenderWebView::OnLockMouse(
    bool user_gesture,
    bool privileged)
{
    if (GetCapture() != d_hwnd.get()) {
        SetCapture(d_hwnd.get());
        d_mouse_locked = true;
    }

    dispatchToRenderViewImpl(
        ViewMsg_LockMouse_ACK(d_renderViewRoutingId,
            GetCapture() == d_hwnd.get()));
}

void RenderWebView::OnSelectionBoundsChanged(
    const ViewHostMsg_SelectionBounds_Params& params)
{
    gfx::SelectionBound anchor_bound, focus_bound;
    anchor_bound.SetEdge(
        gfx::PointF(params.anchor_rect.origin()),
        gfx::PointF(params.anchor_rect.bottom_left()));
    focus_bound.SetEdge(
        gfx::PointF(params.focus_rect.origin()),
        gfx::PointF(params.focus_rect.bottom_left()));

    if (params.anchor_rect == params.focus_rect) {
        anchor_bound.set_type(gfx::SelectionBound::CENTER);
        focus_bound.set_type(gfx::SelectionBound::CENTER);
    } else {
        // Whether text is LTR at the anchor handle.
        bool anchor_LTR = params.anchor_dir == blink::kWebTextDirectionLeftToRight;
        // Whether text is LTR at the focus handle.
        bool focus_LTR = params.focus_dir == blink::kWebTextDirectionLeftToRight;

        if ((params.is_anchor_first && anchor_LTR) ||
            (!params.is_anchor_first && !anchor_LTR)) {
            anchor_bound.set_type(gfx::SelectionBound::LEFT);
        }
        else {
            anchor_bound.set_type(gfx::SelectionBound::RIGHT);
        }

        if ((params.is_anchor_first && focus_LTR) ||
            (!params.is_anchor_first && !focus_LTR)) {
            focus_bound.set_type(gfx::SelectionBound::RIGHT);
        }
        else {
            focus_bound.set_type(gfx::SelectionBound::LEFT);
        }
    }

    if (anchor_bound == d_selection_anchor && focus_bound == d_selection_focus)
        return;

    d_selection_anchor = anchor_bound;
    d_selection_focus = focus_bound;

    d_input_method->OnCaretBoundsChanged(this);
}

void RenderWebView::OnSelectionChanged(
    const base::string16& text,
    uint32_t offset,
    const gfx::Range& range)
{
    d_selection_text = text;
    d_selection_text_offset = offset;
    d_selection_range.set_start(range.start());
    d_selection_range.set_end(range.end());
}

void RenderWebView::OnSetCursor(const content::WebCursor& cursor)
{
    if (!d_current_cursor.IsEqual(cursor)) {
        d_current_cursor = cursor;

        if (!d_current_cursor.IsCustom()) {
            auto native_cursor = d_current_cursor.GetNativeCursor();
            d_cursor_loader->SetPlatformCursor(&native_cursor);

            setPlatformCursor(native_cursor.platform());
        }
        else {
            setPlatformCursor(d_current_cursor.GetPlatformCursor());
        }
    }
}

void RenderWebView::OnSetTooltipText(
    const base::string16& tooltip_text,
    blink::WebTextDirection text_direction_hint)
{
    d_tooltip_text = tooltip_text;
    updateTooltip();
}

void RenderWebView::OnShowWidget(
    int routing_id, const gfx::Rect initial_rect)
{
    new RenderWebView(d_profile, routing_id, initial_rect);
}

void RenderWebView::OnStartDragging(
    const content::DropData& drop_data,
    blink::WebDragOperationsMask operations_allowed,
    const SkBitmap& bitmap,
    const gfx::Vector2d& bitmap_offset_in_dip,
    const content::DragEventSourceInfo& event_info)
{
    d_dragDrop->StartDragging(
        drop_data, operations_allowed, bitmap, bitmap_offset_in_dip, event_info);
}

void RenderWebView::OnTextInputStateChanged(
    const content::TextInputState& text_input_state)
{
    auto changed =
        (d_text_input_state.type               != text_input_state.type)  ||
        (d_text_input_state.mode               != text_input_state.mode)  ||
        (d_text_input_state.flags              != text_input_state.flags) ||
        (d_text_input_state.can_compose_inline != text_input_state.can_compose_inline);

    d_text_input_state = text_input_state;

    if (changed) {
        d_input_method->OnTextInputTypeChanged(this);
    }

    if (d_text_input_state.show_ime_if_needed) {
        d_input_method->ShowVirtualKeyboardIfEnabled();
    }

    if (d_text_input_state.type != ui::TEXT_INPUT_TYPE_NONE) {
        d_widgetInputHandler->
            RequestCompositionUpdates(
                false, true);
    }
    else {
        d_widgetInputHandler->
            RequestCompositionUpdates(
                false, false);
    }
}

void RenderWebView::OnUnlockMouse()
{
    if (GetCapture() != d_hwnd.get()) {
        ReleaseCapture();
        d_mouse_locked = false;
    }
}

void RenderWebView::OnUpdateDragCursor(
    blink::WebDragOperation drag_operation)
{
    d_dragDrop->UpdateDragCursor(drag_operation);
}

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
void RenderWebView::OnHideRubberbandRect()
{
    d_rubberbandOutline.reset();
}

void RenderWebView::OnSetRubberbandRect(const gfx::Rect& rect)
{
    if (!d_rubberbandOutline.get()) {
        d_rubberbandOutline.reset(new ui::RubberbandOutline());
    }

    d_rubberbandOutline->SetRect(d_hwnd.get(), rect.ToRECT());
}
#endif

void RenderWebView::OnDetach()
{
}

}  // close namespace blpwtk2

// vim: ts=4 et


