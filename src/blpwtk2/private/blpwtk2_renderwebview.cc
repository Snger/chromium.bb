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

#include <base/win/scoped_gdi_object.h>
#include <content/common/frame_messages.h>
#include <content/common/drag_messages.h>
#include <content/common/input_messages.h>
#include <content/common/view_messages.h>
#include <content/renderer/render_thread_impl.h>
#include <content/renderer/render_view_impl.h>
#include <content/renderer/gpu/render_widget_compositor.h>
#include <content/public/renderer/render_view.h>
#include <third_party/WebKit/public/web/WebLocalFrame.h>
#include <third_party/WebKit/public/web/WebView.h>
#include <ui/base/cursor/cursor_loader.h>
#include <ui/base/win/lock_state.h>
#include <ui/display/display.h>
#include <ui/display/screen.h>
#include <ui/events/blink/web_input_event.h>
#include "ui/events/blink/web_input_event_traits.h"
#include <ui/base/ime/input_method.h>
#include <ui/base/ime/input_method_factory.h>
#include <ui/base/win/mouse_wheel_util.h>
#include <ui/events/event.h>
#include <ui/events/event_utils.h>
#include <ui/latency/latency_info.h>
#include <ui/gfx/icon_util.h>

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

gfx::Point GetScreenLocationFromEvent(const ui::LocatedEvent& event)
{
    return event.root_location();
}

content::ScreenOrientationValues GetOrientationTypeForDesktop(
    const display::Display& display) {
    static int primary_landscape_angle = -1;
    static int primary_portrait_angle = -1;

    int angle = display.RotationAsDegree();
    const gfx::Rect& bounds = display.bounds();
    bool is_portrait = bounds.height() >= bounds.width();

    if (is_portrait && primary_portrait_angle == -1)
        primary_portrait_angle = angle;

    if (!is_portrait && primary_landscape_angle == -1)
        primary_landscape_angle = angle;

    if (is_portrait) {
        return primary_portrait_angle == angle
            ? content::SCREEN_ORIENTATION_VALUES_PORTRAIT_PRIMARY
            : content::SCREEN_ORIENTATION_VALUES_PORTRAIT_SECONDARY;
    }

    return primary_landscape_angle == angle
        ? content::SCREEN_ORIENTATION_VALUES_LANDSCAPE_PRIMARY
        : content::SCREEN_ORIENTATION_VALUES_LANDSCAPE_SECONDARY;
}

void GetScreenInfoForWindow(content::ScreenInfo* results,
                            HWND hwnd) {
    auto monitor = MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);

    MONITORINFO monitor_info = { sizeof(MONITORINFO) };
    GetMonitorInfo(monitor, &monitor_info);

    display::Screen* screen = display::Screen::GetScreen();
    const display::Display display = screen->GetDisplayMatching(
        gfx::Rect(monitor_info.rcMonitor));
    results->rect = display.bounds();
    results->available_rect = display.work_area();
    results->depth = display.color_depth();
    results->depth_per_component = display.depth_per_component();
    results->is_monochrome = display.is_monochrome();
    results->device_scale_factor = display.device_scale_factor();
    results->color_space = display.color_space();
    results->color_space.GetICCProfile(&results->icc_profile);

    // The Display rotation and the ScreenInfo orientation are not the same
    // angle. The former is the physical display rotation while the later is the
    // rotation required by the content to be shown properly on the screen, in
    // other words, relative to the physical display.
    results->orientation_angle = display.RotationAsDegree();
    if (results->orientation_angle == 90)
        results->orientation_angle = 270;
    else if (results->orientation_angle == 270)
        results->orientation_angle = 90;

    results->orientation_type =
        GetOrientationTypeForDesktop(display);
}

content::RenderWidget *RenderWidgetFromRoutingID(int id)
{
    return static_cast<content::RenderWidget *>(
        content::RenderThreadImpl::current()->GetRouter()->GetRoute(id));
}

}

namespace blpwtk2 {

