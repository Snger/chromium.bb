// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_WEBKIT_FILE_STREAM_WRITER_IMPL_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_WEBKIT_FILE_STREAM_WRITER_IMPL_H_

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "webkit/browser/fileapi/file_stream_writer.h"
#include "webkit/browser/fileapi/file_system_context.h"
#include "webkit/browser/fileapi/file_system_url.h"

namespace fileapi {
class RemoteFileSystemProxyInterface;
}

namespace net {
class IOBuffer;
}

namespace webkit_blob {
class ShareableFileReference;
}

namespace drive {
namespace internal {

// The implementation of fileapi::FileStreamWriter for the Drive File System.
class WebkitFileStreamWriterImpl : public fileapi::FileStreamWriter {
 public:
  // Creates a writer for a file on |remote_filesystem| with path url |url|
  // (like "filesystem:chrome-extension://id/external/drive/...") that
  // starts writing from |offset|. When invalid parameters are set, the first
  // call to Write() method fails.
  // Uses |local_task_runner| for local file operations.
  WebkitFileStreamWriterImpl(
      const scoped_refptr<fileapi::RemoteFileSystemProxyInterface>&
          remote_filesystem,
      const fileapi::FileSystemURL& url,
      int64 offset,
      base::TaskRunner* local_task_runner);
  virtual ~WebkitFileStreamWriterImpl();

  // FileWriter override.
  virtual int Write(net::IOBuffer* buf, int buf_len,
                    const net::CompletionCallback& callback) OVERRIDE;
  virtual int Cancel(const net::CompletionCallback& callback) OVERRIDE;
  virtual int Flush(const net::CompletionCallback& callback) OVERRIDE;

 private:
  // Callback function to do the continuation of the work of the first Write()
  // call, which tries to open the local copy of the file before writing.
  void WriteAfterCreateWritableSnapshotFile(
      net::IOBuffer* buf,
      int buf_len,
      const net::CompletionCallback& callback,
      base::PlatformFileError open_result,
      const base::FilePath& local_path,
      const scoped_refptr<webkit_blob::ShareableFileReference>& file_ref);

  scoped_refptr<fileapi::RemoteFileSystemProxyInterface> remote_filesystem_;
  scoped_refptr<base::TaskRunner> local_task_runner_;
  const fileapi::FileSystemURL url_;
  const int64 initial_offset_;
  scoped_ptr<fileapi::FileStreamWriter> local_file_writer_;
  scoped_refptr<webkit_blob::ShareableFileReference> file_ref_;
  bool has_pending_create_snapshot_;
  net::CompletionCallback pending_cancel_callback_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<WebkitFileStreamWriterImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WebkitFileStreamWriterImpl);
};

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_WEBKIT_FILE_STREAM_WRITER_IMPL_H_
