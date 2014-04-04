// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/gesture_detection/gesture_event_data.h"

#include "base/logging.h"

namespace ui {

GestureEventData::GestureEventData(EventType type,
                                   int motion_event_id,
                                   base::TimeTicks time,
                                   float x,
                                   float y,
                                   const GestureEventDetails& details)
    : type(type),
      motion_event_id(motion_event_id),
      time(time),
      x(x),
      y(y),
      details(details) {
  DCHECK(motion_event_id >= 0);
  DCHECK(ET_GESTURE_TYPE_START <= type && type <= ET_GESTURE_TYPE_END);
}

GestureEventData::GestureEventData(EventType type,
                                   int motion_event_id,
                                   base::TimeTicks time,
                                   float x,
                                   float y)
    : type(type),
      motion_event_id(motion_event_id),
      time(time),
      x(x),
      y(y),
      details(GestureEventDetails(type, 0, 0)) {
  DCHECK(motion_event_id >= 0);
  DCHECK(ET_GESTURE_TYPE_START <= type && type <= ET_GESTURE_TYPE_END);
}

GestureEventData::GestureEventData() : type(ET_UNKNOWN), x(0), y(0) {}

}  //  namespace ui
