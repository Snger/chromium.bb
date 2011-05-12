// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/quota/quota_manager.h"

#include <deque>
#include <algorithm>

#include "base/callback.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop_proxy.h"
#include "base/stl_util-inl.h"
#include "base/sys_info.h"
#include "net/base/net_util.h"
#include "webkit/quota/quota_database.h"
#include "webkit/quota/quota_types.h"
#include "webkit/quota/usage_tracker.h"

using base::ScopedCallbackFactory;

namespace quota {

namespace {
// Returns the initial size of the temporary storage quota.
// (This just gives a default initial size; once its initial size is determined
// it won't automatically be adjusted.)
int64 GetInitialTemporaryStorageQuotaSize(const FilePath& path) {
  int64 free_space = base::SysInfo::AmountOfFreeDiskSpace(path);
  // Returns 0 (disables the temporary storage) if the available space is
  // less than the twice of the default quota size.
  if (free_space < QuotaManager::kTemporaryStorageQuotaDefaultSize * 2) {
    return 0;
  }
  // Returns the default quota size while it is more than 5% of the
  // available space.
  if (free_space < QuotaManager::kTemporaryStorageQuotaDefaultSize * 20) {
    return QuotaManager::kTemporaryStorageQuotaDefaultSize;
  }
  // Returns the 5% of the available space while it does not exceed the
  // maximum quota size (1GB).
  if (free_space < QuotaManager::kTemporaryStorageQuotaMaxSize * 20) {
    return free_space / 20;
  }
  return QuotaManager::kTemporaryStorageQuotaMaxSize;
}

const int64 MBytes = 1024 * 1024;
}  // anonymous namespace

// TODO(kinuko): We will need to have different sizes for different platforms
// (e.g. larger for desktop etc) and may want to have them in preferences.
const int64 QuotaManager::kTemporaryStorageQuotaDefaultSize = 50 * MBytes;
const int64 QuotaManager::kTemporaryStorageQuotaMaxSize = 1 * 1024 * MBytes;
const char QuotaManager::kDatabaseName[] = "QuotaManager";

const int64 QuotaManager::kIncognitoDefaultTemporaryQuota = 5 * MBytes;

// This class is for posting GetUsage/GetQuota tasks, gathering
// results and dispatching GetAndQuota callbacks.
// This class is self-destructed.
class QuotaManager::UsageAndQuotaDispatcherTask : public QuotaTask {
 public:
  typedef std::deque<GetUsageAndQuotaCallback*> CallbackList;

  static UsageAndQuotaDispatcherTask* Create(
      QuotaManager* manager, const std::string& host, StorageType type);

  // Returns true if it is the first call for this task; which means
  // the caller needs to call Start().
  bool AddCallback(GetUsageAndQuotaCallback* callback) {
    callbacks_.push_back(callback);
    return (callbacks_.size() == 1);
  }

  void DidGetGlobalUsage(int64 usage) {
    global_usage_ = usage;
    CheckCompleted();
  }

  void DidGetHostUsage(const std::string& host_unused, int64 usage) {
    host_usage_ = usage;
    CheckCompleted();
  }

  void DidGetGlobalQuota(int64 quota) {
    quota_ = quota;
    CheckCompleted();
  }

  void DidGetHostQuota(const std::string& host_unused, int64 quota) {
    quota_ = quota;
    CheckCompleted();
  }

 protected:
  UsageAndQuotaDispatcherTask(
      QuotaManager* manager,
      const std::string& host,
      StorageType type)
      : QuotaTask(manager),
        host_(host),
        type_(type),
        quota_(-1),
        global_usage_(-1),
        host_usage_(-1),
        callback_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {}

  virtual ~UsageAndQuotaDispatcherTask() {
    STLDeleteContainerPointers(callbacks_.begin(), callbacks_.end());
  }

  virtual bool IsCompleted() const = 0;

  virtual void DispatchCallback(GetUsageAndQuotaCallback* callback) = 0;

  virtual void Aborted() OVERRIDE {
    for (CallbackList::iterator iter = callbacks_.begin();
        iter != callbacks_.end();
        ++iter) {
      (*iter)->Run(kQuotaErrorAbort, 0, 0);
      delete *iter;
    }
    callbacks_.clear();
    delete this;
  }

