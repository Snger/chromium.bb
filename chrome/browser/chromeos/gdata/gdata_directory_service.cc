// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_directory_service.h"

#include <leveldb/db.h>
#include <utility>

#include "base/message_loop_proxy.h"
#include "base/string_number_conversions.h"
#include "base/sequenced_task_runner.h"
#include "base/tracked_objects.h"
#include "chrome/browser/chromeos/gdata/drive.pb.h"
#include "chrome/browser/chromeos/gdata/drive_files.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace gdata {
namespace {

// m: prefix for filesystem metadata db keys, version and largest_changestamp.
// r: prefix for resource id db keys.
const char kDBKeyLargestChangestamp[] = "m:largest_changestamp";
const char kDBKeyVersion[] = "m:version";
const char kDBKeyResourceIdPrefix[] = "r:";

}  // namespace

EntryInfoResult::EntryInfoResult() : error(GDATA_FILE_ERROR_FAILED) {
}

EntryInfoResult::~EntryInfoResult() {
}

EntryInfoPairResult::EntryInfoPairResult() {
}

EntryInfoPairResult::~EntryInfoPairResult() {
}

// ResourceMetadataDB implementation.

// Params for GDatadirectoryServiceDB::Create.
struct CreateDBParams {
  CreateDBParams(const FilePath& db_path,
                 base::SequencedTaskRunner* blocking_task_runner)
                 : db_path(db_path),
                   blocking_task_runner(blocking_task_runner) {
  }

  FilePath db_path;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner;
  scoped_ptr<ResourceMetadataDB> db;
  GDataDirectoryService::SerializedMap serialized_resources;
};

// Wrapper for level db. All methods must be called on blocking thread.
class ResourceMetadataDB {
 public:
  ResourceMetadataDB(const FilePath& db_path,
                          base::SequencedTaskRunner* blocking_task_runner);

  // Initializes the database.
  void Init();

  // Reads the database into |serialized_resources|.
  void Read(GDataDirectoryService::SerializedMap* serialized_resources);

  // Saves |serialized_resources| to the database.
  void Save(const GDataDirectoryService::SerializedMap& serialized_resources);

 private:
  // Clears the database.
  void Clear();

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<leveldb::DB> level_db_;
  FilePath db_path_;
};

ResourceMetadataDB::ResourceMetadataDB(const FilePath& db_path,
    base::SequencedTaskRunner* blocking_task_runner)
  : blocking_task_runner_(blocking_task_runner),
    db_path_(db_path) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
}

// Creates, initializes and reads from the database.
// This must be defined after ResourceMetadataDB and CreateDBParams.
static void CreateResourceMetadataDBOnBlockingPool(
    CreateDBParams* params) {
  DCHECK(params->blocking_task_runner->RunsTasksOnCurrentThread());
  DCHECK(!params->db_path.empty());

  params->db.reset(new ResourceMetadataDB(params->db_path,
                                               params->blocking_task_runner));
  params->db->Init();
  params->db->Read(&params->serialized_resources);
}

void ResourceMetadataDB::Init() {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(!db_path_.empty());

  DVLOG(1) << "Init " << db_path_.value();

  leveldb::DB* level_db = NULL;
  leveldb::Options options;
  options.create_if_missing = true;
  leveldb::Status db_status = leveldb::DB::Open(options, db_path_.value(),
                                                &level_db);
  DCHECK(level_db);
  DCHECK(db_status.ok());
  level_db_.reset(level_db);
}

void ResourceMetadataDB::Read(
  GDataDirectoryService::SerializedMap* serialized_resources) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());
  DCHECK(serialized_resources);
  DVLOG(1) << "Read " << db_path_.value();

  scoped_ptr<leveldb::Iterator> iter(level_db_->NewIterator(
        leveldb::ReadOptions()));
  for (iter->SeekToFirst(); iter->Valid(); iter->Next()) {
    DVLOG(1) << "Read, resource " << iter->key().ToString();
    serialized_resources->insert(std::make_pair(iter->key().ToString(),
                                                iter->value().ToString()));
  }
}

