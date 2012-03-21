// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ash/chrome_shell_delegate.h"

#include "ash/launcher/launcher_types.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/wm/partial_screenshot_view.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/ash/app_list/app_list_view_delegate.h"
#include "chrome/browser/ui/views/ash/launcher/chrome_launcher_delegate.h"
#include "chrome/browser/ui/views/ash/status_area_host_aura.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"
#include "ui/aura/window.h"

#if defined(OS_CHROMEOS)
#include "base/chromeos/chromeos_version.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/system/ash_system_tray_delegate.h"
#endif
namespace {

// Returns a list of Aura windows from a BrowserList, using either a
// const_iterator or const_reverse_iterator.
template<typename IT>
std::vector<aura::Window*> GetBrowserWindows(IT begin, IT end) {
  std::vector<aura::Window*> windows;
  for (IT it = begin; it != end; ++it) {
    Browser* browser = *it;
    if (browser && browser->window()->GetNativeHandle())
      windows.push_back(browser->window()->GetNativeHandle());
  }
  return windows;
}

}  // namespace

// static
ChromeShellDelegate* ChromeShellDelegate::instance_ = NULL;

ChromeShellDelegate::ChromeShellDelegate() {
  instance_ = this;
#if defined(OS_CHROMEOS)
  registrar_.Add(
      this,
      chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED,
      content::NotificationService::AllSources());
#endif
}

ChromeShellDelegate::~ChromeShellDelegate() {
  if (instance_ == this)
    instance_ = NULL;
}

StatusAreaView* ChromeShellDelegate::GetStatusArea() {
  return status_area_host_->GetStatusArea();
}

views::Widget* ChromeShellDelegate::CreateStatusArea() {
  status_area_host_.reset(new StatusAreaHostAura());
  views::Widget* status_area_widget =
      status_area_host_.get()->CreateStatusArea();
  return status_area_widget;
}

bool ChromeShellDelegate::CanCreateLauncher() {
#if defined(OS_CHROMEOS)
  // When running a Chrome OS build outside of a device (i.e. on a developer's
  // workstation), pretend like we're always logged in.
  if (!base::chromeos::IsRunningOnChromeOS())
    return true;

  return chromeos::UserManager::Get()->IsUserLoggedIn();
#else
  return true;
#endif
}

#if defined(OS_CHROMEOS)
void ChromeShellDelegate::LockScreen() {
  if (!CommandLine::ForCurrentProcess()->HasSwitch(switches::kGuestSession)) {
    chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->
        NotifyScreenLockRequested();
  }
}
#endif

void ChromeShellDelegate::Exit() {
  BrowserList::AttemptUserExit();
}

ash::AppListViewDelegate*
ChromeShellDelegate::CreateAppListViewDelegate() {
  // Shell will own the created delegate.
  return new AppListViewDelegate;
}

std::vector<aura::Window*> ChromeShellDelegate::GetCycleWindowList(
    CycleSource source) const {
  // BrowserList maintains a list of browsers sorted by activity.
  return GetBrowserWindows(BrowserList::begin_last_active(),
                           BrowserList::end_last_active());
}

void ChromeShellDelegate::StartPartialScreenshot(
    ash::ScreenshotDelegate* screenshot_delegate) {
  ash::PartialScreenshotView::StartPartialScreenshot(screenshot_delegate);
}

ash::LauncherDelegate* ChromeShellDelegate::CreateLauncherDelegate(
    ash::LauncherModel* model) {
  ChromeLauncherDelegate* delegate = new ChromeLauncherDelegate(NULL, model);
  delegate->Init();
  return delegate;
}

ash::SystemTrayDelegate* ChromeShellDelegate::CreateSystemTrayDelegate(
    ash::SystemTray* tray) {
#if defined(OS_CHROMEOS)
  return chromeos::CreateSystemTrayDelegate(tray);
#else
  return NULL;
#endif
}

void ChromeShellDelegate::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
#if defined(OS_CHROMEOS)
  switch (type) {
    case chrome::NOTIFICATION_LOGIN_USER_PROFILE_PREPARED:
      ash::Shell::GetInstance()->CreateLauncher();
      break;
    default:
      NOTREACHED() << "Unexpected notification " << type;
  }
#else
  // MSVC++ warns about switch statements without any cases.
  NOTREACHED() << "Unexpected notification " << type;
#endif
}
