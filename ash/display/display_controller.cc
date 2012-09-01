// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/display/display_controller.h"

#include <algorithm>

#include "ash/ash_switches.h"
#include "ash/display/multi_display_manager.h"
#include "ash/root_window_controller.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/compositor/dip_util.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#endif

namespace ash {
namespace internal {
namespace {

// The number of pixels to overlap between the primary and secondary displays,
// in case that the offset value is too large.
const int kMinimumOverlapForInvalidOffset = 50;

}

DisplayController::DisplayController()
    : secondary_display_layout_(RIGHT),
      secondary_display_offset_(0) {
  aura::Env::GetInstance()->display_manager()->AddObserver(this);
}

DisplayController::~DisplayController() {
  aura::Env::GetInstance()->display_manager()->RemoveObserver(this);
  // Delete all root window controllers, which deletes root window
  // from the last so that the primary root window gets deleted last.
  for (std::map<int64, aura::RootWindow*>::const_reverse_iterator it =
           root_windows_.rbegin(); it != root_windows_.rend(); ++it) {
    internal::RootWindowController* controller =
        GetRootWindowController(it->second);
    DCHECK(controller);
    delete controller;
  }
}

void DisplayController::InitPrimaryDisplay() {
  aura::DisplayManager* display_manager =
      aura::Env::GetInstance()->display_manager();
  const gfx::Display* display = display_manager->GetDisplayAt(0);
  aura::RootWindow* root = AddRootWindowForDisplay(*display);
  root->SetHostBounds(display->bounds_in_pixel());
}

void DisplayController::InitSecondaryDisplays() {
  aura::DisplayManager* display_manager =
      aura::Env::GetInstance()->display_manager();
  for (size_t i = 1; i < display_manager->GetNumDisplays(); ++i) {
    const gfx::Display* display = display_manager->GetDisplayAt(i);
    aura::RootWindow* root = AddRootWindowForDisplay(*display);
    Shell::GetInstance()->InitRootWindowForSecondaryDisplay(root);
  }
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kAshSecondaryDisplayLayout)) {
    std::string value = command_line->GetSwitchValueASCII(
        switches::kAshSecondaryDisplayLayout);
    char layout;
    int offset;
    if (sscanf(value.c_str(), "%c,%d", &layout, &offset) == 2) {
      if (layout == 't')
        secondary_display_layout_ = TOP;
      else if (layout == 'b')
        secondary_display_layout_ = BOTTOM;
      else if (layout == 'r')
        secondary_display_layout_ = RIGHT;
      else if (layout == 'l')
        secondary_display_layout_ = LEFT;
      secondary_display_offset_ = offset;
    }
  }
  UpdateDisplayBoundsForLayout();
}

aura::RootWindow* DisplayController::GetPrimaryRootWindow() {
  DCHECK(!root_windows_.empty());
  aura::DisplayManager* display_manager =
      aura::Env::GetInstance()->display_manager();
  return root_windows_[display_manager->GetDisplayAt(0)->id()];
}

aura::RootWindow* DisplayController::GetRootWindowForDisplayId(int64 id) {
  return root_windows_[id];
}

