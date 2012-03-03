// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_idle_logout.h"

#include "base/bind.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/kiosk_mode/kiosk_mode_helper.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "chrome/browser/ui/webui/chromeos/idle_logout_dialog.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

namespace {

const int64 kLoginIdleTimeout = 100; // seconds

}  // namespace

namespace browser {

void ShowIdleLogoutDialog() {
  IdleLogoutDialog::ShowIdleLogoutDialog();
}

void CloseIdleLogoutDialog() {
  IdleLogoutDialog::CloseIdleLogoutDialog();
}

}  // namespace browser

namespace chromeos {

KioskModeIdleLogout::KioskModeIdleLogout() {
  if (chromeos::KioskModeHelper::Get()->is_initialized())
    Setup();
  else
    chromeos::KioskModeHelper::Get()->Initialize(
        base::Bind(&KioskModeIdleLogout::Setup,
                   base::Unretained(this)));
}

void KioskModeIdleLogout::Setup() {
  registrar_.Add(this, chrome::NOTIFICATION_LOGIN_USER_CHANGED,
                 content::NotificationService::AllSources());
}

void KioskModeIdleLogout::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  if (type == chrome::NOTIFICATION_LOGIN_USER_CHANGED) {
    // Register our observers only when a user logs on.
    chromeos::PowerManagerClient* power_manager =
        chromeos::DBusThreadManager::Get()->GetPowerManagerClient();
    if (!power_manager->HasObserver(this))
      power_manager->AddObserver(this);

    // Register for the next Idle for kLoginIdleTimeout event.
    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
        RequestIdleNotification(
            chromeos::KioskModeHelper::Get()->GetIdleLogoutTimeout() * 1000);
  }
}

void KioskModeIdleLogout::IdleNotify(int64 threshold) {
  // We're idle, next time we go active, we need to know so we can remove
  // the logout dialog if it's still up.
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RequestActiveNotification();

  browser::ShowIdleLogoutDialog();
}

void KioskModeIdleLogout::ActiveNotify() {
  // Before anything else, close the logout dialog to prevent restart
  browser::CloseIdleLogoutDialog();

  // Now that we're active, register a request for notification for
  // the next time we go idle for kLoginIdleTimeout seconds.
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
      RequestIdleNotification(
          chromeos::KioskModeHelper::Get()->GetIdleLogoutTimeout() * 1000);
}

static base::LazyInstance<KioskModeIdleLogout>
    g_kiosk_mode_idle_logout = LAZY_INSTANCE_INITIALIZER;

void InitializeKioskModeIdleLogout() {
  g_kiosk_mode_idle_logout.Get();
}

}  // namespace chromeos
