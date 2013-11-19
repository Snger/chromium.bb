// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/remote_root_window_host_win.h"

#include <windows.h>

#include <algorithm>

#include "base/message_loop/message_loop.h"
#include "ipc/ipc_message.h"
#include "ipc/ipc_sender.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/root_window.h"
#include "ui/base/cursor/cursor_loader_win.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_win.h"
#include "ui/base/view_prop.h"
#include "ui/gfx/insets.h"
#include "ui/metro_viewer/metro_viewer_messages.h"

namespace aura {

namespace {

const char* kWindowTreeHostWinKey = "__AURA_REMOTE_ROOT_WINDOW_HOST_WIN__";

// Sets the keystate for the virtual key passed in to down or up.
void SetKeyState(uint8* key_states, bool key_down, uint32 virtual_key_code) {
  DCHECK(key_states);

  if (key_down)
    key_states[virtual_key_code] |= 0x80;
  else
    key_states[virtual_key_code] &= 0x7F;
}

// Sets the keyboard states for the Shift/Control/Alt/Caps lock keys.
void SetVirtualKeyStates(uint32 flags) {
  uint8 keyboard_state[256] = {0};
  ::GetKeyboardState(keyboard_state);

  SetKeyState(keyboard_state, !!(flags & ui::EF_SHIFT_DOWN), VK_SHIFT);
  SetKeyState(keyboard_state, !!(flags & ui::EF_CONTROL_DOWN), VK_CONTROL);
  SetKeyState(keyboard_state, !!(flags & ui::EF_ALT_DOWN), VK_MENU);
  SetKeyState(keyboard_state, !!(flags & ui::EF_CAPS_LOCK_DOWN), VK_CAPITAL);
  SetKeyState(keyboard_state, !!(flags & ui::EF_LEFT_MOUSE_BUTTON), VK_LBUTTON);
  SetKeyState(keyboard_state, !!(flags & ui::EF_RIGHT_MOUSE_BUTTON),
              VK_RBUTTON);
  SetKeyState(keyboard_state, !!(flags & ui::EF_MIDDLE_MOUSE_BUTTON),
              VK_MBUTTON);

  ::SetKeyboardState(keyboard_state);
}

}  // namespace

void HandleOpenFile(const base::string16& title,
                    const base::FilePath& default_path,
                    const base::string16& filter,
                    const OpenFileCompletion& on_success,
                    const FileSelectionCanceled& on_failure) {
  DCHECK(aura::RemoteWindowTreeHostWin::Instance());
  aura::RemoteWindowTreeHostWin::Instance()->HandleOpenFile(title,
                                                            default_path,
                                                            filter,
                                                            on_success,
                                                            on_failure);
}

void HandleOpenMultipleFiles(const base::string16& title,
                             const base::FilePath& default_path,
                             const base::string16& filter,
                             const OpenMultipleFilesCompletion& on_success,
                             const FileSelectionCanceled& on_failure) {
  DCHECK(aura::RemoteWindowTreeHostWin::Instance());
  aura::RemoteWindowTreeHostWin::Instance()->HandleOpenMultipleFiles(
      title,
      default_path,
      filter,
      on_success,
      on_failure);
}

void HandleSaveFile(const base::string16& title,
                    const base::FilePath& default_path,
                    const base::string16& filter,
                    int filter_index,
                    const base::string16& default_extension,
                    const SaveFileCompletion& on_success,
                    const FileSelectionCanceled& on_failure) {
  DCHECK(aura::RemoteWindowTreeHostWin::Instance());
  aura::RemoteWindowTreeHostWin::Instance()->HandleSaveFile(title,
                                                            default_path,
                                                            filter,
                                                            filter_index,
                                                            default_extension,
                                                            on_success,
                                                            on_failure);
}

void HandleSelectFolder(const base::string16& title,
                        const SelectFolderCompletion& on_success,
                        const FileSelectionCanceled& on_failure) {
  DCHECK(aura::RemoteWindowTreeHostWin::Instance());
  aura::RemoteWindowTreeHostWin::Instance()->HandleSelectFolder(title,
                                                                on_success,
                                                                on_failure);
}

void HandleActivateDesktop(const base::FilePath& shortcut,
                           const ActivateDesktopCompleted& on_success) {
  DCHECK(aura::RemoteWindowTreeHostWin::Instance());
  aura::RemoteWindowTreeHostWin::Instance()->HandleActivateDesktop(shortcut,
                                                                   on_success);
}

RemoteWindowTreeHostWin* g_instance = NULL;

RemoteWindowTreeHostWin* RemoteWindowTreeHostWin::Instance() {
  if (g_instance)
    return g_instance;
  return Create(gfx::Rect());
}

RemoteWindowTreeHostWin* RemoteWindowTreeHostWin::Create(
    const gfx::Rect& bounds) {
  g_instance = g_instance ? g_instance : new RemoteWindowTreeHostWin(bounds);
  return g_instance;
}

RemoteWindowTreeHostWin::RemoteWindowTreeHostWin(const gfx::Rect& bounds)
    : remote_window_(NULL),
      host_(NULL),
      ignore_mouse_moves_until_set_cursor_ack_(false),
      event_flags_(0) {
  prop_.reset(new ui::ViewProp(NULL, kWindowTreeHostWinKey, this));
  CreateCompositor(GetAcceleratedWidget());
}

RemoteWindowTreeHostWin::~RemoteWindowTreeHostWin() {
  g_instance = NULL;
}

void RemoteWindowTreeHostWin::Connected(IPC::Sender* host, HWND remote_window) {
  CHECK(host_ == NULL);
  host_ = host;
  remote_window_ = remote_window;
}

void RemoteWindowTreeHostWin::Disconnected() {
  // Don't CHECK here, Disconnected is called on a channel error which can
  // happen before we're successfully Connected.
  host_ = NULL;
  remote_window_ = NULL;
}

bool RemoteWindowTreeHostWin::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(RemoteWindowTreeHostWin, message)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_MouseMoved, OnMouseMoved)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_MouseButton, OnMouseButton)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_KeyDown, OnKeyDown)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_KeyUp, OnKeyUp)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_Character, OnChar)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_WindowActivated,
                        OnWindowActivated)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_TouchDown,
                        OnTouchDown)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_TouchUp,
                        OnTouchUp)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_TouchMoved,
                        OnTouchMoved)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_FileSaveAsDone,
                        OnFileSaveAsDone)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_FileOpenDone,
                        OnFileOpenDone)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_MultiFileOpenDone,
                        OnMultiFileOpenDone)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_SelectFolderDone,
                        OnSelectFolderDone)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_SetCursorPosAck,
                        OnSetCursorPosAck)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_WindowSizeChanged,
                        OnWindowSizeChanged)
    IPC_MESSAGE_HANDLER(MetroViewerHostMsg_ActivateDesktopDone,
                        OnDesktopActivated)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void RemoteWindowTreeHostWin::HandleOpenURLOnDesktop(
    const base::FilePath& shortcut,
    const base::string16& url) {
  if (!host_)
    return;
  host_->Send(new MetroViewerHostMsg_OpenURLOnDesktop(shortcut, url));
}