                        // -------------------
                        // class RenderWebView
                        // -------------------

RenderWebView::RenderWebView(WebViewDelegate          *delegate,
                             ProfileImpl              *profile,
                             const WebViewProperties&  properties)
    : d_client(nullptr)
    , d_delegate(delegate)
    , d_profile(profile)
    , d_renderViewRoutingId(0)
    , d_mainFrameRoutingId(0)
    , d_gotRenderViewInfo(false)
    , d_pendingLoadStatus(false)
    , d_isMainFrameAccessible(false)
    , d_pendingDestroy(false)
    , d_properties(properties)
    , d_cursor_loader(ui::CursorLoader::Create())
    , d_current_platform_cursor(LoadCursor(NULL, IDC_ARROW))
    , d_wheel_scroll_latching_enabled(
          base::FeatureList::IsEnabled(
              features::kTouchpadAndWheelScrollLatching))
    , d_raf_aligned_touch_enabled(
          base::FeatureList::IsEnabled(
              features::kRafAlignedTouchInputEvents))
    , d_mouseWheelEventQueue(
	new content::MouseWheelEventQueue(
	    this, false))
{
    d_profile->incrementWebViewCount();

    Init(NULL, gfx::Rect(0, 0, 1, 1));
}

// Only used when RenderWebView is created for a popup window created by the
// renderer:
RenderWebView::RenderWebView(HWND parent_hwnd, int routing_id, const gfx::Rect& initial_rect)
    : d_client(nullptr)
    , d_delegate(nullptr)
    , d_profile(nullptr)
    , d_renderViewRoutingId(0)
    , d_mainFrameRoutingId(0)
    , d_gotRenderViewInfo(false)
    , d_pendingLoadStatus(false)
    , d_isMainFrameAccessible(false)
    , d_pendingDestroy(false)
    , d_properties({ false, false, false, false, false, false })
    , d_cursor_loader(ui::CursorLoader::Create())
    , d_current_platform_cursor(LoadCursor(NULL, IDC_ARROW))
    , d_wheel_scroll_latching_enabled(
          base::FeatureList::IsEnabled(
              features::kTouchpadAndWheelScrollLatching))
    , d_raf_aligned_touch_enabled(
          base::FeatureList::IsEnabled(
              features::kRafAlignedTouchInputEvents))
    , d_mouseWheelEventQueue(
    new content::MouseWheelEventQueue(
        this, false))
{
    Init(parent_hwnd, gfx::Rect(0, 0, 1, 1));

    SetWindowLong(
        d_hwnd.get(), GWL_STYLE,
        GetWindowLong(d_hwnd.get(), GWL_STYLE) | WS_POPUP);

    d_gotRenderViewInfo = true;

    d_renderViewRoutingId = routing_id;
    LOG(INFO) << "routingId=" << routing_id;

    RenderMessageDelegate::GetInstance()->AddRoute(
        d_renderViewRoutingId, this);

    d_compositor->Correlate(d_renderViewRoutingId);

    d_shown = true;

    SetWindowPos(
        d_hwnd.get(),
        0,
        initial_rect.x(),     initial_rect.y(),
        initial_rect.width(), initial_rect.height(),
        SWP_SHOWWINDOW | SWP_FRAMECHANGED |
        SWP_NOACTIVATE | SWP_NOOWNERZORDER);
}

void
RenderWebView::Init(
    HWND parent_hwnd, const gfx::Rect& initial_rect)
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

    windows_session_change_observer_.reset(new views::WindowsSessionChangeObserver(
        base::Bind(&RenderWebView::OnSessionChange,
                   base::Unretained(this))
    ));

    d_dragDrop = new DragDrop(d_hwnd.get(), this);

    RECT rect;
    GetWindowRect(d_hwnd.get(), &rect);
    d_size = gfx::Rect(rect).size();

    d_compositor = RenderCompositorContext::GetInstance()->CreateCompositor(
        d_hwnd.get());

    d_input_method = ui::CreateInputMethod(this, d_hwnd.get());
}

RenderWebView::RenderViewObserver::RenderViewObserver(
    content::RenderView *renderView,
    RenderWebView *renderWebView)
: content::RenderViewObserver(renderView)
, d_renderWebView(renderWebView)
{
}

