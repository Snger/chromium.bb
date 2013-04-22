// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/fileapi/media/media_file_system_mount_point_provider.h"

#include <string>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/platform_file.h"
#include "base/sequenced_task_runner.h"
#include "webkit/blob/local_file_stream_reader.h"
#include "webkit/fileapi/async_file_util_adapter.h"
#include "webkit/fileapi/copy_or_move_file_validator.h"
#include "webkit/fileapi/file_system_callback_dispatcher.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/file_system_file_stream_reader.h"
#include "webkit/fileapi/file_system_operation_context.h"
#include "webkit/fileapi/file_system_task_runners.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/fileapi/isolated_file_util.h"
#include "webkit/fileapi/local_file_stream_writer.h"
#include "webkit/fileapi/local_file_system_operation.h"
#include "webkit/fileapi/media/media_path_filter.h"
#include "webkit/fileapi/media/native_media_file_util.h"
#include "webkit/fileapi/native_file_util.h"

#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
#include "webkit/fileapi/media/device_media_async_file_util.h"
#endif

namespace fileapi {

const char MediaFileSystemMountPointProvider::kMediaPathFilterKey[] =
    "MediaPathFilterKey";
const char MediaFileSystemMountPointProvider::kMTPDeviceDelegateURLKey[] =
    "MTPDeviceDelegateKey";

MediaFileSystemMountPointProvider::MediaFileSystemMountPointProvider(
    const base::FilePath& profile_path)
    : profile_path_(profile_path),
      media_path_filter_(new MediaPathFilter()),
      native_media_file_util_(
          new AsyncFileUtilAdapter(new NativeMediaFileUtil())) {
#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  // TODO(kmadhusu): Initialize |device_media_file_util_| in
  // initialization list.
  device_media_async_file_util_.reset(
      DeviceMediaAsyncFileUtil::Create(profile_path_));
#endif
}

MediaFileSystemMountPointProvider::~MediaFileSystemMountPointProvider() {
}

bool MediaFileSystemMountPointProvider::CanHandleType(
    FileSystemType type) const {
  switch (type) {
    case kFileSystemTypeNativeMedia:
    case kFileSystemTypeDeviceMedia:
      return true;
    default:
      return false;
  }
}

void MediaFileSystemMountPointProvider::ValidateFileSystemRoot(
    const GURL& origin_url,
    FileSystemType type,
    bool create,
    const ValidateFileSystemCallback& callback) {
  // We never allow opening a new isolated FileSystem via usual OpenFileSystem.
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback, base::PLATFORM_FILE_ERROR_SECURITY));
}

base::FilePath
MediaFileSystemMountPointProvider::GetFileSystemRootPathOnFileThread(
    const FileSystemURL& url,
    bool create) {
  // This is not supposed to be used.
  NOTREACHED();
  return base::FilePath();
}

FileSystemFileUtil* MediaFileSystemMountPointProvider::GetFileUtil(
    FileSystemType type) {
  switch (type) {
    case kFileSystemTypeNativeMedia:
      return native_media_file_util_->sync_file_util();
    default:
      NOTREACHED();
  }
  return NULL;
}

AsyncFileUtil* MediaFileSystemMountPointProvider::GetAsyncFileUtil(
    FileSystemType type) {
  switch (type) {
    case kFileSystemTypeNativeMedia:
      return native_media_file_util_.get();
    case kFileSystemTypeDeviceMedia:
#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
      return device_media_async_file_util_.get();
#endif
    default:
      NOTREACHED();
  }
  return NULL;
}

CopyOrMoveFileValidatorFactory*
MediaFileSystemMountPointProvider::GetCopyOrMoveFileValidatorFactory(
    FileSystemType type, base::PlatformFileError* error_code) {
  DCHECK(error_code);
  *error_code = base::PLATFORM_FILE_OK;
  switch (type) {
    case kFileSystemTypeNativeMedia:
    case kFileSystemTypeDeviceMedia:
      if (!media_copy_or_move_file_validator_factory_) {
        *error_code = base::PLATFORM_FILE_ERROR_SECURITY;
        return NULL;
      }
      return media_copy_or_move_file_validator_factory_.get();
    default:
      NOTREACHED();
  }
  return NULL;
}

void
MediaFileSystemMountPointProvider::InitializeCopyOrMoveFileValidatorFactory(
    FileSystemType type,
    scoped_ptr<CopyOrMoveFileValidatorFactory> factory) {
  switch (type) {
    case kFileSystemTypeNativeMedia:
    case kFileSystemTypeDeviceMedia:
      if (!media_copy_or_move_file_validator_factory_)
        media_copy_or_move_file_validator_factory_.reset(factory.release());
      break;
    default:
      NOTREACHED();
  }
}

FilePermissionPolicy MediaFileSystemMountPointProvider::GetPermissionPolicy(
    const FileSystemURL& url, int permissions) const {
  // Access to media file systems should be checked using per-filesystem
  // access permission.
  return FILE_PERMISSION_USE_FILESYSTEM_PERMISSION;
}

FileSystemOperation*
MediaFileSystemMountPointProvider::CreateFileSystemOperation(
    const FileSystemURL& url,
    FileSystemContext* context,
    base::PlatformFileError* error_code) const {
  scoped_ptr<FileSystemOperationContext> operation_context(
      new FileSystemOperationContext(
          context, context->task_runners()->media_task_runner()));

  operation_context->SetUserValue(kMediaPathFilterKey,
                                  media_path_filter_.get());
#if defined(SUPPORT_MTP_DEVICE_FILESYSTEM)
  if (url.type() == kFileSystemTypeDeviceMedia) {
    operation_context->SetUserValue(kMTPDeviceDelegateURLKey,
                                    url.filesystem_id());
  }
#endif

  return new LocalFileSystemOperation(context, operation_context.Pass());
}

scoped_ptr<webkit_blob::FileStreamReader>
MediaFileSystemMountPointProvider::CreateFileStreamReader(
    const FileSystemURL& url,
    int64 offset,
    const base::Time& expected_modification_time,
    FileSystemContext* context) const {
  return scoped_ptr<webkit_blob::FileStreamReader>(
      new webkit_blob::LocalFileStreamReader(
          context->task_runners()->file_task_runner(),
          url.path(), offset, expected_modification_time));
}

scoped_ptr<FileStreamWriter>
MediaFileSystemMountPointProvider::CreateFileStreamWriter(
    const FileSystemURL& url,
    int64 offset,
    FileSystemContext* context) const {
  return scoped_ptr<FileStreamWriter>(
      new LocalFileStreamWriter(url.path(), offset));
}

FileSystemQuotaUtil* MediaFileSystemMountPointProvider::GetQuotaUtil() {
  // No quota support.
  return NULL;
}

void MediaFileSystemMountPointProvider::DeleteFileSystem(
    const GURL& origin_url,
    FileSystemType type,
    FileSystemContext* context,
    const DeleteFileSystemCallback& callback) {
  NOTREACHED();
  callback.Run(base::PLATFORM_FILE_ERROR_INVALID_OPERATION);
}

}  // namespace fileapi