void ResourceMetadataDB::Save(
    const GDataDirectoryService::SerializedMap& serialized_resources) {
  DCHECK(blocking_task_runner_->RunsTasksOnCurrentThread());

  Clear();
  for (GDataDirectoryService::SerializedMap::const_iterator iter =
      serialized_resources.begin();
      iter != serialized_resources.end(); ++iter) {
    DVLOG(1) << "Saving resource " << iter->first << " to db";
    leveldb::Status status = level_db_->Put(leveldb::WriteOptions(),
                                            leveldb::Slice(iter->first),
                                            leveldb::Slice(iter->second));
    if (!status.ok()) {
      LOG(ERROR) << "leveldb Put failed of " << iter->first
                 << ", with " << status.ToString();
      NOTREACHED();
    }
  }
}

void ResourceMetadataDB::Clear() {
  level_db_.reset();
  leveldb::DestroyDB(db_path_.value(), leveldb::Options());
  Init();
}

// GDataDirectoryService class implementation.

GDataDirectoryService::GDataDirectoryService()
    : blocking_task_runner_(NULL),
      serialized_size_(0),
      largest_changestamp_(0),
      origin_(UNINITIALIZED),
      weak_ptr_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
  root_.reset(CreateDriveDirectory());
  if (!util::IsDriveV2ApiEnabled())
    InitializeRootEntry(kGDataRootDirectoryResourceId);
}

GDataDirectoryService::~GDataDirectoryService() {
  ClearRoot();

  // Ensure db is closed on the blocking pool.
  if (blocking_task_runner_ && directory_service_db_.get())
    blocking_task_runner_->DeleteSoon(FROM_HERE,
                                      directory_service_db_.release());
}

DriveEntry* GDataDirectoryService::FromDocumentEntry(const DocumentEntry& doc) {
  DriveEntry* entry = NULL;
  if (doc.is_folder())
    entry = CreateDriveDirectory();
  else if (doc.is_hosted_document() || doc.is_file())
    entry = CreateDriveFile();

  if (entry)
    entry->InitFromDocumentEntry(doc);
  return entry;
}

DriveFile* GDataDirectoryService::CreateDriveFile() {
  return new DriveFile(this);
}

DriveDirectory* GDataDirectoryService::CreateDriveDirectory() {
  return new DriveDirectory(this);
}

void GDataDirectoryService::InitializeRootEntry(const std::string& root_id) {
  root_.reset(CreateDriveDirectory());
  root_->set_title(kGDataRootDirectory);
  root_->SetBaseNameFromTitle();
  root_->set_resource_id(root_id);
  AddEntryToResourceMap(root_.get());
}

void GDataDirectoryService::ClearRoot() {
  // Note that children have a reference to root_,
  // so we need to delete them here.
  root_->RemoveChildren();
  RemoveEntryFromResourceMap(root_->resource_id());
  DCHECK(resource_map_.empty());
  resource_map_.clear();
  root_.reset();
}

void GDataDirectoryService::AddEntryToDirectory(
    DriveDirectory* directory,
    DriveEntry* new_entry,
    const FileMoveCallback& callback) {
  DCHECK(directory);
  DCHECK(new_entry);
  DCHECK(!callback.is_null());

  directory->AddEntry(new_entry);
  DVLOG(1) << "AddEntryToDirectory " << new_entry->GetFilePath().value();
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(callback, GDATA_FILE_OK, new_entry->GetFilePath()));
}

void GDataDirectoryService::MoveEntryToDirectory(
    const FilePath& directory_path,
    DriveEntry* entry,
    const FileMoveCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(entry);
  DCHECK(!callback.is_null());

  if (entry->parent())
    entry->parent()->RemoveChild(entry);

  DriveEntry* destination = FindEntryByPathSync(directory_path);
  FilePath moved_file_path;
  GDataFileError error = GDATA_FILE_ERROR_FAILED;
  if (!destination) {
    error = GDATA_FILE_ERROR_NOT_FOUND;
  } else if (!destination->AsDriveDirectory()) {
    error = GDATA_FILE_ERROR_NOT_A_DIRECTORY;
  } else {
    destination->AsDriveDirectory()->AddEntry(entry);
    moved_file_path = entry->GetFilePath();
    error = GDATA_FILE_OK;
  }
  DVLOG(1) << "MoveEntryToDirectory " << moved_file_path.value();
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE, base::Bind(callback, error, moved_file_path));
}