void RenderWebView::RenderViewObserver::OnDestruct()
{
    d_renderWebView->OnRenderViewDestruct();
    delete this;
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
        d_compositor->SetVisible(false);
        d_compositor.reset();

        dispatchToRenderViewImpl(
            ViewMsg_WasHidden(d_renderViewRoutingId));

        d_hwnd.release();
    } return 0;
    case WM_WINDOWPOSCHANGING: {
        auto windowpos = reinterpret_cast<WINDOWPOS *>(lParam);

        gfx::Size size(windowpos->cx, windowpos->cy);

        if ((size != d_size && !(windowpos->flags & SWP_NOSIZE)) ||
             windowpos->flags & SWP_FRAMECHANGED) {
            d_compositor->Resize(gfx::Size());
        }
    } break;
    case WM_WINDOWPOSCHANGED: {
        auto windowpos = reinterpret_cast<WINDOWPOS *>(lParam);

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
            dispatchToRenderViewImpl(
                ViewMsg_Repaint(d_renderViewRoutingId,
                    gfx::Size(ps.rcPaint.right, ps.rcPaint.bottom)));
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

            dispatchInputEvent(event);

            return 0;
        } break;
        // Mousewheel:
        case WM_MOUSEWHEEL:
        case WM_MOUSEHWHEEL: {
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

            ui::MouseWheelEvent event(msg);

            d_mouseWheelEventQueue->QueueEvent(
                content::MouseWheelEventWithLatencyInfo(
                    ui::MakeWebMouseWheelEvent(
                        event,
                        base::Bind(&GetScreenLocationFromEvent)),
                    ui::LatencyInfo()));

            return 0;
        } break;
        // Keyboard:
        case WM_KEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP: {
            ui::KeyEvent event(msg);

            d_input_method->DispatchKeyEvent(&event);

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
    default:
        break;
    }

    return DefWindowProc(d_hwnd.get(), uMsg, wParam, lParam);
}

RenderWebView::~RenderWebView()
{
    LOG(INFO) << "Destroying RenderWebView, routingId=" << d_renderViewRoutingId;
    if (d_profile) {
        d_profile->decrementWebViewCount();
    }

    if (d_client) {
        auto client = d_client;
        d_client = nullptr;
        client->releaseHost();
    }
}

bool RenderWebView::dispatchToRenderViewImpl(const IPC::Message& message)
{
    return static_cast<IPC::Listener *>(
        content::RenderThreadImpl::current())
            ->OnMessageReceived(message);
}

void RenderWebView::OnRenderViewDestruct()
{
    DCHECK(d_gotRenderViewInfo);

    d_mainFrame.release();
    d_isMainFrameAccessible = false;

    RenderMessageDelegate::GetInstance()->RemoveRoute(
        d_mainFrameRoutingId);
    RenderMessageDelegate::GetInstance()->RemoveRoute(
        d_renderViewRoutingId);

    d_mainFrameRoutingId = 0;
    d_renderViewRoutingId = 0;

    d_gotRenderViewInfo = false;
}

void RenderWebView::ForceRedrawWindow(int attempts) {
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

void RenderWebView::OnSessionChange(WPARAM status_code) {
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
            ViewMsg_WasShown(d_renderViewRoutingId,
                true, ui::LatencyInfo()));
    }
    else {
        dispatchToRenderViewImpl(
            ViewMsg_WasHidden(d_renderViewRoutingId));
    }
}

void RenderWebView::updateFocus()
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    if (d_focused) {
        dispatchToRenderViewImpl(
            InputMsg_SetFocus(d_renderViewRoutingId,
                d_focused));

        dispatchToRenderViewImpl(
            ViewMsg_SetActive(d_renderViewRoutingId,
                d_focused));
    }
    else {
        dispatchToRenderViewImpl(
            ViewMsg_SetActive(d_renderViewRoutingId,
                d_focused));

        dispatchToRenderViewImpl(
            InputMsg_SetFocus(d_renderViewRoutingId,
                d_focused));
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

    content::ResizeParams resize_params = {};
    resize_params.new_size = d_size;
    resize_params.physical_backing_size = d_size;
    resize_params.visible_viewport_size = d_size;
    resize_params.display_mode = blink::kWebDisplayModeBrowser;
    GetScreenInfoForWindow(&resize_params.screen_info, d_hwnd.get());

    dispatchToRenderViewImpl(
        ViewMsg_Resize(d_renderViewRoutingId,
            resize_params));
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

void RenderWebView::sendScreenRects()
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    RECT view_screen_rect, window_screen_rect;

    GetWindowRect(d_hwnd.get(), &view_screen_rect);

    auto root_hwnd = GetAncestor(d_hwnd.get(), GA_ROOT);
    GetWindowRect(root_hwnd, &window_screen_rect);

    if (::IsZoomed(root_hwnd)) {
        auto frame_size_horz = GetSystemMetrics(SM_CXSIZEFRAME),
             frame_size_vert = GetSystemMetrics(SM_CYSIZEFRAME);

        auto border_padding_horz = GetSystemMetrics(SM_CXPADDEDBORDER),
             border_padding_vert = GetSystemMetrics(SM_CXPADDEDBORDER);

        window_screen_rect.left   += frame_size_horz + border_padding_horz;
        window_screen_rect.top    += frame_size_vert + border_padding_vert;
        window_screen_rect.right  -= frame_size_horz + border_padding_horz;
        window_screen_rect.bottom -= frame_size_vert + border_padding_vert;
    }

    dispatchToRenderViewImpl(
        ViewMsg_UpdateScreenRects(d_renderViewRoutingId,
            gfx::Rect(view_screen_rect), gfx::Rect(window_screen_rect)));
}

