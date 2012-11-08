// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/threading/thread.h"
#include "chrome/browser/sync_file_system/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/syncable/canned_syncable_file_system.h"
#include "webkit/fileapi/syncable/file_change.h"
#include "webkit/fileapi/syncable/local_file_sync_context.h"
#include "webkit/fileapi/syncable/sync_status_code.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

using fileapi::FileChange;
using fileapi::FileChangeList;
using fileapi::FileSystemURL;
using fileapi::SyncFileType;
using fileapi::SyncStatusCode;

namespace sync_file_system {

namespace {

const char kOrigin[] = "http://example.com";
const char kServiceName[] = "test";

void DidPrepareForProcessRemoteChange(const tracked_objects::Location& where,
                                      const base::Closure& closure,
                                      SyncStatusCode expected_status,
                                      SyncFileType expected_file_type,
                                      SyncStatusCode status,
                                      SyncFileType file_type,
                                      const FileChangeList& changes) {
  SCOPED_TRACE(testing::Message() << where.ToString());
  ASSERT_EQ(expected_status, status);
  ASSERT_EQ(expected_file_type, file_type);
  ASSERT_TRUE(changes.empty());
  closure.Run();
}

}  // namespace

class LocalFileSyncServiceTest
    : public testing::Test,
      public LocalFileSyncService::Observer {
 protected:
  LocalFileSyncServiceTest() : num_changes_(0) {}

  ~LocalFileSyncServiceTest() {}

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    thread_helper_.SetUp();

    file_system_.reset(new fileapi::CannedSyncableFileSystem(
        GURL(kOrigin), kServiceName,
        thread_helper_.io_task_runner(),
        thread_helper_.file_task_runner()));

    local_service_.reset(new LocalFileSyncService);

    file_system_->SetUp();

    base::RunLoop run_loop;
    SyncStatusCode status = fileapi::SYNC_STATUS_UNKNOWN;
    local_service_->MaybeInitializeFileSystemContext(
        GURL(kOrigin), kServiceName, file_system_->file_system_context(),
        AssignAndQuitCallback(&run_loop, &status));
    run_loop.Run();

    local_service_->AddChangeObserver(this);

    EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_->OpenFileSystem());
  }

  virtual void TearDown() OVERRIDE {
    local_service_->Shutdown();
    file_system_->TearDown();
    fileapi::RevokeSyncableFileSystem(kServiceName);

    thread_helper_.TearDown();
  }

  // LocalChangeObserver overrides.
  virtual void OnLocalChangeAvailable(int64 num_changes) {
    num_changes_ = num_changes;
  }

  void PrepareForProcessRemoteChange(const FileSystemURL& url,
                                     const tracked_objects::Location& where,
                                     SyncStatusCode expected_status,
                                     SyncFileType expected_file_type) {
    base::RunLoop run_loop;
    local_service_->PrepareForProcessRemoteChange(
        url,
        base::Bind(&DidPrepareForProcessRemoteChange,
                   where,
                   run_loop.QuitClosure(),
                   expected_status,
                   expected_file_type));
    run_loop.Run();
  }

  SyncStatusCode ApplyRemoteChange(const FileChange& change,
                                   const FilePath& local_path,
                                   const FileSystemURL& url) {
    base::RunLoop run_loop;
    SyncStatusCode sync_status = fileapi::SYNC_STATUS_UNKNOWN;
    local_service_->ApplyRemoteChange(
        change, local_path, url,
        AssignAndQuitCallback(&run_loop, &sync_status));
    run_loop.Run();
    return sync_status;
  }

  MultiThreadTestHelper thread_helper_;

  ScopedTempDir temp_dir_;

  scoped_ptr<fileapi::CannedSyncableFileSystem> file_system_;
  scoped_ptr<LocalFileSyncService> local_service_;

  int64 num_changes_;
};

