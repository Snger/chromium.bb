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
#include <blpwtk2_webviewclientdelegate.h>
#include <blpwtk2_webviewproperties.h>

#include <content/browser/renderer_host/input/mouse_wheel_event_queue.h>
#include <content/common/text_input_state.h>
#include <content/common/cursors/webcursor.h>
#include <content/public/renderer/render_view_observer.h>
#include <ipc/ipc_listener.h>
#include <third_party/WebKit/public/platform/WebDragOperation.h>
#include <ui/base/ime/input_method_delegate.h>
#include <ui/base/ime/text_input_client.h>
#include <ui/gfx/selection_bound.h>
#include <ui/gfx/geometry/point.h>
#include <ui/gfx/geometry/size.h>
#include <ui/views/win/windows_session_change_observer.h>

struct ViewHostMsg_UpdateRect_Params;

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
class MouseWheelEventQueue;
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
}  // close namespace views

namespace blpwtk2 {

class ProfileImpl;
class RenderCompositor;
class WebFrameImpl;

                        // ===================
                        // class RenderWebView
                        // ===================

class RenderWebView final : public WebView
                          , public WebViewClientDelegate
                          , private IPC::Listener
                          , private ui::internal::InputMethodDelegate
                          , private ui::TextInputClient
                          , private DragDropDelegate
                          , private content::MouseWheelEventQueueClient
{
    class RenderViewObserver : public content::RenderViewObserver {
      private:

        RenderWebView *d_renderWebView;

      public:

        RenderViewObserver(
            content::RenderView *renderView, RenderWebView *renderWebView);

        void OnDestruct() override;
    };

    // DATA
    WebViewClient *d_client;
    WebViewDelegate *d_delegate;

    ProfileImpl *d_profile;
    int d_renderViewRoutingId, d_mainFrameRoutingId;
    bool d_gotRenderViewInfo;
    bool d_pendingLoadStatus;
    bool d_isMainFrameAccessible;
    bool d_pendingDestroy;
    std::string d_url;
    std::unique_ptr<WebFrameImpl> d_mainFrame;

    blpwtk2::WebViewProperties d_properties;

    ScopedHWND d_hwnd;

    bool d_has_parent = false;

    // Manages observation of Windows Session Change messages.
    std::unique_ptr<views::WindowsSessionChangeObserver>
        windows_session_change_observer_;

    bool d_shown = false, d_visible = false;
    gfx::Size d_size;

    std::unique_ptr<RenderCompositor> d_compositor;

    gfx::Point d_mouse_screen_position,
               d_unlocked_mouse_screen_position,
               d_unlocked_mouse_webview_position;

    bool d_nc_hit_test_enabled = false;
    int d_nc_hit_test_result = 0;
    bool d_mouse_entered = false, d_mouse_locked = false;

    // Who knew that cursor-setting would be such a hassle?
    content::WebCursor d_current_cursor;
    std::unique_ptr<ui::CursorLoader> d_cursor_loader;
    bool d_is_cursor_overridden = false;
    HCURSOR d_current_platform_cursor = NULL, d_previous_platform_cursor = NULL;
    bool d_wheel_scroll_latching_enabled = false,
         d_raf_aligned_touch_enabled = false;

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

    std::unique_ptr<content::MouseWheelEventQueue> d_mouseWheelEventQueue;

#if defined(BLPWTK2_FEATURE_RUBBERBAND)
    bool d_enableAltDragRubberBanding = false;
    std::unique_ptr<ui::RubberbandOutline> d_rubberbandOutline;
#endif

    static LPCTSTR GetWindowClass();
    static LRESULT CALLBACK WindowProcedure(HWND   hWnd,
                                            UINT   uMsg,
                                            WPARAM wParam,
                                            LPARAM lParam);
    LRESULT windowProcedure(UINT   uMsg,
                            WPARAM wParam,
                            LPARAM lParam);

    void Init(HWND parent_hwnd, const gfx::Rect& initial_rect);
    // Attempts to force the window to be redrawn, ensuring that it gets
    // onscreen.
    void ForceRedrawWindow(int attempts);
    void OnSessionChange(WPARAM status_code);
    bool dispatchToRenderViewImpl(const IPC::Message& message);
    void OnRenderViewDestruct();
    void updateVisibility();
    void updateSize();
    void updateFocus();
    void setPlatformCursor(HCURSOR cursor);
    void sendScreenRects();
    void dispatchInputEvent(const blink::WebInputEvent& event);

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

    // DragDropDelegate overrides:
    void DragTargetEnter(
        const std::vector<content::DropData::Metadata>& drop_data,
        const gfx::Point& client_pt,
        const gfx::Point& screen_pt,
        blink::WebDragOperationsMask ops_allowed,
        int key_modifiers) override;
    void DragTargetOver(
        const gfx::Point& client_pt,
        const gfx::Point& screen_pt,
        blink::WebDragOperationsMask ops_allowed,
        int key_modifiers) override;
    void DragTargetLeave() override;
    void DragTargetDrop(
        const content::DropData& drop_data,
        const gfx::Point& client_pt,
        const gfx::Point& screen_pt,
        int key_modifiers) override;
    void DragSourceEnded(
        const gfx::Point& client_pt,
        const gfx::Point& screen_pt,
        blink::WebDragOperation drag_operation) override;
    void DragSourceSystemEnded() override;

    // content::MouseWheelEventQueueClient:
    void SendMouseWheelEventImmediately(
        const content::MouseWheelEventWithLatencyInfo& event) override;
    void ForwardGestureEventWithLatencyInfo(
        const blink::WebGestureEvent& event,
        const ui::LatencyInfo& latency_info) override;
    void OnMouseWheelEventAck(
        const content::MouseWheelEventWithLatencyInfo& event,
        content::InputEventAckState ack_result) override;

    // Message handlers
    void OnClose();
    void OnDetach();
    void OnHasTouchEventHandlers(bool has_handlers);
    void OnImeCompositionRangeChanged(
        const gfx::Range& range,
        const std::vector<gfx::Rect>& character_bounds);
    void OnImeCancelComposition();
    void OnInputEventAck(const content::InputEventAck& ack);
    void OnLockMouse(
        bool user_gesture,
        bool privileged);
    void OnSelectionBoundsChanged(
        const ViewHostMsg_SelectionBounds_Params& params);
    void OnSelectionChanged(const base::string16& text,
        uint32_t offset,
        const gfx::Range& range);
    void OnSetCursor(const content::WebCursor& cursor);
    void OnShowWidget(int routing_id, gfx::Rect initial_rect);
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
    void OnUpdateRect(const ViewHostMsg_UpdateRect_Params& params);
#if defined(BLPWTK2_FEATURE_RUBBERBAND)
    void OnHideRubberbandRect();
    void OnSetRubberbandRect(const gfx::Rect& rect);
#endif

    DISALLOW_COPY_AND_ASSIGN(RenderWebView);

  public:
    explicit RenderWebView(WebViewDelegate          *delegate,
                           ProfileImpl              *profile,
                           const WebViewProperties&  properties);
    explicit RenderWebView(HWND                      parent_hwnd,
                           int                       routing_id,
                           const gfx::Rect&          initial_rect);
    ~RenderWebView();

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
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RENDERWEBVIEW_H

// vim: ts=4 et