void RenderWebView::dispatchInputEvent(const blink::WebInputEvent& event)
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    ui::LatencyInfo latency_info;

    dispatchToRenderViewImpl(
        InputMsg_HandleInputEvent(d_renderViewRoutingId,
            &event,
            {},
            latency_info,
            ui::WebInputEventTraits::ShouldBlockEventStream(
                event, d_raf_aligned_touch_enabled, d_wheel_scroll_latching_enabled)?
                content::InputEventDispatchType::DISPATCH_TYPE_BLOCKING :
                content::InputEventDispatchType::DISPATCH_TYPE_NON_BLOCKING));
}

void RenderWebView::destroy()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!d_pendingDestroy);

    if (d_hwnd.is_valid()) {
        d_hwnd.reset();
    }

    if (d_gotRenderViewInfo) {
        dispatchToRenderViewImpl(
            ViewMsg_Close(d_renderViewRoutingId));
    }

    // Schedule a deletion of this RenderWebView.  The reason we don't delete
    // the object right here right now is because there may be a callback
    // that is already scheduled and the callback requires the existence of
    // the WebView.
    d_pendingDestroy = true;
    base::MessageLoop::current()->task_runner()->DeleteSoon(FROM_HERE, this);
}

WebFrame *RenderWebView::mainFrame()
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

int RenderWebView::loadUrl(const StringRef& url)
{
    DCHECK(Statics::isInApplicationMainThread());
    if (d_pendingLoadStatus) {
        return EBUSY;
    }

    d_pendingLoadStatus = true;
    d_url = std::string(url.data(), url.length());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", loadUrl=" << d_url;
    d_client->loadUrl(d_url);
    return 0;
}

#if defined(BLPWTK2_FEATURE_DWM)
void RenderWebView::rootWindowCompositionChanged()
{
    DCHECK(Statics::isInApplicationMainThread());
}
#endif

void RenderWebView::loadInspector(unsigned int pid, int routingId)
{
    DCHECK(Statics::isInApplicationMainThread());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId
              << ", loading inspector for " << routingId;

    d_client->proxy()->loadInspector(pid, routingId);
}

void RenderWebView::inspectElementAt(const POINT& point)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->inspectElementAt(point.x, point.y);
}

#if defined(BLPWTK2_FEATURE_SCREENPRINT)
void RenderWebView::drawContentsToBlob(Blob *blob, const DrawParams& params)
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_isMainFrameAccessible)
        << "You should wait for didFinishLoad";
    DCHECK(d_gotRenderViewInfo);
    DCHECK(blob);

    content::RenderView* rv = content::RenderView::FromRoutingID(d_renderViewRoutingId);
    RendererUtil::drawContentsToBlob(rv, blob, params);
}
#endif

int RenderWebView::goBack()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (d_pendingLoadStatus) {
        return EBUSY;
    }

    d_pendingLoadStatus = true;
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", goBack()";
    d_client->goBack();
    return 0;
}

int RenderWebView::goForward()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (d_pendingLoadStatus) {
        return EBUSY;
    }

    d_pendingLoadStatus = true;
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", goForward()";
    d_client->goForward();
    return 0;
}