void RemoteWindowTreeHostWin::HandleActivateDesktop(
    const base::FilePath& shortcut,
    const ActivateDesktopCompleted& on_success) {
  if (!host_)
    return;
  DCHECK(activate_completed_callback_.is_null());
  activate_completed_callback_ = on_success;
  host_->Send(new MetroViewerHostMsg_ActivateDesktop(shortcut));
}

void RemoteWindowTreeHostWin::HandleOpenFile(
    const base::string16& title,
    const base::FilePath& default_path,
    const base::string16& filter,
    const OpenFileCompletion& on_success,
    const FileSelectionCanceled& on_failure) {
  if (!host_)
    return;

  // Can only have one of these operations in flight.
  DCHECK(file_open_completion_callback_.is_null());
  DCHECK(failure_callback_.is_null());

  file_open_completion_callback_ = on_success;
  failure_callback_ = on_failure;

  host_->Send(new MetroViewerHostMsg_DisplayFileOpen(title,
                                                     filter,
                                                     default_path,
                                                     false));
}

void RemoteWindowTreeHostWin::HandleOpenMultipleFiles(
    const base::string16& title,
    const base::FilePath& default_path,
    const base::string16& filter,
    const OpenMultipleFilesCompletion& on_success,
    const FileSelectionCanceled& on_failure) {
  if (!host_)
    return;

  // Can only have one of these operations in flight.
  DCHECK(multi_file_open_completion_callback_.is_null());
  DCHECK(failure_callback_.is_null());
  multi_file_open_completion_callback_ = on_success;
  failure_callback_ = on_failure;

  host_->Send(new MetroViewerHostMsg_DisplayFileOpen(title,
                                                     filter,
                                                     default_path,
                                                     true));
}

