// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/window_overview_mode.h"

#include <algorithm>
#include <functional>
#include <vector>

#include "base/macros.h"
#include "ui/aura/scoped_window_targeter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_property.h"
#include "ui/aura/window_targeter.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/transform.h"

namespace {

struct WindowOverviewState {
  // The transform for when the window is at the topmost position.
  gfx::Transform top;

  // The transform for when the window is at the bottom-most position.
  gfx::Transform bottom;

  // The current overview state of the window. 0.f means the window is at the
  // topmost position. 1.f means the window is at the bottom-most position.
  float progress;
};

}  // namespace

DECLARE_WINDOW_PROPERTY_TYPE(WindowOverviewState*)
DEFINE_OWNED_WINDOW_PROPERTY_KEY(WindowOverviewState,
                                 kWindowOverviewState,
                                 NULL)
namespace athena {

namespace {

// Sets the progress-state for the window in the overview mode.
void SetWindowProgress(aura::Window* window, float progress) {
  WindowOverviewState* state = window->GetProperty(kWindowOverviewState);
  gfx::Transform transform =
      gfx::Tween::TransformValueBetween(progress, state->top, state->bottom);
  window->SetTransform(transform);
  state->progress = progress;
}

// Resets the overview-related state for |window|.
void RestoreWindowState(aura::Window* window) {
  window->ClearProperty(kWindowOverviewState);

  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  settings.SetPreemptionStrategy(
      ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
  settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(250));
  window->SetTransform(gfx::Transform());
}

// Always returns the same target.
class StaticWindowTargeter : public aura::WindowTargeter {
 public:
  explicit StaticWindowTargeter(aura::Window* target) : target_(target) {}
  virtual ~StaticWindowTargeter() {}

 private:
  // aura::WindowTargeter:
  virtual ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                              ui::Event* event) OVERRIDE {
    return target_;
  }

  virtual ui::EventTarget* FindTargetForLocatedEvent(
      ui::EventTarget* root,
      ui::LocatedEvent* event) OVERRIDE {
    return target_;
  }