  virtual void Completed() OVERRIDE { delete this; }
  QuotaManager* manager() const {
    return static_cast<QuotaManager*>(observer());
  }

  std::string host() const { return host_; }
  StorageType type() const { return type_; }
  int64 quota() const { return quota_; }
  int64 global_usage() const { return global_usage_; }
  int64 host_usage() const { return host_usage_; }

  UsageCallback* NewGlobalUsageCallback() {
    return callback_factory_.NewCallback(
            &UsageAndQuotaDispatcherTask::DidGetGlobalUsage);
  }

  HostUsageCallback* NewHostUsageCallback() {
    return callback_factory_.NewCallback(
            &UsageAndQuotaDispatcherTask::DidGetHostUsage);
  }

  QuotaCallback* NewGlobalQuotaCallback() {
    return callback_factory_.NewCallback(
            &UsageAndQuotaDispatcherTask::DidGetGlobalQuota);
  }

  HostQuotaCallback* NewHostQuotaCallback() {
    return callback_factory_.NewCallback(
            &UsageAndQuotaDispatcherTask::DidGetHostQuota);
  }

 private:
  void CheckCompleted() {
    if (IsCompleted()) {
      // Dispatches callbacks.
      for (CallbackList::iterator iter = callbacks_.begin();
          iter != callbacks_.end();
          ++iter) {
        DispatchCallback(*iter);
        delete *iter;
      }
      callbacks_.clear();
      UsageAndQuotaDispatcherTaskMap& dispatcher_map =
          manager()->usage_and_quota_dispatchers_;
      DCHECK(dispatcher_map.find(std::make_pair(host_, type_)) !=
             dispatcher_map.end());
      dispatcher_map.erase(std::make_pair(host_, type_));
      CallCompleted();
    }
  }

  const std::string host_;
  const StorageType type_;
  int64 quota_;
  int64 global_usage_;
  int64 host_usage_;
  CallbackList callbacks_;
  ScopedCallbackFactory<UsageAndQuotaDispatcherTask> callback_factory_;

  DISALLOW_COPY_AND_ASSIGN(UsageAndQuotaDispatcherTask);
};

class QuotaManager::UsageAndQuotaDispatcherTaskForTemporary
    : public QuotaManager::UsageAndQuotaDispatcherTask {
 public:
  UsageAndQuotaDispatcherTaskForTemporary(
      QuotaManager* manager, const std::string host)
      : UsageAndQuotaDispatcherTask(manager, host, kStorageTypeTemporary) {}

 protected:
  virtual void Run() OVERRIDE {
    manager()->temporary_usage_tracker_->GetGlobalUsage(
        NewGlobalUsageCallback());
    manager()->temporary_usage_tracker_->GetHostUsage(
        host(), NewHostUsageCallback());
    manager()->GetTemporaryGlobalQuota(NewGlobalQuotaCallback());
  }

  virtual bool IsCompleted() const OVERRIDE {
    return (quota() >= 0 && global_usage() >= 0 && host_usage() >= 0);
  }

  virtual void DispatchCallback(GetUsageAndQuotaCallback* callback) OVERRIDE {
    // TODO(kinuko): For now it returns pessimistic quota.  Change this
    // to return {usage, quota - nonevictable_usage} once eviction is
    // supported.
    int64 other_usage = global_usage() - host_usage();
    callback->Run(kQuotaStatusOk, host_usage(), quota() - other_usage);
  }
};

class QuotaManager::UsageAndQuotaDispatcherTaskForPersistent
    : public QuotaManager::UsageAndQuotaDispatcherTask {
 public:
  UsageAndQuotaDispatcherTaskForPersistent(
      QuotaManager* manager, const std::string host)
      : UsageAndQuotaDispatcherTask(manager, host, kStorageTypePersistent) {}

 protected:
  virtual void Run() OVERRIDE {
    manager()->persistent_usage_tracker_->GetHostUsage(
        host(), NewHostUsageCallback());
    manager()->GetPersistentHostQuota(
        host(), NewHostQuotaCallback());
  }

  virtual bool IsCompleted() const OVERRIDE {
    return (quota() >= 0 && host_usage() >= 0);
  }

  virtual void DispatchCallback(GetUsageAndQuotaCallback* callback) OVERRIDE {
    callback->Run(kQuotaStatusOk, host_usage(), quota());
  }
};

// static
QuotaManager::UsageAndQuotaDispatcherTask*
QuotaManager::UsageAndQuotaDispatcherTask::Create(
    QuotaManager* manager, const std::string& host, StorageType type) {
  switch (type) {
    case kStorageTypeTemporary:
      return new UsageAndQuotaDispatcherTaskForTemporary(
          manager, host);
    case kStorageTypePersistent:
      return new UsageAndQuotaDispatcherTaskForPersistent(
          manager, host);
    default:
      NOTREACHED();
  }
  return NULL;
}

class QuotaManager::InitializeTask : public QuotaThreadTask {
 public:
  InitializeTask(
      QuotaManager* manager,
      QuotaDatabase* database,
      scoped_refptr<base::MessageLoopProxy> db_message_loop,
      const FilePath& profile_path)
      : QuotaThreadTask(manager, db_message_loop),
        manager_(manager),
        database_(database),
        profile_path_(profile_path),
        temporary_storage_quota_(-1),
        db_disabled_(false) {
    DCHECK(database_);
  }

