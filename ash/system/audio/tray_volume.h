// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_TRAY_VOLUME_H_
#define ASH_SYSTEM_AUDIO_TRAY_VOLUME_H_
#pragma once

#include "ash/system/audio/audio_controller.h"
#include "ash/system/tray/system_tray_item.h"
#include "base/memory/scoped_ptr.h"

namespace ash {
namespace internal {

namespace tray {
class VolumeView;
}

class TrayVolume : public SystemTrayItem,
                   public AudioController {
 public:
  TrayVolume();
  virtual ~TrayVolume();

 private:
  // Overridden from SystemTrayItem.
  virtual views::View* CreateTrayView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyTrayView() OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;

  // Overridden from AudioController.
  virtual void OnVolumeChanged(float percent) OVERRIDE;

  scoped_ptr<tray::VolumeView> volume_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayVolume);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_TRAY_VOLUME_H_