int RenderWebView::reload()
{
    DCHECK(Statics::isInApplicationMainThread());
    if (d_pendingLoadStatus) {
        return EBUSY;
    }

    d_pendingLoadStatus = true;
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", reload()";
    d_client->reload();
    return 0;
}

void RenderWebView::stop()
{
    DCHECK(Statics::isInApplicationMainThread());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId << ", stop";
    d_client->proxy()->stop();
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
    DCHECK(Statics::isInApplicationMainThread());
    LOG(INFO) << "routingId=" << d_renderViewRoutingId
              << ", setLogicalFocus " << (focused ? "true" : "false");
    if (d_gotRenderViewInfo) {
        // If we have the renderer in-process, then set the logical focus
        // immediately so that handleInputEvents will work as expected.
        content::RenderViewImpl* rv = content::RenderViewImpl::FromRoutingID(d_renderViewRoutingId);
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
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->cutSelection();
}

void RenderWebView::copySelection()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->copySelection();
}

void RenderWebView::paste()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->paste();
}

void RenderWebView::deleteSelection()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->deleteSelection();
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
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->performCustomContextMenuAction(actionId);
}

void RenderWebView::find(const StringRef& text, bool matchCase, bool forward)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->find(std::string(text.data(), text.size()), matchCase, forward);
}

void RenderWebView::stopFind(bool preserveSelection)
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->stopFind(preserveSelection);
}

void RenderWebView::replaceMisspelledRange(const StringRef& text)
{
    DCHECK(Statics::isInApplicationMainThread());
    std::string stext(text.data(), text.length());
    d_client->proxy()->replaceMisspelledRange(stext);
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
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    content::RenderView* rv = content::RenderView::FromRoutingID(d_renderViewRoutingId);
    blink::WebView* webView = rv->GetWebView();
    return webView->ForceStartRubberbanding(x, y);
}

bool RenderWebView::isRubberbanding() const
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    content::RenderView* rv = content::RenderView::FromRoutingID(d_renderViewRoutingId);
    blink::WebView* webView = rv->GetWebView();
    return webView->IsRubberbanding();
}

void RenderWebView::abortRubberbanding()
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    content::RenderView* rv = content::RenderView::FromRoutingID(d_renderViewRoutingId);
    blink::WebView* webView = rv->GetWebView();
    webView->AbortRubberbanding();
}

String RenderWebView::getTextInRubberband(const NativeRect& rect)
{
    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    content::RenderView* rv = content::RenderView::FromRoutingID(d_renderViewRoutingId);
    blink::WebView* webView = rv->GetWebView();
    blink::WebRect webRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
    std::string str = webView->GetTextInRubberband(webRect).Utf8();
    return String(str.data(), str.size());
}
#endif

void RenderWebView::rootWindowPositionChanged()
{
    DCHECK(Statics::isInApplicationMainThread());
    sendScreenRects();
}

void RenderWebView::rootWindowSettingsChanged()
{
    DCHECK(Statics::isInApplicationMainThread());
    sendScreenRects();
}

void RenderWebView::handleInputEvents(const InputEvent *events, size_t eventsCount)
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
    int red = GetRValue(color);
    int green = GetGValue(color);
    int blue = GetBValue(color);
    int alpha = GetAValue(color);

    DCHECK(Statics::isRendererMainThreadMode());
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(d_isMainFrameAccessible) << "You should wait for didFinishLoad";
    DCHECK(d_gotRenderViewInfo);

    content::RenderView* rv = content::RenderView::FromRoutingID(d_renderViewRoutingId);
    blink::WebFrameWidget* frameWidget = rv->GetWebFrameWidget();
    frameWidget->SetBaseBackgroundColor(
        SkColorSetARGB(alpha, red, green, blue));
}

void RenderWebView::setRegion(NativeRegion region)
{
    DCHECK(Statics::isInApplicationMainThread());

    SetWindowRgn(d_hwnd.get(),
                 region,
                 IsWindowVisible(d_hwnd.get()));
}

void RenderWebView::clearTooltip()
{
    DCHECK(Statics::isInApplicationMainThread());
    d_client->proxy()->clearTooltip();
}

