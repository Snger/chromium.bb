// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_file_system.h"

#include <vector>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "base/platform_file.h"
#include "base/stringprintf.h"
#include "base/string_util.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile_dependency_manager.h"
#include "chrome/browser/chromeos/gdata/gdata.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/escape.h"
#include "webkit/fileapi/file_system_file_util_proxy.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"

using content::BrowserThread;

namespace {

// Content refresh time.
const int kRefreshTimeInSec = 5*60;

const char kGDataRootDirectory[] = "gdata";
const char kFeedField[] = "feed";

// Converts gdata error code into file platform error code.
base::PlatformFileError GDataToPlatformError(gdata::GDataErrorCode status) {
  switch (status) {
    case gdata::HTTP_SUCCESS:
    case gdata::HTTP_CREATED:
      return base::PLATFORM_FILE_OK;
    case gdata::HTTP_UNAUTHORIZED:
    case gdata::HTTP_FORBIDDEN:
      return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;
    case gdata::HTTP_NOT_FOUND:
      return base::PLATFORM_FILE_ERROR_NOT_FOUND;
    case gdata::GDATA_PARSE_ERROR:
    case gdata::GDATA_FILE_ERROR:
      return base::PLATFORM_FILE_ERROR_ABORT;
    default:
      return base::PLATFORM_FILE_ERROR_FAILED;
  }
}

// Escapes file names since hosted documents from gdata can actually have
// forward slashes in their titles.
std::string EscapeFileName(const std::string& input) {
  std::string tmp;
  std::string output;
  if (ReplaceChars(input, "%", std::string("%25"), &tmp) &&
      ReplaceChars(tmp, "/", std::string("%2F"), &output)) {
    return output;
  }

  return input;
}

}  // namespace

