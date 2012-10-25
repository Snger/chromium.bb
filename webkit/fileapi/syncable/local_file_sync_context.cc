// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/syncable/local_file_sync_context.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/task_runner_util.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/local_file_change_tracker.h"
#include "webkit/fileapi/syncable/syncable_file_operation_runner.h"

namespace fileapi {

namespace {
const int kMaxConcurrentSyncableOperation = 3;
}  // namespace

LocalFileSyncContext::LocalFileSyncContext(
    base::SingleThreadTaskRunner* ui_task_runner,
    base::SingleThreadTaskRunner* io_task_runner)
    : ui_task_runner_(ui_task_runner),
      io_task_runner_(io_task_runner),
      shutdown_on_ui_(false) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
}

void LocalFileSyncContext::MaybeInitializeFileSystemContext(
    const GURL& source_url,
    FileSystemContext* file_system_context,
    const StatusCallback& callback) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  if (ContainsKey(file_system_contexts_, file_system_context)) {
    DCHECK(!ContainsKey(origin_to_contexts_, source_url) ||
           origin_to_contexts_[source_url] == file_system_context);
    origin_to_contexts_[source_url] = file_system_context;
    // The context has been already initialized. Just dispatch the callback
    // with SYNC_STATUS_OK.
    ui_task_runner_->PostTask(FROM_HERE, base::Bind(callback, SYNC_STATUS_OK));
    return;
  }

  StatusCallbackQueue& callback_queue =
      pending_initialize_callbacks_[file_system_context];
  callback_queue.push_back(callback);
  if (callback_queue.size() > 1)
    return;

  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalFileSyncContext::InitializeFileSystemContextOnIOThread,
                 this, source_url, make_scoped_refptr(file_system_context)));
}

void LocalFileSyncContext::ShutdownOnUIThread() {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  shutdown_on_ui_ = true;
  io_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalFileSyncContext::ShutdownOnIOThread,
                 this));
}

void LocalFileSyncContext::PrepareForSync(
    const FileSystemURL& url,
    const ChangeListCallback& callback) {
  // This is initially called on UI thread and to be relayed to IO thread.
  if (!io_task_runner_->RunsTasksOnCurrentThread()) {
    DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&LocalFileSyncContext::PrepareForSync, this, url, callback));
    return;
  }
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  if (sync_status()->IsWriting(url)) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(callback, SYNC_STATUS_FILE_BUSY, FileChangeList()));
    return;
  }
  sync_status()->StartSyncing(url);
  ui_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&LocalFileSyncContext::DidDisabledWritesForPrepareForSync,
                 this, url, callback));
}

void LocalFileSyncContext::RegisterURLForWaitingSync(
    const FileSystemURL& url,
    const base::Closure& on_syncable_callback) {
  // This is initially called on UI thread and to be relayed to IO thread.
  if (!io_task_runner_->RunsTasksOnCurrentThread()) {
    DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
    io_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&LocalFileSyncContext::RegisterURLForWaitingSync,
                   this, url, on_syncable_callback));
    return;
  }
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  if (sync_status()->IsWritable(url)) {
    // No need to register; fire the callback now.
    ui_task_runner_->PostTask(FROM_HERE, on_syncable_callback);
    return;
  }
  url_waiting_sync_on_io_ = url;
  url_syncable_callback_ = on_syncable_callback;
}

base::WeakPtr<SyncableFileOperationRunner>
LocalFileSyncContext::operation_runner() const {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  if (operation_runner_.get())
    return operation_runner_->AsWeakPtr();
  return base::WeakPtr<SyncableFileOperationRunner>();
}

LocalFileSyncStatus* LocalFileSyncContext::sync_status() const {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  return sync_status_.get();
}

void LocalFileSyncContext::OnSyncEnabled(const FileSystemURL& url) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  if (url_syncable_callback_.is_null() ||
      sync_status()->IsWriting(url_waiting_sync_on_io_))
    return;
  // TODO(kinuko): may want to check how many pending tasks we have.
  sync_status()->StartSyncing(url_waiting_sync_on_io_);
  ui_task_runner_->PostTask(FROM_HERE, url_syncable_callback_);
  url_syncable_callback_.Reset();
}

void LocalFileSyncContext::OnWriteEnabled(const FileSystemURL& url) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  // Nothing to do for now.
}

LocalFileSyncContext::~LocalFileSyncContext() {
}

void LocalFileSyncContext::ShutdownOnIOThread() {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  operation_runner_.reset();
  sync_status_.reset();
}

