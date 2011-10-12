// Copyright (c) 2011 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gestures/include/accel_filter_interpreter.h"

#include <algorithm>
#include <math.h>

#include "gestures/include/gestures.h"
#include "gestures/include/interpreter.h"
#include "gestures/include/logging.h"

namespace gestures {

// Takes ownership of |next|:
AccelFilterInterpreter::AccelFilterInterpreter(PropRegistry* prop_reg,
                                               Interpreter* next)
    : sensitivity_(prop_reg, "Sensitivity", 3),
      custom_point_str_(prop_reg, "Pointer Accel Curve", ""),
      custom_scroll_str_(prop_reg, "Scroll Accel Curve", "") {
  next_.reset(next);

  // Set up default curves.

  // Our pointing curves are the following.
  // x = input speed of movement (mm/s, always >= 0), y = output speed (mm/s)
  // 1: y = x (No acceleration)
  // 2: y = 32x/60   (x < 32), x^2/60   (x < 150), linear with same slope after
  // 3: y = 32x/37.5 (x < 32), x^2/37.5 (x < 150), linear with same slope after
  // 4: y = 32x/30   (x < 32), x^2/30   (x < 150), linear with same slope after
  // 5: y = 32x/25   (x < 32), x^2/25   (x < 150), linear with same slope after

  const float divisors[] = { 0.0, // unused
                             60.0, 37.5, 30.0, 25.0 };  // used


  // i starts as 1 b/c we skip the first slot, since the default is fine for it.
  for (size_t i = 1; i < kMaxAccelCurves; ++i) {
    const float divisor = divisors[i];
    const float linear_until_x = 32.0;
    const float init_slope = linear_until_x / divisor;
    curves_[i][0] = CurveSegment(linear_until_x, 0, init_slope, 0);
    const float x_border = 150;
    curves_[i][1] = CurveSegment(x_border, 1 / divisor, 0, 0);
    const float slope = x_border * 2 / divisor;
    const float y_at_border = x_border * x_border / divisor;
    const float icept = y_at_border - slope * x_border;
    curves_[i][2] = CurveSegment(INFINITY, 0, slope, icept);
  }
}

Gesture* AccelFilterInterpreter::SyncInterpret(HardwareState* hwstate,
                                               stime_t* timeout) {
  Gesture* fg = next_->SyncInterpret(hwstate, timeout);
  if (fg)
    ScaleGesture(fg);
  return fg;
}

Gesture* AccelFilterInterpreter::HandleTimer(stime_t now, stime_t* timeout) {
  Gesture* gs = next_->HandleTimer(now, timeout);
  if (gs)
    ScaleGesture(gs);
  return gs;
}

void AccelFilterInterpreter::ParseCurveString(const char* input,
                                              char* cache,
                                              CurveSegment* out_segs) {
  if (!strncmp(input, cache, strlen(input)))
    return;  // cache hit
  memset(cache, 0, kCacheStrLen);
  strncpy(cache, input, kCacheStrLen - 1);
  // input must be a space-separated list of x, y coord pairs
  float prev_x = 0.0;
  float prev_y = 0.0;
  const char* ptr = input;
  size_t i = 0;
  while (i < kMaxCustomCurveSegs) {
    float cur_x = static_cast<float>(atof(ptr));
    ptr = strchr(ptr, ' ');
    if (!ptr)
      break;
    ++ptr;
    float cur_y = static_cast<float>(atof(ptr));
    float dx = cur_x - prev_x;
    float dy = cur_y - prev_y;
    float slope = dy / dx;
    float icept = cur_y - cur_x * slope;
    out_segs[i] = CurveSegment(cur_x, 0.0, slope, icept);
    ++i;
    ptr = strchr(ptr, ' ');
    if (!ptr)
      break;
    ++ptr;
    prev_x = cur_x;
    prev_y = cur_y;
  }
  if (i == 0)
    out_segs[0] = CurveSegment(INFINITY, 0.0, 1.0, 0.0);  // Sane default
  else
    out_segs[i - 1].x_ = INFINITY;  // Extend final segment
}

void AccelFilterInterpreter::ScaleGesture(Gesture* gs) {
  CurveSegment* segs = NULL;
  float* dx = NULL;
  float* dy = NULL;
  float dt = gs->end_time - gs->start_time;
  size_t max_segs = kMaxCurveSegs;

  switch (gs->type) {
    case kGestureTypeMove:
      dx = &gs->details.move.dx;
      dy = &gs->details.move.dy;
      if (sensitivity_.val_ >= 1 && sensitivity_.val_ <= 5) {
        segs = curves_[sensitivity_.val_ - 1];
      } else {
        segs = custom_point_;
        ParseCurveString(custom_point_str_.val_,
                         last_parsed_custom_point_str_,
                         custom_point_);
        max_segs = kMaxCustomCurveSegs;
      }
      break;
    case kGestureTypeScroll:
      dx = &gs->details.scroll.dx;
      dy = &gs->details.scroll.dy;
      if (sensitivity_.val_ >= 1 && sensitivity_.val_ <= 5) {
        segs = curves_[sensitivity_.val_ - 1];
      } else {
        segs = custom_scroll_;
        ParseCurveString(custom_scroll_str_.val_,
                         last_parsed_custom_scroll_str_,
                         custom_scroll_);
        max_segs = kMaxCustomCurveSegs;
      }
      break;
    default:  // Nothing to accelerate
      return;
  }

  if (dt < 0.00001)
    return;  // Avoid division by 0
  float mag = sqrtf(*dx * *dx + *dy * *dy) / dt;
  if (mag < 0.00001)
    return;  // Avoid division by 0
  for (size_t i = 0; i < max_segs; ++i) {
    if (mag > segs[i].x_)
      continue;
    float ratio = segs[i].sqr_ * mag + segs[i].mul_ + segs[i].int_ / mag;
    *dx *= ratio;
    *dy *= ratio;

    return;
  }
  Err("Overflowed acceleration curve!");
}

void AccelFilterInterpreter::SetHardwareProperties(
    const HardwareProperties& hw_props) {
  next_->SetHardwareProperties(hw_props);
}

}  // namespace gestures
