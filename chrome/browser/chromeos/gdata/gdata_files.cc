// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_files.h"

#include <vector>

#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/tracked_objects.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/gdata/gdata.pb.h"
#include "chrome/browser/chromeos/gdata/gdata_util.h"
#include "chrome/browser/chromeos/gdata/gdata_wapi_parser.h"
#include "net/base/escape.h"

namespace gdata {
namespace {

const char kSlash[] = "/";
const char kEscapedSlash[] = "\xE2\x88\x95";

// Extracts resource_id out of edit url.
std::string ExtractResourceId(const GURL& url) {
  return net::UnescapeURLComponent(url.ExtractFileName(),
                                   net::UnescapeRule::URL_SPECIAL_CHARS);
}

// Returns true if |proto| is a valid proto as the root directory.
// Used to reject incompatible proto.
bool IsValidRootDirectoryProto(const GDataDirectoryProto& proto) {
  const GDataEntryProto& entry_proto = proto.gdata_entry();
  // The title field for the root directory was originally empty, then
  // changed to "gdata", then changed to "drive". Discard the proto data if
  // the older formats are detected. See crbug.com/128133 for details.
  if (entry_proto.title() != "drive") {
    LOG(ERROR) << "Incompatible proto detected (bad title): "
               << entry_proto.title();
    return false;
  }
  // The title field for the root directory was originally empty. Discard
  // the proto data if the older format is detected.
  if (entry_proto.resource_id() != kGDataRootDirectoryResourceId) {
    LOG(ERROR) << "Incompatible proto detected (bad resource ID): "
               << entry_proto.resource_id();
    return false;
  }

  return true;
}

}  // namespace

// GDataEntry class.

GDataEntry::GDataEntry(GDataDirectory* parent,
                       GDataDirectoryService* directory_service)
    : directory_service_(directory_service),
      deleted_(false) {
  SetParent(parent);
}

GDataEntry::~GDataEntry() {
}

GDataFile* GDataEntry::AsGDataFile() {
  return NULL;
}

GDataDirectory* GDataEntry::AsGDataDirectory() {
  return NULL;
}

const GDataFile* GDataEntry::AsGDataFileConst() const {
  // cast away const and call the non-const version. This is safe.
  return const_cast<GDataEntry*>(this)->AsGDataFile();
}

const GDataDirectory* GDataEntry::AsGDataDirectoryConst() const {
  // cast away const and call the non-const version. This is safe.
  return const_cast<GDataEntry*>(this)->AsGDataDirectory();
}

FilePath GDataEntry::GetFilePath() const {
  FilePath path;
  if (parent())
    path = parent()->GetFilePath();
  path = path.Append(base_name());
  return path;
}

void GDataEntry::SetParent(GDataDirectory* parent) {
  parent_ = parent;
  parent_resource_id_ = parent ? parent->resource_id() : "";
}

void GDataEntry::SetBaseNameFromTitle() {
  base_name_ = EscapeUtf8FileName(title_);
}

// static.
GDataEntry* GDataEntry::FromDocumentEntry(
    GDataDirectory* parent,
    DocumentEntry* doc,
    GDataDirectoryService* directory_service) {
  DCHECK(doc);
  if (doc->is_folder())
    return GDataDirectory::FromDocumentEntry(parent, doc, directory_service);
  else if (doc->is_hosted_document() || doc->is_file())
    return GDataFile::FromDocumentEntry(parent, doc, directory_service);

  return NULL;
}

// static.
std::string GDataEntry::EscapeUtf8FileName(const std::string& input) {
  std::string output;
  if (ReplaceChars(input, kSlash, std::string(kEscapedSlash), &output))
    return output;

  return input;
}

// static.
std::string GDataEntry::UnescapeUtf8FileName(const std::string& input) {
  std::string output = input;
  ReplaceSubstringsAfterOffset(&output, 0, std::string(kEscapedSlash), kSlash);
  return output;
}

// GDataFile class implementation.

GDataFile::GDataFile(GDataDirectory* parent,
                     GDataDirectoryService* directory_service)
    : GDataEntry(parent, directory_service),
      kind_(DocumentEntry::UNKNOWN),
      is_hosted_document_(false) {
  file_info_.is_directory = false;
}

GDataFile::~GDataFile() {
}

GDataFile* GDataFile::AsGDataFile() {
  return this;
}

void GDataFile::SetBaseNameFromTitle() {
  if (is_hosted_document_) {
    base_name_ = EscapeUtf8FileName(title_ + document_extension_);
  } else {
    GDataEntry::SetBaseNameFromTitle();
  }
}

// static.
GDataEntry* GDataFile::FromDocumentEntry(
    GDataDirectory* parent,
    DocumentEntry* doc,
    GDataDirectoryService* directory_service) {
  DCHECK(doc->is_hosted_document() || doc->is_file());
  GDataFile* file = new GDataFile(parent, directory_service);

  // For regular files, the 'filename' and 'title' attribute in the metadata
  // may be different (e.g. due to rename). To be consistent with the web
  // interface and other client to use the 'title' attribute, instead of
  // 'filename', as the file name in the local snapshot.
  file->title_ = UTF16ToUTF8(doc->title());

  // Check if this entry is a true file, or...
  if (doc->is_file()) {
    file->file_info_.size = doc->file_size();
    file->file_md5_ = doc->file_md5();

    // The resumable-edit-media link should only be present for regular
    // files as hosted documents are not uploadable.
    const Link* upload_link = doc->GetLinkByType(Link::RESUMABLE_EDIT_MEDIA);
    if (upload_link)
      file->upload_url_ = upload_link->href();
  } else {
    // ... a hosted document.
    // Attach .g<something> extension to hosted documents so we can special
    // case their handling in UI.
    // TODO(zelidrag): Figure out better way how to pass entry info like kind
    // to UI through the File API stack.
    file->document_extension_ = doc->GetHostedDocumentExtension();
    // We don't know the size of hosted docs and it does not matter since
    // is has no effect on the quota.
    file->file_info_.size = 0;
  }
  file->kind_ = doc->kind();
  const Link* edit_link = doc->GetLinkByType(Link::EDIT);
  if (edit_link)
    file->edit_url_ = edit_link->href();
  file->content_url_ = doc->content_url();
  file->content_mime_type_ = doc->content_mime_type();
  file->resource_id_ = doc->resource_id();
  file->is_hosted_document_ = doc->is_hosted_document();
  file->file_info_.last_modified = doc->updated_time();
  file->file_info_.last_accessed = doc->updated_time();
  file->file_info_.creation_time = doc->published_time();
  file->deleted_ = doc->deleted();
  const Link* parent_link = doc->GetLinkByType(Link::PARENT);
  if (parent_link)
    file->parent_resource_id_ = ExtractResourceId(parent_link->href());

  // SetBaseNameFromTitle() must be called after |title_|,
  // |is_hosted_document_| and |document_extension_| are set.
  file->SetBaseNameFromTitle();

  const Link* thumbnail_link = doc->GetLinkByType(Link::THUMBNAIL);
  if (thumbnail_link)
    file->thumbnail_url_ = thumbnail_link->href();

  const Link* alternate_link = doc->GetLinkByType(Link::ALTERNATE);
  if (alternate_link)
    file->alternate_url_ = alternate_link->href();

  return file;
}

// GDataDirectory class implementation.

GDataDirectory::GDataDirectory(GDataDirectory* parent,
                               GDataDirectoryService* directory_service)
    : GDataEntry(parent, directory_service) {
  file_info_.is_directory = true;
}

GDataDirectory::~GDataDirectory() {
  RemoveChildren();
}

GDataDirectory* GDataDirectory::AsGDataDirectory() {
  return this;
}

// static
GDataEntry* GDataDirectory::FromDocumentEntry(
    GDataDirectory* parent,
    DocumentEntry* doc,
    GDataDirectoryService* directory_service) {
  DCHECK(doc->is_folder());
  GDataDirectory* dir = new GDataDirectory(parent, directory_service);
  dir->title_ = UTF16ToUTF8(doc->title());
  // SetBaseNameFromTitle() must be called after |title_| is set.
  dir->SetBaseNameFromTitle();
  dir->file_info_.last_modified = doc->updated_time();
  dir->file_info_.last_accessed = doc->updated_time();
  dir->file_info_.creation_time = doc->published_time();
  dir->resource_id_ = doc->resource_id();
  dir->content_url_ = doc->content_url();
  dir->deleted_ = doc->deleted();

  const Link* edit_link = doc->GetLinkByType(Link::EDIT);
  DCHECK(edit_link) << "No edit link for dir " << dir->title_;
  if (edit_link)
    dir->edit_url_ = edit_link->href();

  const Link* parent_link = doc->GetLinkByType(Link::PARENT);
  if (parent_link)
    dir->parent_resource_id_ = ExtractResourceId(parent_link->href());

  const Link* upload_link = doc->GetLinkByType(Link::RESUMABLE_CREATE_MEDIA);
  if (upload_link)
    dir->upload_url_ = upload_link->href();

  return dir;
}

void GDataDirectory::AddEntry(GDataEntry* entry) {
  // The entry name may have been changed due to prior name de-duplication.
  // We need to first restore the file name based on the title before going
  // through name de-duplication again when it is added to another directory.
  entry->SetBaseNameFromTitle();

  // Do file name de-duplication - find files with the same name and
  // append a name modifier to the name.
  int max_modifier = 1;
  FilePath full_file_name(entry->base_name());
  const std::string extension = full_file_name.Extension();
  const std::string file_name = full_file_name.RemoveExtension().value();
  while (FindChild(full_file_name.value())) {
    if (!extension.empty()) {
      full_file_name = FilePath(base::StringPrintf("%s (%d)%s",
                                                   file_name.c_str(),
                                                   ++max_modifier,
                                                   extension.c_str()));
    } else {
      full_file_name = FilePath(base::StringPrintf("%s (%d)",
                                                   file_name.c_str(),
                                                   ++max_modifier));
    }
  }
  entry->set_base_name(full_file_name.value());

  DVLOG(1) << "AddEntry: dir = " << GetFilePath().value()
           << ", file = " + entry->base_name()
           << ", parent resource = " << entry->parent_resource_id()
           << ", resource = " + entry->resource_id();


  // Add entry to resource map.
  if (directory_service_)
    directory_service_->AddEntryToResourceMap(entry);

  // Setup child and parent links.
  AddChild(entry);
  entry->SetParent(this);
}

bool GDataDirectory::TakeEntry(GDataEntry* entry) {
  DCHECK(entry);
  DCHECK(entry->parent());

  entry->parent()->RemoveChild(entry);
  AddEntry(entry);

  return true;
}

bool GDataDirectory::TakeOverEntries(GDataDirectory* dir) {
  for (GDataFileCollection::iterator iter = dir->child_files_.begin();
       iter != dir->child_files_.end(); ++iter) {
    AddEntry(iter->second);
  }
  dir->child_files_.clear();

  for (GDataDirectoryCollection::iterator iter =
      dir->child_directories_.begin();
       iter != dir->child_directories_.end(); ++iter) {
    AddEntry(iter->second);
  }
  dir->child_directories_.clear();
  return true;
}

bool GDataDirectory::RemoveEntry(GDataEntry* entry) {
  DCHECK(entry);

  if (!RemoveChild(entry))
    return false;

  delete entry;
  return true;
}

GDataEntry* GDataDirectory::FindChild(
    const FilePath::StringType& file_name) const {
  GDataFileCollection::const_iterator it = child_files_.find(file_name);
  if (it != child_files_.end())
    return it->second;

  GDataDirectoryCollection::const_iterator itd =
      child_directories_.find(file_name);
  if (itd != child_directories_.end())
    return itd->second;

  return NULL;
}

void GDataDirectory::AddChild(GDataEntry* entry) {
  DCHECK(entry);

  GDataFile* file = entry->AsGDataFile();
  if (file)
    child_files_.insert(std::make_pair(entry->base_name(), file));

  GDataDirectory* directory = entry->AsGDataDirectory();
  if (directory)
    child_directories_.insert(std::make_pair(entry->base_name(), directory));
}

bool GDataDirectory::RemoveChild(GDataEntry* entry) {
  DCHECK(entry);

  const std::string file_name(entry->base_name());
  GDataEntry* found_entry = FindChild(file_name);
  if (!found_entry)
    return false;

  DCHECK_EQ(entry, found_entry);

  // Remove entry from resource map first.
  if (directory_service_)
    directory_service_->RemoveEntryFromResourceMap(entry);

  // Then delete it from tree.
  child_files_.erase(file_name);
  child_directories_.erase(file_name);

  return true;
}

void GDataDirectory::RemoveChildren() {
  RemoveChildFiles();
  RemoveChildDirectories();
}

void GDataDirectory::RemoveChildFiles() {
  for (GDataFileCollection::const_iterator iter = child_files_.begin();
       iter != child_files_.end(); ++iter) {
    if (directory_service_)
      directory_service_->RemoveEntryFromResourceMap(iter->second);
  }
  STLDeleteValues(&child_files_);
  child_files_.clear();
}

void GDataDirectory::RemoveChildDirectories() {
  for (GDataDirectoryCollection::iterator iter = child_directories_.begin();
       iter != child_directories_.end(); ++iter) {
    GDataDirectory* dir = iter->second;
    // Remove directories recursively.
    dir->RemoveChildren();
    if (directory_service_)
      directory_service_->RemoveEntryFromResourceMap(dir);
  }
  STLDeleteValues(&child_directories_);
  child_directories_.clear();
}

// GDataDirectoryService class implementation.

GDataDirectoryService::GDataDirectoryService()
    : serialized_size_(0),
      largest_changestamp_(0),
      origin_(UNINITIALIZED) {
  root_.reset(new GDataDirectory(NULL, this));
  root_->set_title(kGDataRootDirectory);
  root_->SetBaseNameFromTitle();
  root_->set_resource_id(kGDataRootDirectoryResourceId);
  AddEntryToResourceMap(root_.get());
}

GDataDirectoryService::~GDataDirectoryService() {
  // Note that children have a reference to root_,
  // so we need to delete them here.
  root_->RemoveChildren();
  RemoveEntryFromResourceMap(root_.get());
  DCHECK(resource_map_.empty());
  resource_map_.clear();
}

void GDataDirectoryService::AddEntryToDirectory(
    const FilePath& directory_path,
    GDataEntry* entry,
    const FileOperationCallback& callback) {
  GDataEntry* destination = FindEntryByPathSync(directory_path);
  GDataFileError error = GDATA_FILE_ERROR_FAILED;
  if (!destination) {
    error = GDATA_FILE_ERROR_NOT_FOUND;
  } else if (!destination->AsGDataDirectory()) {
    error = GDATA_FILE_ERROR_NOT_A_DIRECTORY;
  } else {
    destination->AsGDataDirectory()->AddEntry(entry);
    error = GDATA_FILE_OK;
  }
  if (!callback.is_null()) {
    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE, base::Bind(callback, error));
  }
}

