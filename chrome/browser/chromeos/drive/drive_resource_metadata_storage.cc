// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/drive_resource_metadata_storage.h"

#include "base/callback.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/chromeos/drive/drive.pb.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"

namespace drive {

namespace {

const base::FilePath::CharType kResourceMapDBName[] =
    FILE_PATH_LITERAL("resource_metadata_resource_map.db");
const base::FilePath::CharType kChildMapDBName[] =
    FILE_PATH_LITERAL("resource_metadata_child_map.db");

// Meant to be a character which never happen to be in real resource IDs.
const char kDBKeyDelimeter = '\0';

// Returns a string to be used as the key for the header.
std::string GetHeaderDBKey() {
  std::string key;
  key.push_back(kDBKeyDelimeter);
  key.append("HEADER");
  return key;
}

// Returns a string to be used as a key for child entry.
std::string GetChildEntryKey(const std::string& parent_resource_id,
                             const std::string& child_name) {
  std::string key = parent_resource_id;
  key.push_back(kDBKeyDelimeter);
  key.append(child_name);
  key.push_back(kDBKeyDelimeter);
  return key;
}

// Returns true if |key| is a key for a child entry.
bool IsChildEntryKey(const leveldb::Slice& key) {
  return !key.empty() && key[key.size() - 1] == kDBKeyDelimeter;
}

}  // namespace

DriveResourceMetadataStorageMemory::DriveResourceMetadataStorageMemory()
    : largest_changestamp_(0) {
}

DriveResourceMetadataStorageMemory::~DriveResourceMetadataStorageMemory() {
  base::ThreadRestrictions::AssertIOAllowed();
}

bool DriveResourceMetadataStorageMemory::Initialize() {
  base::ThreadRestrictions::AssertIOAllowed();
  largest_changestamp_ = 0;
  resource_map_.clear();
  child_maps_.clear();
  return true;
}

bool DriveResourceMetadataStorageMemory::IsPersistentStorage() {
  base::ThreadRestrictions::AssertIOAllowed();
  return false;
}

void DriveResourceMetadataStorageMemory::SetLargestChangestamp(
    int64 largest_changestamp) {
  base::ThreadRestrictions::AssertIOAllowed();
  largest_changestamp_ = largest_changestamp;
}

int64 DriveResourceMetadataStorageMemory::GetLargestChangestamp() {
  base::ThreadRestrictions::AssertIOAllowed();
  return largest_changestamp_;
}

void DriveResourceMetadataStorageMemory::PutEntry(
    const DriveEntryProto& entry) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!entry.resource_id().empty());
  resource_map_[entry.resource_id()] = entry;
}

scoped_ptr<DriveEntryProto> DriveResourceMetadataStorageMemory::GetEntry(
    const std::string& resource_id) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!resource_id.empty());

  ResourceMap::const_iterator iter = resource_map_.find(resource_id);
  scoped_ptr<DriveEntryProto> entry;
  if (iter != resource_map_.end())
    entry.reset(new DriveEntryProto(iter->second));
  return entry.Pass();
}

void DriveResourceMetadataStorageMemory::RemoveEntry(
    const std::string& resource_id) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!resource_id.empty());

  const size_t result = resource_map_.erase(resource_id);
  DCHECK_EQ(1u, result);  // resource_id was found in the map.
}

void DriveResourceMetadataStorageMemory::Iterate(
    const IterateCallback& callback) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!callback.is_null());

  for (ResourceMap::const_iterator iter = resource_map_.begin();
       iter != resource_map_.end(); ++iter) {
    callback.Run(iter->second);
  }
}

void DriveResourceMetadataStorageMemory::PutChild(
    const std::string& parent_resource_id,
    const std::string& child_name,
    const std::string& child_resource_id) {
  base::ThreadRestrictions::AssertIOAllowed();
  child_maps_[parent_resource_id][child_name] = child_resource_id;
}

