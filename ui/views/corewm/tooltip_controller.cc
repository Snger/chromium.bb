// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/corewm/tooltip_controller.h"

#include <vector>

#include "base/time/time.h"
#include "ui/aura/client/cursor_client.h"
#include "ui/aura/client/drag_drop_client.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"
#include "ui/views/corewm/tooltip.h"
#include "ui/views/corewm/tooltip_aura.h"

namespace {

const int kTooltipTimeoutMs = 500;
const int kDefaultTooltipShownTimeoutMs = 10000;

}  // namespace

namespace views {
namespace corewm {

////////////////////////////////////////////////////////////////////////////////
// TooltipController public:

TooltipController::TooltipController(gfx::ScreenType screen_type)
    : screen_type_(screen_type),
      tooltip_window_(NULL),
      tooltip_window_at_mouse_press_(NULL),
      mouse_pressed_(false),
      tooltips_enabled_(true) {
  tooltip_timer_.Start(FROM_HERE,
      base::TimeDelta::FromMilliseconds(kTooltipTimeoutMs),
      this, &TooltipController::TooltipTimerFired);
}

TooltipController::~TooltipController() {
  if (tooltip_window_)
    tooltip_window_->RemoveObserver(this);
}

void TooltipController::UpdateTooltip(aura::Window* target) {
  // If tooltip is visible, we may want to hide it. If it is not, we are ok.
  if (tooltip_window_ == target && GetTooltip()->IsVisible())
    UpdateIfRequired();

  // If we had stopped the tooltip timer for some reason, we must restart it if
  // there is a change in the tooltip.
  if (!tooltip_timer_.IsRunning()) {
    if (tooltip_window_ != target || (tooltip_window_ &&
        tooltip_text_ != aura::client::GetTooltipText(tooltip_window_))) {
      tooltip_timer_.Start(FROM_HERE,
          base::TimeDelta::FromMilliseconds(kTooltipTimeoutMs),
          this, &TooltipController::TooltipTimerFired);
    }
  }
}

void TooltipController::SetTooltipShownTimeout(aura::Window* target,
                                               int timeout_in_ms) {
  tooltip_shown_timeout_map_[target] = timeout_in_ms;
}

void TooltipController::SetTooltipsEnabled(bool enable) {
  if (tooltips_enabled_ == enable)
    return;
  tooltips_enabled_ = enable;
  UpdateTooltip(tooltip_window_);
}

void TooltipController::OnKeyEvent(ui::KeyEvent* event) {
  // On key press, we want to hide the tooltip and not show it until change.
  // This is the same behavior as hiding tooltips on timeout. Hence, we can
  // simply simulate a timeout.
  if (tooltip_shown_timer_.IsRunning()) {
    tooltip_shown_timer_.Stop();
    TooltipShownTimerFired();
  }
}

void TooltipController::OnMouseEvent(ui::MouseEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  switch (event->type()) {
    case ui::ET_MOUSE_EXITED:
      target = NULL;
      // Fall through.
    case ui::ET_MOUSE_MOVED:
    case ui::ET_MOUSE_DRAGGED:
      if (tooltip_window_ != target) {
        if (tooltip_window_)
          tooltip_window_->RemoveObserver(this);
        tooltip_window_ = target;
        if (tooltip_window_)
          tooltip_window_->AddObserver(this);
      }
      curr_mouse_loc_ = event->location();
      if (tooltip_timer_.IsRunning())
        tooltip_timer_.Reset();

      if (GetTooltip()->IsVisible())
        UpdateIfRequired();
      break;
    case ui::ET_MOUSE_PRESSED:
      if ((event->flags() & ui::EF_IS_NON_CLIENT) == 0) {
        // We don't get a release for non-client areas.
        mouse_pressed_ = true;
        tooltip_window_at_mouse_press_ = target;
        if (target)
          tooltip_text_at_mouse_press_ = aura::client::GetTooltipText(target);
      }
      GetTooltip()->Hide();
      break;
    case ui::ET_MOUSE_RELEASED:
      mouse_pressed_ = false;
      break;
    case ui::ET_MOUSE_CAPTURE_CHANGED:
      // We will not received a mouse release, so reset mouse pressed state.
      mouse_pressed_ = false;
    case ui::ET_MOUSEWHEEL:
      // Hide the tooltip for click, release, drag, wheel events.
      if (GetTooltip()->IsVisible())
        GetTooltip()->Hide();
      break;
    default:
      break;
  }
}

void TooltipController::OnTouchEvent(ui::TouchEvent* event) {
  // TODO(varunjain): need to properly implement tooltips for
  // touch events.
  // Hide the tooltip for touch events.
  GetTooltip()->Hide();
  if (tooltip_window_)
    tooltip_window_->RemoveObserver(this);
  tooltip_window_ = NULL;
}

void TooltipController::OnCancelMode(ui::CancelModeEvent* event) {
  if (tooltip_.get())
    tooltip_->Hide();
}

void TooltipController::OnWindowDestroyed(aura::Window* window) {
  if (tooltip_window_ == window) {
    GetTooltip()->Hide();
    tooltip_shown_timeout_map_.erase(tooltip_window_);
    tooltip_window_->RemoveObserver(this);
    tooltip_window_ = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
// TooltipController private:

void TooltipController::TooltipTimerFired() {
  UpdateIfRequired();
}

void TooltipController::TooltipShownTimerFired() {
  GetTooltip()->Hide();

  // Since the user presumably no longer needs the tooltip, we also stop the
  // tooltip timer so that tooltip does not pop back up. We will restart this
  // timer if the tooltip changes (see UpdateTooltip()).
  tooltip_timer_.Stop();
}

void TooltipController::UpdateIfRequired() {
  if (!tooltips_enabled_ || mouse_pressed_ || IsDragDropInProgress() ||
      !IsCursorVisible()) {
    GetTooltip()->Hide();
    return;
  }

  string16 tooltip_text;
  if (tooltip_window_)
    tooltip_text = aura::client::GetTooltipText(tooltip_window_);

  // If the user pressed a mouse button. We will hide the tooltip and not show
  // it until there is a change in the tooltip.
  if (tooltip_window_at_mouse_press_) {
    if (tooltip_window_ == tooltip_window_at_mouse_press_ &&
        tooltip_text == tooltip_text_at_mouse_press_) {
      GetTooltip()->Hide();
      return;
    }
    tooltip_window_at_mouse_press_ = NULL;
  }

  // We add the !GetTooltip()->IsVisible() below because when we come here from
  // TooltipTimerFired(), the tooltip_text may not have changed but we still
  // want to update the tooltip because the timer has fired.
  // If we come here from UpdateTooltip(), we have already checked for tooltip
  // visibility and this check below will have no effect.
  if (tooltip_text_ != tooltip_text || !GetTooltip()->IsVisible()) {
    tooltip_shown_timer_.Stop();
    tooltip_text_ = tooltip_text;
    if (tooltip_text_.empty()) {
      GetTooltip()->Hide();
    } else {
      gfx::Point widget_loc = curr_mouse_loc_ +
          tooltip_window_->GetBoundsInScreen().OffsetFromOrigin();
      GetTooltip()->SetText(tooltip_window_, tooltip_text_, widget_loc);
      GetTooltip()->Show();
      int timeout = GetTooltipShownTimeout();
      if (timeout > 0) {
        tooltip_shown_timer_.Start(FROM_HERE,
            base::TimeDelta::FromMilliseconds(timeout),
            this, &TooltipController::TooltipShownTimerFired);
      }
    }
  }
}

bool TooltipController::IsTooltipVisible() {
  return GetTooltip()->IsVisible();
}

bool TooltipController::IsDragDropInProgress() {
  if (!tooltip_window_)
    return false;
  aura::client::DragDropClient* client =
      aura::client::GetDragDropClient(tooltip_window_->GetRootWindow());
  return client && client->IsDragDropInProgress();
}

Tooltip* TooltipController::GetTooltip() {
  if (!tooltip_.get())
    tooltip_.reset(new TooltipAura(screen_type_));
  return tooltip_.get();
}

bool TooltipController::IsCursorVisible() {
  if (!tooltip_window_)
    return false;
  aura::RootWindow* root = tooltip_window_->GetRootWindow();
  if (!root)
    return false;
  aura::client::CursorClient* cursor_client =
      aura::client::GetCursorClient(root);
  // |cursor_client| may be NULL in tests, treat NULL as always visible.
  return !cursor_client || cursor_client->IsCursorVisible();
}

int TooltipController::GetTooltipShownTimeout() {
  std::map<aura::Window*, int>::const_iterator it =
      tooltip_shown_timeout_map_.find(tooltip_window_);
  if (it == tooltip_shown_timeout_map_.end())
    return kDefaultTooltipShownTimeoutMs;
  return it->second;
}

}  // namespace corewm
}  // namespace views