void GDataDirectoryService::RemoveEntryFromParent(
    DriveEntry* entry,
    const FileMoveCallback& callback) {
  DriveDirectory* parent = entry->parent();
  DCHECK(parent);
  DCHECK(!callback.is_null());
  DVLOG(1) << "RemoveEntryFromParent " << entry->GetFilePath().value();

  parent->RemoveEntry(entry);
  base::MessageLoopProxy::current()->PostTask(FROM_HERE,
      base::Bind(callback, GDATA_FILE_OK, parent->GetFilePath()));
}

void GDataDirectoryService::AddEntryToResourceMap(DriveEntry* entry) {
  DVLOG(1) << "AddEntryToResourceMap " << entry->resource_id();
  DCHECK(!entry->resource_id().empty());
  std::pair<ResourceMap::iterator, bool> ret =
      resource_map_.insert(std::make_pair(entry->resource_id(), entry));
  DCHECK(ret.second);  // resource_id did not previously exist in the map.
}

void GDataDirectoryService::RemoveEntryFromResourceMap(
    const std::string& resource_id) {
  DVLOG(1) << "RemoveEntryFromResourceMap " << resource_id;
  DCHECK(!resource_id.empty());
  size_t ret = resource_map_.erase(resource_id);
  DCHECK_EQ(1u, ret);  // resource_id was found in the map.
}

DriveEntry* GDataDirectoryService::FindEntryByPathSync(
    const FilePath& file_path) {
  if (file_path == root_->GetFilePath())
    return root_.get();

  std::vector<FilePath::StringType> components;
  file_path.GetComponents(&components);
  DriveDirectory* current_dir = root_.get();

  for (size_t i = 1; i < components.size() && current_dir; ++i) {
    std::string resource_id = current_dir->FindChild(components[i]);
    if (resource_id.empty())
      return NULL;

    DriveEntry* entry = GetEntryByResourceId(resource_id);
    DCHECK(entry);

    if (i == components.size() - 1)  // Last component.
      return entry;
    else
      current_dir = entry->AsDriveDirectory();
  }
  return NULL;
}

DriveEntry* GDataDirectoryService::GetEntryByResourceId(
    const std::string& resource_id) {
  DCHECK(!resource_id.empty());
  ResourceMap::const_iterator iter = resource_map_.find(resource_id);
  return iter == resource_map_.end() ? NULL : iter->second;
}

void GDataDirectoryService::GetEntryByResourceIdAsync(
    const std::string& resource_id,
    const GetEntryByResourceIdCallback& callback) {
  DriveEntry* entry = GetEntryByResourceId(resource_id);
  callback.Run(entry);
}

void GDataDirectoryService::GetEntryInfoByResourceId(
      const std::string& resource_id,
      const GetEntryInfoWithFilePathCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<DriveEntryProto> entry_proto;
  GDataFileError error = GDATA_FILE_ERROR_FAILED;
  FilePath drive_file_path;

  DriveEntry* entry = GetEntryByResourceId(resource_id);
  if (entry) {
    entry_proto.reset(new DriveEntryProto);
    entry->ToProtoFull(entry_proto.get());
    error = GDATA_FILE_OK;
    drive_file_path = entry->GetFilePath();
  } else {
    error = GDATA_FILE_ERROR_NOT_FOUND;
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 error,
                 drive_file_path,
                 base::Passed(&entry_proto)));
}

void GDataDirectoryService::GetEntryInfoByPath(
    const FilePath& path,
    const GetEntryInfoCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<DriveEntryProto> entry_proto;
  GDataFileError error = GDATA_FILE_ERROR_FAILED;

  DriveEntry* entry = FindEntryByPathSync(path);
  if (entry) {
    entry_proto.reset(new DriveEntryProto);
    entry->ToProtoFull(entry_proto.get());
    error = GDATA_FILE_OK;
  } else {
    error = GDATA_FILE_ERROR_NOT_FOUND;
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, base::Passed(&entry_proto)));
}

