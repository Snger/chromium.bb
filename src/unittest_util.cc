// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/unittest_util.h"

#include "gestures/include/gestures.h"

namespace gestures {

TestInterpreterWrapper::TestInterpreterWrapper(Interpreter* interpreter,
    const HardwareProperties* hwprops)
    : interpreter_(interpreter),
      hwprops_(hwprops) {
  Reset(interpreter);
}

TestInterpreterWrapper::TestInterpreterWrapper(Interpreter* interpreter)
    : interpreter_(interpreter),
      hwprops_(NULL) {
  Reset(interpreter);
}

void TestInterpreterWrapper::Reset(Interpreter* interpreter) {
  memset(&dummy_, 0, sizeof(HardwareProperties));
  if (!hwprops_)
    hwprops_ = &dummy_;

  interpreter_ = interpreter;
  if (interpreter_) {
    interpreter_->Initialize(hwprops_, this);
  }
}

void TestInterpreterWrapper::Reset(Interpreter* interpreter,
                                   const HardwareProperties* hwprops) {
  hwprops_ = hwprops;
  Reset(interpreter);
}

Gesture* TestInterpreterWrapper::SyncInterpret(HardwareState* state,
                                               stime_t* timeout) {
  gesture_ = Gesture();
  interpreter_->SyncInterpret(state, timeout);
  if (gesture_.type == kGestureTypeNull)
    return NULL;
  return &gesture_;
}

Gesture* TestInterpreterWrapper::HandleTimer(stime_t now, stime_t* timeout) {
  gesture_.type = kGestureTypeNull;
  interpreter_->HandleTimer(now, timeout);
  if (gesture_.type == kGestureTypeNull)
    return NULL;
  return &gesture_;
}

void TestInterpreterWrapper::ConsumeGesture(const Gesture& gesture) {
  Assert(gesture_.type == kGestureTypeNull);
  gesture_ = gesture;
}


}  // namespace gestures
