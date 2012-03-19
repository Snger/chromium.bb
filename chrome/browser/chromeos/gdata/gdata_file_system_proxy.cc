// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_file_system_proxy.h"

#include <vector>

#include "base/bind.h"
#include "base/platform_file.h"
#include "base/string_util.h"
#include "base/values.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/blob/shareable_file_reference.h"
#include "webkit/fileapi/file_system_file_util_proxy.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"

using base::MessageLoopProxy;
using content::BrowserThread;
using fileapi::FileSystemOperationInterface;

namespace {

const char kGDataRootDirectory[] = "gdata";
const char kFeedField[] = "feed";

// Helper function to run SnapshotFileCallback from
// GDataFileSystemProxy::CreateSnapshotFile().
void CallSnapshotFileCallback(
    const FileSystemOperationInterface::SnapshotFileCallback& callback,
    base::PlatformFileInfo file_info,
    base::PlatformFileError error,
    const FilePath& local_path) {
  // TODO(satorux): We should use the last parameter
  // (webkit_blob::ShareableFileReference) to create temporary JSON files
  // on-the-fly for hosted documents so users can attach hosted files to web
  // apps via File API. crosbug.com/27690
  callback.Run(error, file_info, local_path, NULL);
}

}  // namespace

namespace gdata {

base::FileUtilProxy::Entry GDataFileToFileUtilProxyEntry(
    const GDataFileBase& file) {
  base::FileUtilProxy::Entry entry;
  entry.is_directory = file.file_info().is_directory;

  // TODO(zelidrag): Add file name modification logic to enforce uniquness of
  // file paths across this file system.
  entry.name = file.file_name();

  entry.size = file.file_info().size;
  entry.last_modified_time = file.file_info().last_modified;
  return entry;
}

// GDataFileSystemProxy class implementation.

GDataFileSystemProxy::GDataFileSystemProxy(Profile* profile)
    : file_system_(GDataFileSystemFactory::GetForProfile(profile)) {
  // Should be created from the file browser extension API (AddMountFunction)
  // on UI thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

GDataFileSystemProxy::~GDataFileSystemProxy() {
  // Should be deleted from the CrosMountPointProvider on IO thread.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
}

void GDataFileSystemProxy::GetFileInfo(const GURL& file_url,
    const FileSystemOperationInterface::GetMetadataCallback& callback) {
  scoped_refptr<base::MessageLoopProxy> proxy =
      base::MessageLoopProxy::current();
  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    proxy->PostTask(FROM_HERE,
                    base::Bind(callback,
                               base::PLATFORM_FILE_ERROR_NOT_FOUND,
                               base::PlatformFileInfo(),
                               FilePath()));
    return;
  }

  file_system_->FindFileByPathAsync(
      file_path,
      base::Bind(&GDataFileSystemProxy::OnGetMetadata,
                 this,
                 file_path,
                 proxy,
                 callback));
}

void GDataFileSystemProxy::Copy(const GURL& src_file_url,
    const GURL& dest_file_url,
    const FileSystemOperationInterface::StatusCallback& callback) {
  FilePath src_file_path, dest_file_path;
  if (!ValidateUrl(src_file_url, &src_file_path) ||
      !ValidateUrl(dest_file_url, &dest_file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  file_system_->Copy(src_file_path, dest_file_path, callback);
}

void GDataFileSystemProxy::Move(const GURL& src_file_url,
    const GURL& dest_file_url,
    const FileSystemOperationInterface::StatusCallback& callback) {
  FilePath src_file_path, dest_file_path;
  if (!ValidateUrl(src_file_url, &src_file_path) ||
      !ValidateUrl(dest_file_url, &dest_file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  file_system_->Move(src_file_path, dest_file_path, callback);
}

void DoNothing(base::PlatformFileError /*error*/,
               int /*bytes_total*/,
               int /*bytes_used*/) {
}

void GDataFileSystemProxy::ReadDirectory(const GURL& file_url,
    const FileSystemOperationInterface::ReadDirectoryCallback& callback) {
  scoped_refptr<base::MessageLoopProxy> proxy =
      base::MessageLoopProxy::current();
  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    proxy->PostTask(FROM_HERE,
                    base::Bind(callback,
                               base::PLATFORM_FILE_ERROR_NOT_FOUND,
                               std::vector<base::FileUtilProxy::Entry>(),
                               false));
    return;
  }

  file_system_->FindFileByPathAsync(
      file_path,
      base::Bind(&GDataFileSystemProxy::OnReadDirectory,
                 this,
                 proxy,
                 callback));
}

void GDataFileSystemProxy::Remove(const GURL& file_url, bool recursive,
    const FileSystemOperationInterface::StatusCallback& callback) {
  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  file_system_->Remove(file_path, recursive, callback);
}

void GDataFileSystemProxy::CreateDirectory(
    const GURL& file_url,
    bool exclusive,
    bool recursive,
    const FileSystemOperationInterface::StatusCallback& callback) {
  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback, base::PLATFORM_FILE_ERROR_NOT_FOUND));
    return;
  }

  file_system_->CreateDirectory(file_path, exclusive, recursive, callback);
}

void GDataFileSystemProxy::CreateSnapshotFile(
    const GURL& file_url,
    const FileSystemOperationInterface::SnapshotFileCallback& callback) {
  FilePath file_path;
  if (!ValidateUrl(file_url, &file_path)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback,
                    base::PLATFORM_FILE_ERROR_NOT_FOUND,
                    base::PlatformFileInfo(),
                    FilePath(),
                    scoped_refptr<webkit_blob::ShareableFileReference>(NULL)));
    return;
  }


  base::PlatformFileInfo file_info;
  if (!file_system_->GetFileInfoFromPath(file_path, &file_info)) {
    MessageLoopProxy::current()->PostTask(FROM_HERE,
         base::Bind(callback,
                    base::PLATFORM_FILE_ERROR_NOT_FOUND,
                    base::PlatformFileInfo(),
                    FilePath(),
                    scoped_refptr<webkit_blob::ShareableFileReference>(NULL)));
    return;
  }

  file_system_->GetFile(file_path,
                        base::Bind(&CallSnapshotFileCallback,
                                   callback,
                                   file_info));
}

