// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_logging.h"

#import <Foundation/Foundation.h>

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/installer/util/google_update_settings.h"

namespace child_process_logging {

using base::debug::SetCrashKeyValueFuncT;
using base::debug::ClearCrashKeyValueFuncT;
using base::debug::SetCrashKeyValue;
using base::debug::ClearCrashKey;

const char* kGuidParamName = "guid";
const char* kPrinterInfoNameFormat = "prn-info-%zu";

// Account for the terminating null character.
static const size_t kClientIdSize = 32 + 1;
static char g_client_id[kClientIdSize];

void SetClientIdImpl(const std::string& client_id,
                     SetCrashKeyValueFuncT set_key_func) {
  set_key_func(kGuidParamName, client_id);
}

void SetClientId(const std::string& client_id) {
  std::string str(client_id);
  ReplaceSubstringsAfterOffset(&str, 0, "-", "");

  base::strlcpy(g_client_id, str.c_str(), kClientIdSize);
    SetClientIdImpl(str, SetCrashKeyValue);

  std::wstring wstr = ASCIIToWide(str);
  GoogleUpdateSettings::SetMetricsId(wstr);
}

std::string GetClientId() {
  return std::string(g_client_id);
}

void SetPrinterInfo(const char* printer_info) {
  std::vector<std::string> info;
  base::SplitString(printer_info, ';', &info);
  info.resize(kMaxReportedPrinterRecords);
  for (size_t i = 0; i < info.size(); ++i) {
    std::string key = base::StringPrintf(kPrinterInfoNameFormat, i);
    if (!info[i].empty()) {
      SetCrashKeyValue(key, info[i]);
    } else {
      ClearCrashKey(key);
    }
  }
}

void SetCommandLine(const CommandLine* command_line) {
  DCHECK(command_line);
  if (!command_line)
    return;

  // These should match the corresponding strings in breakpad_win.cc.
  const char* kNumSwitchesKey = "num-switches";
  const char* kSwitchKeyFormat = "switch-%zu";  // 1-based.

  // Note the total number of switches, not including the exec path.
  const CommandLine::StringVector& argv = command_line->argv();
  SetCrashKeyValue(kNumSwitchesKey,
                   base::StringPrintf("%zu", argv.size() - 1));

  size_t key_i = 0;
  for (size_t i = 1; i < argv.size() && key_i < kMaxSwitches; ++i, ++key_i) {
    // TODO(shess): Skip boring switches.
    std::string key = base::StringPrintf(kSwitchKeyFormat, key_i + 1);
    SetCrashKeyValue(key, argv[i]);
  }

  // Clear out any stale keys.
  for (; key_i < kMaxSwitches; ++key_i) {
    std::string key = base::StringPrintf(kSwitchKeyFormat, key_i + 1);
    ClearCrashKey(key);
  }
}

void SetExperimentList(const std::vector<string16>& experiments) {
  // These should match the corresponding strings in breakpad_win.cc.
  const char* kNumExperimentsKey = "num-experiments";
  const char* kExperimentChunkFormat = "experiment-chunk-%zu";  // 1-based

  std::vector<string16> chunks;
  chrome_variations::GenerateVariationChunks(experiments, &chunks);

  // Store up to |kMaxReportedVariationChunks| chunks.
  for (size_t i = 0; i < kMaxReportedVariationChunks; ++i) {
    std::string key = base::StringPrintf(kExperimentChunkFormat, i + 1);
    if (i < chunks.size()) {
      std::string value = UTF16ToUTF8(chunks[i]);
      SetCrashKeyValue(key, value);
    } else {
      ClearCrashKey(key);
    }
  }

  // Make note of the total number of experiments, which may be greater than
  // what was able to fit in |kMaxReportedVariationChunks|. This is useful when
  // correlating stability with the number of experiments running
  // simultaneously.
  SetCrashKeyValue(kNumExperimentsKey,
                   base::StringPrintf("%zu", experiments.size()));
}

}  // namespace child_process_logging