void GDataDirectoryService::AddEntryToResourceMap(GDataEntry* entry) {
  // GDataFileSystem has already locked.
  DVLOG(1) << "AddEntryToResourceMap " << entry->resource_id();
  resource_map_.insert(std::make_pair(entry->resource_id(), entry));
}

void GDataDirectoryService::RemoveEntryFromResourceMap(GDataEntry* entry) {
  // GDataFileSystem has already locked.
  resource_map_.erase(entry->resource_id());
}

GDataEntry* GDataDirectoryService::FindEntryByPathSync(
    const FilePath& file_path) {
  std::vector<FilePath::StringType> components;
  file_path.GetComponents(&components);

  GDataDirectory* current_dir = root_.get();
  FilePath directory_path;

  for (size_t i = 0; i < components.size() && current_dir; i++) {
    directory_path = directory_path.Append(current_dir->base_name());

    // Last element must match, if not last then it must be a directory.
    if (i == components.size() - 1) {
      if (current_dir->base_name() == components[i])
        return current_dir;
      else
        return NULL;
    }

    // Not the last part of the path, search for the next segment.
    GDataEntry* entry = current_dir->FindChild(components[i + 1]);
    if (!entry) {
      return NULL;
    }

    // Found file, must be the last segment.
    if (entry->file_info().is_directory) {
      // Found directory, continue traversal.
      current_dir = entry->AsGDataDirectory();
    } else {
      if ((i + 1) == (components.size() - 1))
        return entry;
      else
        return NULL;
    }
  }
  return NULL;
}

