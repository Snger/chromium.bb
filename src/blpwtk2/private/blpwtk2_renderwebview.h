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

#ifndef INCLUDED_BLPWTK2_RENDERWEBVIEW_H
#define INCLUDED_BLPWTK2_RENDERWEBVIEW_H

#include <blpwtk2.h>
#include <blpwtk2_config.h>

#include <blpwtk2_dragdrop.h>
#include <blpwtk2_scopedhwnd.h>
#include <blpwtk2_webview.h>
#include <blpwtk2_webviewdelegate.h>
#include <blpwtk2_webviewclientdelegate.h>
#include <blpwtk2_webviewproperties.h>
#include <blpwtk2_webviewproxy.h>

#include <base/timer/timer.h>
#include <cc/input/touch_action.h>
#include <content/browser/renderer_host/input/fling_controller.h>
#include <content/browser/renderer_host/input/input_disposition_handler.h>
#include <content/browser/renderer_host/input/input_router_impl.h>
#include <content/browser/renderer_host/input/mouse_wheel_event_queue.h>
#include <content/common/text_input_state.h>
#include <content/common/cursors/webcursor.h>
#include <content/common/input/input_handler.mojom.h>
#include <content/public/common/input_event_ack_state.h>
#include <content/public/renderer/render_view_observer.h>
#include <ipc/ipc_listener.h>
#include <mojo/public/cpp/bindings/binding.h>
#include <third_party/blink/public/platform/web_drag_operation.h>
#include <third_party/blink/public/web/web_text_direction.h>
#include <ui/base/ime/input_method_delegate.h>
#include <ui/base/ime/text_input_client.h>
#include <ui/events/blink/did_overscroll_params.h>
#include <ui/gfx/selection_bound.h>
#include <ui/gfx/geometry/point.h>
#include <ui/gfx/geometry/size.h>
#include <ui/views/win/windows_session_change_observer.h>



struct ViewHostMsg_SelectionBounds_Params;
class SkBitmap;

namespace blink {
class WebInputEvent;
} // close namespace blink

namespace content {
struct DragEventSourceInfo;
struct DropData;
struct InputEventAck;
struct TextInputState;
class InputRouterImpl;
class WebCursor;
}  // close namespace content

namespace gfx {
class Point;
}  // close namespace gfx

namespace ui {
class CursorLoader;
class DropTargetWin;
class InputMethod;

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
class RubberbandOutline;
#endif
}  // close namespace ui

namespace views {
class WindowsSessionChangeObserver;

namespace corewm {
class Tooltip;
}  // close namespace wm

}  // close namespace views

namespace blpwtk2 {

class ProfileImpl;
class RenderCompositor;
class WebFrameImpl;
class WebViewProxy;