void GDataDirectoryService::ReadDirectoryByPath(
    const FilePath& path,
    const ReadDirectoryCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<DriveEntryProtoVector> entries;
  GDataFileError error = GDATA_FILE_ERROR_FAILED;

  DriveEntry* entry = FindEntryByPathSync(path);
  if (entry && entry->AsDriveDirectory()) {
    entries = entry->AsDriveDirectory()->ToProtoVector();
    error = GDATA_FILE_OK;
  } else if (entry && !entry->AsDriveDirectory()) {
    error = GDATA_FILE_ERROR_NOT_A_DIRECTORY;
  } else {
    error = GDATA_FILE_ERROR_NOT_FOUND;
  }

  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, error, base::Passed(&entries)));
}

void GDataDirectoryService::GetEntryInfoPairByPaths(
    const FilePath& first_path,
    const FilePath& second_path,
    const GetEntryInfoPairCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  // Get the first entry.
  GetEntryInfoByPath(
      first_path,
      base::Bind(&GDataDirectoryService::GetEntryInfoPairByPathsAfterGetFirst,
                 weak_ptr_factory_.GetWeakPtr(),
                 first_path,
                 second_path,
                 callback));
}

void GDataDirectoryService::RefreshFile(scoped_ptr<DriveFile> fresh_file) {
  DCHECK(fresh_file.get());

  // Need to get a reference here because Passed() could get evaluated first.
  const std::string& resource_id = fresh_file->resource_id();
  GetEntryByResourceIdAsync(
      resource_id,
      base::Bind(&GDataDirectoryService::RefreshFileInternal,
                 base::Passed(&fresh_file)));
}

// static
void GDataDirectoryService::RefreshFileInternal(
    scoped_ptr<DriveFile> fresh_file,
    DriveEntry* old_entry) {
  DriveDirectory* entry_parent = old_entry ? old_entry->parent() : NULL;
  if (entry_parent) {
    DCHECK_EQ(fresh_file->resource_id(), old_entry->resource_id());
    DCHECK(old_entry->AsDriveFile());

    entry_parent->RemoveEntry(old_entry);
    entry_parent->AddEntry(fresh_file.release());
  }
}

void GDataDirectoryService::RefreshDirectory(
    const std::string& directory_resource_id,
    const ResourceMap& file_map,
    const FileMoveCallback& callback) {
  DCHECK(!callback.is_null());
  GetEntryByResourceIdAsync(
      directory_resource_id,
      base::Bind(&GDataDirectoryService::RefreshDirectoryInternal,
                 file_map,
                 callback));
}

// static
void GDataDirectoryService::RefreshDirectoryInternal(
    const ResourceMap& file_map,
    const FileMoveCallback& callback,
    DriveEntry* directory_entry) {
  DCHECK(!callback.is_null());

  if (!directory_entry) {
    callback.Run(GDATA_FILE_ERROR_NOT_FOUND, FilePath());
    return;
  }

  DriveDirectory* directory = directory_entry->AsDriveDirectory();
  if (!directory) {
    callback.Run(GDATA_FILE_ERROR_NOT_A_DIRECTORY, FilePath());
    return;
  }

  DVLOG(1) << "RefreshDirectoryInternal";
  directory->RemoveChildFiles();
  // Add files from file_map.
  for (ResourceMap::const_iterator it = file_map.begin();
       it != file_map.end(); ++it) {
    scoped_ptr<DriveEntry> entry(it->second);
    // Skip if it's not a file (i.e. directory).
    if (!entry->AsDriveFile())
      continue;
    directory->AddEntry(entry.release());
  }

  callback.Run(GDATA_FILE_OK, directory->GetFilePath());
}

void GDataDirectoryService::InitFromDB(
    const FilePath& db_path,
    base::SequencedTaskRunner* blocking_task_runner,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!db_path.empty());
  DCHECK(blocking_task_runner);

  if (directory_service_db_.get()) {
    if (!callback.is_null())
      callback.Run(GDATA_FILE_ERROR_FAILED);
    return;
  }

  blocking_task_runner_ = blocking_task_runner;

  DVLOG(1) << "InitFromDB " << db_path.value();

  CreateDBParams* create_params =
      new CreateDBParams(db_path, blocking_task_runner);
  blocking_task_runner_->PostTaskAndReply(
      FROM_HERE,
      base::Bind(&CreateResourceMetadataDBOnBlockingPool,
                 create_params),
      base::Bind(&GDataDirectoryService::InitResourceMap,
                 weak_ptr_factory_.GetWeakPtr(),
                 base::Owned(create_params),
                 callback));
}