 protected:
  virtual void RunOnTargetThread() OVERRIDE {
    // Initializes the global temporary quota.
    if (!database_->GetGlobalQuota(
            kStorageTypeTemporary, &temporary_storage_quota_)) {
      // If the temporary storage quota size has not been initialized,
      // make up one and store it in the database.
      temporary_storage_quota_ = GetInitialTemporaryStorageQuotaSize(
          profile_path_);
      if (!database_->SetGlobalQuota(
              kStorageTypeTemporary, temporary_storage_quota_)) {
        db_disabled_ = true;
      }
    }
  }

  virtual void Completed() OVERRIDE {
    DCHECK(manager_);
    if (manager_->temporary_global_quota_ < 0)
      manager_->DidGetTemporaryGlobalQuota(temporary_storage_quota_);
    manager_->db_initialized_ = !db_disabled_;
    manager_->db_disabled_ = db_disabled_;
  }

 private:
  QuotaManager* manager_;
  QuotaDatabase* database_;
  FilePath profile_path_;
  int64 temporary_storage_quota_;
  bool db_disabled_;
};

class QuotaManager::TemporaryGlobalQuotaUpdateTask : public QuotaThreadTask {
 public:
  TemporaryGlobalQuotaUpdateTask(
      QuotaManager* manager,
      QuotaDatabase* database,
      scoped_refptr<base::MessageLoopProxy> db_message_loop,
      int64 new_quota)
      : QuotaThreadTask(manager, db_message_loop),
        manager_(manager),
        database_(database),
        new_quota_(new_quota),
        db_disabled_(false) {
    DCHECK(database_);
  }

 protected:
  virtual void RunOnTargetThread() OVERRIDE {
    if (!database_->SetGlobalQuota(kStorageTypeTemporary, new_quota_))
      db_disabled_ = true;
  }

  virtual void Completed() OVERRIDE {
    DCHECK(manager_);
    manager_->db_disabled_ = db_disabled_;
  }

