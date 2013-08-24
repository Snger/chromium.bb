// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/window_selector_controller.h"

#include "ash/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/overview/window_selector.h"
#include "ash/wm/window_util.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/root_window.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_handler.h"

namespace ash {

namespace {

// Filter to watch for the termination of a keyboard gesture to cycle through
// multiple windows.
class WindowSelectorEventFilter : public ui::EventHandler {
 public:
  WindowSelectorEventFilter();
  virtual ~WindowSelectorEventFilter();

  // Overridden from ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(WindowSelectorEventFilter);
};

// Watch for all keyboard events by filtering the root window.
WindowSelectorEventFilter::WindowSelectorEventFilter() {
  Shell::GetInstance()->AddPreTargetHandler(this);
}

WindowSelectorEventFilter::~WindowSelectorEventFilter() {
  Shell::GetInstance()->RemovePreTargetHandler(this);
}

void WindowSelectorEventFilter::OnKeyEvent(ui::KeyEvent* event) {
  // Views uses VKEY_MENU for both left and right Alt keys.
  if (event->key_code() == ui::VKEY_MENU &&
      event->type() == ui::ET_KEY_RELEASED) {
    Shell::GetInstance()->window_selector_controller()->AltKeyReleased();
    // Warning: |this| will be deleted from here on.
  }
}

}  // namespace

WindowSelectorController::WindowSelectorController()
    : restore_focus_window_(NULL) {
}

WindowSelectorController::~WindowSelectorController() {
}

// static
bool WindowSelectorController::CanSelect() {
  // Don't allow a window overview if the screen is locked or a modal dialog is
  // open.
  return !Shell::GetInstance()->session_state_delegate()->IsScreenLocked() &&
         !Shell::GetInstance()->IsSystemModalWindowOpen();
}

void WindowSelectorController::ToggleOverview() {
  if (IsSelecting()) {
    OnSelectionCanceled();
  } else {
    std::vector<aura::Window*> windows = ash::Shell::GetInstance()->
        mru_window_tracker()->BuildMruWindowList();
    // Don't enter overview mode with no windows.
    if (windows.empty())
      return;

    // Removing focus will hide popup windows like the omnibar or open menus.
    RemoveFocusAndSetRestoreWindow();
    window_selector_.reset(
        new WindowSelector(windows, WindowSelector::OVERVIEW, this));
  }
}

void WindowSelectorController::HandleCycleWindow(
    WindowSelector::Direction direction) {
  if (!CanSelect())
    return;

  if (!IsSelecting()) {
    event_handler_.reset(new WindowSelectorEventFilter());
    std::vector<aura::Window*> windows = ash::Shell::GetInstance()->
        mru_window_tracker()->BuildMruWindowList();
    // Removing focus will hide popup windows like the omnibar or open menus.
    RemoveFocusAndSetRestoreWindow();
    window_selector_.reset(
        new WindowSelector(windows, WindowSelector::CYCLE, this));
    window_selector_->Step(direction);
  } else if (window_selector_->mode() == WindowSelector::CYCLE) {
    window_selector_->Step(direction);
  }
}

void WindowSelectorController::AltKeyReleased() {
  event_handler_.reset();
  window_selector_->SelectWindow();
}

bool WindowSelectorController::IsSelecting() {
  return window_selector_.get() != NULL;
}

void WindowSelectorController::OnWindowSelected(aura::Window* window) {
  window_selector_.reset();
  ResetFocusRestoreWindow(false);
  wm::ActivateWindow(window);
}

void WindowSelectorController::OnSelectionCanceled() {
  window_selector_.reset();
  ResetFocusRestoreWindow(true);
}

void WindowSelectorController::OnWindowDestroyed(aura::Window* window) {
  DCHECK_EQ(window, restore_focus_window_);
  restore_focus_window_->RemoveObserver(this);
  restore_focus_window_ = NULL;
}

void WindowSelectorController::RemoveFocusAndSetRestoreWindow() {
  aura::client::FocusClient* focus_client = aura::client::GetFocusClient(
      Shell::GetActiveRootWindow());
  DCHECK(!restore_focus_window_);
  restore_focus_window_ = focus_client->GetFocusedWindow();
  if (restore_focus_window_) {
    focus_client->FocusWindow(NULL);
    restore_focus_window_->AddObserver(this);
  }
}

void WindowSelectorController::ResetFocusRestoreWindow(bool focus) {
  if (!restore_focus_window_)
    return;
  if (focus)
    restore_focus_window_->Focus();
  restore_focus_window_->RemoveObserver(this);
  restore_focus_window_ = NULL;
}

}  // namespace ash
