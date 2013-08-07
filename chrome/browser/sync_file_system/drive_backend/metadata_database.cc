// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend/metadata_database.h"

#include <stack>

#include "base/bind.h"
#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_database.pb.h"
#include "chrome/browser/sync_file_system/drive_backend/metadata_db_migration_util.h"
#include "chrome/browser/sync_file_system/logger.h"
#include "chrome/browser/sync_file_system/syncable_file_system_util.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/write_batch.h"
#include "webkit/common/fileapi/file_system_util.h"

namespace sync_file_system {
namespace drive_backend {

const char kDatabaseVersionKey[] = "VERSION";
const int64 kCurrentDatabaseVersion = 3;
const char kServiceMetadataKey[] = "SERVICE";
const char kFileMetadataKeyPrefix[] = "FILE: ";
const char kFileTrackerKeyPrefix[] = "TRACKER: ";

struct DatabaseContents {
  scoped_ptr<ServiceMetadata> service_metadata;
  ScopedVector<FileMetadata> file_metadata;
  ScopedVector<FileTracker> file_trackers;
};

namespace {

typedef MetadataDatabase::FileByID FileByID;
typedef MetadataDatabase::TrackerByID TrackerByID;
typedef MetadataDatabase::TrackersByParentAndTitle TrackersByParentAndTitle;
typedef MetadataDatabase::TrackersByTitle TrackersByTitle;

std::string RemovePrefix(const std::string& str, const std::string& prefix) {
  if (StartsWithASCII(str, prefix, true))
    return str.substr(prefix.size());
  return str;
}

base::FilePath ReverseConcatPathComponents(
    const std::vector<base::FilePath>& components) {
  if (components.empty())
    return base::FilePath(FILE_PATH_LITERAL("/")).NormalizePathSeparators();

  size_t total_size = 0;
  typedef std::vector<base::FilePath> PathComponents;
  for (PathComponents::const_iterator itr = components.begin();
       itr != components.end(); ++itr)
    total_size += itr->value().size() + 1;

  base::FilePath::StringType result;
  result.reserve(total_size);
  for (PathComponents::const_reverse_iterator itr = components.rbegin();
       itr != components.rend(); ++itr) {
    result.append(1, base::FilePath::kSeparators[0]);
    result.append(itr->value());
  }

  return base::FilePath(result).NormalizePathSeparators();
}

void AdaptLevelDBStatusToSyncStatusCode(const SyncStatusCallback& callback,
                                        const leveldb::Status& status) {
  callback.Run(LevelDBStatusToSyncStatusCode(status));
}

void PutServiceMetadataToBatch(const ServiceMetadata& service_metadata,
                               leveldb::WriteBatch* batch) {
  std::string value;
  bool success = service_metadata.SerializeToString(&value);
  DCHECK(success);
  batch->Put(kServiceMetadataKey, value);
}

void PutFileToBatch(const FileMetadata& file, leveldb::WriteBatch* batch) {
  std::string value;
  bool success = file.SerializeToString(&value);
  DCHECK(success);
  batch->Put(kFileMetadataKeyPrefix + file.file_id(), value);
}

void PutTrackerToBatch(const FileTracker& tracker, leveldb::WriteBatch* batch) {
  std::string value;
  bool success = tracker.SerializeToString(&value);
  DCHECK(success);
  batch->Put(kFileTrackerKeyPrefix + base::Int64ToString(tracker.tracker_id()),
             value);
}

void PutFileDeletionToBatch(const std::string& file_id,
                            leveldb::WriteBatch* batch) {
  batch->Delete(kFileMetadataKeyPrefix + file_id);
}

void PutTrackerDeletionToBatch(int64 tracker_id, leveldb::WriteBatch* batch) {
  batch->Delete(kFileTrackerKeyPrefix + base::Int64ToString(tracker_id));
}

std::string GetTrackerTitle(const FileTracker& tracker) {
  if (tracker.has_synced_details())
    return tracker.synced_details().title();
  return std::string();
}

// Returns true if |db| has no content.
bool IsDatabaseEmpty(leveldb::DB* db) {
  DCHECK(db);
  scoped_ptr<leveldb::Iterator> itr(db->NewIterator(leveldb::ReadOptions()));
  itr->SeekToFirst();
  return !itr->Valid();
}

SyncStatusCode OpenDatabase(const base::FilePath& path,
                            scoped_ptr<leveldb::DB>* db_out,
                            bool* created) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(db_out);
  DCHECK(created);