void GDataDirectoryService::InitResourceMap(
    CreateDBParams* create_params,
    const FileOperationCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(create_params);
  DCHECK(!directory_service_db_.get());

  SerializedMap* serialized_resources = &create_params->serialized_resources;
  directory_service_db_ = create_params->db.Pass();
  if (serialized_resources->empty()) {
    origin_ = INITIALIZING;
    if (!callback.is_null())
      callback.Run(GDATA_FILE_ERROR_NOT_FOUND);
    return;
  }

  ClearRoot();

  // Version check.
  int32 version = 0;
  SerializedMap::iterator iter = serialized_resources->find(kDBKeyVersion);
  if (iter == serialized_resources->end() ||
      !base::StringToInt(iter->second, &version) ||
      version != kProtoVersion) {
    if (!callback.is_null())
      callback.Run(GDATA_FILE_ERROR_FAILED);
    return;
  }
  serialized_resources->erase(iter);

  // Get the largest changestamp.
  iter = serialized_resources->find(kDBKeyLargestChangestamp);
  if (iter == serialized_resources->end() ||
      !base::StringToInt64(iter->second, &largest_changestamp_)) {
    NOTREACHED() << "Could not find/parse largest_changestamp";
    if (!callback.is_null())
      callback.Run(GDATA_FILE_ERROR_FAILED);
    return;
  } else {
    DVLOG(1) << "InitResourceMap largest_changestamp_" << largest_changestamp_;
    serialized_resources->erase(iter);
  }

  ResourceMap resource_map;
  for (SerializedMap::const_iterator iter = serialized_resources->begin();
      iter != serialized_resources->end(); ++iter) {
    if (iter->first.find(kDBKeyResourceIdPrefix) != 0) {
      NOTREACHED() << "Incorrect prefix for db key " << iter->first;
      continue;
    }

    const std::string resource_id =
        iter->first.substr(strlen(kDBKeyResourceIdPrefix));
    scoped_ptr<DriveEntry> entry = FromProtoString(iter->second);
    if (entry.get()) {
      DVLOG(1) << "Inserting resource " << resource_id
               << " into resource_map";
      resource_map.insert(std::make_pair(resource_id, entry.release()));
    } else {
      NOTREACHED() << "Failed to parse DriveEntry for resource " << resource_id;
    }
  }

  // Fix up parent-child relations.
  for (ResourceMap::iterator iter = resource_map.begin();
      iter != resource_map.end(); ++iter) {
    DriveEntry* entry = iter->second;
    ResourceMap::iterator parent_it =
        resource_map.find(entry->parent_resource_id());
    if (parent_it != resource_map.end()) {
      DriveDirectory* parent = parent_it->second->AsDriveDirectory();
      if (parent) {
        DVLOG(1) << "Adding " << entry->resource_id()
                 << " as a child of " << parent->resource_id();
        parent->AddEntry(entry);
      } else {
        NOTREACHED() << "Parent is not a directory " << parent->resource_id();
      }
    } else if (entry->resource_id() == kGDataRootDirectoryResourceId) {
      root_.reset(entry->AsDriveDirectory());
      DCHECK(root_.get());
      AddEntryToResourceMap(root_.get());
    } else {
      NOTREACHED() << "Missing parent id " << entry->parent_resource_id()
                   << " for resource " << entry->resource_id();
    }
  }

  DCHECK(root_.get());
  DCHECK_EQ(resource_map.size(), resource_map_.size());
  DCHECK_EQ(resource_map.size(), serialized_resources->size());

  origin_ = FROM_CACHE;

  if (!callback.is_null())
    callback.Run(GDATA_FILE_OK);
}