void GDataDirectoryService::FindEntryByPathAndRunSync(
    const FilePath& search_file_path,
    const FindEntryCallback& callback) {
  GDataEntry* entry = FindEntryByPathSync(search_file_path);
  callback.Run(entry ? GDATA_FILE_OK : GDATA_FILE_ERROR_NOT_FOUND, entry);
}

GDataEntry* GDataDirectoryService::GetEntryByResourceId(
    const std::string& resource) {
  // GDataFileSystem has already locked.
  ResourceMap::const_iterator iter = resource_map_.find(resource);
  return iter == resource_map_.end() ? NULL : iter->second;
}

void GDataDirectoryService::GetEntryByResourceIdAsync(
    const std::string& resource_id,
    const GetEntryByResourceIdCallback& callback) {
  GDataEntry* entry = GetEntryByResourceId(resource_id);
  callback.Run(entry);
}

void GDataDirectoryService::RefreshFile(scoped_ptr<GDataFile> fresh_file) {
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
    scoped_ptr<GDataFile> fresh_file,
    GDataEntry* old_entry) {
  GDataDirectory* entry_parent = old_entry ? old_entry->parent() : NULL;
  if (entry_parent) {
    DCHECK_EQ(fresh_file->resource_id(), old_entry->resource_id());
    DCHECK(old_entry->AsGDataFile());

    entry_parent->RemoveEntry(old_entry);
    entry_parent->AddEntry(fresh_file.release());
  }
}

