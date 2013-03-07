// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_service.h"

#include "chrome/browser/instant/instant_io_context.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ntp/thumbnail_source.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/url_data_source.h"

using content::BrowserThread;

InstantService::InstantService(Profile* profile)
    : profile_(profile) {
  // Stub for unit tests.
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI))
    return;

  registrar_.Add(this,
                 content::NOTIFICATION_RENDERER_PROCESS_TERMINATED,
                 content::NotificationService::AllSources());

  instant_io_context_ = new InstantIOContext();

  if (profile_ && profile_->GetResourceContext()) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&InstantIOContext::SetUserDataOnIO,
                   profile->GetResourceContext(), instant_io_context_));
  }

  content::URLDataSource::Add(profile, new ThumbnailSource(profile));
}

InstantService::~InstantService() {
}

void InstantService::AddInstantProcess(int process_id) {
  process_ids_.insert(process_id);

  if (instant_io_context_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&InstantIOContext::AddInstantProcessOnIO,
                   instant_io_context_, process_id));
  }
}

bool InstantService::IsInstantProcess(int process_id) const {
  return process_ids_.find(process_id) != process_ids_.end();
}

void InstantService::Shutdown() {
  process_ids_.clear();

  if (instant_io_context_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&InstantIOContext::ClearInstantProcessesOnIO,
                   instant_io_context_));
  }
  instant_io_context_ = NULL;
}

void InstantService::Observe(int type,
                             const content::NotificationSource& source,
                             const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_RENDERER_PROCESS_TERMINATED);
  int process_id = content::Source<content::RenderProcessHost>(source)->GetID();
  process_ids_.erase(process_id);

  if (instant_io_context_) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(&InstantIOContext::RemoveInstantProcessOnIO,
                   instant_io_context_, process_id));
  }
}