  leveldb::Options options;
  options.max_open_files = 0;  // Use minimum.
  options.create_if_missing = true;
  leveldb::DB* db = NULL;
  leveldb::Status db_status =
      leveldb::DB::Open(options, path.AsUTF8Unsafe(), &db);
  SyncStatusCode status = LevelDBStatusToSyncStatusCode(db_status);
  if (status != SYNC_STATUS_OK) {
    delete db;
    return status;
  }

  *created = IsDatabaseEmpty(db);
  db_out->reset(db);
  return status;
}

SyncStatusCode MigrateDatabaseIfNeeded(leveldb::DB* db) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(db);
  std::string value;
  leveldb::Status status =
      db->Get(leveldb::ReadOptions(), kDatabaseVersionKey, &value);
  int64 version = 0;
  if (status.ok()) {
    if (!base::StringToInt64(value, &version))
      return SYNC_DATABASE_ERROR_FAILED;
  } else {
    if (!status.IsNotFound())
      return SYNC_DATABASE_ERROR_FAILED;
  }

  switch (version) {
    case 0:
      drive_backend::MigrateDatabaseFromV0ToV1(db);
      // fall-through
    case 1:
      drive_backend::MigrateDatabaseFromV1ToV2(db);
      // fall-through
    case 2:
      // TODO(tzik): Migrate database from version 2 to 3.
      //   * Add sync-root folder as active, dirty and needs_folder_listing
      //     folder.
      //   * Add app-root folders for each origins.  Each app-root folder for
      //     an enabled origin should be a active, dirty and
      //     needs_folder_listing folder.  And Each app-root folder for a
      //     disabled origin should be an inactive, dirty and
      //     non-needs_folder_listing folder.
      //   * Add a file metadata for each file in previous version.
      NOTIMPLEMENTED();
      return SYNC_DATABASE_ERROR_FAILED;
      // fall-through
    case 3:
      DCHECK_EQ(3, kCurrentDatabaseVersion);
      return SYNC_STATUS_OK;
    default:
      return SYNC_DATABASE_ERROR_FAILED;
  }
}

SyncStatusCode WriteVersionInfo(leveldb::DB* db) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(db);
  return LevelDBStatusToSyncStatusCode(
      db->Put(leveldb::WriteOptions(),
              kDatabaseVersionKey,
              base::Int64ToString(kCurrentDatabaseVersion)));
}

SyncStatusCode ReadDatabaseContents(leveldb::DB* db,
                                    DatabaseContents* contents) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(db);
  DCHECK(contents);

  scoped_ptr<leveldb::Iterator> itr(db->NewIterator(leveldb::ReadOptions()));
  for (itr->SeekToFirst(); itr->Valid(); itr->Next()) {
    std::string key = itr->key().ToString();
    std::string value = itr->value().ToString();
    if (key == kServiceMetadataKey) {
      scoped_ptr<ServiceMetadata> service_metadata(new ServiceMetadata);
      if (!service_metadata->ParseFromString(value)) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse SyncServiceMetadata");
        continue;
      }

      contents->service_metadata = service_metadata.Pass();
      continue;
    }

    if (StartsWithASCII(key, kFileMetadataKeyPrefix, true)) {
      std::string file_id = RemovePrefix(key, kFileMetadataKeyPrefix);

      scoped_ptr<FileMetadata> file(new FileMetadata);
      if (!file->ParseFromString(itr->value().ToString())) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse a FileMetadata");
        continue;
      }

      contents->file_metadata.push_back(file.release());
      continue;
    }

    if (StartsWithASCII(key, kFileTrackerKeyPrefix, true)) {
      int64 tracker_id = 0;
      if (!base::StringToInt64(RemovePrefix(key, kFileTrackerKeyPrefix),
                               &tracker_id)) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse TrackerID");
        continue;
      }

      scoped_ptr<FileTracker> tracker(new FileTracker);
      if (!tracker->ParseFromString(itr->value().ToString())) {
        util::Log(logging::LOG_WARNING, FROM_HERE,
                  "Failed to parse a Tracker");
        continue;
      }
      contents->file_trackers.push_back(tracker.release());
      continue;
    }
  }

  return SYNC_STATUS_OK;
}