// More complete tests for PrepareForProcessRemoteChange and ApplyRemoteChange
// are also in content_unittest:LocalFileSyncContextTest.
TEST_F(LocalFileSyncServiceTest, RemoteSyncStepsSimple) {
  const FileSystemURL kFile(file_system_->URL("file"));
  const FileSystemURL kDir(file_system_->URL("dir"));
  const char kTestFileData[] = "0123456789";
  const int kTestFileDataSize = static_cast<int>(arraysize(kTestFileData) - 1);

  FilePath local_path;
  ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(),
                                                  &local_path));
  ASSERT_EQ(kTestFileDataSize,
            file_util::WriteFile(local_path, kTestFileData, kTestFileDataSize));

  // Run PrepareForProcessRemoteChange for kFile.
  PrepareForProcessRemoteChange(kFile, FROM_HERE,
                                fileapi::SYNC_STATUS_OK,
                                fileapi::SYNC_FILE_TYPE_UNKNOWN);

  // Run ApplyRemoteChange for kFile.
  FileChange change(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                    fileapi::SYNC_FILE_TYPE_FILE);
  EXPECT_EQ(fileapi::SYNC_STATUS_OK,
            ApplyRemoteChange(change, local_path, kFile));

  // Verify the file is synced.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_->VerifyFile(kFile, kTestFileData));

  // Run PrepareForProcessRemoteChange for kDir.
  PrepareForProcessRemoteChange(kDir, FROM_HERE,
                                fileapi::SYNC_STATUS_OK,
                                fileapi::SYNC_FILE_TYPE_UNKNOWN);

  // Run ApplyRemoteChange for kDir.
  change = FileChange(FileChange::FILE_CHANGE_ADD_OR_UPDATE,
                      fileapi::SYNC_FILE_TYPE_DIRECTORY);
  EXPECT_EQ(fileapi::SYNC_STATUS_OK,
            ApplyRemoteChange(change, FilePath(), kDir));

  // Verify the directory.
  EXPECT_EQ(base::PLATFORM_FILE_OK,
            file_system_->DirectoryExists(kDir));

  // Run ApplyRemoteChange for kDir deletion.
  change = FileChange(FileChange::FILE_CHANGE_DELETE,
                      fileapi::SYNC_FILE_TYPE_UNKNOWN);
  EXPECT_EQ(fileapi::SYNC_STATUS_OK,
            ApplyRemoteChange(change, FilePath(), kDir));

  // Now the directory must have deleted.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND,
            file_system_->DirectoryExists(kDir));
}

TEST_F(LocalFileSyncServiceTest, LocalChangeObserver) {
  file_system_->file_system_context()->sync_context()->
      set_mock_notify_changes_duration_in_sec(0);

  const FileSystemURL kFile(file_system_->URL("file"));
  const FileSystemURL kDir(file_system_->URL("dir"));
  const char kTestFileData[] = "0123456789";
  const int kTestFileDataSize = static_cast<int>(arraysize(kTestFileData) - 1);

  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_->CreateFile(kFile));

  EXPECT_EQ(1, num_changes_);

  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_->CreateDirectory(kDir));
  EXPECT_EQ(kTestFileDataSize,
            file_system_->WriteString(kFile, kTestFileData));

  EXPECT_EQ(2, num_changes_);
}

TEST_F(LocalFileSyncServiceTest, LocalChangeObserverMultipleContexts) {
  const char kOrigin2[] = "http://foo";
  fileapi::CannedSyncableFileSystem file_system2(
      GURL(kOrigin2), kServiceName,
      thread_helper_.io_task_runner(),
      thread_helper_.file_task_runner());
  file_system2.SetUp();
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system2.OpenFileSystem());

  base::RunLoop run_loop;
  SyncStatusCode status = fileapi::SYNC_STATUS_UNKNOWN;
  local_service_->MaybeInitializeFileSystemContext(
      GURL(kOrigin2), kServiceName, file_system2.file_system_context(),
      AssignAndQuitCallback(&run_loop, &status));
  run_loop.Run();

  file_system_->file_system_context()->sync_context()->
      set_mock_notify_changes_duration_in_sec(0);
  file_system2.file_system_context()->sync_context()->
      set_mock_notify_changes_duration_in_sec(0);

  const FileSystemURL kFile1(file_system_->URL("file1"));
  const FileSystemURL kFile2(file_system_->URL("file2"));
  const FileSystemURL kFile3(file_system2.URL("file3"));
  const FileSystemURL kFile4(file_system2.URL("file4"));

  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_->CreateFile(kFile1));
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_->CreateFile(kFile2));
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system2.CreateFile(kFile3));
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system2.CreateFile(kFile4));

  EXPECT_EQ(4, num_changes_);

  file_system2.TearDown();
}

}  // namespace sync_file_system
