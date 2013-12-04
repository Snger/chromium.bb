// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_APP_SHELL_BROWSER_CONTEXT_H_
#define APPS_SHELL_APP_SHELL_BROWSER_CONTEXT_H_

#include "base/compiler_specific.h"
#include "content/shell/browser/shell_browser_context.h"

class PrefService;

namespace apps {

// The BrowserContext used by the content, apps and extensions systems in
// app_shell.
class AppShellBrowserContext : public content::ShellBrowserContext {
 public:
  AppShellBrowserContext();
  virtual ~AppShellBrowserContext();

  // HACK: Pad the virtual function table so we trip an assertion if someone
  // tries to use |this| as a Profile.
  virtual void ProfileFunctionCallOnNonProfileBrowserContext1();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext2();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext3();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext4();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext5();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext6();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext7();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext8();
  virtual void ProfileFunctionCallOnNonProfileBrowserContext9();

 private:
  scoped_ptr<PrefService> prefs_;

  DISALLOW_COPY_AND_ASSIGN(AppShellBrowserContext);
};

}  // namespace apps

#endif  // APPS_SHELL_APP_SHELL_BROWSER_CONTEXT_H_
