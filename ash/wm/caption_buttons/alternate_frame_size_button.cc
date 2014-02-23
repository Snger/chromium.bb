// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/caption_buttons/alternate_frame_size_button.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/touch/touch_uma.h"
#include "ash/wm/window_state.h"
#include "ash/wm/workspace/phantom_window_controller.h"
#include "ash/wm/workspace/snap_sizer.h"
#include "ui/gfx/vector2d.h"
#include "ui/views/widget/widget.h"

namespace {

// The default delay between the user pressing the size button and the buttons
// adjacent to the size button morphing into buttons for snapping left and
// right.
const int kSetButtonsToSnapModeDelayMs = 150;

// The amount that a user can overshoot one of the caption buttons while in
// "snap mode" and keep the button hovered/pressed.
const int kMaxOvershootX = 200;
const int kMaxOvershootY = 50;

// Returns true if a mouse drag while in "snap mode" at |location_in_screen|
// would hover/press |button| or keep it hovered/pressed.
bool HitTestButton(const ash::FrameCaptionButton* button,
                   const gfx::Point& location_in_screen) {
  gfx::Rect expanded_bounds_in_screen = button->GetBoundsInScreen();
  if (button->state() == views::Button::STATE_HOVERED ||
      button->state() == views::Button::STATE_PRESSED) {
    expanded_bounds_in_screen.Inset(-kMaxOvershootX, -kMaxOvershootY);
  }
  return expanded_bounds_in_screen.Contains(location_in_screen);
}

}  // namespace

namespace ash {

AlternateFrameSizeButton::AlternateFrameSizeButton(
    views::ButtonListener* listener,
    views::Widget* frame,
    AlternateFrameSizeButtonDelegate* delegate)
    : FrameCaptionButton(listener, CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE),
      frame_(frame),
      delegate_(delegate),
      set_buttons_to_snap_mode_delay_ms_(kSetButtonsToSnapModeDelayMs),
      in_snap_mode_(false),
      snap_type_(SNAP_NONE) {
}

AlternateFrameSizeButton::~AlternateFrameSizeButton() {
}

bool AlternateFrameSizeButton::OnMousePressed(const ui::MouseEvent& event) {
  // The minimize and close buttons are set to snap left and right when snapping
  // is enabled. Do not enable snapping if the minimize button is not visible.
  // The close button is always visible.
  if (IsTriggerableEvent(event) &&
      !in_snap_mode_ &&
      delegate_->IsMinimizeButtonVisible()) {
    StartSetButtonsToSnapModeTimer(event);
  }
  FrameCaptionButton::OnMousePressed(event);
  return true;
}

bool AlternateFrameSizeButton::OnMouseDragged(const ui::MouseEvent& event) {
  UpdateSnapType(event);
  // By default a FrameCaptionButton reverts to STATE_NORMAL once the mouse
  // leaves its bounds. Skip FrameCaptionButton's handling when
  // |in_snap_mode_| == true because we want different behavior.
  if (!in_snap_mode_)
    FrameCaptionButton::OnMouseDragged(event);
  return true;
}

void AlternateFrameSizeButton::OnMouseReleased(const ui::MouseEvent& event) {
  if (!IsTriggerableEvent(event) || !CommitSnap(event))
    FrameCaptionButton::OnMouseReleased(event);
}

void AlternateFrameSizeButton::OnMouseCaptureLost() {
  SetButtonsToNormalMode(AlternateFrameSizeButtonDelegate::ANIMATE_YES);
  FrameCaptionButton::OnMouseCaptureLost();
}

void AlternateFrameSizeButton::OnMouseMoved(const ui::MouseEvent& event) {
  // Ignore any synthetic mouse moves during a drag.
  if (!in_snap_mode_)
    FrameCaptionButton::OnMouseMoved(event);
}

void AlternateFrameSizeButton::OnGestureEvent(ui::GestureEvent* event) {
  if (event->details().touch_points() > 1) {
    SetButtonsToNormalMode(AlternateFrameSizeButtonDelegate::ANIMATE_YES);
    return;
  }

  if (event->type() == ui::ET_GESTURE_TAP_DOWN) {
    StartSetButtonsToSnapModeTimer(*event);
    // Go through FrameCaptionButton's handling so that the button gets pressed.
    FrameCaptionButton::OnGestureEvent(event);
    return;
  }

  if (event->type() == ui::ET_GESTURE_SCROLL_BEGIN ||
      event->type() == ui::ET_GESTURE_SCROLL_UPDATE) {
    UpdateSnapType(*event);
    event->SetHandled();
    return;
  }

  if (event->type() == ui::ET_GESTURE_TAP ||
      event->type() == ui::ET_GESTURE_SCROLL_END ||
      event->type() == ui::ET_SCROLL_FLING_START ||
      event->type() == ui::ET_GESTURE_END) {
    if (CommitSnap(*event)) {
      if (event->type() == ui::ET_GESTURE_TAP) {
        TouchUMA::GetInstance()->RecordGestureAction(
            TouchUMA::GESTURE_FRAMEMAXIMIZE_TAP);
      }
      event->SetHandled();
      return;
    }
  }

  FrameCaptionButton::OnGestureEvent(event);
}

void AlternateFrameSizeButton::StartSetButtonsToSnapModeTimer(
    const ui::LocatedEvent& event) {
  set_buttons_to_snap_mode_timer_event_location_ = event.location();
  if (set_buttons_to_snap_mode_delay_ms_ == 0) {
    SetButtonsToSnapMode();
  } else {
    set_buttons_to_snap_mode_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(set_buttons_to_snap_mode_delay_ms_),
        this,
        &AlternateFrameSizeButton::SetButtonsToSnapMode);
  }
}