  aura::Window* target_;
  DISALLOW_COPY_AND_ASSIGN(StaticWindowTargeter);
};

class WindowOverviewModeImpl : public WindowOverviewMode,
                               public ui::EventHandler {
 public:
  WindowOverviewModeImpl(aura::Window* container,
                         WindowOverviewModeDelegate* delegate)
      : container_(container),
        delegate_(delegate),
        scoped_targeter_(new aura::ScopedWindowTargeter(
            container,
            scoped_ptr<ui::EventTargeter>(
                new StaticWindowTargeter(container)))) {
    container_->set_target_handler(this);

    // Prepare the desired transforms for all the windows, and set the initial
    // state on the windows.
    ComputeTerminalStatesForAllWindows();
    SetInitialWindowStates();
  }

  virtual ~WindowOverviewModeImpl() {
    container_->set_target_handler(container_->delegate());

    aura::Window::Windows windows = container_->children();
    if (windows.empty())
      return;
    std::for_each(windows.begin(), windows.end(), &RestoreWindowState);
  }

 private:
  // Computes the transforms for all windows in both the topmost and bottom-most
  // positions. The transforms are set in the |kWindowOverviewState| property of
  // the windows.
  void ComputeTerminalStatesForAllWindows() {
    aura::Window::Windows windows = container_->children();
    size_t window_count = windows.size();
    size_t index = 0;
    const gfx::Size container_size = container_->bounds().size();

    const int kGapBetweenWindowsBottom = 10;
    const int kGapBetweenWindowsTop = 5;
    const float kMinScale = 0.6f;
    const float kMaxScale = 0.95f;

    for (aura::Window::Windows::reverse_iterator iter = windows.rbegin();
         iter != windows.rend();
         ++iter, ++index) {
      aura::Window* window = (*iter);

      gfx::Transform top_transform;
      int top = (window_count - index - 1) * kGapBetweenWindowsTop;
      float x_translate = container_size.width() * (1 - kMinScale) / 2.;
      top_transform.Translate(x_translate, top);
      top_transform.Scale(kMinScale, kMinScale);

      gfx::Transform bottom_transform;
      int bottom = container_size.height() - (index * kGapBetweenWindowsBottom);
      x_translate = container_size.width() * (1 - kMaxScale) / 2.;
      bottom_transform.Translate(x_translate, bottom - window->bounds().y());
      bottom_transform.Scale(kMaxScale, kMaxScale);

      WindowOverviewState* state = new WindowOverviewState;
      state->top = top_transform;
      state->bottom = bottom_transform;
      state->progress = 0.f;
      window->SetProperty(kWindowOverviewState, state);
    }
  }

  // Sets the initial position for the windows for the overview mode.
  void SetInitialWindowStates() {
    aura::Window::Windows windows = container_->children();
    size_t window_count = windows.size();
    // The initial overview state of the topmost three windows.
    const float kInitialProgress[] = { 0.5f, 0.05f, 0.01f };
    for (size_t i = 0; i < window_count; ++i) {
      float progress = 0.f;
      aura::Window* window = windows[window_count - 1 - i];
      if (i < arraysize(kInitialProgress))
        progress = kInitialProgress[i];

      scoped_refptr<ui::LayerAnimator> animator =
          window->layer()->GetAnimator();

      // Unset any in-progress animation.
      {
        ui::ScopedLayerAnimationSettings settings(animator);
        settings.SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_SET_NEW_TARGET);
        window->Show();
        window->SetTransform(gfx::Transform());
      }
      // Setup the animation.
      {
        ui::ScopedLayerAnimationSettings settings(animator);
        settings.SetPreemptionStrategy(
            ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
        settings.SetTransitionDuration(base::TimeDelta::FromMilliseconds(250));
        SetWindowProgress(window, progress);
      }
    }
  }

  aura::Window* SelectWindowAt(ui::LocatedEvent* event) {
    CHECK_EQ(container_, event->target());
    // Find the old targeter to find the target of the event.
    ui::EventTarget* window = container_;
    ui::EventTargeter* targeter = scoped_targeter_->old_targeter();
    while (!targeter && window->GetParentTarget()) {
      window = window->GetParentTarget();
      targeter = window->GetEventTargeter();
    }
    if (!targeter)
      return NULL;
    aura::Window* target = static_cast<aura::Window*>(
        targeter->FindTargetForLocatedEvent(container_, event));
    while (target && target->parent() != container_)
      target = target->parent();
    return target;
  }

  // ui::EventHandler:
  virtual void OnMouseEvent(ui::MouseEvent* mouse) OVERRIDE {
    if (mouse->type() != ui::ET_MOUSE_PRESSED)
      return;
    aura::Window* select = SelectWindowAt(mouse);
    if (select) {
      mouse->SetHandled();
      delegate_->OnSelectWindow(select);
    }
  }

  virtual void OnGestureEvent(ui::GestureEvent* gesture) OVERRIDE {
    if (gesture->type() != ui::ET_GESTURE_TAP)
      return;
    aura::Window* select = SelectWindowAt(gesture);
    if (select) {
      gesture->SetHandled();
      delegate_->OnSelectWindow(select);
    }
  }

  aura::Window* container_;
  WindowOverviewModeDelegate* delegate_;
  scoped_ptr<aura::ScopedWindowTargeter> scoped_targeter_;

  DISALLOW_COPY_AND_ASSIGN(WindowOverviewModeImpl);
};

}  // namespace

scoped_ptr<WindowOverviewMode> WindowOverviewMode::Create(
    aura::Window* window,
    WindowOverviewModeDelegate* delegate) {
  return scoped_ptr<WindowOverviewMode>(
      new WindowOverviewModeImpl(window, delegate));
}

}  // namespace athena