std::string DriveResourceMetadataStorageMemory::GetChild(
    const std::string& parent_resource_id,
    const std::string& child_name) {
  base::ThreadRestrictions::AssertIOAllowed();

  ChildMaps::const_iterator iter = child_maps_.find(parent_resource_id);
  if (iter == child_maps_.end())
    return std::string();
  const ChildMap& child_map = iter->second;
  ChildMap::const_iterator sub_iter = child_map.find(child_name);
  if (sub_iter == child_map.end())
    return std::string();
  return sub_iter->second;
}

void DriveResourceMetadataStorageMemory::GetChildren(
    const std::string& parent_resource_id,
    std::vector<std::string>* children) {
  base::ThreadRestrictions::AssertIOAllowed();

  ChildMaps::const_iterator iter = child_maps_.find(parent_resource_id);
  if (iter == child_maps_.end())
    return;

  const ChildMap& child_map = iter->second;
  for (ChildMap::const_iterator sub_iter = child_map.begin();
       sub_iter != child_map.end(); ++sub_iter)
    children->push_back(sub_iter->second);
}

void DriveResourceMetadataStorageMemory::RemoveChild(
    const std::string& parent_resource_id,
    const std::string& child_name) {
  base::ThreadRestrictions::AssertIOAllowed();

  ChildMaps::iterator iter = child_maps_.find(parent_resource_id);
  DCHECK(iter != child_maps_.end());

  ChildMap* child_map = &iter->second;
  const size_t result = child_map->erase(child_name);
  DCHECK_EQ(1u, result);  // child_name was found in the map.

  // Erase the map if it got empty.
  if (child_map->empty())
    child_maps_.erase(iter);
}

DriveResourceMetadataStorageDB::DriveResourceMetadataStorageDB(
    const base::FilePath& directory_path)
    : directory_path_(directory_path) {
}

DriveResourceMetadataStorageDB::~DriveResourceMetadataStorageDB() {
  base::ThreadRestrictions::AssertIOAllowed();
}

bool DriveResourceMetadataStorageDB::Initialize() {
  base::ThreadRestrictions::AssertIOAllowed();

  // Remove unused child map DB.
  const base::FilePath child_map_path = directory_path_.Append(kChildMapDBName);
  file_util::Delete(child_map_path, true /* recursive */);

  resource_map_.reset();

  const base::FilePath resource_map_path =
      directory_path_.Append(kResourceMapDBName);

  // Try to open the existing DB.
  leveldb::DB* db = NULL;
  leveldb::Options options;
  options.create_if_missing = false;

  leveldb::Status status =
      leveldb::DB::Open(options, resource_map_path.value(), &db);
  if (status.ok())
    resource_map_.reset(db);

  // Check the validity of existing DB.
  if (resource_map_) {
    if (!CheckValidity()) {
      LOG(ERROR) << "Reject invalid DB.";
      resource_map_.reset();
    }
  }

  // Failed to open the existing DB, create new DB.
  if (!resource_map_) {
    resource_map_.reset();

    // Clean up the destination.
    const bool kRecursive = true;
    file_util::Delete(resource_map_path, kRecursive);

    // Create DB.
    options.create_if_missing = true;

    status = leveldb::DB::Open(options, resource_map_path.value(), &db);
    if (!status.ok()) {
      LOG(ERROR) << "Failed to create resource map DB: " << status.ToString();
      return false;
    }
    resource_map_.reset(db);

    // Set up header.
    DriveResourceMetadataHeader header;
    header.set_version(kDBVersion);
    PutHeader(header);
  }

  DCHECK(resource_map_);
  return true;
}

bool DriveResourceMetadataStorageDB::IsPersistentStorage() {
  base::ThreadRestrictions::AssertIOAllowed();
  return true;
}

void DriveResourceMetadataStorageDB::SetLargestChangestamp(
    int64 largest_changestamp) {
  base::ThreadRestrictions::AssertIOAllowed();

  scoped_ptr<DriveResourceMetadataHeader> header = GetHeader();
  DCHECK(header);
  header->set_largest_changestamp(largest_changestamp);
  PutHeader(*header);
}