void RemoteWindowTreeHostWin::HandleSaveFile(
    const base::string16& title,
    const base::FilePath& default_path,
    const base::string16& filter,
    int filter_index,
    const base::string16& default_extension,
    const SaveFileCompletion& on_success,
    const FileSelectionCanceled& on_failure) {
  if (!host_)
    return;

  MetroViewerHostMsg_SaveAsDialogParams params;
  params.title = title;
  params.default_extension = default_extension;
  params.filter = filter;
  params.filter_index = filter_index;
  params.suggested_name = default_path;

  // Can only have one of these operations in flight.
  DCHECK(file_saveas_completion_callback_.is_null());
  DCHECK(failure_callback_.is_null());
  file_saveas_completion_callback_ = on_success;
  failure_callback_ = on_failure;

  host_->Send(new MetroViewerHostMsg_DisplayFileSaveAs(params));
}

void RemoteWindowTreeHostWin::HandleSelectFolder(
    const base::string16& title,
    const SelectFolderCompletion& on_success,
    const FileSelectionCanceled& on_failure) {
  if (!host_)
    return;

  // Can only have one of these operations in flight.
  DCHECK(select_folder_completion_callback_.is_null());
  DCHECK(failure_callback_.is_null());
  select_folder_completion_callback_ = on_success;
  failure_callback_ = on_failure;

  host_->Send(new MetroViewerHostMsg_DisplaySelectFolder(title));
}

bool RemoteWindowTreeHostWin::IsForegroundWindow() {
  return ::GetForegroundWindow() == remote_window_;
}

Window* RemoteWindowTreeHostWin::GetAshWindow() {
  return GetRootWindow()->window();
}

RootWindow* RemoteWindowTreeHostWin::GetRootWindow() {
  return delegate_->AsRootWindow();
}

gfx::AcceleratedWidget RemoteWindowTreeHostWin::GetAcceleratedWidget() {
  if (remote_window_)
    return remote_window_;
  // Getting here should only happen for ash_unittests.exe and related code.
  return ::GetDesktopWindow();
}

void RemoteWindowTreeHostWin::Show() {
}

void RemoteWindowTreeHostWin::Hide() {
  NOTIMPLEMENTED();
}

void RemoteWindowTreeHostWin::ToggleFullScreen() {
}

gfx::Rect RemoteWindowTreeHostWin::GetBounds() const {
  gfx::Rect r(gfx::Point(0, 0), aura::WindowTreeHost::GetNativeScreenSize());
  return r;
}

void RemoteWindowTreeHostWin::SetBounds(const gfx::Rect& bounds) {
  NotifyHostResized(bounds.size());
}

gfx::Insets RemoteWindowTreeHostWin::GetInsets() const {
  return gfx::Insets();
}

void RemoteWindowTreeHostWin::SetInsets(const gfx::Insets& insets) {
}

gfx::Point RemoteWindowTreeHostWin::GetLocationOnNativeScreen() const {
  return gfx::Point(0, 0);
}

void RemoteWindowTreeHostWin::SetCursor(gfx::NativeCursor native_cursor) {
  if (!host_)
    return;
  host_->Send(
      new MetroViewerHostMsg_SetCursor(uint64(native_cursor.platform())));
}

void RemoteWindowTreeHostWin::SetCapture() {
}

void RemoteWindowTreeHostWin::ReleaseCapture() {
}

bool RemoteWindowTreeHostWin::QueryMouseLocation(gfx::Point* location_return) {
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(GetRootWindow()->window());
  if (cursor_client && !cursor_client->IsMouseEventsEnabled()) {
    *location_return = gfx::Point(0, 0);
    return false;
  }
  POINT pt;
  GetCursorPos(&pt);
  *location_return =
      gfx::Point(static_cast<int>(pt.x), static_cast<int>(pt.y));
  return true;
}

bool RemoteWindowTreeHostWin::ConfineCursorToRootWindow() {
  return true;
}

void RemoteWindowTreeHostWin::UnConfineCursor() {
}

void RemoteWindowTreeHostWin::OnCursorVisibilityChanged(bool show) {
  NOTIMPLEMENTED();
}

