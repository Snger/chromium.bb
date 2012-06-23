// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/monitor/monitor_controller.h"

#include "ash/ash_switches.h"
#include "ash/monitor/multi_monitor_manager.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {
namespace {
// True if the extended desktop mode is enabled.
bool extended_desktop_enabled = false;

// True if the virtual screen coordinates is enabled.
bool virtual_screen_coordinates_enabled = false;
}

MonitorController::MonitorController()
    : secondary_display_layout_(RIGHT) {
  aura::Env::GetInstance()->monitor_manager()->AddObserver(this);
}

MonitorController::~MonitorController() {
  aura::Env::GetInstance()->monitor_manager()->RemoveObserver(this);
  // Delete all root window controllers, which deletes root window
  // from the last so that the primary root window gets deleted last.
  for (std::map<int, aura::RootWindow*>::const_reverse_iterator it =
           root_windows_.rbegin(); it != root_windows_.rend(); ++it) {
    internal::RootWindowController* controller =
        wm::GetRootWindowController(it->second);
    // RootWindow may not have RootWindowController in non
    // extended desktop mode.
    if (controller)
      delete controller;
    else
      delete it->second;
  }
}

void MonitorController::InitPrimaryDisplay() {
  aura::MonitorManager* monitor_manager =
      aura::Env::GetInstance()->monitor_manager();
  const gfx::Display& display = monitor_manager->GetDisplayAt(0);
  DCHECK_EQ(0, display.id());
  aura::RootWindow* root = AddRootWindowForDisplay(display);
  root->SetHostBounds(display.bounds_in_pixel());
}

void MonitorController::InitSecondaryDisplays() {
  aura::MonitorManager* monitor_manager =
      aura::Env::GetInstance()->monitor_manager();
  for (size_t i = 1; i < monitor_manager->GetNumDisplays(); ++i) {
    const gfx::Display& display = monitor_manager->GetDisplayAt(i);
    aura::RootWindow* root = AddRootWindowForDisplay(display);
    Shell::GetInstance()->InitRootWindowForSecondaryMonitor(root);
  }
}

aura::RootWindow* MonitorController::GetPrimaryRootWindow() {
  DCHECK(!root_windows_.empty());
  return root_windows_[0];
}

void MonitorController::CloseChildWindows() {
  for (std::map<int, aura::RootWindow*>::const_iterator it =
           root_windows_.begin(); it != root_windows_.end(); ++it) {
    aura::RootWindow* root_window = it->second;
    internal::RootWindowController* controller =
        wm::GetRootWindowController(root_window);
    if (controller) {
      controller->CloseChildWindows();
    } else {
      while (!root_window->children().empty()) {
        aura::Window* child = root_window->children()[0];
        delete child;
      }
    }
  }
}

std::vector<aura::RootWindow*> MonitorController::GetAllRootWindows() {
  std::vector<aura::RootWindow*> windows;
  for (std::map<int, aura::RootWindow*>::const_iterator it =
           root_windows_.begin(); it != root_windows_.end(); ++it) {
    DCHECK(it->second);
    if (wm::GetRootWindowController(it->second))
      windows.push_back(it->second);
  }
  return windows;
}

std::vector<internal::RootWindowController*>
MonitorController::GetAllRootWindowControllers() {
  std::vector<internal::RootWindowController*> controllers;
  for (std::map<int, aura::RootWindow*>::const_iterator it =
           root_windows_.begin(); it != root_windows_.end(); ++it) {
    internal::RootWindowController* controller =
        wm::GetRootWindowController(it->second);
    if (controller)
      controllers.push_back(controller);
  }
  return controllers;
}

void MonitorController::SetSecondaryDisplayLayout(
    SecondaryDisplayLayout layout) {
  secondary_display_layout_ = layout;
}