SyncStatusCode InitializeServiceMetadata(DatabaseContents* contents,
                                         leveldb::WriteBatch* batch) {

  if (!contents->service_metadata) {
    contents->service_metadata.reset(new ServiceMetadata);
    contents->service_metadata->set_next_tracker_id(1);

    std::string value;
    contents->service_metadata->SerializeToString(&value);
    batch->Put(kServiceMetadataKey, value);
  }
  return SYNC_STATUS_OK;
}

SyncStatusCode RemoveUnreachableItems(DatabaseContents* contents,
                                      leveldb::WriteBatch* batch) {
  TrackerByID unvisited_trackers;
  typedef std::map<int64, std::set<FileTracker*> > TrackersByParent;
  TrackersByParent trackers_by_parent;

  for (ScopedVector<FileTracker>::iterator itr =
           contents->file_trackers.begin();
       itr != contents->file_trackers.end();
       ++itr) {
    FileTracker* tracker = *itr;
    DCHECK(!ContainsKey(unvisited_trackers, tracker->tracker_id()));
    unvisited_trackers[tracker->tracker_id()] = tracker;
    if (tracker->parent_tracker_id())
      trackers_by_parent[tracker->parent_tracker_id()].insert(tracker);
  }

  // Traverse synced tracker tree. Take only active items, app-root and their
  // children. Drop unreachable items.
  ScopedVector<FileTracker> reachable_trackers;
  std::stack<int64> pending;
  if (contents->service_metadata->sync_root_tracker_id())
    pending.push(contents->service_metadata->sync_root_tracker_id());

  while (!pending.empty()) {
    int64 tracker_id = pending.top();
    pending.pop();

    {
      TrackerByID::iterator found = unvisited_trackers.find(tracker_id);
      if (found == unvisited_trackers.end())
        continue;

      FileTracker* tracker = found->second;
      unvisited_trackers.erase(found);
      reachable_trackers.push_back(tracker);

      if (!tracker->active() && !tracker->is_app_root())
        continue;
    }

    TrackersByParent::iterator found = trackers_by_parent.find(tracker_id);
    if (found == trackers_by_parent.end())
      continue;

    for (std::set<FileTracker*>::const_iterator itr =
             found->second.begin();
         itr != found->second.end();
         ++itr)
      pending.push((*itr)->tracker_id());
  }

  // Delete all unreachable trackers.
  for (TrackerByID::iterator itr = unvisited_trackers.begin();
       itr != unvisited_trackers.end(); ++itr) {
    FileTracker* tracker = itr->second;
    PutTrackerDeletionToBatch(tracker->tracker_id(), batch);
    delete tracker;
  }
  unvisited_trackers.clear();

  // |reachable_trackers| contains all files/folders reachable from sync-root
  // folder via active folders and app-root folders.
  // Update the tracker set in database contents with the reachable tracker set.
  contents->file_trackers.weak_clear();
  contents->file_trackers.swap(reachable_trackers);

  // Do the similar traverse for FileMetadata and remove FileMetadata that don't
  // have reachable trackers.
  FileByID unreferred_files;
  for (ScopedVector<FileMetadata>::const_iterator itr =
           contents->file_metadata.begin();
       itr != contents->file_metadata.end();
       ++itr) {
    unreferred_files.insert(std::make_pair((*itr)->file_id(), *itr));
  }

  ScopedVector<FileMetadata> referred_files;
  for (ScopedVector<FileTracker>::const_iterator itr =
           contents->file_trackers.begin();
       itr != contents->file_trackers.end();
       ++itr) {
    FileByID::iterator found = unreferred_files.find((*itr)->file_id());
    if (found != unreferred_files.end()) {
      referred_files.push_back(found->second);
      unreferred_files.erase(found);
    }
  }

  for (FileByID::iterator itr = unreferred_files.begin();
       itr != unreferred_files.end(); ++itr) {
    FileMetadata* file = itr->second;
    PutFileDeletionToBatch(file->file_id(), batch);
    delete file;
  }
  unreferred_files.clear();

  contents->file_metadata.weak_clear();
  contents->file_metadata.swap(referred_files);

  return SYNC_STATUS_OK;
}