void AlternateFrameSizeButton::SetButtonsToSnapMode() {
  if (in_snap_mode_)
    return;
  in_snap_mode_ = true;
  delegate_->SetButtonIcons(CAPTION_BUTTON_ICON_LEFT_SNAPPED,
                            CAPTION_BUTTON_ICON_RIGHT_SNAPPED,
                            AlternateFrameSizeButtonDelegate::ANIMATE_YES);
}

void AlternateFrameSizeButton::UpdateSnapType(const ui::LocatedEvent& event) {
  if (!in_snap_mode_) {
    // Set the buttons adjacent to the size button to snap left and right early
    // if the user drags past the drag threshold.
    // |set_buttons_to_snap_mode_timer_| is checked to avoid entering the snap
    // mode as a result of an unsupported drag type (e.g. only the right mouse
    // button is pressed).
    gfx::Vector2d delta(
        event.location() - set_buttons_to_snap_mode_timer_event_location_);
    if (!set_buttons_to_snap_mode_timer_.IsRunning() ||
        !views::View::ExceededDragThreshold(delta)) {
      return;
    }
    SetButtonsToSnapMode();
  }

  gfx::Point event_location_in_screen(event.location());
  views::View::ConvertPointToScreen(this, &event_location_in_screen);
  const FrameCaptionButton* to_hover =
      GetButtonToHover(event_location_in_screen);
  bool press_size_button =
      to_hover || HitTestButton(this, event_location_in_screen);
  delegate_->SetHoveredAndPressedButtons(
      to_hover, press_size_button ? this : NULL);

  snap_type_ = SNAP_NONE;
  if (to_hover) {
    switch (to_hover->icon()) {
      case CAPTION_BUTTON_ICON_LEFT_SNAPPED:
        snap_type_ = SNAP_LEFT;
        break;
      case CAPTION_BUTTON_ICON_RIGHT_SNAPPED:
        snap_type_ = SNAP_RIGHT;
        break;
      case CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE:
      case CAPTION_BUTTON_ICON_MINIMIZE:
      case CAPTION_BUTTON_ICON_CLOSE:
      case CAPTION_BUTTON_ICON_COUNT:
        NOTREACHED();
        break;
    }
  }

  if (snap_type_ == SNAP_LEFT || snap_type_ == SNAP_RIGHT) {
    if (!phantom_window_controller_.get()) {
      phantom_window_controller_.reset(
          new internal::PhantomWindowController(frame_->GetNativeWindow()));
    }

    using internal::SnapSizer;
    SnapSizer snap_sizer(wm::GetWindowState(frame_->GetNativeWindow()),
                         gfx::Point(),
                         snap_type_ == SNAP_LEFT ?
                             SnapSizer::LEFT_EDGE : SnapSizer::RIGHT_EDGE,
                         SnapSizer::OTHER_INPUT);
    phantom_window_controller_->Show(ScreenUtil::ConvertRectToScreen(
          frame_->GetNativeView()->parent(),
          snap_sizer.target_bounds()));
  } else {
    phantom_window_controller_.reset();
  }
}

const FrameCaptionButton* AlternateFrameSizeButton::GetButtonToHover(
    const gfx::Point& event_location_in_screen) const {
  const FrameCaptionButton* closest_button = delegate_->GetButtonClosestTo(
      event_location_in_screen);
  if ((closest_button->icon() == CAPTION_BUTTON_ICON_LEFT_SNAPPED ||
       closest_button->icon() == CAPTION_BUTTON_ICON_RIGHT_SNAPPED) &&
      HitTestButton(closest_button, event_location_in_screen)) {
    return closest_button;
  }
  return NULL;
}

bool AlternateFrameSizeButton::CommitSnap(const ui::LocatedEvent& event) {
  // The position of |event| may be different than the position of the previous
  // event.
  UpdateSnapType(event);

  if (in_snap_mode_ &&
      (snap_type_ == SNAP_LEFT || snap_type_ == SNAP_RIGHT)) {
    using internal::SnapSizer;
    SnapSizer::SnapWindow(ash::wm::GetWindowState(frame_->GetNativeWindow()),
                          snap_type_ == SNAP_LEFT ?
                              SnapSizer::LEFT_EDGE : SnapSizer::RIGHT_EDGE);
    ash::Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        snap_type_ == SNAP_LEFT ?
            ash::UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_LEFT :
            ash::UMA_WINDOW_MAXIMIZE_BUTTON_MAXIMIZE_RIGHT);
    SetButtonsToNormalMode(AlternateFrameSizeButtonDelegate::ANIMATE_NO);
    return true;
  }
  SetButtonsToNormalMode(AlternateFrameSizeButtonDelegate::ANIMATE_YES);
  return false;
}

void AlternateFrameSizeButton::SetButtonsToNormalMode(
    AlternateFrameSizeButtonDelegate::Animate animate) {
  in_snap_mode_ = false;
  snap_type_ = SNAP_NONE;
  set_buttons_to_snap_mode_timer_.Stop();
  delegate_->SetButtonsToNormal(animate);
  phantom_window_controller_.reset();
}

}  // namespace ash
