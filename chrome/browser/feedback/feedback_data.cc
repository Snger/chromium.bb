// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/feedback_data.h"

#include "base/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/feedback/feedback_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#endif

using content::BrowserThread;

namespace {

const char kMultilineIndicatorString[] = "<multiline>\n";
const char kMultilineStartString[] = "---------- START ----------\n";
const char kMultilineEndString[] = "---------- END ----------\n\n";

std::string LogsToString(SystemLogsMap* sys_info) {
  std::string syslogs_string;
  for (SystemLogsMap::const_iterator it = sys_info->begin();
      it != sys_info->end(); ++it) {
    std::string key = it->first;
    std::string value = it->second;

    TrimString(key, "\n ", &key);
    TrimString(value, "\n ", &value);

    if (value.find("\n") != std::string::npos) {
      syslogs_string.append(
          key + "=" + kMultilineIndicatorString +
          kMultilineStartString +
          value + "\n" +
          kMultilineEndString);
    } else {
      syslogs_string.append(key + "=" + value + "\n");
    }
  }
  return syslogs_string;
}

void ZipLogs(SystemLogsMap* sys_info, std::string* compressed_logs) {
  DCHECK(compressed_logs);
  std::string logs_string = LogsToString(sys_info);
  if (!feedback_util::ZipString(logs_string, compressed_logs)) {
    compressed_logs->clear();
  }
}

}  // namespace

FeedbackData::FeedbackData() : profile_(NULL),
                               feedback_page_data_complete_(false),
                               syslogs_compression_complete_(false) {
}

FeedbackData::~FeedbackData() {
}

bool FeedbackData::IsDataComplete() {
  return (syslogs_compression_complete_ || !sys_info_.get()) &&
      feedback_page_data_complete_;
}
void FeedbackData::SendReport() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (IsDataComplete())
    feedback_util::SendReport(this);
}

void FeedbackData::OnFeedbackPageDataComplete() {
  feedback_page_data_complete_ = true;
  SendReport();
}

void FeedbackData::set_sys_info(
    scoped_ptr<SystemLogsMap> sys_info) {
  if (sys_info.get())
    CompressSyslogs(sys_info.Pass());
}

void FeedbackData::CompressSyslogs(
    scoped_ptr<SystemLogsMap> sys_info) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // We get the pointer first since base::Passed will nullify the scoper, hence
  // it's not safe to use <scoper>.get() as a parameter to PostTaskAndReply.
  SystemLogsMap* sys_info_ptr = sys_info.get();
  std::string* compressed_logs_ptr = new std::string;
  scoped_ptr<std::string> compressed_logs(compressed_logs_ptr);
  BrowserThread::PostBlockingPoolTaskAndReply(
      FROM_HERE,
      base::Bind(&ZipLogs,
                 sys_info_ptr,
                 compressed_logs_ptr),
      base::Bind(&FeedbackData::OnCompressLogsComplete,
                 this,
                 base::Passed(&sys_info),
                 base::Passed(&compressed_logs)));
}

void FeedbackData::OnCompressLogsComplete(
    scoped_ptr<SystemLogsMap> sys_info,
    scoped_ptr<std::string> compressed_logs) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  sys_info_ = sys_info.Pass();
  compressed_logs_ = compressed_logs.Pass();
  syslogs_compression_complete_ = true;

  SendReport();
}