void LocalFileSyncContext::InitializeFileSystemContextOnIOThread(
    const GURL& source_url,
    FileSystemContext* file_system_context) {
  DCHECK(io_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(file_system_context);
  if (!file_system_context->change_tracker()) {
    // Create and initialize LocalFileChangeTracker and call back this method
    // later again.
    scoped_ptr<LocalFileChangeTracker>* tracker_ptr(
        new scoped_ptr<LocalFileChangeTracker>);
    base::PostTaskAndReplyWithResult(
        file_system_context->task_runners()->file_task_runner(),
        FROM_HERE,
        base::Bind(&LocalFileSyncContext::InitializeChangeTrackerOnFileThread,
                   this, tracker_ptr,
                   make_scoped_refptr(file_system_context)),
        base::Bind(&LocalFileSyncContext::DidInitializeChangeTracker, this,
                   base::Owned(tracker_ptr),
                   source_url,
                   make_scoped_refptr(file_system_context)));
    return;
  }
  if (!operation_runner_.get()) {
    DCHECK(!sync_status_.get());
    sync_status_.reset(new LocalFileSyncStatus);
    operation_runner_.reset(new SyncableFileOperationRunner(
            kMaxConcurrentSyncableOperation,
            sync_status_.get()));
    sync_status_->AddObserver(this);
  }
  file_system_context->set_sync_context(this);
  DidInitialize(source_url, file_system_context, SYNC_STATUS_OK);
}

SyncStatusCode LocalFileSyncContext::InitializeChangeTrackerOnFileThread(
    scoped_ptr<LocalFileChangeTracker>* tracker_ptr,
    FileSystemContext* file_system_context) {
  DCHECK(file_system_context);
  DCHECK(tracker_ptr);
  tracker_ptr->reset(new LocalFileChangeTracker(
          file_system_context->partition_path(),
          file_system_context->task_runners()->file_task_runner()));
  return (*tracker_ptr)->Initialize(file_system_context);
}

void LocalFileSyncContext::DidInitializeChangeTracker(
    scoped_ptr<LocalFileChangeTracker>* tracker_ptr,
    const GURL& source_url,
    FileSystemContext* file_system_context,
    SyncStatusCode status) {
  DCHECK(file_system_context);
  if (status != SYNC_STATUS_OK) {
    DidInitialize(source_url, file_system_context, status);
    return;
  }
  file_system_context->SetLocalFileChangeTracker(tracker_ptr->Pass());
  InitializeFileSystemContextOnIOThread(source_url, file_system_context);
}

void LocalFileSyncContext::DidInitialize(
    const GURL& source_url,
    FileSystemContext* file_system_context,
    SyncStatusCode status) {
  if (!ui_task_runner_->RunsTasksOnCurrentThread()) {
    ui_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&LocalFileSyncContext::DidInitialize,
                   this, source_url,
                   make_scoped_refptr(file_system_context), status));
    return;
  }
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!ContainsKey(file_system_contexts_, file_system_context));
  DCHECK(ContainsKey(pending_initialize_callbacks_, file_system_context));
  DCHECK(file_system_context->change_tracker());

  file_system_contexts_.insert(file_system_context);

  DCHECK(!ContainsKey(origin_to_contexts_, source_url));
  origin_to_contexts_[source_url] = file_system_context;

  StatusCallbackQueue& callback_queue =
      pending_initialize_callbacks_[file_system_context];
  for (StatusCallbackQueue::iterator iter = callback_queue.begin();
       iter != callback_queue.end(); ++iter) {
    ui_task_runner_->PostTask(FROM_HERE, base::Bind(*iter, status));
  }
  pending_initialize_callbacks_.erase(file_system_context);
}

void LocalFileSyncContext::DidDisabledWritesForPrepareForSync(
    const FileSystemURL& url,
    const ChangeListCallback& callback) {
  DCHECK(ui_task_runner_->RunsTasksOnCurrentThread());
  if (shutdown_on_ui_) {
    callback.Run(SYNC_STATUS_ABORT, FileChangeList());
    return;
  }
  DCHECK(ContainsKey(origin_to_contexts_, url.origin()));
  FileSystemContext* context = origin_to_contexts_[url.origin()];
  DCHECK(context);
  DCHECK(context->change_tracker());

  FileChangeList changes;
  context->change_tracker()->GetChangesForURL(url, &changes);
  callback.Run(SYNC_STATUS_OK, changes);
}

}  // namespace fileapi
