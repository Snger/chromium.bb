// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_PROXY_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_PROXY_H_

#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "webkit/chromeos/fileapi/remote_file_system_proxy.h"

class Profile;

namespace gdata {

// The interface class for remote file system proxy.
class GDataFileSystemProxy : public fileapi::RemoteFileSystemProxyInterface {
 public:
  // |profile| is used to create GDataFileSystem, which is a per-profile
  // instance.
  explicit GDataFileSystemProxy(Profile* profile);
  virtual ~GDataFileSystemProxy();

  // fileapi::RemoteFileSystemProxyInterface overrides.
  virtual void GetFileInfo(const GURL& path,
      const fileapi::FileSystemOperationInterface::GetMetadataCallback&
          callback) OVERRIDE;
  virtual void ReadDirectory(const GURL& path,
     const fileapi::FileSystemOperationInterface::ReadDirectoryCallback&
         callback) OVERRIDE;
  virtual void Remove(const GURL& path, bool recursive,
      const fileapi::FileSystemOperationInterface::StatusCallback& callback)
          OVERRIDE;
  // TODO(zelidrag): More methods to follow as we implement other parts of FSO.

 private:

  // Routes reply from simple file operations to the calling thread.
  static void OnFileOperationCompleted(
      scoped_refptr<base::MessageLoopProxy> proxy,
      const fileapi::FileSystemOperationInterface::StatusCallback& callback,
      base::PlatformFileError result);

    // Checks if a given |url| belongs to this file system. If it does,
  // the call will return true and fill in |file_path| with a file path of
  // a corresponding element within this file system.
  static bool ValidateUrl(const GURL& url, FilePath* file_path);

  scoped_refptr<GDataFileSystem> file_system_;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_PROXY_H_
