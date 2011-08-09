// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <base/scoped_ptr.h>

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"

#ifndef GESTURES_SCALING_FILTER_INTERPRETER_H_
#define GESTURES_SCALING_FILTER_INTERPRETER_H_

namespace gestures {

// This interpreter scales both incoming hardware state and outgoing gesture
// objects to make it easier for library code to do interpretation work.

// Incoming hardware events are in the units of touchpad pixels, which may
// not be square. We convert these to a same-sized touchpad such that
// the units are mm with a (0,0) origin. Correspondingly, we convert the
// gestures from mm units to screen pixels.

// For example, say we have a touchpad that has these properties:
// size: 100m x 60mm, left/right: 133/10279, top/bottom: 728/5822.
// This class will scale/translate it, so that the next Interpreter is told
// the hardware has these properties:
// size: 100m x 60mm, left/right: 0.0/100.0, top/bottom: 0.0/60.0.
// Incoming hardware states will be scaled in transit.

// Also, the screen DPI will be scaled, so that it exactly matches the
// touchpad, by having 1 dot per mm. Thus, the screen DPI told to the next
// Interpreter will be 25.4.
// Outgoing gesture objects will be scaled in transit to what the screen
// actually uses.

class ScalingFilterInterpreter : public Interpreter {
 public:
  // Takes ownership of |next|:
  explicit ScalingFilterInterpreter(Interpreter* next);
  virtual ~ScalingFilterInterpreter();

  virtual Gesture* SyncInterpret(HardwareState* hwstate,
                                 stime_t* timeout);

  virtual Gesture* HandleTimer(stime_t now, stime_t* timeout);

  virtual void SetHardwareProperties(const HardwareProperties& hwprops);

  virtual void Configure(GesturesPropProvider* pp, void* data);

  virtual void Deconfigure(GesturesPropProvider* pp, void* data);

 private:
  void ScaleHardwareState(HardwareState* hwstate);
  void ScaleGesture(Gesture* gs);

  scoped_ptr<Interpreter> next_;

  float tp_x_scale_, tp_y_scale_;
  float tp_x_translate_, tp_y_translate_;

  float screen_x_scale_, screen_y_scale_;
};

}  // namespace gestures

#endif  // GESTURES_SCALING_FILTER_INTERPRETER_H_