namespace gdata {

// FindFileDelegate class implementation.

FindFileDelegate::~FindFileDelegate() {
}

// ReadOnlyFindFileDelegate class implementation.

ReadOnlyFindFileDelegate::ReadOnlyFindFileDelegate() : file_(NULL) {
}

void ReadOnlyFindFileDelegate::OnFileFound(gdata::GDataFile* file) {
  // file_ should be set only once since OnFileFound() is a terminal
  // function.
  DCHECK(!file_);
  DCHECK(!file->file_info().is_directory);
  file_ = file;
}

void ReadOnlyFindFileDelegate::OnDirectoryFound(const FilePath&,
                                                GDataDirectory* dir) {
  // file_ should be set only once since OnDirectoryFound() is a terminal
  // function.
  DCHECK(!file_);
  DCHECK(dir->file_info().is_directory);
  file_ = dir;
}

FindFileDelegate::FindFileTraversalCommand
ReadOnlyFindFileDelegate::OnEnterDirectory(const FilePath&, GDataDirectory*) {
  // Keep traversing while doing read only lookups.
  return FIND_FILE_CONTINUES;
}

void ReadOnlyFindFileDelegate::OnError(base::PlatformFileError) {
  file_ = NULL;
}

// GDataFileBase class.

GDataFileBase::GDataFileBase(GDataDirectory* parent) : parent_(parent) {
}

GDataFileBase::~GDataFileBase() {
}

GDataFile* GDataFileBase::AsGDataFile() {
  return NULL;
}

FilePath GDataFileBase::GetFilePath() {
  FilePath path;
  std::vector<FilePath::StringType> parts;
  for (GDataFileBase* file = this; file != NULL; file = file->parent())
    parts.push_back(file->file_name());

  // Paste paths parts back together in reverse order from upward tree
  // traversal.
  for (std::vector<FilePath::StringType>::reverse_iterator iter =
           parts.rbegin();
       iter != parts.rend(); ++iter) {
    path = path.Append(*iter);
  }
  return path;
}

GDataDirectory* GDataFileBase::AsGDataDirectory() {
  return NULL;
}


GDataFileBase* GDataFileBase::FromDocumentEntry(GDataDirectory* parent,
                                                DocumentEntry* doc) {
  DCHECK(parent);
  DCHECK(doc);
  if (doc->is_folder())
    return GDataDirectory::FromDocumentEntry(parent, doc);
  else if (doc->is_hosted_document() || doc->is_file())
    return GDataFile::FromDocumentEntry(parent, doc);

  return NULL;
}

GDataFileBase* GDataFile::FromDocumentEntry(GDataDirectory* parent,
                                            DocumentEntry* doc) {
  DCHECK(doc->is_hosted_document() || doc->is_file());
  GDataFile* file = new GDataFile(parent);
  // Check if this entry is a true file, or...
  if (doc->is_file()) {
    file->original_file_name_ = UTF16ToUTF8(doc->filename());
    file->file_name_ =
        EscapeFileName(file->original_file_name_);
    file->file_info_.size = doc->file_size();
    file->file_md5_ = doc->file_md5();
  } else {
    // ... a hosted document.
    file->original_file_name_ = UTF16ToUTF8(doc->title());
    // Attach .g<something> extension to hosted documents so we can special
    // case their handling in UI.
    // TODO(zelidrag): Figure out better way how to pass entry info like kind
    // to UI through the File API stack.
    file->file_name_ = EscapeFileName(
        base::StringPrintf("%s.g%s",
                           file->original_file_name_.c_str(),
                           doc->GetEntryKindText().c_str()));
    // We don't know the size of hosted docs and it does not matter since
    // is has no effect on the quota.
    file->file_info_.size = 0;
  }
  file->kind_ = doc->kind();
  const Link* self_link = doc->GetLinkByType(Link::SELF);
  if (self_link)
    file->self_url_ = self_link->href();
  file->content_url_ = doc->content_url();
  file->content_mime_type_ = doc->content_mime_type();
  file->etag_ = doc->etag();
  file->resource_id_ = doc->resource_id();
  file->id_ = doc->id();
  file->file_info_.last_modified = doc->updated_time();
  file->file_info_.last_accessed = doc->updated_time();
  file->file_info_.creation_time = doc->published_time();
  return file;
}

// GDataFile class implementation.

GDataFile::GDataFile(GDataDirectory* parent)
    : GDataFileBase(parent), kind_(gdata::DocumentEntry::UNKNOWN) {
  DCHECK(parent);
}

GDataFile::~GDataFile() {
}

GDataFile* GDataFile::AsGDataFile() {
  return this;
}

// GDataDirectory class implementation.

GDataDirectory::GDataDirectory(GDataDirectory* parent) : GDataFileBase(parent) {
  file_info_.is_directory = true;
}

GDataDirectory::~GDataDirectory() {
  RemoveChildren();
}

GDataDirectory* GDataDirectory::AsGDataDirectory() {
  return this;
}

// static
GDataFileBase* GDataDirectory::FromDocumentEntry(GDataDirectory* parent,
                                                 DocumentEntry* doc) {
  DCHECK(parent);
  DCHECK(doc->is_folder());
  GDataDirectory* dir = new GDataDirectory(parent);
  dir->file_name_ = UTF16ToUTF8(doc->title());
  dir->file_info_.last_modified = doc->updated_time();
  dir->file_info_.last_accessed = doc->updated_time();
  dir->file_info_.creation_time = doc->published_time();
  // Extract feed link.
  dir->start_feed_url_ = doc->content_url();
  return dir;
}

void GDataDirectory::RemoveChildren() {
  STLDeleteValues(&children_);
  children_.clear();
}

bool GDataDirectory::NeedsRefresh(GURL* feed_url) {
  if ((base::Time::Now() - refresh_time_).InSeconds() < kRefreshTimeInSec)
    return false;

  *feed_url = start_feed_url_;
  return true;
}

void GDataDirectory::AddFile(GDataFileBase* file) {
  // Do file name de-duplication - find files with the same name and
  // append a name modifier to the name.
  int max_modifier = 1;
  FilePath full_file_name(file->file_name());
  std::string extension = full_file_name.Extension();
  std::string file_name = full_file_name.RemoveExtension().value();
  while (children_.find(full_file_name.value()) !=  children_.end()) {
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
  if (full_file_name.value() != file->file_name())
    file->set_file_name(full_file_name.value());
  children_.insert(std::make_pair(file->file_name(), file));
}

bool GDataDirectory::RemoveFile(GDataFileBase* file) {
  GDataFileCollection::iterator iter = children_.find(file->file_name());
  if (children_.find(file->file_name()) ==  children_.end())
    return false;

  DCHECK(iter->second);
  delete iter->second;
  children_.erase(iter);
  return true;
}

// GDataFileSystem::FindFileParams struct implementation.

GDataFileSystem::FindFileParams::FindFileParams(
    const FilePath& in_file_path,
    bool in_require_content,
    const FilePath& in_directory_path,
    const GURL& in_feed_url,
    bool in_initial_feed,
    scoped_refptr<FindFileDelegate> in_delegate)
    : file_path(in_file_path),
      require_content(in_require_content),
      directory_path(in_directory_path),
      feed_url(in_feed_url),
      initial_feed(in_initial_feed),
      delegate(in_delegate) {
}

GDataFileSystem::FindFileParams::~FindFileParams() {
}

// GDataFileSystem class implementatsion.

GDataFileSystem::GDataFileSystem(Profile* profile)
    : profile_(profile),
      documents_service_(new DocumentsService) {
  documents_service_->Initialize(profile_);
  root_.reset(new GDataDirectory(NULL));
  root_->set_file_name(kGDataRootDirectory);
}

GDataFileSystem::~GDataFileSystem() {
}

void GDataFileSystem::Shutdown() {
  // TODO(satorux): We should probably cancel or wait for the in-flight
  // operation here.
}

void GDataFileSystem::Authenticate(const AuthStatusCallback& callback) {
  if (documents_service_->IsFullyAuthenticated()) {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, gdata::HTTP_SUCCESS,
                   documents_service_->oauth2_auth_token()));
  } else if (documents_service_->IsPartiallyAuthenticated()) {
    // We have refresh token, let's gets authenticated.
    documents_service_->StartAuthentication(callback);
  } else {
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(callback, gdata::HTTP_SUCCESS, std::string()));
  }
}

