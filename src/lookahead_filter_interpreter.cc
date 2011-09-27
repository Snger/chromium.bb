// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/lookahead_filter_interpreter.h"

#include <algorithm>
#include <math.h>

using std::max;
using std::min;

namespace gestures {

namespace {
static const stime_t kMaxDelay = 0.09;  // 90ms
}

LookaheadFilterInterpreter::LookaheadFilterInterpreter(Interpreter* next)
    : max_fingers_per_hwstate_(0), interpreter_due_(-1.0),
      min_nonsuppress_speed_(200.0), min_nonsuppress_speed_prop_(NULL),
      delay_(0.05), delay_prop_(NULL) {
  next_.reset(next);
}

LookaheadFilterInterpreter::~LookaheadFilterInterpreter() {}

Gesture* LookaheadFilterInterpreter::SyncInterpret(HardwareState* hwstate,
                                                   stime_t* timeout) {
  // Push back into queue
  if (free_list_.Empty()) {
    Err("Can't accept new hwstate b/c we're out of nodes!");
    Err("Now: %f, interpreter_due_ %f", hwstate->timestamp, interpreter_due_);
    Err("Dump of queue:");
    for (QState* it = queue_.Begin(); it != queue_.End(); it = it->next_)
      Err("Due: %f%s", it->due_, it->completed_ ? " (c)" : "");
    return NULL;
  }
  QState* node = free_list_.PopFront();
  node->set_state(*hwstate);
  double delay = max(0.0, min(kMaxDelay, delay_));
  node->due_ = hwstate->timestamp + delay;
  node->completed_ = false;
  if (!queue_.Empty() && queue_.Tail()->due_ > node->due_) {
    Err("Clock changed backwards. Clearing queue.");
    do {
      free_list_.PushBack(queue_.PopFront());
    } while (!queue_.Empty());
    interpreter_due_ = -1.0;
  }
  queue_.PushBack(node);

  return HandleTimer(hwstate->timestamp, timeout);
}

Gesture* LookaheadFilterInterpreter::HandleTimer(stime_t now,
                                                 stime_t* timeout) {
  result_ = Gesture();
  Gesture* result = NULL;
  stime_t next_timeout = -1.0;
  while (true) {
    if (interpreter_due_ > 0.0) {
      if (interpreter_due_ > now)
        break;  // Spurious callback
      next_timeout = -1.0;
      result = next_->HandleTimer(now, &next_timeout);
    } else {
      if (queue_.Empty())
        break;
      // Get next uncompleted and overdue hwstate
      QState* node = queue_.Head();
      while (node != queue_.Tail() && node->completed_)
        node = node->next_;
      if (node->completed_ || node->due_ > now)
        break;
      next_timeout = -1.0;
      result = next_->SyncInterpret(&node->state_, &next_timeout);

      // Clear previously completed nodes
      while (!queue_.Empty() && queue_.Head()->completed_)
        free_list_.PushBack(queue_.PopFront());

      // Mark current node completed. This should be the only completed
      // node in the queue.
      node->completed_ = true;
    }
    if (result && ShouldSuppressResult(result, queue_.Head()))
      result = NULL;
    CombineGestures(&result_, result);
    UpdateInterpreterDue(next_timeout, now, timeout);
  }
  UpdateInterpreterDue(next_timeout, now, timeout);
  return result_.type == kGestureTypeNull ? NULL : &result_;
}

bool LookaheadFilterInterpreter::ShouldSuppressResult(const Gesture* gesture,
                                                      QState* node) {
  float distance_sq = 0.0;
  // Slow movements should potentially be suppressed
  switch (gesture->type) {
    case kGestureTypeMove:
      distance_sq = gesture->details.move.dx * gesture->details.move.dx +
          gesture->details.move.dy * gesture->details.move.dy;
      break;
    case kGestureTypeScroll:
      distance_sq = gesture->details.scroll.dx * gesture->details.scroll.dx +
          gesture->details.scroll.dy * gesture->details.scroll.dy;
      break;
    default:
      // Non-movement: just allow it.
      return false;
  }
  stime_t time_delta = gesture->end_time - gesture->start_time;
  float min_nonsuppress_dist_sq =
      min_nonsuppress_speed_ * min_nonsuppress_speed_ *
      time_delta * time_delta;
  if (distance_sq >= min_nonsuppress_dist_sq)
    return false;
  // Speed is slow. Suppress if fingers have changed.
  for (QState* iter = node->next_; iter != queue_.End(); iter = iter->next_) {
    if (node->state_.finger_cnt != iter->state_.finger_cnt)
      return true;
    for (size_t i = 0; i < node->state_.finger_cnt; ++i)
      if (!iter->state_.GetFingerState(node->state_.fingers[i].tracking_id))
        return true;
  }
  return false;
}

void LookaheadFilterInterpreter::UpdateInterpreterDue(
    stime_t new_interpreter_timeout,
    stime_t now,
    stime_t* timeout) {
  stime_t next_hwstate_timeout = -1.0;
  // Scan queue_ to find when next hwstate is due.
  for (QState* node = queue_.Begin(); node != queue_.End();
       node = node->next_) {
    if (node->completed_)
      continue;
    next_hwstate_timeout = node->due_ - now;
    break;
  }

  interpreter_due_ = -1.0;
  if (new_interpreter_timeout >= 0.0 &&
      (new_interpreter_timeout < next_hwstate_timeout ||
       next_hwstate_timeout < 0.0)) {
    interpreter_due_ = new_interpreter_timeout + now;
    *timeout = new_interpreter_timeout;
  } else if (next_hwstate_timeout >= 0.0) {
    *timeout = next_hwstate_timeout;
  }
}

void LookaheadFilterInterpreter::SetHardwareProperties(
    const HardwareProperties& hwprops) {
  const size_t kMaxQNodes = 16;
  queue_.DeleteAll();
  free_list_.DeleteAll();
  for (size_t i = 0; i < kMaxQNodes; ++i) {
    QState* node = new QState(hwprops.max_finger_cnt);
    free_list_.PushBack(node);
  }
  next_->SetHardwareProperties(hwprops);
}

void LookaheadFilterInterpreter::Configure(GesturesPropProvider* pp,
                                           void* data) {
  next_->Configure(pp, data);
  min_nonsuppress_speed_prop_ = pp->create_real_fn(
      data, "Input Queue Min Nonsuppression Speed",
      &min_nonsuppress_speed_, min_nonsuppress_speed_);
  delay_prop_ = pp->create_real_fn(data, "Input Queue Delay", &delay_, delay_);
}

void LookaheadFilterInterpreter::Deconfigure(GesturesPropProvider* pp,
                                             void* data) {
  pp->free_fn(data, min_nonsuppress_speed_prop_);
  min_nonsuppress_speed_prop_ = NULL;
  pp->free_fn(data, delay_prop_);
  delay_prop_ = NULL;
  next_->Deconfigure(pp, data);
}

void LookaheadFilterInterpreter::CombineGestures(Gesture* gesture,
                                                 const Gesture* addend) {
  if (!gesture) {
    Err("gesture must be non-NULL.");
    return;
  }
  if (!addend)
    return;
  if (gesture->type == kGestureTypeNull) {
    *gesture = *addend;
    return;
  }
  if (gesture->type == addend->type &&
      gesture->type != kGestureTypeButtonsChange) {
    // Same type; merge them
    if (gesture->type == kGestureTypeMove) {
      gesture->details.move.dx += addend->details.move.dx;
      gesture->details.move.dy += addend->details.move.dy;
    } else if (gesture->type == kGestureTypeScroll) {
      gesture->details.scroll.dx += addend->details.scroll.dx;
      gesture->details.scroll.dy += addend->details.scroll.dy;
    }
    return;
  }
  if (addend->type != kGestureTypeButtonsChange) {
    // Either |gesture| is a button gesture, or neither is. Either way, keep
    // |gesture| as is.
    Err("Losing gesture");
    return;
  }
  // |addend| must be a button gesture if we get to here.
  if (gesture->type != kGestureTypeButtonsChange) {
    Err("Losing gesture");
    *gesture = *addend;
    return;
  }
  // We have 2 button events. merge them
  unsigned buttons[] = { GESTURES_BUTTON_LEFT,
                         GESTURES_BUTTON_MIDDLE,
                         GESTURES_BUTTON_RIGHT };
  for (size_t i = 0; i < arraysize(buttons); ++i) {
    unsigned button = buttons[i];
    unsigned g_down = gesture->details.buttons.down & button;
    unsigned g_up = gesture->details.buttons.up & button;
    unsigned a_down = addend->details.buttons.down & button;
    unsigned a_up = addend->details.buttons.up & button;
    // How we merge buttons: Remember that a button gesture event can send
    // some button down events, then button up events. Ideally we can combine
    // them simply: e.g. if |gesture| has button down and |addend| has button
    // up, we can put those both into |gesture|. If there is a conflict (e.g.
    // button up followed by button down/up), there is no proper way to
    // represent that in a single gesture. We work around that case by removing
    // pairs of down/up, so in the example just given, the result would be just
    // button up. There is one exception to these two rules: if |gesture| is
    // button up, and |addend| is button down, combing them into one gesture
    // would mean a click, because when executing the gestures, the down
    // actions happen before the up. So for that case, we just remove all
    // button action.
    if (!g_down && g_up && a_down && !a_up) {
      // special case
      g_down = 0;
      g_up = 0;
    } else if ((g_down & a_down) | (g_up & a_up)) {
      // If we have a conflict, this logic seems to remove the full click.
      g_down = (~(g_down ^ a_down)) & button;
      g_up = (~(g_up ^ a_up)) & button;
    } else {
      // Non-conflict case
      g_down |= a_down;
      g_up |= a_up;
    }
    gesture->details.buttons.down =
        (gesture->details.buttons.down & ~button) | g_down;
    gesture->details.buttons.up =
        (gesture->details.buttons.up & ~button) | g_up;
  }
  if (!gesture->details.buttons.down && !gesture->details.buttons.up)
    *gesture = Gesture();
}

LookaheadFilterInterpreter::QState::QState()
    : max_fingers_(0), completed_(false), next_(NULL), prev_(NULL) {
  fs_.reset();
  state_.fingers = NULL;
}

LookaheadFilterInterpreter::QState::QState(unsigned short max_fingers)
    : max_fingers_(max_fingers), completed_(false), next_(NULL), prev_(NULL) {
  fs_.reset(new FingerState[max_fingers]);
  state_.fingers = fs_.get();
}

void LookaheadFilterInterpreter::QState::set_state(
    const HardwareState& new_state) {
  state_.timestamp = new_state.timestamp;
  state_.buttons_down = new_state.buttons_down;
  state_.touch_cnt = new_state.touch_cnt;
  unsigned short copy_count = new_state.finger_cnt;
  if (new_state.finger_cnt > max_fingers_) {
    Err("State with too many fingers! (%u vs %u)",
        new_state.finger_cnt,
        max_fingers_);
    copy_count = max_fingers_;
  }
  state_.finger_cnt = copy_count;
  std::copy(new_state.fingers, new_state.fingers + copy_count, state_.fingers);
}

}  // namespace gestures
