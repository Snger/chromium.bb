// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include "base/time.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/system/statistics_provider.h"
#include "chromeos/chromeos_constants.h"

base::TimeDelta
ChromeViewsDelegate::GetDefaultTextfieldObscuredRevealDuration() {
  // Enable password echo during OOBE when keyboard driven flag is set.
  if (chromeos::UserManager::IsInitialized() &&
      !chromeos::UserManager::Get()->IsUserLoggedIn() &&
      !chromeos::StartupUtils::IsOobeCompleted()) {
    bool keyboard_driven_oobe = false;
    chromeos::system::StatisticsProvider::GetInstance()->GetMachineFlag(
        chromeos::kOemKeyboardDrivenOobeKey, &keyboard_driven_oobe);
    if (keyboard_driven_oobe)
      return base::TimeDelta::FromSeconds(1);
  }

  return base::TimeDelta();
}