void GDataFileSystem::FindFileByPath(
    const FilePath& file_path, scoped_refptr<FindFileDelegate> delegate) {
  base::AutoLock lock(lock_);
  UnsafeFindFileByPath(file_path, delegate);
}

void GDataFileSystem::StartDirectoryRefresh(
    const FindFileParams& params) {
  // Kick off document feed fetching here if we don't have complete data
  // to finish this call.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &GDataFileSystem::RefreshFeedOnUIThread,
          this,
          params.feed_url,
          base::Bind(&GDataFileSystem::OnGetDocuments,
                     this,
                     params)));
}

void GDataFileSystem::Remove(const FilePath& file_path,
    bool is_recursive,
    const FileOperationCallback& callback) {
  // Kick off document deletion.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &GDataFileSystem::RemoveOnUIThread,
          this,
          file_path,
          is_recursive,
          base::Bind(&GDataFileSystem::OnRemovedDocument,
                     this,
                     callback,
                     file_path)));
}

void GDataFileSystem::UnsafeFindFileByPath(
    const FilePath& file_path, scoped_refptr<FindFileDelegate> delegate) {
  lock_.AssertAcquired();

  std::vector<FilePath::StringType> components;
  file_path.GetComponents(&components);

  GDataDirectory* current_dir = root_.get();
  FilePath directory_path;
  for (size_t i = 0; i < components.size() && current_dir; i++) {
    directory_path = directory_path.Append(current_dir->file_name());

    // Last element must match, if not last then it must be a directory.
    if (i == components.size() - 1) {
      if (current_dir->file_name() == components[i])
        delegate->OnDirectoryFound(directory_path, current_dir);
      else
        delegate->OnError(base::PLATFORM_FILE_ERROR_NOT_FOUND);

      return;
    }

    if (delegate->OnEnterDirectory(directory_path, current_dir) ==
        FindFileDelegate::FIND_FILE_TERMINATES) {
      return;
    }

    // Not the last part of the path, search for the next segment.
    GDataFileCollection::const_iterator file_iter =
        current_dir->children().find(components[i + 1]);
    if (file_iter == current_dir->children().end()) {
      delegate->OnError(base::PLATFORM_FILE_ERROR_NOT_FOUND);
      return;
    }

    // Found file, must be the last segment.
    if (file_iter->second->file_info().is_directory) {
      // Found directory, continue traversal.
      current_dir = file_iter->second->AsGDataDirectory();
    } else {
      if ((i + 1) == (components.size() - 1))
        delegate->OnFileFound(file_iter->second->AsGDataFile());
      else
        delegate->OnError(base::PLATFORM_FILE_ERROR_NOT_FOUND);

      return;
    }
  }
  delegate->OnError(base::PLATFORM_FILE_ERROR_NOT_FOUND);
}