v8::MaybeLocal<v8::Value> RenderWebView::callFunction(
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

    v8::Local<v8::Value> result =
        localWebFrame->CallFunctionEvenIfScriptDisabled(func, recv, argc, argv);

    return v8::MaybeLocal<v8::Value>(result);
}

#if defined(BLPWTK2_FEATURE_PRINTPDF)
String RenderWebView::printToPDF(const StringRef& propertyName)
{
    auto* rv = content::RenderView::FromRoutingID(d_renderViewRoutingId);
    return RendererUtil::printToPDF(rv, propertyName.toStdString());
}
#endif

// blpwtk2::WebViewClientDelegate overrides
void RenderWebView::setClient(WebViewClient *client)
{
    d_client = client;
}

void RenderWebView::ncHitTest()
{
    d_delegate->requestNCHitTest(this);
    // Note: The embedder is expected to call WebView::onNCHitTestResult
}

void RenderWebView::ncDragBegin(int hitTestCode, const gfx::Point& point)
{
    POINT winPoint = { point.x(), point.y() };
    d_delegate->ncDragBegin(this, hitTestCode, winPoint);
}

void RenderWebView::ncDragMove(const gfx::Point& point)
{
    POINT winPoint = { point.x(), point.y() };
    d_delegate->ncDragMove(this, winPoint);
}

void RenderWebView::ncDragEnd(const gfx::Point& point)
{
    POINT winPoint = { point.x(), point.y() };
    d_delegate->ncDragEnd(this, winPoint);
}

void RenderWebView::ncDoubleClick(const gfx::Point& point)
{
    POINT winPoint = { point.x(), point.y() };
    d_delegate->ncDoubleClick(this, winPoint);
}

void RenderWebView::focused()
{
    d_delegate->focused(this);
}

void RenderWebView::blurred()
{
    d_delegate->blurred(this);
}

void RenderWebView::showContextMenu(const ContextMenuParams& params)
{
    d_delegate->showContextMenu(this, params);
}

void RenderWebView::findReply(int  numberOfMatches,
                             int  activeMatchOrdinal,
                             bool finalUpdate)
{
    d_delegate->findState(
            this, numberOfMatches, activeMatchOrdinal, finalUpdate);
}

void RenderWebView::preResize(const gfx::Size& size)
{
}

void RenderWebView::notifyRoutingId(int id)
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
            base::Bind(&RenderWebView::notifyRoutingId,
                       base::Unretained(this),
                       id));
        return;
    }

    d_gotRenderViewInfo = true;

    d_renderViewRoutingId = id;
    LOG(INFO) << "routingId=" << id;

    d_mainFrameRoutingId = rv->GetMainRenderFrame()->GetRoutingID();

    RenderMessageDelegate::GetInstance()->AddRoute(
        d_renderViewRoutingId, this);
    RenderMessageDelegate::GetInstance()->AddRoute(
        d_mainFrameRoutingId, this);

    new RenderViewObserver(rv, this);

    d_compositor->Correlate(d_renderViewRoutingId);

    // A (no-op) frame sink may have been created for the RenderWidget before
    // the call to 'Correlate()' above. So, force the creation of a new
    // frame sink:
    rv->compositor()->SetVisible(false);
    rv->compositor()->ReleaseLayerTreeFrameSink();

    updateVisibility();
    updateSize();
    updateFocus();
#if defined(BLPWTK2_FEATURE_RUBBERBAND)
    updateAltDragRubberBanding();
#endif
}

// ui::internal::InputMethodDelegate overrides:
ui::EventDispatchDetails RenderWebView::DispatchKeyEventPostIME(
    ui::KeyEvent* key_event)
{
    if (!key_event->handled()) {
        dispatchInputEvent(ui::MakeWebKeyboardEvent(*key_event));
    }

    return ui::EventDispatchDetails();
}

