// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_DELEGATE_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "webkit/common/fileapi/file_system_types.h"

namespace base {
class Time;
}  // namespace base

namespace fileapi {
class AsyncFileUtil;
class FileSystemContext;
class FileSystemOperation;
class FileSystemURL;
class FileStreamWriter;
}  // namespace fileapi

namespace webkit_blob {
class FileStreamReader;
}  // namespace webkit_blob

namespace chromeos {

// This is delegate interface to inject the implementation of the some methods
// of CrosMountPointProvider. The main goal is to inject Drive File System.
class CrosMountPointProviderDelegate {
 public:
  virtual ~CrosMountPointProviderDelegate() {}

  // Called from CrosMountPointProvider::GetAsyncFileUtil().
  virtual fileapi::AsyncFileUtil* GetAsyncFileUtil(
      fileapi::FileSystemType type) = 0;

  // Called from CrosMountPointProvider::CreateFileStreamReader().
  virtual scoped_ptr<webkit_blob::FileStreamReader> CreateFileStreamReader(
      const fileapi::FileSystemURL& url,
      int64 offset,
      const base::Time& expected_modification_time,
      fileapi::FileSystemContext* context) = 0;

  // Called from CrosMountPointProvider::CreateFileStreamWriter().
  virtual scoped_ptr<fileapi::FileStreamWriter> CreateFileStreamWriter(
      const fileapi::FileSystemURL& url,
      int64 offset,
      fileapi::FileSystemContext* context) = 0;

  // Called from CrosMountPointProvider::CreateFileSystemOperation().
  // TODO(hidehiko): Get rid of this method when we merge the
  // {Remote,Local}FileSystemOperation.
  virtual fileapi::FileSystemOperation* CreateFileSystemOperation(
      const fileapi::FileSystemURL& url,
      fileapi::FileSystemContext* context,
      base::PlatformFileError* error_code) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_FILEAPI_CROS_MOUNT_POINT_PROVIDER_DELEGATE_H_