bool MonitorController::WarpMouseCursorIfNecessary(
    aura::Window* current_root,
    const gfx::Point& location_in_root) {
  if (root_windows_.size() < 2)
    return false;
  // Only 1 external display is supported in extended desktop mode.
  DCHECK_EQ(2U, root_windows_.size());

  bool in_primary = current_root == root_windows_[0];

  std::map<int, aura::RootWindow*>::iterator iter = root_windows_.begin();
  aura::RootWindow* alternate_root = iter->second != current_root ?
      iter->second : (++iter)->second;
  gfx::Rect alternate_bounds = alternate_root->bounds();
  gfx::Point alternate_point;

  gfx::Rect display_area(
      gfx::Screen::GetDisplayNearestWindow(current_root).bounds());

  // TODO(oshima): This is temporary code until the virtual screen
  // coordinate is implemented.
  if (location_in_root.x() <= display_area.x()) {
    if (location_in_root.y() < alternate_bounds.height() &&
        ((in_primary && secondary_display_layout_ == LEFT) ||
         (!in_primary && secondary_display_layout_ == RIGHT))) {
      alternate_point = gfx::Point(
          alternate_bounds.right() - (location_in_root.x() - display_area.x()),
          location_in_root.y());
    } else {
      alternate_root = NULL;
    }
  } else if (location_in_root.x() >= display_area.right() - 1) {
    if (location_in_root.y() < alternate_bounds.height() &&
        ((in_primary && secondary_display_layout_ == RIGHT) ||
         (!in_primary && secondary_display_layout_ == LEFT))) {
      alternate_point = gfx::Point(location_in_root.x() - display_area.right(),
                                   location_in_root.y());
    } else {
      alternate_root = NULL;
    }
  } else if (location_in_root.y() < display_area.y()) {
    if (location_in_root.x() < alternate_bounds.width() &&
        ((in_primary && secondary_display_layout_ == TOP) ||
         (!in_primary && secondary_display_layout_ == BOTTOM))) {
      alternate_point = gfx::Point(
          location_in_root.x(),
          alternate_bounds.bottom() -
          (location_in_root.y() - display_area.y()));
    } else {
      alternate_root = NULL;
    }
  } else if (location_in_root.y() >= display_area.bottom() - 1) {
    if (location_in_root.x() < alternate_bounds.width() &&
        ((in_primary && secondary_display_layout_ == BOTTOM) ||
         (!in_primary && secondary_display_layout_ == TOP))) {
      alternate_point = gfx::Point(
          location_in_root.x(), location_in_root.y() - display_area.bottom());
    } else {
      alternate_root = NULL;
    }
  } else {
    alternate_root = NULL;
  }
  if (alternate_root) {
    DCHECK_NE(alternate_root, current_root);
    alternate_root->MoveCursorTo(alternate_point);
    return true;
  }
  return false;
}

void MonitorController::OnDisplayBoundsChanged(const gfx::Display& display) {
  root_windows_[display.id()]->SetHostBounds(display.bounds_in_pixel());
}

void MonitorController::OnDisplayAdded(const gfx::Display& display) {
  if (root_windows_.empty()) {
    DCHECK_EQ(0, display.id());
    root_windows_[display.id()] = Shell::GetPrimaryRootWindow();
    Shell::GetPrimaryRootWindow()->SetHostBounds(display.bounds_in_pixel());
    return;
  }
  aura::RootWindow* root = AddRootWindowForDisplay(display);
  Shell::GetInstance()->InitRootWindowForSecondaryMonitor(root);
}

void MonitorController::OnDisplayRemoved(const gfx::Display& display) {
  aura::RootWindow* root = root_windows_[display.id()];
  DCHECK(root);
  // Primary monitor should never be removed by MonitorManager.
  DCHECK(root != Shell::GetPrimaryRootWindow());
  // Monitor for root window will be deleted when the Primary RootWindow
  // is deleted by the Shell.
  if (root != Shell::GetPrimaryRootWindow()) {
    root_windows_.erase(display.id());
    internal::RootWindowController* controller =
        wm::GetRootWindowController(root);
    if (controller)
      delete controller;
    else
      delete root;
  }
}

// static
bool MonitorController::IsExtendedDesktopEnabled(){
  return extended_desktop_enabled ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshExtendedDesktop);
}

// static
void MonitorController::SetExtendedDesktopEnabled(bool enabled) {
  extended_desktop_enabled = enabled;
}

// static
bool MonitorController::IsVirtualScreenCoordinatesEnabled() {
  return virtual_screen_coordinates_enabled ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshVirtualScreenCoordinates);
}

// static
void MonitorController::SetVirtualScreenCoordinatesEnabled(bool enabled) {
  virtual_screen_coordinates_enabled = enabled;
}

aura::RootWindow* MonitorController::AddRootWindowForDisplay(
    const gfx::Display& display) {
  aura::RootWindow* root = aura::Env::GetInstance()->monitor_manager()->
      CreateRootWindowForMonitor(display);
  root_windows_[display.id()] = root;
  // Confine the cursor within the window if
  // 1) Extended desktop is enabled or
  // 2) the display is primary monitor and the host window
  // is set to be fullscreen (this is old behavior).
  if (IsExtendedDesktopEnabled() ||
      (aura::MonitorManager::use_fullscreen_host_window() &&
       display.id() == 0)) {
    root->ConfineCursorToWindow();
  }
  return root;
}

}  // namespace internal
}  // namespace ash