// static.
bool GDataFileSystemProxy::ValidateUrl(const GURL& url, FilePath* file_path) {
  // what platform you're on.
  FilePath raw_path;
  fileapi::FileSystemType type = fileapi::kFileSystemTypeUnknown;
  if (!fileapi::CrackFileSystemURL(url, NULL, &type, file_path) ||
      type != fileapi::kFileSystemTypeExternal) {
    return false;
  }
  return true;
}

void GDataFileSystemProxy::OnGetMetadata(
    const FilePath& file_path,
    scoped_refptr<base::MessageLoopProxy> proxy,
    const FileSystemOperationInterface::GetMetadataCallback& callback,
    base::PlatformFileError error,
    const FilePath& directory_path,
    GDataFileBase* file) {
  if (error != base::PLATFORM_FILE_OK) {
    proxy->PostTask(FROM_HERE,
                    base::Bind(callback,
                               error,
                               base::PlatformFileInfo(),
                               FilePath()));
    return;
  }

  proxy->PostTask(FROM_HERE,
                  base::Bind(callback,
                             base::PLATFORM_FILE_OK,
                             file->file_info(),
                             file_path));
}

void GDataFileSystemProxy::OnReadDirectory(
    scoped_refptr<base::MessageLoopProxy> proxy,
    const FileSystemOperationInterface::ReadDirectoryCallback& callback,
    base::PlatformFileError error,
    const FilePath& directory_path,
    GDataFileBase* file) {
  DCHECK(file);
  GDataDirectory* directory = file->AsGDataDirectory();
  if (!directory)
    error = base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY;

  if (error != base::PLATFORM_FILE_OK) {
    proxy->PostTask(FROM_HERE,
                    base::Bind(callback,
                               error,
                               std::vector<base::FileUtilProxy::Entry>(),
                               false));
    return;
  }
  std::vector<base::FileUtilProxy::Entry> entries;
  // Convert gdata files to something File API stack can understand.
  for (GDataFileCollection::const_iterator iter =
            directory->children().begin();
       iter != directory->children().end(); ++iter) {
    entries.push_back(GDataFileToFileUtilProxyEntry(*(iter->second)));
  }

  proxy->PostTask(FROM_HERE,
                  base::Bind(callback,
                             base::PLATFORM_FILE_OK,
                             entries,
                             false));
}

}  // namespace gdata