template <typename Container, typename Key, typename Value>
bool FindItem(const Container& container, const Key& key, Value* value) {
  typename Container::const_iterator found = container.find(key);
  if (found == container.end())
    return false;
  if (value)
    *value = *found->second;
  return true;
}

void RunSoon(const tracked_objects::Location& from_here,
             const base::Closure& closure) {
  base::MessageLoopProxy::current()->PostTask(from_here, closure);
}

}  // namespace

bool MetadataDatabase::DirtyTrackerComparator::operator()(
    const FileTracker* left,
    const FileTracker* right) const {
  return left->tracker_id() < right->tracker_id();
}

// static
void MetadataDatabase::Create(base::SequencedTaskRunner* task_runner,
                              const base::FilePath& database_path,
                              const CreateCallback& callback) {
  task_runner->PostTask(FROM_HERE, base::Bind(
      &CreateOnTaskRunner,
      base::MessageLoopProxy::current(),
      make_scoped_refptr(task_runner),
      database_path, callback));
}

MetadataDatabase::~MetadataDatabase() {
  task_runner_->DeleteSoon(FROM_HERE, db_.release());
  STLDeleteContainerPairSecondPointers(
      file_by_id_.begin(), file_by_id_.end());
  STLDeleteContainerPairSecondPointers(
      tracker_by_id_.begin(), tracker_by_id_.end());
}

int64 MetadataDatabase::GetLargestChangeID() const {
  return service_metadata_->largest_change_id();
}

