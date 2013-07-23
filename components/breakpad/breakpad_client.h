// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREAKPAD_BREAKPAD_CLIENT_H_
#define COMPONENTS_BREAKPAD_BREAKPAD_CLIENT_H_

#include <string>

#include "base/strings/string16.h"
#include "build/build_config.h"

namespace base {
class FilePath;
}

namespace breakpad {

class BreakpadClient;

// Setter and getter for the client.  The client should be set early, before any
// breakpad code is called, and should stay alive throughout the entire runtime.
void SetBreakpadClient(BreakpadClient* client);

// Breakpad's embedder API should only be used by breakpad.
BreakpadClient* GetBreakpadClient();

// Interface that the embedder implements.
class BreakpadClient {
 public:
  BreakpadClient();
  virtual ~BreakpadClient();

#if defined(OS_WIN)
  // Returns true if an alternative location to store the minidump files was
  // specified. Returns true if |crash_dir| was set.
  virtual bool GetAlternativeCrashDumpLocation(base::FilePath* crash_dir);

  // Returns a textual description of the product type and version to include
  // in the crash report.
  virtual void GetProductNameAndVersion(const base::FilePath& exe_path,
                                        base::string16* product_name,
                                        base::string16* version,
                                        base::string16* special_build,
                                        base::string16* channel_name);

  // Returns true if a restart dialog should be displayed. In that case,
  // |message| and |title| are set to a message to display in a dialog box with
  // the given title before restarting, and |is_rtl_locale| indicates whether
  // to display the text as RTL.
  virtual bool ShouldShowRestartDialog(base::string16* title,
                                       base::string16* message,
                                       bool* is_rtl_locale);

  // Returns true if it is ok to restart the application. Invoked right before
  // restarting after a crash.
  virtual bool AboutToRestart();

  // Returns a GUID to embed in the crash report.
  virtual base::string16 GetCrashGUID();

  // Returns true if the crash report uploader supports deferred uploads.
  virtual bool GetDeferredUploadsSupported(bool is_per_user_install);

  // Returns true if the running binary is a per-user installation.
  virtual bool GetIsPerUserInstall(const base::FilePath& exe_path);

  // Returns true if larger crash dumps should be dumped.
  virtual bool GetShouldDumpLargerDumps(bool is_per_user_install);
#endif

#if defined(OS_POSIX) && !defined(OS_MACOSX) && !defined(OS_IOS)
  // Returns a textual description of the product type and version to include
  // in the crash report.
  virtual void GetProductNameAndVersion(std::string* product_name,
                                        std::string* version);

  virtual base::FilePath GetReporterLogFilename();
#endif

  // The location where minidump files should be written. Returns true if
  // |crash_dir| was set.
  virtual bool GetCrashDumpLocation(base::FilePath* crash_dir);

#if defined(OS_POSIX)
  // Sets a function that'll be invoked to dump the current process when
  // without crashing.
  virtual void SetDumpWithoutCrashingFunction(void (*function)());
#endif

  // Register all of the potential crash keys that can be sent to the crash
  // reporting server. Returns the size of the union of all keys.
  virtual size_t RegisterCrashKeys();

  // Returns true if running in unattended mode (for automated testing).
  virtual bool IsRunningUnattended();

#if defined(OS_WIN) || defined(OS_MACOSX)
  // Returns true if the user has given consent to collect stats.
  virtual bool GetCollectStatsConsent();
#endif
};

}  // namespace breakpad

#endif  // COMPONENTS_BREAKPAD_BREAKPAD_CLIENT_H_