void DisplayController::CloseChildWindows() {
  for (std::map<int64, aura::RootWindow*>::const_iterator it =
           root_windows_.begin(); it != root_windows_.end(); ++it) {
    aura::RootWindow* root_window = it->second;
    internal::RootWindowController* controller =
        GetRootWindowController(root_window);
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

std::vector<aura::RootWindow*> DisplayController::GetAllRootWindows() {
  std::vector<aura::RootWindow*> windows;
  for (std::map<int64, aura::RootWindow*>::const_iterator it =
           root_windows_.begin(); it != root_windows_.end(); ++it) {
    DCHECK(it->second);
    if (GetRootWindowController(it->second))
      windows.push_back(it->second);
  }
  return windows;
}

std::vector<internal::RootWindowController*>
DisplayController::GetAllRootWindowControllers() {
  std::vector<internal::RootWindowController*> controllers;
  for (std::map<int64, aura::RootWindow*>::const_iterator it =
           root_windows_.begin(); it != root_windows_.end(); ++it) {
    internal::RootWindowController* controller =
        GetRootWindowController(it->second);
    if (controller)
      controllers.push_back(controller);
  }
  return controllers;
}

void DisplayController::SetSecondaryDisplayLayout(
    SecondaryDisplayLayout layout) {
  secondary_display_layout_ = layout;
  UpdateDisplayBoundsForLayout();
}

void DisplayController::SetSecondaryDisplayOffset(int offset) {
  secondary_display_offset_ = offset;
  UpdateDisplayBoundsForLayout();
}


void DisplayController::OnDisplayBoundsChanged(const gfx::Display& display) {
  root_windows_[display.id()]->SetHostBounds(display.bounds_in_pixel());
  UpdateDisplayBoundsForLayout();
}

void DisplayController::OnDisplayAdded(const gfx::Display& display) {
  DCHECK(!root_windows_.empty());
  aura::RootWindow* root = AddRootWindowForDisplay(display);
  Shell::GetInstance()->InitRootWindowForSecondaryDisplay(root);
  UpdateDisplayBoundsForLayout();
}

void DisplayController::OnDisplayRemoved(const gfx::Display& display) {
  aura::RootWindow* root = root_windows_[display.id()];
  DCHECK(root);
  // Primary display should never be removed by DisplayManager.
  DCHECK(root != GetPrimaryRootWindow());
  // Display for root window will be deleted when the Primary RootWindow
  // is deleted by the Shell.
  if (root != GetPrimaryRootWindow()) {
    root_windows_.erase(display.id());
    internal::RootWindowController* controller =
        GetRootWindowController(root);
    if (controller) {
      controller->MoveWindowsTo(GetPrimaryRootWindow());
      delete controller;
    } else {
      delete root;
    }
  }
}

aura::RootWindow* DisplayController::AddRootWindowForDisplay(
    const gfx::Display& display) {
  static bool force_constrain_pointer_to_root =
      CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kAshConstrainPointerToRoot);

  aura::RootWindow* root = aura::Env::GetInstance()->display_manager()->
      CreateRootWindowForDisplay(display);
  root_windows_[display.id()] = root;

#if defined(OS_CHROMEOS)
  if (base::chromeos::IsRunningOnChromeOS() || force_constrain_pointer_to_root)
    root->ConfineCursorToWindow();
#endif
  return root;
}

void DisplayController::UpdateDisplayBoundsForLayout() {
  if (gfx::Screen::GetNumDisplays() <= 1)
    return;

  DCHECK_EQ(2, gfx::Screen::GetNumDisplays());
  aura::DisplayManager* display_manager =
      aura::Env::GetInstance()->display_manager();
  const gfx::Rect& primary_bounds = display_manager->GetDisplayAt(0)->bounds();
  gfx::Display* secondary_display = display_manager->GetDisplayAt(1);
  const gfx::Rect& secondary_bounds = secondary_display->bounds();
  gfx::Point new_secondary_origin = primary_bounds.origin();

  // Ignore the offset in case the secondary display doesn't share edges with
  // the primary display.
  int offset = secondary_display_offset_;
  if (secondary_display_layout_ == TOP || secondary_display_layout_ == BOTTOM) {
    offset = std::min(
        offset, primary_bounds.width() - kMinimumOverlapForInvalidOffset);
    offset = std::max(
        offset, -secondary_bounds.width() + kMinimumOverlapForInvalidOffset);
  } else {
    offset = std::min(
        offset, primary_bounds.height() - kMinimumOverlapForInvalidOffset);
    offset = std::max(
        offset, -secondary_bounds.height() + kMinimumOverlapForInvalidOffset);
  }
  switch (secondary_display_layout_) {
    case TOP:
      new_secondary_origin.Offset(offset, -secondary_bounds.height());
      break;
    case RIGHT:
      new_secondary_origin.Offset(primary_bounds.width(), offset);
      break;
    case BOTTOM:
      new_secondary_origin.Offset(offset, primary_bounds.height());
      break;
    case LEFT:
      new_secondary_origin.Offset(-secondary_bounds.width(), offset);
      break;
  }
  gfx::Insets insets = secondary_display->GetWorkAreaInsets();
  secondary_display->set_bounds(
      gfx::Rect(new_secondary_origin, secondary_bounds.size()));
  secondary_display->UpdateWorkAreaFromInsets(insets);
}

}  // namespace internal
}  // namespace ash