// Convert to/from proto.

// static
void GDataEntry::ConvertProtoToPlatformFileInfo(
    const PlatformFileInfoProto& proto,
    base::PlatformFileInfo* file_info) {
  file_info->size = proto.size();
  file_info->is_directory = proto.is_directory();
  file_info->is_symbolic_link = proto.is_symbolic_link();
  file_info->last_modified = base::Time::FromInternalValue(
      proto.last_modified());
  file_info->last_accessed = base::Time::FromInternalValue(
      proto.last_accessed());
  file_info->creation_time = base::Time::FromInternalValue(
      proto.creation_time());
}

// static
void GDataEntry::ConvertPlatformFileInfoToProto(
    const base::PlatformFileInfo& file_info,
    PlatformFileInfoProto* proto) {
  proto->set_size(file_info.size);
  proto->set_is_directory(file_info.is_directory);
  proto->set_is_symbolic_link(file_info.is_symbolic_link);
  proto->set_last_modified(file_info.last_modified.ToInternalValue());
  proto->set_last_accessed(file_info.last_accessed.ToInternalValue());
  proto->set_creation_time(file_info.creation_time.ToInternalValue());
}

bool GDataEntry::FromProto(const GDataEntryProto& proto) {
  ConvertProtoToPlatformFileInfo(proto.file_info(), &file_info_);

  // Don't copy from proto.base_name() as base_name_ is computed in
  // SetBaseNameFromTitle().
  title_ = proto.title();
  resource_id_ = proto.resource_id();
  parent_resource_id_ = proto.parent_resource_id();
  edit_url_ = GURL(proto.edit_url());
  content_url_ = GURL(proto.content_url());
  upload_url_ = GURL(proto.upload_url());
  SetBaseNameFromTitle();

  // Reject older protobuf that does not contain the upload URL.  This URL is
  // necessary for uploading files.
  if (!proto.has_upload_url()) {
    LOG(ERROR) << "Incompatible proto detected (no upload URL): "
               << proto.title();
    return false;
  }

  return true;
}

