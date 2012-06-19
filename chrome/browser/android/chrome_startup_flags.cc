// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <jni.h>

#include "chrome/browser/android/chrome_startup_flags.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/common/chrome_switches.h"

namespace {

void SetCommandLineSwitch(const std::string& switch_string) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (!command_line->HasSwitch(switch_string))
    command_line->AppendSwitch(switch_string);
}

bool IsTabletUi() {
  NOTIMPLEMENTED() << "TODO(yfriedman): Upstream this";
  return false;
}
} // namespace

void SetChromeSpecificCommandLineFlags() {
  CommandLine* parsed_command_line = CommandLine::ForCurrentProcess();

  // Always enable SPDY
  parsed_command_line->AppendSwitch(switches::kEnableNpn);

  // Turn on autofill
  SetCommandLineSwitch(switches::kExternalAutofillPopup);

  // Tablet UI switch (used for using correct version of NTP HTML).
  if (IsTabletUi())
    parsed_command_line->AppendSwitch(switches::kTabletUi);

  // TODO(jcivelli): Enable the History Quick Provider and figure out
  //                 why it reports the wrong results for some pages.
  parsed_command_line->AppendSwitch(switches::kDisableHistoryQuickProvider);

  // Enable prerender for the omnibox.
  parsed_command_line->AppendSwitchASCII(
      switches::kPrerenderMode, switches::kPrerenderModeSwitchValueEnabled);
  parsed_command_line->AppendSwitchASCII(
      switches::kPrerenderFromOmnibox,
      switches::kPrerenderFromOmniboxSwitchValueEnabled);
}