int64 DriveResourceMetadataStorageDB::GetLargestChangestamp() {
  base::ThreadRestrictions::AssertIOAllowed();
  scoped_ptr<DriveResourceMetadataHeader> header = GetHeader();
  DCHECK(header);
  return header->largest_changestamp();
}

void DriveResourceMetadataStorageDB::PutEntry(
    const DriveEntryProto& entry) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!entry.resource_id().empty());

  std::string serialized_entry;
  if (!entry.SerializeToString(&serialized_entry)) {
    DLOG(ERROR) << "Failed to serialize the entry: " << entry.resource_id();
    return;
  }

  const leveldb::Status status = resource_map_->Put(
      leveldb::WriteOptions(),
      leveldb::Slice(entry.resource_id()),
      leveldb::Slice(serialized_entry));
  DCHECK(status.ok());
}

scoped_ptr<DriveEntryProto> DriveResourceMetadataStorageDB::GetEntry(
    const std::string& resource_id) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!resource_id.empty());

  std::string serialized_entry;
  const leveldb::Status status = resource_map_->Get(leveldb::ReadOptions(),
                                                    leveldb::Slice(resource_id),
                                                    &serialized_entry);
  if (!status.ok())
    return scoped_ptr<DriveEntryProto>();

  scoped_ptr<DriveEntryProto> entry(new DriveEntryProto);
  if (!entry->ParseFromString(serialized_entry))
    return scoped_ptr<DriveEntryProto>();
  return entry.Pass();
}

void DriveResourceMetadataStorageDB::RemoveEntry(
    const std::string& resource_id) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!resource_id.empty());

  const leveldb::Status status = resource_map_->Delete(
      leveldb::WriteOptions(),
      leveldb::Slice(resource_id));
  DCHECK(status.ok());
}

void DriveResourceMetadataStorageDB::Iterate(const IterateCallback& callback) {
  base::ThreadRestrictions::AssertIOAllowed();
  DCHECK(!callback.is_null());

  scoped_ptr<leveldb::Iterator> it(
      resource_map_->NewIterator(leveldb::ReadOptions()));

  // Skip the header entry.
  // Note: The header entry comes before all other entries because its key
  // starts with kDBKeyDelimeter. (i.e. '\0')
  it->Seek(leveldb::Slice(GetHeaderDBKey()));
  it->Next();

  DriveEntryProto entry;
  for (; it->Valid(); it->Next()) {
    if (!IsChildEntryKey(it->key()) &&
        entry.ParseFromArray(it->value().data(), it->value().size()))
      callback.Run(entry);
  }
}

void DriveResourceMetadataStorageDB::PutChild(
    const std::string& parent_resource_id,
    const std::string& child_name,
    const std::string& child_resource_id) {
  base::ThreadRestrictions::AssertIOAllowed();

  const leveldb::Status status = resource_map_->Put(
      leveldb::WriteOptions(),
      leveldb::Slice(GetChildEntryKey(parent_resource_id, child_name)),
      leveldb::Slice(child_resource_id));
  DCHECK(status.ok());
}

std::string DriveResourceMetadataStorageDB::GetChild(
    const std::string& parent_resource_id,
    const std::string& child_name) {
  base::ThreadRestrictions::AssertIOAllowed();

  std::string child_resource_id;
  resource_map_->Get(
      leveldb::ReadOptions(),
      leveldb::Slice(GetChildEntryKey(parent_resource_id, child_name)),
      &child_resource_id);
  return child_resource_id;
}

void DriveResourceMetadataStorageDB::GetChildren(
    const std::string& parent_resource_id,
    std::vector<std::string>* children) {
  base::ThreadRestrictions::AssertIOAllowed();

  // Iterate over all entries with keys starting with |parent_resource_id|.
  scoped_ptr<leveldb::Iterator> it(
      resource_map_->NewIterator(leveldb::ReadOptions()));
  for (it->Seek(parent_resource_id);
       it->Valid() && it->key().starts_with(leveldb::Slice(parent_resource_id));
       it->Next()) {
    if (IsChildEntryKey(it->key()))
      children->push_back(it->value().ToString());
  }
  DCHECK(it->status().ok());
}