void GDataEntry::ToProto(GDataEntryProto* proto) const {
  ConvertPlatformFileInfoToProto(file_info_, proto->mutable_file_info());

  // The base_name field is used in GetFileInfoByPathAsync(). As shown in
  // FromProto(), the value is discarded when deserializing from proto.
  proto->set_base_name(base_name_);
  proto->set_title(title_);
  proto->set_resource_id(resource_id_);
  proto->set_parent_resource_id(parent_resource_id_);
  proto->set_edit_url(edit_url_.spec());
  proto->set_content_url(content_url_.spec());
  proto->set_upload_url(upload_url_.spec());
}

void GDataEntry::ToProtoFull(GDataEntryProto* proto) const {
  if (AsGDataFileConst()) {
    AsGDataFileConst()->ToProto(proto);
  } else if (AsGDataDirectoryConst()) {
    // Unlike files, directories don't have directory specific info, so just
    // calling GDataEntry::ToProto().
    ToProto(proto);
  } else {
    NOTREACHED();
  }
}

bool GDataFile::FromProto(const GDataEntryProto& proto) {
  DCHECK(!proto.file_info().is_directory());

  if (!GDataEntry::FromProto(proto))
    return false;

  thumbnail_url_ = GURL(proto.file_specific_info().thumbnail_url());
  alternate_url_ = GURL(proto.file_specific_info().alternate_url());
  content_mime_type_ = proto.file_specific_info().content_mime_type();
  file_md5_ = proto.file_specific_info().file_md5();
  document_extension_ = proto.file_specific_info().document_extension();
  is_hosted_document_ = proto.file_specific_info().is_hosted_document();

  return true;
}

