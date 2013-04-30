// Copyright (c) 2012 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/filter_interpreter.h"

#include <base/values.h>

namespace gestures {

void FilterInterpreter::SyncInterpretImpl(HardwareState* hwstate,
                                          stime_t* timeout) {
  next_->SyncInterpret(hwstate, timeout);
}

void FilterInterpreter::HandleTimerImpl(stime_t now, stime_t* timeout) {
  next_->HandleTimer(now, timeout);
}

void FilterInterpreter::Initialize(const HardwareProperties* hwprops,
                                   GestureConsumer* consumer) {
  Interpreter::Initialize(hwprops, consumer);
  if (next_)
    next_->Initialize(hwprops, this);
}

void FilterInterpreter::ConsumeGesture(const Gesture& gesture) {
  ProduceGesture(gesture);
}

DictionaryValue* FilterInterpreter::EncodeCommonInfo() {
  DictionaryValue *root = Interpreter::EncodeCommonInfo();
#ifdef DEEP_LOGS
  root->Set(ActivityLog::kKeyNext, next_->EncodeCommonInfo());
#endif
  return root;
}

void FilterInterpreter::Clear() {
  if (log_.get())
    log_->Clear();
  next_->Clear();
}
}  // namespace gestures