void RemoteWindowTreeHostWin::MoveCursorTo(const gfx::Point& location) {
  VLOG(1) << "In MoveCursorTo: " << location.x() << ", " << location.y();
  if (!host_)
    return;

  // This function can be called in cases like when the mouse cursor is
  // restricted within a viewport (For e.g. LockCursor) which assumes that
  // subsequent mouse moves would be received starting with the new cursor
  // coordinates. This is a challenge for Windows ASH for the reasons
  // outlined below.
  // Other cases which don't expect this behavior should continue to work
  // without issues.

  // The mouse events are received by the viewer process and sent to the
  // browser. If we invoke the SetCursor API here we continue to receive
  // mouse messages from the viewer which were posted before the SetCursor
  // API executes which messes up the state in the browser. To workaround
  // this we invoke the SetCursor API in the viewer process and ignore
  // mouse messages until we received an ACK from the viewer indicating that
  // the SetCursor operation completed.
  ignore_mouse_moves_until_set_cursor_ack_ = true;
  VLOG(1) << "In MoveCursorTo. Sending IPC";
  host_->Send(new MetroViewerHostMsg_SetCursorPos(location.x(), location.y()));
}

void RemoteWindowTreeHostWin::PostNativeEvent(
    const base::NativeEvent& native_event) {
}

void RemoteWindowTreeHostWin::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
  NOTIMPLEMENTED();
}

void RemoteWindowTreeHostWin::PrepareForShutdown() {
}

void RemoteWindowTreeHostWin::OnMouseMoved(int32 x, int32 y, int32 flags) {
  if (ignore_mouse_moves_until_set_cursor_ack_)
    return;

  gfx::Point location(x, y);
  ui::MouseEvent event(ui::ET_MOUSE_MOVED, location, location, flags);
  delegate_->OnHostMouseEvent(&event);
}

void RemoteWindowTreeHostWin::OnMouseButton(
    int32 x,
    int32 y,
    int32 extra,
    ui::EventType type,
    ui::EventFlags flags) {
  gfx::Point location(x, y);
  ui::MouseEvent mouse_event(type, location, location, flags);

  SetEventFlags(flags | key_event_flags());
  if (type == ui::ET_MOUSEWHEEL) {
    ui::MouseWheelEvent wheel_event(mouse_event, 0, extra);
    delegate_->OnHostMouseEvent(&wheel_event);
  } else if (type == ui::ET_MOUSE_PRESSED) {
    // TODO(shrikant): Ideally modify code in event.cc by adding automatic
    // tracking of double clicks in synthetic MouseEvent constructor code.
    // Non-synthetic MouseEvent constructor code does automatically track
    // this. Need to use some caution while modifying synthetic constructor
    // as many tests and other code paths depend on it and apparently
    // specifically depend on non implicit tracking of previous mouse event.
    if (last_mouse_click_event_ &&
        ui::MouseEvent::IsRepeatedClickEvent(mouse_event,
                                             *last_mouse_click_event_)) {
      mouse_event.SetClickCount(2);
    } else {
      mouse_event.SetClickCount(1);
    }
    last_mouse_click_event_ .reset(new ui::MouseEvent(mouse_event));
    delegate_->OnHostMouseEvent(&mouse_event);
  } else {
    delegate_->OnHostMouseEvent(&mouse_event);
  }
}

void RemoteWindowTreeHostWin::OnKeyDown(uint32 vkey,
                                        uint32 repeat_count,
                                        uint32 scan_code,
                                        uint32 flags) {
  DispatchKeyboardMessage(ui::ET_KEY_PRESSED, vkey, repeat_count, scan_code,
                          flags, false);
}

void RemoteWindowTreeHostWin::OnKeyUp(uint32 vkey,
                                      uint32 repeat_count,
                                      uint32 scan_code,
                                      uint32 flags) {
  DispatchKeyboardMessage(ui::ET_KEY_RELEASED, vkey, repeat_count, scan_code,
                          flags, false);
}

void RemoteWindowTreeHostWin::OnChar(uint32 key_code,
                                     uint32 repeat_count,
                                     uint32 scan_code,
                                     uint32 flags) {
  DispatchKeyboardMessage(ui::ET_KEY_PRESSED, key_code, repeat_count,
                          scan_code, flags, true);
}

void RemoteWindowTreeHostWin::OnWindowActivated() {
  delegate_->OnHostActivated();
}