                        // ===================
                        // class RenderWebView
                        // ===================

class RenderWebView final : public WebView
                          , public WebViewClientDelegate
                          , private WebViewDelegate
                          , private IPC::Listener
                          , private ui::internal::InputMethodDelegate
                          , private ui::TextInputClient
                          , private DragDropDelegate
                          , private content::InputRouterImplClient
                          , private content::InputDispositionHandler
                          , private content::FlingControllerSchedulerClient
{
    // DATA
    WebView *d_proxy;
    WebViewDelegate *d_delegate;
    ProfileImpl *d_profile;
#if defined(BLPWTK2_FEATURE_FOCUS) || defined(BLPWTK2_FEATURE_REROUTEMOUSEWHEEL)
    WebViewProperties d_properties;
#endif

    ScopedHWND d_hwnd;

    bool d_has_parent = false;

    bool d_shown = false, d_visible = false;
    gfx::Size d_size;

    gfx::Point d_mouse_screen_position,
               d_unlocked_mouse_screen_position,
               d_unlocked_mouse_webview_position;

    bool d_nc_hit_test_enabled = false;
    int d_nc_hit_test_result = 0;
    bool d_mouse_pressed = false;
    bool d_mouse_entered = false, d_mouse_locked = false;

    // Who knew that cursor-setting would be such a hassle?
    content::WebCursor d_current_cursor;
    std::unique_ptr<ui::CursorLoader> d_cursor_loader;
    bool d_is_cursor_overridden = false;
    HCURSOR d_current_platform_cursor = NULL, d_previous_platform_cursor = NULL;

    bool d_focused = false;
    std::unique_ptr<ui::InputMethod> d_input_method;
    gfx::Range d_composition_range;
    std::vector<gfx::Rect> d_composition_character_bounds;
    bool d_has_composition_text = false;
    content::TextInputState d_text_input_state;
    gfx::SelectionBound d_selection_anchor, d_selection_focus;
    base::string16 d_selection_text;
    std::size_t d_selection_text_offset = 0;
    gfx::Range d_selection_range;

    scoped_refptr<DragDrop> d_dragDrop;

    std::unique_ptr<content::InputRouterImpl> d_inputRouterImpl;

    base::OneShotTimer d_mouse_wheel_end_dispatch_timer;
    blink::WebMouseWheelEvent d_last_mouse_wheel_event;
    gfx::Vector2dF d_first_wheel_location;
    blink::WebMouseWheelEvent d_initial_wheel_event;

    enum class FirstScrollUpdateAckState {
        kNotArrived = 0,
        kConsumed,
        kNotConsumed,
    };

    FirstScrollUpdateAckState d_first_scroll_update_ack_state = FirstScrollUpdateAckState::kNotArrived;

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
    bool d_enableAltDragRubberBanding = false;
    std::unique_ptr<ui::RubberbandOutline> d_rubberbandOutline;
#endif

    std::unique_ptr<views::WindowsSessionChangeObserver> d_windows_session_change_observer;

    base::string16 d_tooltip_text, d_last_tooltip_text, d_tooltip_text_at_mouse_press;
    std::unique_ptr<views::corewm::Tooltip> d_tooltip;

    base::OneShotTimer d_tooltip_defer_timer;
        // Timer for requesting delayed updates of the tooltip.

    base::OneShotTimer d_tooltip_shown_timer;
        // Timer to timeout the life of an on-screen tooltip. We hide the tooltip
        // when this timer fires.

    int d_renderViewRoutingId, d_renderWidgetRoutingId, d_mainFrameRoutingId;
    bool d_gotRenderViewInfo;

    content::mojom::WidgetInputHandlerPtr d_widgetInputHandler;

    bool d_pendingDestroy;
    std::unique_ptr<RenderCompositor> d_compositor;

    static LPCTSTR GetWindowClass();
    static LRESULT CALLBACK WindowProcedure(HWND   hWnd,
                                            UINT   uMsg,
                                            WPARAM wParam,
                                            LPARAM lParam);
    LRESULT windowProcedure(UINT   uMsg,
                            WPARAM wParam,
                            LPARAM lParam);

    void initialize();
    void finishNotifyRoutingId(int id);
    bool dispatchToRenderViewImpl(const IPC::Message& message);
    void updateVisibility();
    void updateSize();
    void sendScreenRects();
    void updateFocus();
    void setPlatformCursor(HCURSOR cursor);
    void onQueueWheelEventWithPhaseEnded();
    void ForceRedrawWindow(int attempts);
    void OnSessionChange(WPARAM status_code);
    void showTooltip();
    void hideTooltip();
    void updateTooltip();

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
    void updateAltDragRubberBanding();
#endif

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
    void activateKeyboardLayout(unsigned int hkl) override;
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

#if defined(BLPWTK2_FEATURE_FASTRESIZE)
    void disableResizeOptimization() override;
#endif

    void setSecurityToken(v8::Isolate *isolate,
                          v8::Local<v8::Value> token) override;

    // WebViewDelegate overrides
    void created(WebView *source) override;
    void didFinishLoad(WebView *source, const StringRef& url) override;
    void didFailLoad(WebView *source, const StringRef& url) override;
    void focused(WebView* source) override;
    void blurred(WebView* source) override;
    void showContextMenu(WebView                  *source,
                         const ContextMenuParams&  params) override;
    void requestNCHitTest(WebView *source) override;
    void ncDragBegin(WebView      *source,
                     int           hitTestCode,
                     const POINT&  startPoint) override;
    void ncDragMove(WebView *source, const POINT& movePoint) override;
    void ncDragEnd(WebView *source, const POINT& endPoint) override;
    void ncDoubleClick(WebView *source, const POINT& point) override;
    void findState(WebView *source,
                   int      numberOfMatches,
                   int      activeMatchOrdinal,
                   bool     finalUpdate) override;
    void devToolsAgentHostAttached(WebView *source) override;
    void devToolsAgentHostDetached(WebView *source) override;

    // IPC::Listener overrides
    bool OnMessageReceived(const IPC::Message& message) override;

    // ui::internal::InputMethodDelegate overrides:
    ui::EventDispatchDetails DispatchKeyEventPostIME(
        ui::KeyEvent* key_event) override;

    // ui::TextInputClient overrides:
    void SetCompositionText(const ui::CompositionText& composition) override;
    void ConfirmCompositionText() override;
    void ClearCompositionText() override;
    void InsertText(const base::string16& text) override;
    void InsertChar(const ui::KeyEvent& event) override;
    ui::TextInputType GetTextInputType() const override;
    ui::TextInputMode GetTextInputMode() const override;
    base::i18n::TextDirection GetTextDirection() const override;
    int GetTextInputFlags() const override;
    bool CanComposeInline() const override;
    gfx::Rect GetCaretBounds() const override;
    bool GetCompositionCharacterBounds(uint32_t index,
        gfx::Rect* rect) const override;
    bool HasCompositionText() const override;
    FocusReason GetFocusReason() const override;
    bool GetTextRange(gfx::Range* range) const override;
    bool GetCompositionTextRange(gfx::Range* range) const override;
    bool GetSelectionRange(gfx::Range* range) const override;
    bool SetSelectionRange(const gfx::Range& range) override;
    bool DeleteRange(const gfx::Range& range) override;
    bool GetTextFromRange(const gfx::Range& range,
        base::string16* text) const override;
    void OnInputMethodChanged() override;
    bool ChangeTextDirectionAndLayoutAlignment(
        base::i18n::TextDirection direction) override;
    void ExtendSelectionAndDelete(size_t before, size_t after) override;
    void EnsureCaretNotInRect(const gfx::Rect& rect) override;
    bool IsTextEditCommandEnabled(ui::TextEditCommand command) const override;
    void SetTextEditCommandForNextKeyEvent(ui::TextEditCommand command) override;
    ukm::SourceId GetClientSourceForMetrics() const override;
    bool ShouldDoLearning() override;

    // DragDropDelegate overrides:
    void DragTargetEnter(
        const std::vector<content::DropData::Metadata>& drop_data,
        const gfx::PointF& client_pt,
        const gfx::PointF& screen_pt,
        blink::WebDragOperationsMask ops_allowed,
        int key_modifiers) override;
    void DragTargetOver(
        const gfx::PointF& client_pt,
        const gfx::PointF& screen_pt,
        blink::WebDragOperationsMask ops_allowed,
        int key_modifiers) override;
    void DragTargetLeave() override;
    void DragTargetDrop(
        const content::DropData& drop_data,
        const gfx::PointF& client_pt,
        const gfx::PointF& screen_pt,
        int key_modifiers) override;
    void DragSourceEnded(
        const gfx::PointF& client_pt,
        const gfx::PointF& screen_pt,
        blink::WebDragOperation drag_operation) override;
    void DragSourceSystemEnded() override;

    // content::InputRouterClient overrides:
    content::InputEventAckState FilterInputEvent(
        const blink::WebInputEvent& input_event,
        const ui::LatencyInfo& latency_info) override;
    void IncrementInFlightEventCount() override {};
    void DecrementInFlightEventCount(content::InputEventAckSource ack_source) override {};
    void OnHasTouchEventHandlers(bool has_handlers) override;
    void DidOverscroll(const ui::DidOverscrollParams& params) override {};
    void OnSetWhiteListedTouchAction(cc::TouchAction touch_action) override {};
    void DidStartScrollingViewport() override {};
    void ForwardGestureEventWithLatencyInfo(
        const blink::WebGestureEvent& gesture_event,
        const ui::LatencyInfo& latency_info) override;
    void ForwardWheelEventWithLatencyInfo(
        const blink::WebMouseWheelEvent& wheel_event,
        const ui::LatencyInfo& latency_info) override {};
    bool IsWheelScrollInProgress() override;
    void SetMouseCapture(bool capture) override {};

    // content::InputRouterImplClient overrides:
    content::mojom::WidgetInputHandler* GetWidgetInputHandler() override;
    void OnImeCancelComposition() override;
    void OnImeCompositionRangeChanged(
        const gfx::Range& range,
        const std::vector<gfx::Rect>& bounds) override;

    // content::InputDispositionHandler overrides:
    void OnKeyboardEventAck(
        const content::NativeWebKeyboardEventWithLatencyInfo& event,
        content::InputEventAckSource ack_source,
        content::InputEventAckState ack_result) override {};
    void OnMouseEventAck(
        const content::MouseEventWithLatencyInfo& event,
        content::InputEventAckSource ack_source,
        content::InputEventAckState ack_result) override {};
    void OnWheelEventAck(
        const content::MouseWheelEventWithLatencyInfo& event,
        content::InputEventAckSource ack_source,
        content::InputEventAckState ack_result) override {};
    void OnTouchEventAck(
        const content::TouchEventWithLatencyInfo& event,
        content::InputEventAckSource ack_source,
        content::InputEventAckState ack_result) override {};
    void OnGestureEventAck(
        const content::GestureEventWithLatencyInfo& event,
        content::InputEventAckSource ack_source,
        content::InputEventAckState ack_result) override;
    void OnUnexpectedEventAck(UnexpectedEventAckType type) override {};

    // content::FlingControllerSchedulerClient overrides:
    void ScheduleFlingProgress(
        base::WeakPtr<content::FlingController> fling_controller) override {};
    void DidStopFlingingOnBrowser(
        base::WeakPtr<content::FlingController> fling_controller) override {};
    bool NeedsBeginFrameForFlingProgress() override;

    // Message handlers
    void OnClose();
    void OnDetach();
    void OnLockMouse(
        bool user_gesture,
        bool privileged);
    void OnSelectionBoundsChanged(
        const ViewHostMsg_SelectionBounds_Params& params);
    void OnSelectionChanged(const base::string16& text,
        uint32_t offset,
        const gfx::Range& range);
    void OnSetCursor(const content::WebCursor& cursor);
    void OnSetTooltipText(
        const base::string16& tooltip_text,
        blink::WebTextDirection text_direction_hint);
    void OnShowWidget(
        int routing_id, const gfx::Rect initial_rect);
    void OnStartDragging(
        const content::DropData& drop_data,
        blink::WebDragOperationsMask operations_allowed,
        const SkBitmap& bitmap,
        const gfx::Vector2d& bitmap_offset_in_dip,
        const content::DragEventSourceInfo& event_info);
    void OnTextInputStateChanged(const content::TextInputState& params);
    void OnUnlockMouse();
    void OnUpdateDragCursor(
        blink::WebDragOperation drag_operation);
#if defined(BLPWTK2_FEATURE_RUBBERBAND)
    void OnHideRubberbandRect();
    void OnSetRubberbandRect(const gfx::Rect& rect);
#endif

    DISALLOW_COPY_AND_ASSIGN(RenderWebView);

  public:
    explicit RenderWebView(WebViewDelegate          *delegate,
                           ProfileImpl              *profile,
                           const WebViewProperties&  properties);
    explicit RenderWebView(ProfileImpl              *profile,
                           int                       routingId,
                           const gfx::Rect&          initialRect);
    ~RenderWebView() final;

    // blpwtk2::WebViewClientDelegate overrides
    void setClient(WebViewClient *client) override;
    void ncHitTest() override;
    void ncDragBegin(int hitTestCode, const gfx::Point& point) override;
    void ncDragMove(const gfx::Point& point) override;
    void ncDragEnd(const gfx::Point& point) override;
    void ncDoubleClick(const gfx::Point& point) override;
    void focused() override;
    void blurred() override;
    void showContextMenu(const ContextMenuParams& params) override;
    void findReply(int  numberOfMatches,
                   int  activeMatchOrdinal,
                   bool finalUpdate) override;
    void preResize(const gfx::Size& size) override;
    void notifyRoutingId(int id) override;
    void onLoadStatus(int status) override;
#if defined(BLPWTK2_FEATURE_DEVTOOLSINTEGRATION)
    void devToolsAgentHostAttached() override;
    void devToolsAgentHostDetached() override;
#endif
    void didFinishLoadForFrame(int              routingId,
                               const StringRef& url) override;
    void didFailLoadForFrame(int              routingId,
                             const StringRef& url) override;
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RENDERWEBVIEW_H

// vim: ts=4 et

