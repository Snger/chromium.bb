// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_layout_manager.h"

#include "ash/screen_ash.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace.h"
#include "ash/wm/workspace/workspace_manager.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_property.h"
#include "ui/base/ui_base_types.h"
#include "ui/gfx/rect.h"
#include "ui/views/widget/native_widget_aura.h"

DECLARE_WINDOW_PROPERTY_TYPE(ui::WindowShowState)

namespace ash {
namespace internal {

namespace {

// Used to remember the show state before the window was minimized.
DEFINE_WINDOW_PROPERTY_KEY(
    ui::WindowShowState, kRestoreShowStateKey, ui::SHOW_STATE_DEFAULT);

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WorkspaceLayoutManager, public:

WorkspaceLayoutManager::WorkspaceLayoutManager(
    aura::RootWindow* root_window,
    WorkspaceManager* workspace_manager)
    : BaseLayoutManager(root_window),
      workspace_manager_(workspace_manager) {
}

WorkspaceLayoutManager::~WorkspaceLayoutManager() {
}

void WorkspaceLayoutManager::OnWindowResized() {
  // Workspace is updated via OnRootWindowResized.
}

void WorkspaceLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  BaseLayoutManager::OnWindowAddedToLayout(child);
  if (!workspace_manager_->IsManagedWindow(child))
    return;

  if (child->IsVisible()) {
    workspace_manager_->AddWindow(child);
  } else if (wm::IsWindowNormal(child)) {
    // Align non-maximized/fullscreen windows to a grid.
    SetChildBoundsDirect(
        child, workspace_manager_->AlignBoundsToGrid(child->GetTargetBounds()));
  }
}

void WorkspaceLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
  workspace_manager_->RemoveWindow(child);
  BaseLayoutManager::OnWillRemoveWindowFromLayout(child);
}

void WorkspaceLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
  if (!workspace_manager_->IsManagedWindow(child))
    return;
  if (visible) {
    if (wm::IsWindowMinimized(child)) {
      // Attempting to show a minimized window. Unminimize it.
      child->SetProperty(aura::client::kShowStateKey,
                         child->GetProperty(kRestoreShowStateKey));
      child->ClearProperty(kRestoreShowStateKey);
    }
    workspace_manager_->AddWindow(child);
  } else {
    workspace_manager_->RemoveWindow(child);
  }
}

void WorkspaceLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  if (GetTrackedByWorkspace(child))
    BaseLayoutManager::SetChildBounds(child, requested_bounds);
  else
    SetChildBoundsDirect(child, requested_bounds);
}

void WorkspaceLayoutManager::OnRootWindowResized(const gfx::Size& new_size) {
  workspace_manager_->SetWorkspaceSize(new_size);
}

void WorkspaceLayoutManager::OnScreenWorkAreaInsetsChanged() {
  workspace_manager_->OnScreenWorkAreaInsetsChanged();
}

void WorkspaceLayoutManager::OnWindowPropertyChanged(aura::Window* window,
                                                     const void* key,
                                                     intptr_t old) {
  BaseLayoutManager::OnWindowPropertyChanged(window, key, old);
  if (key == aura::client::kShowStateKey &&
      workspace_manager_->IsManagedWindow(window)) {
    if (wm::IsWindowMinimized(window)) {
      // Save the previous show state so that we can correctly restore it.
      window->SetProperty(kRestoreShowStateKey,
                          static_cast<ui::WindowShowState>(old));
      workspace_manager_->RemoveWindow(window);
      // Effectively hide the window.
      window->layer()->SetVisible(false);
      // Activate another window.
      if (wm::IsActiveWindow(window))
        wm::DeactivateWindow(window);
      return;
    }
    if (window->TargetVisibility() &&
        !workspace_manager_->IsManagingWindow(window)) {
      workspace_manager_->AddWindow(window);
      if (!window->layer()->visible()) {
        // The layer may be hidden if the window was previously minimized. Make
        // sure it's visible.
        window->Show();
      }
      return;
    }
    workspace_manager_->ShowStateChanged(window);
  } else if (key == ash::kWindowTrackedByWorkspaceSplitPropKey &&
             ash::GetTrackedByWorkspace(window)) {
    // We currently don't need to support transitioning from true to false, so
    // we ignore it.
    workspace_manager_->AddWindow(window);
  }
}

}  // namespace internal
}  // namespace ash
