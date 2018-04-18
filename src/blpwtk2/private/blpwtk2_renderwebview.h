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

#include <blpwtk2_scopedhwnd.h>
#include <blpwtk2_webview.h>
#include <blpwtk2_webviewclientdelegate.h>
#include <blpwtk2_webviewproperties.h>

#include <content/common/text_input_state.h>
#include <content/common/cursors/webcursor.h>
#include <content/public/renderer/render_view_observer.h>
#include <ipc/ipc_listener.h>
#include <ui/base/ime/input_method_delegate.h>
#include <ui/base/ime/text_input_client.h>
#include <ui/gfx/selection_bound.h>
#include <ui/gfx/geometry/point.h>
#include <ui/gfx/geometry/size.h>
#include <ui/views/win/windows_session_change_observer.h>

struct ViewHostMsg_SelectionBounds_Params;

namespace blink {
class WebInputEvent;
} // close namespace blink

namespace content {
struct InputEventAck;
struct TextInputState;
class WebCursor;
}  // close namespace content

namespace gfx {
class Point;
}  // close namespace gfx

namespace ui {
class CursorLoader;
class InputMethod;
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
{
    class RenderViewObserver : public content::RenderViewObserver {
      private:

        RenderWebView *d_renderWebView;

      public:

        RenderViewObserver(
            content::RenderView *renderView, RenderWebView *renderWebView);

        void OnDestruct() override;

        // IPC::Listener implementation.
        bool OnMessageReceived(const IPC::Message& message) override;
    };

    // DATA
    WebViewClient *d_client;
    WebViewDelegate *d_delegate;
#if defined(BLPWTK2_FEATURE_FOCUS) ||
    defined(BLPWTK2_FEATURE_REROUTEMOUSEWHEEL)
    WebViewProperties d_properties;
#endif

    ProfileImpl *d_profile;
    int d_renderViewRoutingId, d_mainFrameRoutingId;
    bool d_gotRenderViewInfo;
    bool d_pendingLoadStatus;
    bool d_isMainFrameAccessible;
    bool d_pendingDestroy;
    std::string d_url;
    std::unique_ptr<WebFrameImpl> d_mainFrame;

    ScopedHWND d_hwnd;

    bool d_has_parent = false;

    // Manages observation of Windows Session Change messages.
    std::unique_ptr<views::WindowsSessionChangeObserver>
        windows_session_change_observer_;

    bool d_shown = false, d_visible = false;
    gfx::Size d_size;
    bool d_dispatchingResize = false, d_ignoreResizeACK = false;

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
    bool d_wheel_scroll_latching_enabled = false;

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

    static LPCTSTR GetWindowClass();
    static LRESULT CALLBACK WindowProcedure(HWND   hWnd,
                                            UINT   uMsg,
                                            WPARAM wParam,
                                            LPARAM lParam);
    LRESULT windowProcedure(UINT   uMsg,
                            WPARAM wParam,
                            LPARAM lParam);

    // Attempts to force the window to be redrawn, ensuring that it gets
    // onscreen.
    void ForceRedrawWindow(int attempts);
    void OnSessionChange(WPARAM status_code);
    bool dispatchToRenderViewImpl(const IPC::Message& message);
    void OnRenderViewDestruct();
    bool OnRenderViewResize();
    void updateVisibility();
    void updateSize();
    void updateFocus();
    void setPlatformCursor(HCURSOR cursor);
    void dispatchInputEvent(const blink::WebInputEvent& event);

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
    void find(const StringRef& text, bool matchCase, bool forward) override;
    void stopFind(bool preserveSelection) override;
    void replaceMisspelledRange(const StringRef& text) override;
    void rootWindowPositionChanged() override;
    void rootWindowSettingsChanged() override;

    void handleInputEvents(const InputEvent *events,
                           size_t            eventsCount) override;
    void setDelegate(WebViewDelegate *delegate) override;
    int getRoutingId() const override;
    void setBackgroundColor(NativeColor color) override;
    void setRegion(NativeRegion region) override;
    void clearTooltip() override;
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
    const std::string& GetClientSourceInfo() const override;

    // Message handlers
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
    void OnTextInputStateChanged(const content::TextInputState& params);
    void OnUnlockMouse();
    void OnDetach();
    bool OnResizeOrRepaintACK();

    DISALLOW_COPY_AND_ASSIGN(RenderWebView);

  public:
    explicit RenderWebView(WebViewDelegate          *delegate,
                           ProfileImpl              *profile,
                           const WebViewProperties&  properties);
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
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_RENDERWEBVIEW_H

// vim: ts=4 et

