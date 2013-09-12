// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_PROCESS_LOGGING_H_
#define CHROME_COMMON_CHILD_PROCESS_LOGGING_H_

#include <set>
#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/debug/crash_logging.h"
#include "base/strings/string16.h"

class CommandLine;

// The maximum number of variation chunks we will report.
// Also used in chrome/app, but we define it here to avoid a common->app
// dependency.
static const size_t kMaxReportedVariationChunks = 15;

// The maximum size of a variation chunk. This size was picked to be
// consistent between platforms and the value was chosen from the Windows
// limit of google_breakpad::CustomInfoEntry::kValueMaxLength.
static const size_t kMaxVariationChunkSize = 64;

// The maximum number of prn-info-* records.
static const size_t kMaxReportedPrinterRecords = 4;

// The maximum number of command line switches to include in the crash
// report's metadata. Note that the mini-dump itself will also contain the
// (original) command line arguments within the PEB.
// Also used in chrome/app, but we define it here to avoid a common->app
// dependency.
static const size_t kMaxSwitches = 15;

namespace child_process_logging {

#if defined(OS_POSIX) && !defined(OS_MACOSX)
// These are declared here so the crash reporter can access them directly in
// compromised context without going through the standard library.
extern char g_channel[];
extern char g_client_id[];
extern char g_num_switches[];
extern char g_num_variations[];
extern char g_printer_info[];
extern char g_switches[];
extern char g_variation_chunks[];

// Assume command line switches are less than 64 chars.
static const size_t kSwitchLen = 64;

// Assume printer info strings are less than 64 chars.
static const size_t kPrinterInfoStrLen = 64;
#endif

// Sets the Client ID that is used as GUID if a Chrome process crashes.
void SetClientId(const std::string& client_id);

// Gets the Client ID to be used as GUID for crash reporting. Returns the client
// id in |client_id| if it's known, an empty string otherwise.
std::string GetClientId();

// Sets the data on the printer to send along with crash reports. Data may be
// separated by ';' up to kMaxReportedPrinterRecords strings. Each substring
// would be cut to 63 chars.
void SetPrinterInfo(const char* printer_info);

// Sets the command line arguments to send along with crash reports to the
// values in |command_line|.
void SetCommandLine(const CommandLine* command_line);

// Initialize the list of experiment info to send along with crash reports.
void SetExperimentList(const std::vector<string16>& state);

#if defined(OS_LINUX) || defined(OS_OPENBSD) || defined(OS_MACOSX)
// Sets the product channel data to send along with crash reports to |channel|.
void SetChannel(const std::string& channel);
#endif

// Set/clear information about currently accessed printer.
class ScopedPrinterInfoSetter {
 public:
  explicit ScopedPrinterInfoSetter(const std::string& printer_info) {
    SetPrinterInfo(printer_info.c_str());
  }

  ~ScopedPrinterInfoSetter() {
    SetPrinterInfo("");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ScopedPrinterInfoSetter);
};

}  // namespace child_process_logging

#if defined(OS_WIN)
namespace child_process_logging {

// Sets up the base/debug/crash_logging.h mechanism.
void Init();

}  // namespace child_process_logging
#endif  // defined(OS_WIN)

#endif  // CHROME_COMMON_CHILD_PROCESS_LOGGING_H_
