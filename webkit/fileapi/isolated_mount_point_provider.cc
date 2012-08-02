// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/isolated_mount_point_provider.h"

#include <string>

#include "base/bind.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/sequenced_task_runner.h"
#include "webkit/blob/local_file_stream_reader.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_stream_reader.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/isolated_file_util.h"
#include "webkit/fileapi/local_file_stream_writer.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/media/media_path_filter.h"
#include "webkit/fileapi/media/native_media_file_util.h"
#include "webkit/fileapi/native_file_util.h"

namespace fileapi {

namespace {

IsolatedContext* isolated_context() {
  return IsolatedContext::GetInstance();
}

}  // namespace

IsolatedMountPointProvider::IsolatedMountPointProvider()
    : media_path_filter_(new MediaPathFilter()),
      isolated_file_util_(new IsolatedFileUtil()),
      dragged_file_util_(new DraggedFileUtil()),
      native_media_file_util_(new NativeMediaFileUtil()) {
}

IsolatedMountPointProvider::~IsolatedMountPointProvider() {
}

void IsolatedMountPointProvider::ValidateFileSystemRoot(
    const GURL& origin_url,
    FileSystemType type,
    bool create,
    const ValidateFileSystemCallback& callback) {
  // We never allow opening a new isolated FileSystem via usual OpenFileSystem.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, base::PLATFORM_FILE_ERROR_SECURITY));
}

FilePath IsolatedMountPointProvider::GetFileSystemRootPathOnFileThread(
    const GURL& origin_url,
    FileSystemType type,
    const FilePath& virtual_path,
    bool create) {
  // This is not supposed to be used.
  NOTREACHED();
  return FilePath();
}

bool IsolatedMountPointProvider::IsAccessAllowed(
    const GURL& origin_url, FileSystemType type, const FilePath& virtual_path) {
  return true;
}

bool IsolatedMountPointProvider::IsRestrictedFileName(
    const FilePath& filename) const {
  // TODO(kinuko): We need to check platform-specific restricted file names
  // before we actually start allowing file creation in isolated file systems.
  return false;
}

FileSystemFileUtil* IsolatedMountPointProvider::GetFileUtil(
    FileSystemType type) {
  switch (type) {
    case kFileSystemTypeIsolated:
      return isolated_file_util_.get();
    case kFileSystemTypeDragged:
      return dragged_file_util_.get();
    case kFileSystemTypeNativeMedia:
      return native_media_file_util_.get();

    case kFileSystemTypeDeviceMedia:
    case kFileSystemTypeTemporary:
    case kFileSystemTypePersistent:
    case kFileSystemTypeExternal:
    case kFileSystemTypeTest:
    case kFileSystemTypeUnknown:
      NOTREACHED();
  }
  return NULL;
}

FilePath IsolatedMountPointProvider::GetPathForPermissionsCheck(
    const FilePath& virtual_path) const {
  // For isolated filesystems we only check per-filesystem permissions.
  NOTREACHED();
  return virtual_path;
}

FileSystemOperationInterface*
IsolatedMountPointProvider::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context) const {
  scoped_ptr<FileSystemOperationContext> operation_context(
      new FileSystemOperationContext(context));
  if (url.type() == kFileSystemTypeNativeMedia ||
      url.type() == kFileSystemTypeDeviceMedia)
    operation_context->set_media_path_filter(media_path_filter_.get());
  return new LocalFileSystemOperation(context, operation_context.Pass());
}

webkit_blob::FileStreamReader*
IsolatedMountPointProvider::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return new webkit_blob::LocalFileStreamReader(
      context->file_task_runner(), url.path(), offset, base::Time());
}

FileStreamWriter* IsolatedMountPointProvider::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return new LocalFileStreamWriter(url.path(), offset);
}

FileSystemQuotaUtil* IsolatedMountPointProvider::GetQuotaUtil() {
  // No quota support.
  return NULL;
}

}  // namespace fileapi
