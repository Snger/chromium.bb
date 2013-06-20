// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/request_registry.h"

#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace google_apis {

RequestProgressStatus::RequestProgressStatus()
    : request_id(-1),
      transfer_state(REQUEST_NOT_STARTED) {
}

RequestRegistry::Request::Request(RequestRegistry* registry)
    : registry_(registry) {
}

RequestRegistry::Request::~Request() {
}

void RequestRegistry::Request::Cancel() {
  DoCancel();
  NotifyFinish(REQUEST_FAILED);
}

void RequestRegistry::Request::NotifyStart() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  // Some request_ids may be restarted. Report only the first "start".
  if (progress_status_.transfer_state == REQUEST_NOT_STARTED) {
    progress_status_.transfer_state = REQUEST_STARTED;
    registry_->OnRequestStart(this, &progress_status_.request_id);
  }
}

void RequestRegistry::Request::NotifyFinish(
    RequestTransferState status) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  progress_status_.transfer_state = status;
  registry_->OnRequestFinish(progress_status().request_id);
}

RequestRegistry::RequestRegistry() {
  in_flight_requests_.set_check_on_null_data(true);
}

RequestRegistry::~RequestRegistry() {
}

void RequestRegistry::CancelRequest(Request* request) {
  request->Cancel();
}

void RequestRegistry::OnRequestStart(
    RequestRegistry::Request* request,
    RequestID* id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  *id = in_flight_requests_.Add(request);
  DVLOG(1) << "Request[" << *id << "] started.";
}

void RequestRegistry::OnRequestFinish(RequestID id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  DVLOG(1) << "Request[" << id << "] finished.";
  if (in_flight_requests_.Lookup(id))
    in_flight_requests_.Remove(id);
}

}  // namespace google_apis
