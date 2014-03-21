// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/mouse_interpreter.h"
#include "gestures/include/tracer.h"

namespace gestures {

MouseInterpreter::MouseInterpreter(PropRegistry* prop_reg, Tracer* tracer)
    : Interpreter(NULL, tracer, false) {
  InitName();
  memset(&prev_state_, 0, sizeof(prev_state_));
}

void MouseInterpreter::SyncInterpretImpl(HardwareState* hwstate,
                                             stime_t* timeout) {
  InterpretMouseEvent(prev_state_, *hwstate);

  // Pass max_finger_cnt = 0 to DeepCopy() since we don't care fingers and
  // did not allocate any space for fingers.
  prev_state_.DeepCopy(*hwstate, 0);
}

void MouseInterpreter::InterpretMouseEvent(const HardwareState& prev_state,
                         const HardwareState& hwstate) {
  const unsigned buttons[3] = {
    GESTURES_BUTTON_LEFT,
    GESTURES_BUTTON_MIDDLE,
    GESTURES_BUTTON_RIGHT,
  };
  unsigned down = 0, up = 0;

  for (unsigned i = 0; i < arraysize(buttons); i++) {
    if (!(prev_state.buttons_down & buttons[i]) &&
        (hwstate.buttons_down & buttons[i]))
      down |= buttons[i];
    if ((prev_state.buttons_down & buttons[i]) &&
        !(hwstate.buttons_down & buttons[i]))
      up |= buttons[i];
  }

  if (down || up) {
    ProduceGesture(Gesture(kGestureButtonsChange,
                           prev_state.timestamp,
                           hwstate.timestamp,
                           down,
                           up));
  } else if (hwstate.rel_hwheel || hwstate.rel_wheel) {
    ProduceGesture(Gesture(kGestureScroll,
                           prev_state.timestamp,
                           hwstate.timestamp,
                           -hwstate.rel_hwheel,
                           -hwstate.rel_wheel));
  } else if (hwstate.rel_x || hwstate.rel_y) {
    ProduceGesture(Gesture(kGestureMove,
                           prev_state.timestamp,
                           hwstate.timestamp,
                           hwstate.rel_x,
                           hwstate.rel_y));
  }
}

}  // namespace gestures
