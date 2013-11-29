// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LOCAL_TO_REMOTE_SYNCER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LOCAL_TO_REMOTE_SYNCER_H_

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/sync_file_system/file_change.h"
#include "chrome/browser/sync_file_system/sync_callbacks.h"
#include "chrome/browser/sync_file_system/sync_file_metadata.h"
#include "chrome/browser/sync_file_system/sync_task.h"

namespace drive {
class DriveServiceInterface;
class DriveUploaderInterface;
}

namespace google_apis {
class ResourceEntry;
class ResourceList;
}

namespace sync_file_system {

class RemoteChangeProcessor;

namespace drive_backend {

class FileTracker;
class MetadataDatabase;
class SyncEngineContext;

class LocalToRemoteSyncer : public SyncTask {
 public:
  LocalToRemoteSyncer(SyncEngineContext* sync_context,
                      const FileChange& local_change,
                      const base::FilePath& local_path,
                      const fileapi::FileSystemURL& url);
  virtual ~LocalToRemoteSyncer();
  virtual void Run(const SyncStatusCallback& callback) OVERRIDE;

 private:
  void SyncCompleted(const SyncStatusCallback& callback,
                     SyncStatusCode status);

  void HandleMissingRemoteFile(const SyncStatusCallback& callback);
  void HandleConflict(const SyncStatusCallback& callback);
  void HandleExistingRemoteFile(const SyncStatusCallback& callback);
  void HandleMissingParentCase(const SyncStatusCallback& callback);

  void DeleteRemoteFile(const SyncStatusCallback& callback);
  void DidDeleteRemoteFile(const SyncStatusCallback& callback,
                           google_apis::GDataErrorCode error);

  void UploadExistingFile(const SyncStatusCallback& callback);
  void DidGetMD5ForUpload(const SyncStatusCallback& callback,
                          const std::string& local_file_md5);
  void DidUploadExistingFile(const SyncStatusCallback& callback,
                             google_apis::GDataErrorCode error,
                             const GURL&,
                             scoped_ptr<google_apis::ResourceEntry>);
  void UpdateRemoteMetadata(const SyncStatusCallback& callback);
  void DidGetRemoteMetadata(const SyncStatusCallback& callback,
                            google_apis::GDataErrorCode error,
                            scoped_ptr<google_apis::ResourceEntry> entry);

  void DidDeleteForUploadNewFile(const SyncStatusCallback& callback,
                                 SyncStatusCode status);
  void DidDeleteForCreateFolder(const SyncStatusCallback& callback,
                                SyncStatusCode status);

  void UploadNewFile(const SyncStatusCallback& callback);
  void DidUploadNewFile(const SyncStatusCallback& callback,
                        google_apis::GDataErrorCode error,
                        const GURL& upload_location,
                        scoped_ptr<google_apis::ResourceEntry> entry);
  void DidUpdateDatabaseForUpload(const SyncStatusCallback& callback,
                                  const std::string& file_id,
                                  SyncStatusCode status);

  void CreateRemoteFolder(const SyncStatusCallback& callback);
  void DidCreateRemoteFolder(const SyncStatusCallback& callback,
                             google_apis::GDataErrorCode error,
                             scoped_ptr<google_apis::ResourceEntry> entry);
  void DidListFolderForEnsureUniqueness(
      const SyncStatusCallback& callback,
      ScopedVector<google_apis::ResourceEntry> candidates,
      google_apis::GDataErrorCode error,
      scoped_ptr<google_apis::ResourceList> resource_list);

  bool IsContextReady();
  drive::DriveServiceInterface* drive_service();
  drive::DriveUploaderInterface* drive_uploader();
  MetadataDatabase* metadata_database();

  SyncEngineContext* sync_context_;  // Not owned.

  FileChange local_change_;
  base::FilePath local_path_;
  fileapi::FileSystemURL url_;

  scoped_ptr<FileTracker> remote_file_tracker_;
  scoped_ptr<FileTracker> remote_parent_folder_tracker_;
  base::FilePath target_path_;

  base::WeakPtrFactory<LocalToRemoteSyncer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(LocalToRemoteSyncer);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_LOCAL_TO_REMOTE_SYNCER_H_