void GDataFileSystem::RefreshFeedOnUIThread(const GURL& feed_url,
    const GetDataCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  documents_service_->GetDocuments(feed_url, callback);
}

void GDataFileSystem::RemoveOnUIThread(
    const FilePath& file_path, bool is_recursive,
    const EntryActionCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  GURL document_url = GetDocumentUrlFromPath(file_path);
  if (document_url.is_empty()) {
    if (!callback.is_null())
      callback.Run(HTTP_NOT_FOUND, GURL());

    return;
  }
  documents_service_->DeleteDocument(document_url, callback);
}

GURL GDataFileSystem::GetDocumentUrlFromPath(const FilePath& file_path) {
  base::AutoLock lock(lock_);
  // Find directory element within the cached file system snapshot.
  scoped_refptr<ReadOnlyFindFileDelegate> find_delegate(
      new ReadOnlyFindFileDelegate());
  UnsafeFindFileByPath(file_path, find_delegate);
  if (!find_delegate->file())
    return GURL();

  return find_delegate->file()->self_url();
}

void GDataFileSystem::OnGetDocuments(
    const FindFileParams& params,
    GDataErrorCode status,
    base::Value* data) {
  base::PlatformFileError error = GDataToPlatformError(status);

  if (error == base::PLATFORM_FILE_OK &&
      (!data || data->GetType() != Value::TYPE_DICTIONARY)) {
    LOG(WARNING) << "No feed content!";
    error = base::PLATFORM_FILE_ERROR_FAILED;
  }

  if (error != base::PLATFORM_FILE_OK) {
    params.delegate->OnError(error);
    return;
  }

  GURL next_feed_url;
  error = UpdateDirectoryWithDocumentFeed(
     params.directory_path, params.feed_url, data, params.initial_feed,
     &next_feed_url);
  if (error != base::PLATFORM_FILE_OK) {
    params.delegate->OnError(error);
    return;
  }

  // Fetch the rest of the content if the feed is not completed.
  if (!next_feed_url.is_empty()) {
    StartDirectoryRefresh(FindFileParams(params.file_path,
                                         params.require_content,
                                         params.directory_path,
                                         next_feed_url,
                                         false,  /* initial_feed */
                                         params.delegate));
    return;
  }

  // Continue file content search operation.
  FindFileByPath(params.file_path,
                 params.delegate);
}


void GDataFileSystem::OnRemovedDocument(
    const FileOperationCallback& callback,
    const FilePath& file_path,
    GDataErrorCode status, const GURL& document_url) {
  base::PlatformFileError error = GDataToPlatformError(status);

  if (error == base::PLATFORM_FILE_OK)
    error = RemoveFileFromFileSystem(file_path);

  if (!callback.is_null())
    callback.Run(error);
}

