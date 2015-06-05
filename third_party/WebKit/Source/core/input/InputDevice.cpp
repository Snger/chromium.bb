// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/input/InputDevice.h"

namespace blink {

InputDevice::InputDevice(bool firesTouchEvents)
{
    m_firesTouchEvents = firesTouchEvents;
}

InputDevice::InputDevice(const InputDeviceInit& initializer)
{
    m_firesTouchEvents = initializer.firesTouchEvents();
}

InputDevice::~InputDevice()
{
}

} // namespace blink
