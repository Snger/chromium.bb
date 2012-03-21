// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/shelf_layout_manager.h"

#include "ash/launcher/launcher.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "base/auto_reset.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/compositor/layer.h"
#include "ui/gfx/compositor/layer_animation_observer.h"
#include "ui/gfx/compositor/layer_animator.h"
#include "ui/gfx/compositor/scoped_layer_animation_settings.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace internal {

namespace {

ui::Layer* GetLayer(views::Widget* widget) {
  return widget->GetNativeView()->layer();
}

}  // namespace

// static
const int ShelfLayoutManager::kWorkspaceAreaBottomInset = 2;

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, public:

ShelfLayoutManager::ShelfLayoutManager(views::Widget* status)
    : in_layout_(false),
      visible_(true),
      max_height_(status->GetWindowScreenBounds().height()),
      launcher_(NULL),
      status_(status) {
  root_window_ = status->GetNativeView()->GetRootWindow();
}

ShelfLayoutManager::~ShelfLayoutManager() {
}

void ShelfLayoutManager::SetLauncherWidget(views::Widget* launcher) {
  if (launcher == launcher_)
    return;

  launcher_ = launcher;
  max_height_ =
      std::max(max_height_, launcher->GetWindowScreenBounds().height());
  LayoutShelf();
  SetVisible(visible());
}

void ShelfLayoutManager::LayoutShelf() {
  AutoReset<bool> auto_reset_in_layout(&in_layout_, true);
  StopAnimating();
  TargetBounds target_bounds;
  float target_opacity = visible_ ? 1.0f : 0.0f;
  CalculateTargetBounds(visible_, &target_bounds);
  GetLayer(status_)->SetOpacity(target_opacity);
  status_->SetBounds(target_bounds.status_bounds);

  if (launcher_) {
    GetLayer(launcher_)->SetOpacity(target_opacity);
    launcher_->SetBounds(target_bounds.launcher_bounds);
    ash::Shell::GetInstance()->launcher()->SetStatusWidth(
        target_bounds.status_bounds.width());
  }

  Shell::GetInstance()->SetMonitorWorkAreaInsets(
      Shell::GetRootWindow(),
      target_bounds.work_area_insets);
}

void ShelfLayoutManager::SetVisible(bool visible) {
  ui::Layer* launcher_layer = launcher_ ? GetLayer(launcher_) : NULL;
  ui::Layer* status_layer = GetLayer(status_);

  // TODO(vollick): once visibility is animatable, use GetTargetVisibility.
  bool current_visibility = visible_ &&
      status_layer->GetTargetOpacity() > 0.0f;
  if (launcher_layer)
      current_visibility =
          current_visibility && launcher_layer->GetTargetOpacity() > 0.0f;

  if (visible == current_visibility)
    return;  // Nothing changed.

  StopAnimating();

  visible_ = visible;
  TargetBounds target_bounds;
  float target_opacity = visible ? 1.0f : 0.0f;
  CalculateTargetBounds(visible, &target_bounds);

  ui::ScopedLayerAnimationSettings status_animation_setter(
      status_layer->GetAnimator());
  status_animation_setter.AddObserver(this);
  status_layer->SetBounds(target_bounds.status_bounds);
  status_layer->SetOpacity(target_opacity);

  if (launcher_layer) {
    ui::ScopedLayerAnimationSettings launcher_animation_setter(
        launcher_layer->GetAnimator());
    launcher_animation_setter.AddObserver(this);
    launcher_layer->SetBounds(target_bounds.launcher_bounds);
    launcher_layer->SetOpacity(target_opacity);
  }
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, aura::LayoutManager implementation:

void ShelfLayoutManager::OnWindowResized() {
  LayoutShelf();
}

void ShelfLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
}

void ShelfLayoutManager::OnWillRemoveWindowFromLayout(aura::Window* child) {
}

void ShelfLayoutManager::OnChildWindowVisibilityChanged(aura::Window* child,
                                                        bool visible) {
}

void ShelfLayoutManager::SetChildBounds(aura::Window* child,
                                        const gfx::Rect& requested_bounds) {
  SetChildBoundsDirect(child, requested_bounds);
  if (!in_layout_)
    LayoutShelf();
}

////////////////////////////////////////////////////////////////////////////////
// ShelfLayoutManager, private:

void ShelfLayoutManager::StopAnimating() {
  StopObservingImplicitAnimations();
  if (launcher_)
    GetLayer(launcher_)->GetAnimator()->StopAnimating();
  GetLayer(status_)->GetAnimator()->StopAnimating();
}

void ShelfLayoutManager::CalculateTargetBounds(bool visible,
                                               TargetBounds* target_bounds) {
  const gfx::Rect& available_bounds(root_window_->bounds());
  int y = available_bounds.bottom() - (visible ? max_height_ : 0);
  gfx::Rect status_bounds(status_->GetWindowScreenBounds());
  // The status widget should extend to the bottom and right edges.
  target_bounds->status_bounds = gfx::Rect(
      available_bounds.right() - status_bounds.width(),
      y + max_height_ - status_bounds.height(),
      status_bounds.width(), status_bounds.height());
  gfx::Rect launcher_bounds =
      launcher_ ? launcher_->GetWindowScreenBounds() : gfx::Rect();
  target_bounds->launcher_bounds = gfx::Rect(
      available_bounds.x(), y + (max_height_ - launcher_bounds.height()) / 2,
      available_bounds.width(),
      launcher_bounds.height());
  if (visible)
    target_bounds->work_area_insets = gfx::Insets(
        0, 0, max_height_ + kWorkspaceAreaBottomInset, 0);
}

void ShelfLayoutManager::OnImplicitAnimationsCompleted() {
  TargetBounds target_bounds;
  CalculateTargetBounds(visible_, &target_bounds);
  Shell::GetInstance()->SetMonitorWorkAreaInsets(
      Shell::GetRootWindow(),
      target_bounds.work_area_insets);
}

}  // namespace internal
}  // namespace ash