base::PlatformFileError GDataFileSystem::RemoveFileFromFileSystem(
    const FilePath& file_path) {
  // We need to lock here as well (despite FindFileByPath lock) since directory
  // instance below is a 'live' object.
  base::AutoLock lock(lock_);

  // Find directory element within the cached file system snapshot.
  scoped_refptr<ReadOnlyFindFileDelegate> update_delegate(
      new ReadOnlyFindFileDelegate());
  UnsafeFindFileByPath(file_path, update_delegate);

  GDataFileBase* file = update_delegate->file();

  if (!file)
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  // You can't remove root element.
  if (!file->parent())
    return base::PLATFORM_FILE_ERROR_ACCESS_DENIED;

  if (!file->parent()->RemoveFile(file))
    return base::PLATFORM_FILE_ERROR_NOT_FOUND;

  return base::PLATFORM_FILE_OK;
}


base::PlatformFileError GDataFileSystem::UpdateDirectoryWithDocumentFeed(
    const FilePath& directory_path, const GURL& feed_url,
    base::Value* data, bool is_initial_feed, GURL* next_feed) {
  base::DictionaryValue* feed_dict = NULL;
  scoped_ptr<DocumentFeed> feed;
  if (!static_cast<base::DictionaryValue*>(data)->GetDictionary(
          kFeedField, &feed_dict)) {
    return base::PLATFORM_FILE_ERROR_FAILED;
  }

  // Parse the document feed.
  feed.reset(DocumentFeed::CreateFrom(feed_dict));
  if (!feed.get())
    return base::PLATFORM_FILE_ERROR_FAILED;

  // We need to lock here as well (despite FindFileByPath lock) since directory
  // instance below is a 'live' object.
  base::AutoLock lock(lock_);

  // Find directory element within the cached file system snapshot.
  scoped_refptr<ReadOnlyFindFileDelegate> update_delegate(
      new ReadOnlyFindFileDelegate());
  UnsafeFindFileByPath(directory_path, update_delegate);

  GDataFileBase* file = update_delegate->file();
  if (!file)
    return base::PLATFORM_FILE_ERROR_FAILED;

  GDataDirectory* dir = file->AsGDataDirectory();
  if (!dir)
    return base::PLATFORM_FILE_ERROR_FAILED;

  dir->set_start_feed_url(feed_url);
  dir->set_refresh_time(base::Time::Now());
  if (feed->GetNextFeedURL(next_feed))
    dir->set_next_feed_url(*next_feed);

  // Remove all child elements if we are refreshing the entire content.
  if (is_initial_feed)
    dir->RemoveChildren();

  for (ScopedVector<DocumentEntry>::const_iterator iter =
           feed->entries().begin();
       iter != feed->entries().end(); ++iter) {
    DocumentEntry* doc = *iter;

    // For now, skip elements of the root directory feed that have parent.
    // TODO(zelidrag): In theory, we could reconstruct the entire FS snapshot
    // of the root file feed only instead of fetching one dir/collection at the
    // time.
    if (dir == root_.get()) {
      const Link* parent_link = doc->GetLinkByType(Link::PARENT);
      if (parent_link)
        continue;
    }

    GDataFileBase* file = GDataFileBase::FromDocumentEntry(dir, doc);
    if (file)
      dir->AddFile(file);
  }
  return base::PLATFORM_FILE_OK;
}

// static
GDataFileSystem* GDataFileSystemFactory::GetForProfile(
    Profile* profile) {
  return static_cast<GDataFileSystem*>(
      GetInstance()->GetServiceForProfile(profile, true));
}

// static
GDataFileSystemFactory* GDataFileSystemFactory::GetInstance() {
  return Singleton<GDataFileSystemFactory>::get();
}

GDataFileSystemFactory::GDataFileSystemFactory()
    : ProfileKeyedServiceFactory("GDataFileSystem",
                                 ProfileDependencyManager::GetInstance()) {
}

GDataFileSystemFactory::~GDataFileSystemFactory() {
}

ProfileKeyedService* GDataFileSystemFactory::BuildServiceInstanceFor(
    Profile* profile) const {
  return new GDataFileSystem(profile);
}

}  // namespace gdata