void GDataFile::ToProto(GDataEntryProto* proto) const {
  GDataEntry::ToProto(proto);
  DCHECK(!proto->file_info().is_directory());
  GDataFileSpecificInfo* file_specific_info =
      proto->mutable_file_specific_info();
  file_specific_info->set_thumbnail_url(thumbnail_url_.spec());
  file_specific_info->set_alternate_url(alternate_url_.spec());
  file_specific_info->set_content_mime_type(content_mime_type_);
  file_specific_info->set_file_md5(file_md5_);
  file_specific_info->set_document_extension(document_extension_);
  file_specific_info->set_is_hosted_document(is_hosted_document_);
}

bool GDataDirectory::FromProto(const GDataDirectoryProto& proto) {
  DCHECK(proto.gdata_entry().file_info().is_directory());
  DCHECK(!proto.gdata_entry().has_file_specific_info());

  for (int i = 0; i < proto.child_files_size(); ++i) {
    scoped_ptr<GDataFile> file(new GDataFile(this, directory_service_));
    if (!file->FromProto(proto.child_files(i))) {
      RemoveChildren();
      return false;
    }
    AddEntry(file.release());
  }
  for (int i = 0; i < proto.child_directories_size(); ++i) {
    scoped_ptr<GDataDirectory> dir(new GDataDirectory(this,
                                                      directory_service_));
    if (!dir->FromProto(proto.child_directories(i))) {
      RemoveChildren();
      return false;
    }
    AddEntry(dir.release());
  }

  // The states of the directory should be updated after children are
  // handled successfully, so that incomplete states are not left.
  if (!GDataEntry::FromProto(proto.gdata_entry()))
    return false;

  return true;
}

