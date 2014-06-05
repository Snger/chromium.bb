// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/app/shell_main_delegate.h"
#include "apps/shell/browser/shell_browser_main_delegate.h"
#include "apps/shell/browser/shell_desktop_controller.h"
#include "athena/content/public/content_activity_factory.h"
#include "athena/main/athena_launcher.h"
#include "athena/main/placeholder.h"
#include "athena/main/placeholder_content.h"
#include "content/public/app/content_main.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/core/visibility_controller.h"

class AthenaBrowserMainDelegate : public apps::ShellBrowserMainDelegate {
 public:
  AthenaBrowserMainDelegate() {}
  virtual ~AthenaBrowserMainDelegate() {}

  // apps::ShellBrowserMainDelegate:
  virtual void Start(content::BrowserContext* context) OVERRIDE {
    athena::StartAthena(
        apps::ShellDesktopController::instance()->host()->window());
    athena::ActivityFactory::RegisterActivityFactory(
        new athena::ContentActivityFactory());
    CreateTestWindows();
    CreateTestPages(context);
  }

  virtual void Shutdown() OVERRIDE {
    // TODO(mukai):cleanup the start/shutdown processes and the dependency to
    // ContentActivityFactory.
    athena::ActivityFactory::Shutdown();
    athena::ShutdownAthena();
  }

  virtual apps::ShellDesktopController* CreateDesktopController() OVERRIDE {
    // TODO(mukai): create Athena's own ShellDesktopController subclass so that
    // it can initialize its own window manager logic.
    return new apps::ShellDesktopController();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaBrowserMainDelegate);
};

class AthenaMainDelegate : public apps::ShellMainDelegate {
 public:
  AthenaMainDelegate() {}
  virtual ~AthenaMainDelegate() {}

 private:
  // apps::ShellMainDelegate:
  virtual apps::ShellBrowserMainDelegate* CreateShellBrowserMainDelegate()
      OVERRIDE {
    return new AthenaBrowserMainDelegate();
  }

  DISALLOW_COPY_AND_ASSIGN(AthenaMainDelegate);
};

int main(int argc, const char** argv) {
  AthenaMainDelegate delegate;
  content::ContentMainParams params(&delegate);

  params.argc = argc;
  params.argv = argv;

  return content::ContentMain(params);
}