// ui::TextInputClient overrides:
void RenderWebView::SetCompositionText(const ui::CompositionText& composition)
{
    std::vector<blink::WebImeTextSpan> spans(
        composition.ime_text_spans.size());
    std::transform(
        composition.ime_text_spans.begin(), composition.ime_text_spans.end(),
        spans.begin(),
        [=](const auto& ime_text_span) -> blink::WebImeTextSpan {
            return blink::WebImeTextSpan(
                [=](const auto& type) {
                    switch (type) {
                    case ui::ImeTextSpan::Type::kComposition:
                        return blink::WebImeTextSpan::Type::kComposition;
                    case ui::ImeTextSpan::Type::kSuggestion:
                        return blink::WebImeTextSpan::Type::kSuggestion;
                    default:
                        NOTREACHED();
                        return blink::WebImeTextSpan::Type::kComposition;
                    }
                }(ime_text_span.type),
                ime_text_span.start_offset, ime_text_span.end_offset,
                ime_text_span.underline_color,
                ime_text_span.thick,
                ime_text_span.background_color,
                ime_text_span.suggestion_highlight_color,
                ime_text_span.suggestions);
        });

    dispatchToRenderViewImpl(
        InputMsg_ImeSetComposition(d_renderViewRoutingId,
            composition.text,
            spans,
            gfx::Range::InvalidRange(),
            composition.selection.end(), composition.selection.end()));

    d_has_composition_text = !composition.text.empty();
}

void RenderWebView::ConfirmCompositionText()
{
    if (d_has_composition_text) {
        dispatchToRenderViewImpl(
            InputMsg_ImeFinishComposingText(d_renderViewRoutingId,
                false));
    }

    d_has_composition_text = false;
}

void RenderWebView::ClearCompositionText()
{
    if (d_has_composition_text) {
        dispatchToRenderViewImpl(
            InputMsg_ImeSetComposition(d_renderViewRoutingId,
                base::string16(),
                std::vector<blink::WebImeTextSpan>(),
                gfx::Range::InvalidRange(),
                0, 0));
    }

    d_has_composition_text = false;
}

void RenderWebView::InsertText(const base::string16& text)
{
    if (!text.empty()) {
        dispatchToRenderViewImpl(
            InputMsg_ImeCommitText(d_renderViewRoutingId,
                text,
                std::vector<blink::WebImeTextSpan>(),
                gfx::Range::InvalidRange(),
                0));
    }
    else {
        dispatchToRenderViewImpl(
            InputMsg_ImeFinishComposingText(d_renderViewRoutingId,
                false));
    }

    d_has_composition_text = false;
}

void RenderWebView::InsertChar(const ui::KeyEvent& event)
{
    dispatchInputEvent(ui::MakeWebKeyboardEvent(event));
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
        ViewMsg_SetTextDirection(d_renderViewRoutingId,
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

// DragDropDelegate overrides:
void RenderWebView::DragTargetEnter(
    const std::vector<content::DropData::Metadata>& drag_data_metadata,
    const gfx::Point& client_pt,
    const gfx::Point& screen_pt,
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
    const gfx::Point& client_pt,
    const gfx::Point& screen_pt,
    blink::WebDragOperationsMask ops_allowed,
    int key_modifiers)
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    dispatchToRenderViewImpl(
        DragMsg_TargetDragOver(d_renderViewRoutingId,
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
        DragMsg_TargetDragLeave(d_renderViewRoutingId,
            gfx::Point(), gfx::Point()));
}

void RenderWebView::DragTargetDrop(
    const content::DropData& drop_data,
    const gfx::Point& client_pt,
    const gfx::Point& screen_pt,
    int key_modifiers)
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    dispatchToRenderViewImpl(
        DragMsg_TargetDrop(d_renderViewRoutingId,
            drop_data,
            client_pt, screen_pt,
            key_modifiers));
}

void RenderWebView::DragSourceEnded(
    const gfx::Point& client_pt,
    const gfx::Point& screen_pt,
    blink::WebDragOperation drag_operation)
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    dispatchToRenderViewImpl(
        DragMsg_SourceEnded(d_renderViewRoutingId,
            client_pt, screen_pt,
            drag_operation));
}

void RenderWebView::DragSourceSystemEnded()
{
    if (!d_gotRenderViewInfo) {
        return;
    }

    dispatchToRenderViewImpl(
        DragMsg_SourceSystemDragEnded(d_renderViewRoutingId));
}

// content::MouseWheelEventQueueClient:
void RenderWebView::SendMouseWheelEventImmediately(
    const content::MouseWheelEventWithLatencyInfo& event)
{
    dispatchInputEvent(event.event);
}

void RenderWebView::ForwardGestureEventWithLatencyInfo(
    const blink::WebGestureEvent& event,
    const ui::LatencyInfo& latency_info)
{
    dispatchInputEvent(event);
}