 private:
  QuotaManager* manager_;
  QuotaDatabase* database_;
  int64 new_quota_;
  bool db_disabled_;
};

QuotaManager::QuotaManager(bool is_incognito,
                           const FilePath& profile_path,
                           base::MessageLoopProxy* io_thread,
                           base::MessageLoopProxy* db_thread)
  : is_incognito_(is_incognito),
    profile_path_(profile_path),
    proxy_(new QuotaManagerProxy(
        ALLOW_THIS_IN_INITIALIZER_LIST(this), io_thread)),
    db_initialized_(false),
    db_disabled_(false),
    io_thread_(io_thread),
    db_thread_(db_thread),
    temporary_global_quota_(-1) {
}

QuotaManager::~QuotaManager() {
  DCHECK(io_thread_->BelongsToCurrentThread());
  proxy_->manager_ = NULL;
  std::for_each(clients_.begin(), clients_.end(),
                std::mem_fun(&QuotaClient::OnQuotaManagerDestroyed));
  if (database_.get())
    db_thread_->DeleteSoon(FROM_HERE, database_.release());
}

void QuotaManager::GetUsageAndQuota(
    const GURL& origin, StorageType type,
    GetUsageAndQuotaCallback* callback_ptr) {
  scoped_ptr<GetUsageAndQuotaCallback> callback(callback_ptr);
  LazyInitialize();
  if (is_incognito_) {
    int64 quota = 0;
    if (type == kStorageTypeTemporary)
      quota = clients_.size() * kIncognitoDefaultTemporaryQuota;
    // TODO(kinuko): This does not return useful usage value for now.
    callback->Run(kQuotaStatusOk, 0, quota);
    return;
  }

  if (type == kStorageTypeUnknown) {
    // Quota only supports temporary/persistent types.
    callback->Run(kQuotaErrorNotSupported, 0, 0);
    return;
  }

  std::string host = net::GetHostOrSpecFromURL(origin);
  UsageAndQuotaDispatcherTaskMap::iterator found =
      usage_and_quota_dispatchers_.find(std::make_pair(host, type));
  if (found == usage_and_quota_dispatchers_.end()) {
    UsageAndQuotaDispatcherTask* dispatcher =
        UsageAndQuotaDispatcherTask::Create(this, host, type);
    found = usage_and_quota_dispatchers_.insert(
        std::make_pair(std::make_pair(host, type), dispatcher)).first;
  }
  if (found->second->AddCallback(callback.release()))
    found->second->Start();
}

void QuotaManager::RequestQuota(
    const GURL& origin, StorageType type,
    int64 requested_size,
    RequestQuotaCallback* callback) {
  LazyInitialize();
  // TODO(kinuko): implement me.
  callback->Run(kQuotaErrorNotSupported, 0);
  delete callback;
}

void QuotaManager::GetTemporaryGlobalQuota(QuotaCallback* callback) {
  LazyInitialize();
  if (temporary_global_quota_ >= 0) {
    // TODO(kinuko): The in-memory quota value should be periodically
    // updated not to exceed the current available space in the hard drive.
    callback->Run(temporary_global_quota_);
    delete callback;
    return;
  }
  // They are called upon completion of InitializeTask.
  temporary_global_quota_callbacks_.Add(callback);
}

void QuotaManager::SetTemporaryGlobalQuota(int64 new_quota) {
  LazyInitialize();
  DCHECK(new_quota >= 0);
  DidGetTemporaryGlobalQuota(new_quota);
  if (!db_disabled_) {
    scoped_refptr<TemporaryGlobalQuotaUpdateTask> task(
        new TemporaryGlobalQuotaUpdateTask(
            this, database_.get(), db_thread_, new_quota));
    task->Start();
  }
}

void QuotaManager::GetPersistentHostQuota(const std::string& host,
                                          HostQuotaCallback* callback) {
  LazyInitialize();
  std::map<std::string, int64>::iterator found =
      persistent_host_quota_.find(host);
  if (found != persistent_host_quota_.end()) {
    callback->Run(host, found->second);
    delete callback;
    return;
  }
  if (persistent_host_quota_callbacks_.Add(host, callback)) {
    // This is the first call for this host.
    // TODO(kinuko): Dispatch a task to get the host quota for the host
    // once QuotaDatabase is updated to accept hosts instead of origins.
  }
}

void QuotaManager::SetPersistentHostQuota(const std::string& host,
                                          int64 new_quota) {
  LazyInitialize();
  // TODO(kinuko): Implement once QuotaDatabase is updated.
}

void QuotaManager::LazyInitialize() {
  DCHECK(io_thread_->BelongsToCurrentThread());
  if (database_.get()) {
    // Initialization seems to be done already.
    return;
  }

  if (is_incognito_)
    return;

  database_.reset(new QuotaDatabase(profile_path_.AppendASCII(kDatabaseName)));

  temporary_usage_tracker_.reset(
      new UsageTracker(clients_, kStorageTypeTemporary));
  persistent_usage_tracker_.reset(
      new UsageTracker(clients_, kStorageTypePersistent));

  scoped_refptr<InitializeTask> task(
      new InitializeTask(this, database_.get(), db_thread_, profile_path_));
  task->Start();
}

void QuotaManager::RegisterClient(QuotaClient* client) {
  DCHECK(io_thread_->BelongsToCurrentThread());
  DCHECK(!database_.get());
  clients_.push_back(client);
}

void QuotaManager::NotifyStorageAccessed(
    QuotaClient::ID client_id,
    const GURL& origin, StorageType type) {
  // TODO(michaeln): write me
}

void QuotaManager::NotifyStorageModified(
    QuotaClient::ID client_id,
    const GURL& origin, StorageType type, int64 delta) {
  LazyInitialize();
  UsageTracker* tracker = GetUsageTracker(type);
  DCHECK(tracker);
  tracker->UpdateUsageCache(client_id, origin, delta);
}

void QuotaManager::NotifyOriginInUse(const GURL& origin) {
  DCHECK(io_thread_->BelongsToCurrentThread());
  origins_in_use_[origin]++;
}

void QuotaManager::NotifyOriginNoLongerInUse(const GURL& origin) {
  DCHECK(io_thread_->BelongsToCurrentThread());
  DCHECK(IsOriginInUse(origin));
  int& count = origins_in_use_[origin];
  if (--count == 0)
    origins_in_use_.erase(origin);
}

UsageTracker* QuotaManager::GetUsageTracker(StorageType type) const {
  switch (type) {
    case kStorageTypeTemporary:
      return temporary_usage_tracker_.get();
    case kStorageTypePersistent:
      return persistent_usage_tracker_.get();
    default:
      NOTREACHED();
  }
  return NULL;
}

void QuotaManager::DidGetTemporaryGlobalQuota(int64 quota) {
  temporary_global_quota_ = quota;
  temporary_global_quota_callbacks_.Run(quota);
}

void QuotaManager::DidGetPersistentHostQuota(const std::string& host,
                                             int64 quota) {
  DCHECK(persistent_host_quota_.find(host) == persistent_host_quota_.end());
  persistent_host_quota_[host] = quota;
  persistent_host_quota_callbacks_.Run(host, host, quota);
}

void QuotaManager::DeleteOnCorrectThread() const {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->DeleteSoon(FROM_HERE, this);
    return;
  }
  delete this;
}