void DriveResourceMetadataStorageDB::RemoveChild(
    const std::string& parent_resource_id,
    const std::string& child_name) {
  base::ThreadRestrictions::AssertIOAllowed();

  const leveldb::Status status = resource_map_->Delete(
      leveldb::WriteOptions(),
      leveldb::Slice(GetChildEntryKey(parent_resource_id, child_name)));
  DCHECK(status.ok());
}

void DriveResourceMetadataStorageDB::PutHeader(
    const DriveResourceMetadataHeader& header) {
  base::ThreadRestrictions::AssertIOAllowed();

  std::string serialized_header;
  if (!header.SerializeToString(&serialized_header)) {
    DLOG(ERROR) << "Failed to serialize the header";
    return;
  }

  const leveldb::Status status = resource_map_->Put(
      leveldb::WriteOptions(),
      leveldb::Slice(GetHeaderDBKey()),
      leveldb::Slice(serialized_header));
  DCHECK(status.ok());
}

scoped_ptr<DriveResourceMetadataHeader>
DriveResourceMetadataStorageDB::GetHeader() {
  base::ThreadRestrictions::AssertIOAllowed();

  std::string serialized_header;
  const leveldb::Status status = resource_map_->Get(
      leveldb::ReadOptions(),
      leveldb::Slice(GetHeaderDBKey()),
      &serialized_header);
  if (!status.ok())
    return scoped_ptr<DriveResourceMetadataHeader>();

  scoped_ptr<DriveResourceMetadataHeader> header(
      new DriveResourceMetadataHeader);
  if (!header->ParseFromString(serialized_header))
    return scoped_ptr<DriveResourceMetadataHeader>();
  return header.Pass();
}

bool DriveResourceMetadataStorageDB::CheckValidity() {
  base::ThreadRestrictions::AssertIOAllowed();

  // Perform read with checksums verification enalbed.
  leveldb::ReadOptions options;
  options.verify_checksums = true;

  scoped_ptr<leveldb::Iterator> it(resource_map_->NewIterator(options));
  it->SeekToFirst();

  // Check the header.
  DriveResourceMetadataHeader header;
  if (!it->Valid() ||
      it->key() != GetHeaderDBKey() ||  // Header entry must come first.
      !header.ParseFromArray(it->value().data(), it->value().size()) ||
      header.version() != kDBVersion) {
    DLOG(ERROR) << "Invalid header detected. version = " << header.version();
    return false;
  }

  // Check all entires.
  size_t num_entries_with_parent = 0;
  size_t num_child_entries = 0;
  DriveEntryProto entry;
  std::string serialized_parent_entry;
  std::string child_resource_id;
  for (it->Next(); it->Valid(); it->Next()) {
    // Count child entries.
    if (IsChildEntryKey(it->key())) {
      ++num_child_entries;
      continue;
    }

    // Check if stored data is broken.
    if (!entry.ParseFromArray(it->value().data(), it->value().size()) ||
        entry.resource_id() != it->key()) {
      DLOG(ERROR) << "Broken entry detected";
      return false;
    }

    if (!entry.parent_resource_id().empty()) {
      // Check if the parent entry is stored.
      leveldb::Status status = resource_map_->Get(
          options,
          leveldb::Slice(entry.parent_resource_id()),
          &serialized_parent_entry);
      if (!status.ok()) {
        DLOG(ERROR) << "Can't get parent entry. status = " << status.ToString();
        return false;
      }

      // Check if parent-child relationship is stored correctly.
      status = resource_map_->Get(
          options,
          leveldb::Slice(GetChildEntryKey(entry.parent_resource_id(),
                                          entry.base_name())),
          &child_resource_id);
      if (!status.ok() || child_resource_id != entry.resource_id()) {
        DLOG(ERROR) << "Child map is broken. status = " << status.ToString();
        return false;
      }
      ++num_entries_with_parent;
    }
  }
  if (!it->status().ok() || num_child_entries != num_entries_with_parent) {
    DLOG(ERROR) << "Error during checking resource map. status = "
                << it->status().ToString();
    return false;
  }
  return true;
}

}  // namespace drive
