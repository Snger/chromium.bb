// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_DEVICE_DELEGATE_IMPL_LINUX_H_
#define CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_DEVICE_DELEGATE_IMPL_LINUX_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/platform_file.h"
#include "base/synchronization/waitable_event.h"
#include "webkit/fileapi/file_system_file_util.h"
#include "webkit/fileapi/media/mtp_device_delegate.h"

class FilePath;

namespace base {
class SequencedTaskRunner;
}

namespace chrome {

// This class communicates with the MTP storage to complete the isolated file
// system operations. This class contains platform specific code to communicate
// with the attached MTP storage. Instantiate this class per MTP storage. This
// object is constructed on the UI thread. This object is operated and
// destructed on the sequenced task runner thread. ScopedMTPDeviceMapEntry class
// manages the lifetime of this object via MTPDeviceMapService class. This class
// supports weak pointers because the base class supports weak pointers.
class MTPDeviceDelegateImplLinux : public fileapi::MTPDeviceDelegate {
 public:
  // Should only be called by ScopedMTPDeviceMapEntry. Use
  // GetAsWeakPtrOnIOThread() to get a weak pointer instance of this class.
  // Defer the device initializations until the first file operation request.
  // Do all the initializations in LazyInit() function.
  explicit MTPDeviceDelegateImplLinux(const std::string& device_location);

  // MTPDeviceDelegate:
  virtual base::PlatformFileError GetFileInfo(
      const FilePath& file_path,
      base::PlatformFileInfo* file_info) OVERRIDE;
  virtual scoped_ptr<fileapi::FileSystemFileUtil::AbstractFileEnumerator>
      CreateFileEnumerator(const FilePath& root,
                           bool recursive) OVERRIDE;
  virtual base::PlatformFileError CreateSnapshotFile(
      const FilePath& device_file_path,
      const FilePath& local_path,
      base::PlatformFileInfo* file_info) OVERRIDE;
  virtual base::SequencedTaskRunner* GetMediaTaskRunner() OVERRIDE;
  virtual void CancelPendingTasksAndDeleteDelegate() OVERRIDE;
  virtual base::WeakPtr<fileapi::MTPDeviceDelegate> GetAsWeakPtrOnIOThread()
      OVERRIDE;

 private:
  // Destructed via DeleteDelegateOnTaskRunner(). Do all the clean up in
  // DeleteDelegateOnTaskRunner().
  virtual ~MTPDeviceDelegateImplLinux();

  // Opens the device for communication. This function is called on
  // |media_task_runner_|. Returns true if the device is ready for
  // communication, else false.
  bool LazyInit();

  // Deletes the delegate on the task runner thread. Called by
  // CancelPendingTasksAndDeleteDelegate(). Performs clean up that needs to
  // happen on |media_task_runner_|.
  void DeleteDelegateOnTaskRunner();

  // Stores the registered file system device path value. This path does not
  // correspond to a real device path (E.g.: "/usb:2,2:81282").
  const std::string device_path_;

  // Stores the device handle returned by
  // MediaTransferProtocolManager::OpenStorage().
  std::string device_handle_;

  // Stores a reference to worker pool thread. All requests and response of file
  // operations are posted on |media_task_runner_|.
  scoped_refptr<base::SequencedTaskRunner> media_task_runner_;

  // |media_task_runner_| can wait on this event until the requested task is
  // complete.
  base::WaitableEvent on_task_completed_event_;

  // Used to notify |media_task_runner_| pending tasks about the shutdown
  // sequence. Signaled on the IO thread.
  base::WaitableEvent on_shutdown_event_;

  DISALLOW_COPY_AND_ASSIGN(MTPDeviceDelegateImplLinux);
};

}  // namespace chrome

#endif  // CHROME_BROWSER_MEDIA_GALLERY_LINUX_MTP_DEVICE_DELEGATE_IMPL_LINUX_H_
