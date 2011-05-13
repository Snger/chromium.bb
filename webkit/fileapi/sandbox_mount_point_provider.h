// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_FILEAPI_SANDBOX_MOUNT_POINT_PROVIDER_H_
#define WEBKIT_FILEAPI_SANDBOX_MOUNT_POINT_PROVIDER_H_

#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "googleurl/src/gurl.h"
#include "webkit/fileapi/file_system_mount_point_provider.h"
#include "webkit/fileapi/obfuscated_file_system_file_util.h"

namespace base {
class MessageLoopProxy;
}

namespace fileapi {

class SandboxMountPointProvider : public FileSystemMountPointProvider {
 public:
  // Origin enumerator interface.
  // An instance of this interface is assumed to be called on the file thread.
  class OriginEnumerator {
   public:
    virtual ~OriginEnumerator() {}

    // Returns the next origin.  Returns empty if there are no more origins.
    virtual GURL Next() = 0;

    // Returns the current origin's information.
    virtual bool HasFileSystemType(FileSystemType type) const = 0;
  };

  SandboxMountPointProvider(
      FileSystemPathManager* path_manager,
      scoped_refptr<base::MessageLoopProxy> file_message_loop,
      const FilePath& profile_path);
  virtual ~SandboxMountPointProvider();

  // Checks if access to |virtual_path| is allowed from |origin_url|.
  virtual bool IsAccessAllowed(const GURL& origin_url,
                               FileSystemType type,
                               const FilePath& virtual_path);

  // Retrieves the root path for the given |origin_url| and |type|, and
  // calls the given |callback| with the root path and name.
  // If |create| is true this also creates the directory if it doesn't exist.
  virtual void ValidateFileSystemRootAndGetURL(
      const GURL& origin_url,
      FileSystemType type,
      bool create,
      FileSystemPathManager::GetRootPathCallback* callback);

  // Like GetFileSystemRootPath, but synchronous, and can be called only while
  // running on the file thread.
  virtual FilePath ValidateFileSystemRootAndGetPathOnFileThread(
      const GURL& origin_url,
      FileSystemType type,
      const FilePath& unused,
      bool create);

  // The FileSystem directory name.
  static const FilePath::CharType kFileSystemDirectory[];

  const FilePath& base_path() const {
    return base_path_;
  }

  // Checks if a given |name| contains any restricted names/chars in it.
  virtual bool IsRestrictedFileName(const FilePath& filename) const;

  virtual std::vector<FilePath> GetRootDirectories() const;

  // Returns an origin enumerator of this provider.
  // This method is supposed to be called on the file thread.
  OriginEnumerator* CreateOriginEnumerator() const;

  // Gets a base directory path of the sandboxed filesystem that is
  // specified by |origin_url|.
  // (The path is similar to the origin's root path but doesn't contain
  // the 'unique' and 'type' part.)
  // This method is portable and can be called on any threads.
  FilePath GetBaseDirectoryForOrigin(const GURL& origin_url) const;

  // Gets a base directory path of the sandboxed filesystem that is
  // specified by |origin_url| and |type|.
  // (The path is similar to the origin's root path but doesn't contain
  // the 'unique' part.)
  // Returns an empty path if the given type is invalid.
  // This method is portable and can be called on any threads.
  FilePath GetBaseDirectoryForOriginAndType(
      const GURL& origin_url,
      fileapi::FileSystemType type) const;

  ObfuscatedFileSystemFileUtil* sandbox_file_util() {
    return sandbox_file_util_.get();
  }

 private:
  bool GetOriginBasePathAndName(
      const GURL& origin_url,
      FilePath* base_path,
      FileSystemType type,
      std::string* name);

  class GetFileSystemRootPathTask;

  // The path_manager_ isn't owned by this instance; this instance is owned by
  // the path_manager_, and they have the same lifetime.
  FileSystemPathManager* path_manager_;

  scoped_refptr<base::MessageLoopProxy> file_message_loop_;

  const FilePath base_path_;

  scoped_ptr<ObfuscatedFileSystemFileUtil> sandbox_file_util_;

  DISALLOW_COPY_AND_ASSIGN(SandboxMountPointProvider);
};

}  // namespace fileapi

#endif  // WEBKIT_FILEAPI_SANDBOX_MOUNT_POINT_PROVIDER_H_
