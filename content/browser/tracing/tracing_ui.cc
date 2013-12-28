// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/tracing/tracing_ui.h"

#include <string>

#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/tracing_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/common/url_constants.h"
#include "grit/tracing_resources.h"

namespace content {
namespace {

void OnGotCategories(const WebUIDataSource::GotDataCallback& callback,
                     const std::set<std::string>& categorySet) {

  scoped_ptr<base::ListValue> category_list(new base::ListValue());
  for (std::set<std::string>::const_iterator it = categorySet.begin();
       it != categorySet.end(); it++) {
    category_list->AppendString(*it);
  }

  base::RefCountedString* res = new base::RefCountedString();
  base::JSONWriter::Write(category_list.get(), &res->data());
  callback.Run(res);
}

bool GetTracingOptions(const std::string& data64,
                       std::string* category_filter_string,
                       int* tracing_options)
{
  std::string data;
  if (!base::Base64Decode(data64, &data)) {
    LOG(ERROR) << "Options were not base64 encoded.";
    return false;
  }

  scoped_ptr<base::Value> optionsRaw(base::JSONReader::Read(data));
  if (!optionsRaw) {
    LOG(ERROR) << "Options were not valid JSON";
    return false;
  }
  base::DictionaryValue* options;
  if (!optionsRaw->GetAsDictionary(&options)) {
    LOG(ERROR) << "Options must be dict";
    return false;
  }

  bool use_system_tracing;
  bool use_continuous_tracing;
  bool use_sampling;

  bool options_ok = true;
  options_ok &= options->GetString("categoryFilter", category_filter_string);
  options_ok &= options->GetBoolean("useSystemTracing", &use_system_tracing);
  options_ok &= options->GetBoolean("useContinuousTracing",
                                    &use_continuous_tracing);
  options_ok &= options->GetBoolean("useSampling", &use_sampling);
  if (!options_ok) {
    LOG(ERROR) << "Malformed options";
    return false;
  }

  *tracing_options = 0;
  if (use_system_tracing)
    *tracing_options |= TracingController::ENABLE_SYSTRACE;
  if (use_sampling)
    *tracing_options |= TracingController::ENABLE_SAMPLING;
  if (use_continuous_tracing)
    *tracing_options |= TracingController::RECORD_CONTINUOUSLY;
  return true;
}

void OnRecordingEnabledAck(const WebUIDataSource::GotDataCallback& callback);

bool OnBeginRecording(const std::string& data64,
                      const WebUIDataSource::GotDataCallback& callback) {
  std::string category_filter_string;
  int tracing_options = 0;
  if (!GetTracingOptions(data64, &category_filter_string, &tracing_options))
    return false;

  return TracingController::GetInstance()->EnableRecording(
      category_filter_string,
      static_cast<TracingController::Options>(tracing_options),
      base::Bind(&OnRecordingEnabledAck, callback));
}

void OnRecordingEnabledAck(const WebUIDataSource::GotDataCallback& callback) {
  base::RefCountedString* res = new base::RefCountedString();
  callback.Run(res);
}

void OnTraceBufferPercentFullResult(
    const WebUIDataSource::GotDataCallback& callback, float result) {
  std::string str = base::DoubleToString(result);
  callback.Run(base::RefCountedString::TakeString(&str));
}

void ReadRecordingResult(const WebUIDataSource::GotDataCallback& callback,
                         const base::FilePath& path) {
  std::string tmp;
  if (!base::ReadFileToString(path, &tmp))
    LOG(ERROR) << "Failed to read file " << path.value();
  base::DeleteFile(path, false);
  callback.Run(base::RefCountedString::TakeString(&tmp));
}

void BeginReadingRecordingResult(
    const WebUIDataSource::GotDataCallback& callback,
    const base::FilePath& path) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(ReadRecordingResult, callback, path));
}

void OnMonitoringEnabledAck(const WebUIDataSource::GotDataCallback& callback);

bool OnEnableMonitoring(const std::string& data64,
                        const WebUIDataSource::GotDataCallback& callback) {
  std::string category_filter_string;
  int tracing_options = 0;
  if (!GetTracingOptions(data64, &category_filter_string, &tracing_options))
    return false;

  return TracingController::GetInstance()->EnableMonitoring(
      category_filter_string,
      static_cast<TracingController::Options>(tracing_options),
      base::Bind(OnMonitoringEnabledAck, callback));
}

void OnMonitoringEnabledAck(const WebUIDataSource::GotDataCallback& callback) {
  base::RefCountedString* res = new base::RefCountedString();
  callback.Run(res);
}

void OnMonitoringDisabled(const WebUIDataSource::GotDataCallback& callback) {
  base::RefCountedString* res = new base::RefCountedString();
  callback.Run(res);
}

void ReadMonitoringSnapshot(const WebUIDataSource::GotDataCallback& callback,
                            const base::FilePath& path) {
  std::string tmp;
  if (!base::ReadFileToString(path, &tmp))
    LOG(ERROR) << "Failed to read file " << path.value();
  base::DeleteFile(path, false);
  callback.Run(base::RefCountedString::TakeString(&tmp));
}

void OnMonitoringSnapshotCaptured(
    const WebUIDataSource::GotDataCallback& callback,
    const base::FilePath& path) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(ReadMonitoringSnapshot, callback, path));
}

bool OnTracingRequest(const std::string& path,
                    const WebUIDataSource::GotDataCallback& callback) {
  if (path == "json/categories") {
    TracingController::GetInstance()->GetCategories(
        base::Bind(OnGotCategories, callback));
    return true;
  }

  const char* beginRecordingPath = "json/begin_recording?";
  if (path.find(beginRecordingPath) == 0) {
    std::string data = path.substr(strlen(beginRecordingPath));
    return OnBeginRecording(data, callback);
  }
  if (path == "json/get_buffer_percent_full") {
    return TracingController::GetInstance()->GetTraceBufferPercentFull(
        base::Bind(OnTraceBufferPercentFullResult, callback));
  }
  if (path == "json/end_recording") {
    return TracingController::GetInstance()->DisableRecording(
        base::FilePath(), base::Bind(BeginReadingRecordingResult, callback));
  }

  const char* enableMonitoringPath = "json/begin_monitoring?";
  if (path.find(enableMonitoringPath) == 0) {
    std::string data = path.substr(strlen(enableMonitoringPath));
    return OnEnableMonitoring(data, callback);
  }
  if (path == "json/end_monitoring") {
    return TracingController::GetInstance()->DisableMonitoring(
        base::Bind(OnMonitoringDisabled, callback));
  }
  if (path == "json/capture_monitoring") {
    TracingController::GetInstance()->CaptureMonitoringSnapshot(
        base::FilePath(), base::Bind(OnMonitoringSnapshotCaptured, callback));
    return true;
  }

  if (StartsWithASCII(path, "json/", true))
    LOG(ERROR) << "Unhandled request to " << path;
  return false;
}

}  // namespace


////////////////////////////////////////////////////////////////////////////////
//
// TracingUI
//
////////////////////////////////////////////////////////////////////////////////

TracingUI::TracingUI(WebUI* web_ui) : WebUIController(web_ui) {
  // Set up the chrome://tracing/ source.
  BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();

  WebUIDataSource* source = WebUIDataSource::Create(kChromeUITracingHost);
  source->SetJsonPath("strings.js");
  source->SetDefaultResource(IDR_TRACING_HTML);
  source->AddResourcePath("tracing.js", IDR_TRACING_JS);
  source->SetRequestFilter(base::Bind(OnTracingRequest));
  WebUIDataSource::Add(browser_context, source);
}

}  // namespace content
