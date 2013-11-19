// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_
#define CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_

#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/lazy_instance.h"
#include "content/public/browser/trace_subscriber.h"
#include "content/public/browser/tracing_controller.h"

namespace content {

class TraceMessageFilter;

class TracingControllerImpl : public TracingController {
 public:
  static TracingControllerImpl* GetInstance();

  // TracingController implementation.
  virtual void GetCategories(
      const GetCategoriesDoneCallback& callback) OVERRIDE;
  virtual bool EnableRecording(
      const base::debug::CategoryFilter& filter,
      TracingController::Options options,
      const EnableRecordingDoneCallback& callback) OVERRIDE;
  virtual bool DisableRecording(
      const base::FilePath& result_file_path,
      const TracingFileResultCallback& callback) OVERRIDE;
  virtual bool EnableMonitoring(const base::debug::CategoryFilter& filter,
      TracingController::Options options,
      const EnableMonitoringDoneCallback& callback) OVERRIDE;
  virtual bool DisableMonitoring(
      const DisableMonitoringDoneCallback& callback) OVERRIDE;
  virtual void GetMonitoringStatus(
      bool* out_enabled,
      base::debug::CategoryFilter* out_filter,
      TracingController::Options* out_options) OVERRIDE;
  virtual void CaptureMonitoringSnapshot(
      const base::FilePath& result_file_path,
      const TracingFileResultCallback& callback) OVERRIDE;

 private:
  typedef std::set<scoped_refptr<TraceMessageFilter> > FilterMap;
  class ResultFile;

  friend struct base::DefaultLazyInstanceTraits<TracingControllerImpl>;
  friend class TraceMessageFilter;

  TracingControllerImpl();
  virtual ~TracingControllerImpl();

  bool can_enable_recording() const {
    return !is_recording_;
  }

  bool can_disable_recording() const {
    return is_recording_ && !result_file_;
  }

  bool can_enable_monitoring() const {
    return !is_monitoring_;
  }

  bool can_disable_monitoring() const {
    return is_monitoring_ && !monitoring_snapshot_file_;
  }

  // Methods for use by TraceMessageFilter.
  void AddFilter(TraceMessageFilter* filter);
  void RemoveFilter(TraceMessageFilter* filter);

  void OnTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr);
  void OnMonitoringTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr);

  // Callback of TraceLog::Flush() for the local trace.
  void OnLocalTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr,
      bool has_more_events);
  // Callback of TraceLog::FlushMonitoring() for the local trace.
  void OnLocalMonitoringTraceDataCollected(
      const scoped_refptr<base::RefCountedString>& events_str_ptr,
      bool has_more_events);

  void OnDisableRecordingAcked(
      const std::vector<std::string>& known_category_groups);
  void OnResultFileClosed();

  void OnCaptureMonitoringSnapshotAcked();
  void OnMonitoringSnapshotFileClosed();

  FilterMap filters_;
  // Pending acks for DisableRecording.
  int pending_disable_recording_ack_count_;
  // Pending acks for CaptureMonitoringSnapshot.
  int pending_capture_monitoring_snapshot_ack_count_;
  bool is_recording_;
  bool is_monitoring_;
  GetCategoriesDoneCallback pending_get_categories_done_callback_;
  TracingFileResultCallback pending_disable_recording_done_callback_;
  TracingFileResultCallback pending_capture_monitoring_snapshot_done_callback_;
  std::set<std::string> known_category_groups_;
  base::debug::TraceLog::Options trace_options_;
  base::debug::CategoryFilter category_filter_;
  scoped_ptr<ResultFile> result_file_;
  scoped_ptr<ResultFile> monitoring_snapshot_file_;
  DISALLOW_COPY_AND_ASSIGN(TracingControllerImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_TRACING_TRACING_CONTROLLER_IMPL_H_