// QuotaManagerProxy ----------------------------------------------------------

void QuotaManagerProxy::RegisterClient(QuotaClient* client) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &QuotaManagerProxy::RegisterClient, client));
    return;
  }
  if (manager_)
    manager_->RegisterClient(client);
  else
    client->OnQuotaManagerDestroyed();
}

void QuotaManagerProxy::NotifyStorageAccessed(
    QuotaClient::ID client_id,
    const GURL& origin,
    StorageType type) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &QuotaManagerProxy::NotifyStorageAccessed,
        client_id, origin, type));
    return;
  }
  if (manager_)
    manager_->NotifyStorageAccessed(client_id, origin, type);
}

void QuotaManagerProxy::NotifyStorageModified(
    QuotaClient::ID client_id,
    const GURL& origin,
    StorageType type,
    int64 delta) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &QuotaManagerProxy::NotifyStorageModified,
        client_id, origin, type, delta));
    return;
  }
  if (manager_)
    manager_->NotifyStorageModified(client_id, origin, type, delta);
}

void QuotaManagerProxy::NotifyOriginInUse(
    const GURL& origin) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &QuotaManagerProxy::NotifyOriginInUse, origin));
    return;
  }
  if (manager_)
    manager_->NotifyOriginInUse(origin);
}

void QuotaManagerProxy::NotifyOriginNoLongerInUse(
    const GURL& origin) {
  if (!io_thread_->BelongsToCurrentThread()) {
    io_thread_->PostTask(FROM_HERE, NewRunnableMethod(
        this, &QuotaManagerProxy::NotifyOriginNoLongerInUse, origin));
    return;
  }
  if (manager_)
    manager_->NotifyOriginNoLongerInUse(origin);
}

QuotaManager* QuotaManagerProxy::quota_manager() const {
  DCHECK(!io_thread_ || io_thread_->BelongsToCurrentThread());
  return manager_;
}

QuotaManagerProxy::QuotaManagerProxy(
    QuotaManager* manager, base::MessageLoopProxy* io_thread)
    : manager_(manager), io_thread_(io_thread) {
}

QuotaManagerProxy::~QuotaManagerProxy() {
}

}  // namespace quota