void GDataDirectory::ToProto(GDataDirectoryProto* proto) const {
  GDataEntry::ToProto(proto->mutable_gdata_entry());
  DCHECK(proto->gdata_entry().file_info().is_directory());

  for (GDataFileCollection::const_iterator iter = child_files_.begin();
       iter != child_files_.end(); ++iter) {
    GDataFile* file = iter->second;
    file->ToProto(proto->add_child_files());
  }
  for (GDataDirectoryCollection::const_iterator iter =
       child_directories_.begin();
       iter != child_directories_.end(); ++iter) {
    GDataDirectory* dir = iter->second;
    dir->ToProto(proto->add_child_directories());
  }
}

void GDataEntry::SerializeToString(std::string* serialized_proto) const {
  const GDataFile* file = AsGDataFileConst();
  const GDataDirectory* dir = AsGDataDirectoryConst();

  if (file) {
    GDataEntryProto entry_proto;
    file->ToProto(&entry_proto);
    const bool ok = entry_proto.SerializeToString(serialized_proto);
    DCHECK(ok);
  } else if (dir) {
    GDataDirectoryProto dir_proto;
    dir->ToProto(&dir_proto);
    const bool ok = dir_proto.SerializeToString(serialized_proto);
    DCHECK(ok);
  }
}

// static
scoped_ptr<GDataEntry> GDataEntry::FromProtoString(
    const std::string& serialized_proto) {
  // First try to parse as GDataDirectoryProto. Note that this can succeed for
  // a serialized_proto that's really a GDataEntryProto - we have to check
  // is_directory to be sure.
  GDataDirectoryProto dir_proto;
  bool ok = dir_proto.ParseFromString(serialized_proto);
  if (ok && dir_proto.gdata_entry().file_info().is_directory()) {
    scoped_ptr<GDataDirectory> dir(new GDataDirectory(NULL, NULL));
    if (!dir->FromProto(dir_proto))
      return scoped_ptr<GDataEntry>(NULL);
    return scoped_ptr<GDataEntry>(dir.release());
  }

  GDataEntryProto entry_proto;
  ok = entry_proto.ParseFromString(serialized_proto);
  if (ok) {
    DCHECK(!entry_proto.file_info().is_directory());
    scoped_ptr<GDataFile> file(new GDataFile(NULL, NULL));
    if (!file->FromProto(entry_proto))
      return scoped_ptr<GDataEntry>(NULL);
    return scoped_ptr<GDataEntry>(file.release());
  }
  return scoped_ptr<GDataEntry>(NULL);
}

void GDataDirectoryService::SerializeToString(
    std::string* serialized_proto) const {
  GDataRootDirectoryProto proto;
  root_->ToProto(proto.mutable_gdata_directory());
  proto.set_largest_changestamp(largest_changestamp_);
  proto.set_version(kProtoVersion);

  const bool ok = proto.SerializeToString(serialized_proto);
  DCHECK(ok);
}

bool GDataDirectoryService::ParseFromString(
    const std::string& serialized_proto) {
  GDataRootDirectoryProto proto;
  if (!proto.ParseFromString(serialized_proto))
    return false;

  if (proto.version() != kProtoVersion) {
    LOG(ERROR) << "Incompatible proto detected (incompatible version): "
               << proto.version();
    return false;
  }

  if (!IsValidRootDirectoryProto(proto.gdata_directory()))
    return false;

  if (!root_->FromProto(proto.gdata_directory()))
    return false;

  origin_ = FROM_CACHE;
  largest_changestamp_ = proto.largest_changestamp();

  return true;
}

}  // namespace gdata