void MetadataDatabase::RegisterApp(const std::string& app_id,
                                   const std::string& folder_id,
                                   const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void MetadataDatabase::DisableApp(const std::string& app_id,
                                  const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void MetadataDatabase::EnableApp(const std::string& app_id,
                                 const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void MetadataDatabase::UnregisterApp(const std::string& app_id,
                                     const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

void MetadataDatabase::UpdateByChangeList(
    ScopedVector<google_apis::ChangeResource> changes,
    const SyncStatusCallback& callback) {
  NOTIMPLEMENTED();
}

MetadataDatabase::MetadataDatabase(base::SequencedTaskRunner* task_runner)
    : task_runner_(task_runner), weak_ptr_factory_(this) {
  DCHECK(task_runner);
}

// static
void MetadataDatabase::CreateOnTaskRunner(
    base::SingleThreadTaskRunner* callback_runner,
    base::SequencedTaskRunner* task_runner,
    const base::FilePath& database_path,
    const CreateCallback& callback) {
  scoped_ptr<MetadataDatabase> metadata_database(
      new MetadataDatabase(task_runner));
  SyncStatusCode status =
      metadata_database->InitializeOnTaskRunner(database_path);
  if (status != SYNC_STATUS_OK)
    metadata_database.reset();

  callback_runner->PostTask(FROM_HERE, base::Bind(
      callback, status, base::Passed(&metadata_database)));
}

// static
SyncStatusCode MetadataDatabase::CreateForTesting(
    scoped_ptr<leveldb::DB> db,
    scoped_ptr<MetadataDatabase>* metadata_database_out) {
  scoped_ptr<MetadataDatabase> metadata_database(
      new MetadataDatabase(base::MessageLoopProxy::current()));
  metadata_database->db_ = db.Pass();
  SyncStatusCode status =
      metadata_database->InitializeOnTaskRunner(base::FilePath());
  if (status == SYNC_STATUS_OK)
    *metadata_database_out = metadata_database.Pass();
  return status;
}

SyncStatusCode MetadataDatabase::InitializeOnTaskRunner(
    const base::FilePath& database_path) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(task_runner_->RunsTasksOnCurrentThread());

  SyncStatusCode status = SYNC_STATUS_UNKNOWN;
  bool created = false;
  // Open database unless |db_| is overridden for testing.
  if (!db_) {
    status = OpenDatabase(database_path, &db_, &created);
    if (status != SYNC_STATUS_OK)
      return status;
  }

  if (created) {
    status = WriteVersionInfo(db_.get());
    if (status != SYNC_STATUS_OK)
      return status;
  } else {
    status = MigrateDatabaseIfNeeded(db_.get());
    if (status != SYNC_STATUS_OK)
      return status;
  }

  DatabaseContents contents;
  status = ReadDatabaseContents(db_.get(), &contents);
  if (status != SYNC_STATUS_OK)
    return status;

  leveldb::WriteBatch batch;
  status = InitializeServiceMetadata(&contents, &batch);
  if (status != SYNC_STATUS_OK)
    return status;

  status = RemoveUnreachableItems(&contents, &batch);
  if (status != SYNC_STATUS_OK)
    return status;

  status = LevelDBStatusToSyncStatusCode(
      db_->Write(leveldb::WriteOptions(), &batch));
  if (status != SYNC_STATUS_OK)
    return status;

  BuildIndexes(&contents);
  return status;
}

void MetadataDatabase::BuildIndexes(DatabaseContents* contents) {
  service_metadata_ = contents->service_metadata.Pass();

  for (ScopedVector<FileMetadata>::const_iterator itr =
           contents->file_metadata.begin();
       itr != contents->file_metadata.end();
       ++itr) {
    file_by_id_[(*itr)->file_id()] = *itr;
  }
  contents->file_metadata.weak_clear();

  for (ScopedVector<FileTracker>::const_iterator itr =
           contents->file_trackers.begin();
       itr != contents->file_trackers.end();
       ++itr) {
    FileTracker* tracker = *itr;
    tracker_by_id_[tracker->tracker_id()] = tracker;
    trackers_by_file_id_[tracker->file_id()].Insert(tracker);

    if (tracker->is_app_root())
      app_root_by_app_id_[tracker->app_id()] = tracker;

    if (tracker->parent_tracker_id()) {
      std::string title = GetTrackerTitle(*tracker);
      TrackerSet* trackers =
          &trackers_by_parent_and_title_[tracker->parent_tracker_id()][title];
      trackers->Insert(tracker);
    }

    if (tracker->dirty())
      dirty_trackers_.insert(tracker);
  }
  contents->file_trackers.weak_clear();
}

void MetadataDatabase::WriteToDatabase(scoped_ptr<leveldb::WriteBatch> batch,
                                       const SyncStatusCallback& callback) {
  base::PostTaskAndReplyWithResult(
      task_runner_.get(),
      FROM_HERE,
      base::Bind(&leveldb::DB::Write,
                 base::Unretained(db_.get()),
                 leveldb::WriteOptions(),
                 base::Owned(batch.release())),
      base::Bind(&AdaptLevelDBStatusToSyncStatusCode, callback));
}

}  // namespace drive_backend
}  // namespace sync_file_system