void GDataDirectoryService::SaveToDB() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!blocking_task_runner_ || !directory_service_db_.get()) {
    NOTREACHED();
    return;
  }

  size_t serialized_size = 0;
  SerializedMap serialized_resources;
  for (ResourceMap::const_iterator iter = resource_map_.begin();
      iter != resource_map_.end(); ++iter) {
    DriveEntryProto proto;
    iter->second->ToProtoFull(&proto);
    std::string serialized_string;
    const bool ok = proto.SerializeToString(&serialized_string);
    DCHECK(ok);
    if (ok) {
      serialized_resources.insert(
          std::make_pair(std::string(kDBKeyResourceIdPrefix) + iter->first,
                         serialized_string));
      serialized_size += serialized_string.size();
    }
  }

  serialized_resources.insert(std::make_pair(kDBKeyVersion,
      base::IntToString(kProtoVersion)));
  serialized_resources.insert(std::make_pair(kDBKeyLargestChangestamp,
      base::IntToString(largest_changestamp_)));
  set_last_serialized(base::Time::Now());
  set_serialized_size(serialized_size);

  blocking_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ResourceMetadataDB::Save,
                 base::Unretained(directory_service_db_.get()),
                 serialized_resources));
}

void GDataDirectoryService::SerializeToString(
    std::string* serialized_proto) const {
  DriveRootDirectoryProto proto;
  root_->ToProto(proto.mutable_gdata_directory());
  proto.set_largest_changestamp(largest_changestamp_);
  proto.set_version(kProtoVersion);

  const bool ok = proto.SerializeToString(serialized_proto);
  DCHECK(ok);
}

bool GDataDirectoryService::ParseFromString(
    const std::string& serialized_proto) {
  DriveRootDirectoryProto proto;
  if (!proto.ParseFromString(serialized_proto))
    return false;

  if (proto.version() != kProtoVersion) {
    LOG(ERROR) << "Incompatible proto detected (incompatible version): "
               << proto.version();
    return false;
  }

  root_->FromProto(proto.gdata_directory());

  origin_ = FROM_CACHE;
  largest_changestamp_ = proto.largest_changestamp();

  return true;
}

scoped_ptr<DriveEntry> GDataDirectoryService::FromProtoString(
    const std::string& serialized_proto) {
  DriveEntryProto entry_proto;
  if (!entry_proto.ParseFromString(serialized_proto))
    return scoped_ptr<DriveEntry>();

  scoped_ptr<DriveEntry> entry;
  if (entry_proto.file_info().is_directory()) {
    entry.reset(CreateDriveDirectory());
    // Call DriveEntry::FromProto instead of DriveDirectory::FromProto because
    // the proto does not include children.
    entry->FromProto(entry_proto);
  } else {
    scoped_ptr<DriveFile> file(CreateDriveFile());
    // Call DriveFile::FromProto.
    file->FromProto(entry_proto);
    entry.reset(file.release());
  }
  return entry.Pass();
}

void GDataDirectoryService::GetEntryInfoPairByPathsAfterGetFirst(
    const FilePath& first_path,
    const FilePath& second_path,
    const GetEntryInfoPairCallback& callback,
    GDataFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());

  scoped_ptr<EntryInfoPairResult> result(new EntryInfoPairResult);
  result->first.path = first_path;
  result->first.error = error;
  result->first.proto = entry_proto.Pass();

  // If the first one is not found, don't continue.
  if (error != GDATA_FILE_OK) {
    callback.Run(result.Pass());
    return;
  }

  // Get the second entry.
  GetEntryInfoByPath(
      second_path,
      base::Bind(&GDataDirectoryService::GetEntryInfoPairByPathsAfterGetSecond,
                 weak_ptr_factory_.GetWeakPtr(),
                 second_path,
                 callback,
                 base::Passed(&result)));
}

void GDataDirectoryService::GetEntryInfoPairByPathsAfterGetSecond(
    const FilePath& second_path,
    const GetEntryInfoPairCallback& callback,
    scoped_ptr<EntryInfoPairResult> result,
    GDataFileError error,
    scoped_ptr<DriveEntryProto> entry_proto) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!callback.is_null());
  DCHECK(result.get());

  result->second.path = second_path;
  result->second.error = error;
  result->second.proto = entry_proto.Pass();

  callback.Run(result.Pass());
}

}  // namespace gdata