void RemoteWindowTreeHostWin::OnTouchDown(int32 x,
                                          int32 y,
                                          uint64 timestamp,
                                          uint32 pointer_id) {
  ui::TouchEvent event(ui::ET_TOUCH_PRESSED,
                       gfx::Point(x, y),
                       pointer_id,
                       base::TimeDelta::FromMicroseconds(timestamp));
  delegate_->OnHostTouchEvent(&event);
}

void RemoteWindowTreeHostWin::OnTouchUp(int32 x,
                                        int32 y,
                                        uint64 timestamp,
                                        uint32 pointer_id) {
  ui::TouchEvent event(ui::ET_TOUCH_RELEASED,
                       gfx::Point(x, y),
                       pointer_id,
                       base::TimeDelta::FromMicroseconds(timestamp));
  delegate_->OnHostTouchEvent(&event);
}

void RemoteWindowTreeHostWin::OnTouchMoved(int32 x,
                                           int32 y,
                                           uint64 timestamp,
                                           uint32 pointer_id) {
  ui::TouchEvent event(ui::ET_TOUCH_MOVED,
                       gfx::Point(x, y),
                       pointer_id,
                       base::TimeDelta::FromMicroseconds(timestamp));
  delegate_->OnHostTouchEvent(&event);
}

void RemoteWindowTreeHostWin::OnFileSaveAsDone(bool success,
                                               const base::FilePath& filename,
                                               int filter_index) {
  if (success)
    file_saveas_completion_callback_.Run(filename, filter_index, NULL);
  else
    failure_callback_.Run(NULL);
  file_saveas_completion_callback_.Reset();
  failure_callback_.Reset();
}


void RemoteWindowTreeHostWin::OnFileOpenDone(bool success,
                                             const base::FilePath& filename) {
  if (success)
    file_open_completion_callback_.Run(base::FilePath(filename), 0, NULL);
  else
    failure_callback_.Run(NULL);
  file_open_completion_callback_.Reset();
  failure_callback_.Reset();
}

void RemoteWindowTreeHostWin::OnMultiFileOpenDone(
    bool success,
    const std::vector<base::FilePath>& files) {
  if (success)
    multi_file_open_completion_callback_.Run(files, NULL);
  else
    failure_callback_.Run(NULL);
  multi_file_open_completion_callback_.Reset();
  failure_callback_.Reset();
}

void RemoteWindowTreeHostWin::OnSelectFolderDone(
    bool success,
    const base::FilePath& folder) {
  if (success)
    select_folder_completion_callback_.Run(base::FilePath(folder), 0, NULL);
  else
    failure_callback_.Run(NULL);
  select_folder_completion_callback_.Reset();
  failure_callback_.Reset();
}

void RemoteWindowTreeHostWin::OnSetCursorPosAck() {
  DCHECK(ignore_mouse_moves_until_set_cursor_ack_);
  ignore_mouse_moves_until_set_cursor_ack_ = false;
}

void RemoteWindowTreeHostWin::OnWindowSizeChanged(uint32 width, uint32 height) {
  SetBounds(gfx::Rect(0, 0, width, height));
}

void RemoteWindowTreeHostWin::OnDesktopActivated() {
  ActivateDesktopCompleted temp = activate_completed_callback_;
  activate_completed_callback_.Reset();
  temp.Run();
}

void RemoteWindowTreeHostWin::DispatchKeyboardMessage(ui::EventType type,
                                                      uint32 vkey,
                                                      uint32 repeat_count,
                                                      uint32 scan_code,
                                                      uint32 flags,
                                                      bool is_character) {
  SetEventFlags(flags | mouse_event_flags());
  if (base::MessageLoop::current()->IsNested()) {
    uint32 message = is_character ? WM_CHAR :
        (type == ui::ET_KEY_PRESSED ? WM_KEYDOWN : WM_KEYUP);
    ::PostThreadMessage(::GetCurrentThreadId(),
                        message,
                        vkey,
                        repeat_count | scan_code >> 15);
  } else {
    ui::KeyEvent event(type,
                       ui::KeyboardCodeForWindowsKeyCode(vkey),
                       flags,
                       is_character);
    delegate_->OnHostKeyEvent(&event);
  }
}

void RemoteWindowTreeHostWin::SetEventFlags(uint32 flags) {
  if (flags == event_flags_)
    return;
  event_flags_ = flags;
  SetVirtualKeyStates(event_flags_);
}

}  // namespace aura
