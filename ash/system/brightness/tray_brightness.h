// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_BRIGHTNESS_TRAY_BRIGHTNESS_H_
#define ASH_SYSTEM_BRIGHTNESS_TRAY_BRIGHTNESS_H_
#pragma once

#include "ash/system/tray/system_tray_item.h"
#include "base/memory/scoped_ptr.h"

class TrayBrightness : public ash::SystemTrayItem {
 public:
  TrayBrightness();
  virtual ~TrayBrightness();

 private:
  // Overridden from ash::SystemTrayItem
  virtual views::View* CreateTrayView() OVERRIDE;
  virtual views::View* CreateDefaultView() OVERRIDE;
  virtual views::View* CreateDetailedView() OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TrayBrightness);
};

#endif  // ASH_SYSTEM_BRIGHTNESS_TRAY_BRIGHTNESS_H_
