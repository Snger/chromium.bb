// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_H_

#include <string>

#include "base/callback_forward.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "webkit/browser/fileapi/syncable/sync_callbacks.h"
#include "webkit/browser/fileapi/syncable/sync_status_code.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace leveldb {
class DB;
class WriteBatch;
}

namespace google_apis {
class ChangeResource;
class FileResource;
class ResourceEntry;
}

namespace sync_file_system {
namespace drive_backend {

class ServiceMetadata;
class DriveFileMetadata;
struct InitializeInfo;

// This class holds a snapshot of the server side metadata.
class MetadataDatabase {
 public:
  explicit MetadataDatabase(base::SequencedTaskRunner* task_runner);
  ~MetadataDatabase();

  // Initializes the internal database and loads its content to memory.
  // This function works asynchronously.
  void Initialize(const base::FilePath& database_path,
                  const SyncStatusCallback& callback);

  int64 GetLargestChangeID() const;

  // Registers existing folder as the app-root for |app_id|.  The folder
  // must be an inactive folder that does not yet associated to any App.
  // This method associates the folder with |app_id| and activates it.
  void RegisterApp(const std::string& app_id,
                   const std::string& folder_id,
                   const SyncStatusCallback& callback);

  // Inactivates the folder associated to the app to disable |app_id|.
  // Does nothing if |app_id| is already disabled.
  void DisableApp(const std::string& app_id,
                  const SyncStatusCallback& callback);

  // Activates the folder associated to |app_id| to enable |app_id|.
  // Does nothing if |app_id| is already enabled.
  void EnableApp(const std::string& app_id,
                 const SyncStatusCallback& callback);

  // Unregister the folder as the app-root for |app_id|.  If |app_id| does not
  // exist, does nothing.
  void UnregisterApp(const std::string& app_id,
                     const SyncStatusCallback& callback);

  // Finds the app-root folder for |app_id|.  Returns true if exists.
  // Copies the result to |folder| if it is non-NULL.
  bool FindAppRootFolder(const std::string& app_id,
                         DriveFileMetadata* folder) const;

  // Finds file by |file_id|.  Returns true if the file was found.
  // Copies the DriveFileMetadata instance identified by |file_id| into
  // |file| if exists and |file| is non-NULL.
  bool FindFileByFileID(const std::string& file_id,
                        DriveFileMetadata* file) const;

  // Finds files by |title| under the folder identified by |folder_id|, and
  // returns the number of the files.
  // Copies the DriveFileMetadata instances to |files| if it is non-NULL.
  size_t FindFilesByParentAndTitle(
      const std::string& folder_id,
      const std::string& title,
      ScopedVector<DriveFileMetadata>* files) const;

  // Finds active file by |title| under the folder identified by |folder_id|.
  // Returns true if the file exists.
  bool FindActiveFileByParentAndTitle(
      const std::string& folder_id,
      const std::string& title,
      DriveFileMetadata* file) const;

  // Finds the active file identified by |app_id| and |path|, which must be
  // unique.  Returns true if the file was found.
  // Copies the DriveFileMetadata instance into |file| if the file is found and
  // |file| is non-NULL.
  // |path| must be an absolute path in |app_id|. (i.e. relative to the app-root
  // folder.)
  bool FindActiveFileByPath(const std::string& app_id,
                            const base::FilePath& path,
                            DriveFileMetadata* file) const;

  // Looks up FilePath from FileID.  Returns true on success.
  // |path| must be non-NULL.
  bool ConstructPathForFile(const std::string& file_id,
                            base::FilePath* path) const;

  // Updates database by |changes|.
  // Marks dirty for each changed file if the file has the metadata in the
  // database.  Adds new metadata to track the file if the file doesn't have
  // the metadata and its parent folder has the active metadata.
  void UpdateByChangeList(ScopedVector<google_apis::ChangeResource> changes,
                          const SyncStatusCallback& callback);

  // Populates |folder| with |children|.  Each |children| initially has empty
  // |synced_details| and |remote_details|.
  void PopulateFolder(const std::string& folder_id,
                      ScopedVector<google_apis::ResourceEntry> children,
                      const SyncStatusCallback& callback);

 private:
  DISALLOW_COPY_AND_ASSIGN(MetadataDatabase);
};

}  // namespace drive_backend
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_DRIVE_BACKEND_METADATA_DATABASE_H_