void RenderWebView::OnMouseWheelEventAck(
    const content::MouseWheelEventWithLatencyInfo& event,
    content::InputEventAckState ack_result)
{
}

// Message handlers

// Only called when RenderWebView is a popup window created by the renderer:
void RenderWebView::OnClose()
{
    this->destroy();

    d_mainFrame.release();
    d_isMainFrameAccessible = false;

    RenderMessageDelegate::GetInstance()->RemoveRoute(
        d_renderViewRoutingId);

    d_mainFrameRoutingId = 0;
    d_renderViewRoutingId = 0;

    d_gotRenderViewInfo = false;
}

void RenderWebView::OnHasTouchEventHandlers(bool has_handlers)
{
}

void RenderWebView::OnImeCompositionRangeChanged(
        const gfx::Range& range,
        const std::vector<gfx::Rect>& character_bounds)
{
    d_composition_character_bounds = character_bounds;
}

void RenderWebView::OnImeCancelComposition()
{
    d_input_method->CancelComposition(this);
    d_has_composition_text = false;
}

void RenderWebView::OnInputEventAck(const content::InputEventAck& ack)
{
    switch (ack.type) {
    case blink::WebInputEvent::kMouseWheel: {
        d_mouseWheelEventQueue->ProcessMouseWheelAck(
            ack.state, ack.latency);
    } break;
    default:
        break;
    }
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

void RenderWebView::OnShowWidget(int routing_id, gfx::Rect initial_rect)
{
    // Will receive 'OnClose()' to destroy:
    new RenderWebView(
        GetAncestor(d_hwnd.get(), GA_ROOT),
        routing_id,
        initial_rect);
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
        d_input_method->ShowImeIfNeeded();
    }

    if (d_text_input_state.type != ui::TEXT_INPUT_TYPE_NONE) {
        dispatchToRenderViewImpl(
            InputMsg_RequestCompositionUpdates(d_renderViewRoutingId,
                false, true));
    }
    else {
        dispatchToRenderViewImpl(
            InputMsg_RequestCompositionUpdates(d_renderViewRoutingId,
                false, false));
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

void RenderWebView::onLoadStatus(int status)
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

#if defined(BLPWTK2_FEATURE_DEVTOOLSINTEGRATION)
void RenderWebView::devToolsAgentHostAttached()
{
    if (d_delegate) {
        d_delegate->devToolsAgentHostAttached(this);
    }
}

void RenderWebView::devToolsAgentHostDetached()
{
    if (d_delegate) {
        d_delegate->devToolsAgentHostDetached(this);
    }
}
#endif

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
        IPC_MESSAGE_HANDLER(InputHostMsg_HandleInputEvent_ACK,
            OnInputEventAck)
        IPC_MESSAGE_HANDLER(InputHostMsg_ImeCancelComposition,
            OnImeCancelComposition)
        IPC_MESSAGE_HANDLER(InputHostMsg_ImeCompositionRangeChanged,
            OnImeCompositionRangeChanged)
        IPC_MESSAGE_HANDLER(ViewHostMsg_Close,
            OnClose)
        IPC_MESSAGE_HANDLER(ViewHostMsg_HasTouchEventHandlers,
            OnHasTouchEventHandlers)
        IPC_MESSAGE_HANDLER(ViewHostMsg_LockMouse,
            OnLockMouse)
        IPC_MESSAGE_HANDLER(ViewHostMsg_SetCursor,
            OnSetCursor)
        IPC_MESSAGE_HANDLER(ViewHostMsg_SelectionBoundsChanged,
            OnSelectionBoundsChanged)
        IPC_MESSAGE_HANDLER(ViewHostMsg_ShowWidget,
            OnShowWidget)
        IPC_MESSAGE_HANDLER(ViewHostMsg_UnlockMouse,
            OnUnlockMouse)
    	IPC_MESSAGE_HANDLER(ViewHostMsg_UpdateRect,
    	    OnUpdateRect)
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

// IPC message handlers
void RenderWebView::OnDetach()
{
}

void RenderWebView::OnUpdateRect(const ViewHostMsg_UpdateRect_Params& params)
{
}

}  // close namespace blpwtk2

// vim: ts=4 et


