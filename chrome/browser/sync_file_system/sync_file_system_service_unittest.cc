// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/sync_file_system/local_file_sync_service.h"
#include "chrome/browser/sync_file_system/mock_remote_file_sync_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_service.h"
#include "chrome/browser/sync_file_system/sync_file_system_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/fileapi/syncable/canned_syncable_file_system.h"
#include "webkit/fileapi/syncable/sync_callbacks.h"
#include "webkit/fileapi/syncable/sync_file_metadata.h"
#include "webkit/fileapi/syncable/sync_status_code.h"
#include "webkit/fileapi/syncable/syncable_file_system_util.h"

using fileapi::FileSystemURL;
using fileapi::FileSystemURLSet;
using fileapi::SyncFileMetadata;
using fileapi::SyncStatusCode;
using ::testing::StrictMock;
using ::testing::_;

namespace sync_file_system {

namespace {

const char kOrigin[] = "http://example.com";
const char kServiceName[] = "test";

template <typename R>
void AssignValueAndQuit(base::RunLoop* run_loop,
                        SyncStatusCode* status_out, R* value_out,
                        SyncStatusCode status, const R& value) {
  DCHECK(status_out);
  DCHECK(value_out);
  DCHECK(run_loop);
  *status_out = status;
  *value_out = value;
  run_loop->Quit();
}

}  // namespace

class SyncFileSystemServiceTest : public testing::Test {
 protected:
  SyncFileSystemServiceTest() {}
  ~SyncFileSystemServiceTest() {}

  virtual void SetUp() OVERRIDE {
    thread_helper_.SetUp();

    file_system_.reset(new fileapi::CannedSyncableFileSystem(
        GURL(kOrigin), kServiceName,
        thread_helper_.io_task_runner(),
        thread_helper_.file_task_runner()));

    local_service_ = new LocalFileSyncService;
    remote_service_ = new StrictMock<MockRemoteFileSyncService>;
    sync_service_.reset(new SyncFileSystemService(&profile_));

    EXPECT_CALL(*mock_remote_service(),
                AddObserver(sync_service_.get())).Times(1);

    sync_service_->Initialize(
        make_scoped_ptr(local_service_),
        scoped_ptr<RemoteFileSyncService>(remote_service_));

    file_system_->SetUp();
  }

  virtual void TearDown() OVERRIDE {
    sync_service_->Shutdown();

    file_system_->TearDown();
    fileapi::RevokeSyncableFileSystem(kServiceName);
    thread_helper_.TearDown();
  }

  void InitializeApp() {
    base::RunLoop run_loop;
    SyncStatusCode status = fileapi::SYNC_STATUS_UNKNOWN;

    EXPECT_CALL(*mock_remote_service(),
                RegisterOriginForTrackingChanges(GURL(kOrigin), _)).Times(1);

    sync_service_->InitializeForApp(
        file_system_->file_system_context(),
        kServiceName, GURL(kOrigin),
        AssignAndQuitCallback(&run_loop, &status));

    run_loop.Run();

    EXPECT_EQ(fileapi::SYNC_STATUS_OK, status);
    EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_->OpenFileSystem());
  }

  FileSystemURL URL(const std::string& path) const {
    return file_system_->URL(path);
  }

  StrictMock<MockRemoteFileSyncService>* mock_remote_service() {
    return remote_service_;
  }

  TestingProfile profile_;
  MultiThreadTestHelper thread_helper_;
  scoped_ptr<fileapi::CannedSyncableFileSystem> file_system_;

  // Their ownerships are transferred to SyncFileSystemService.
  LocalFileSyncService* local_service_;
  StrictMock<MockRemoteFileSyncService>* remote_service_;

  scoped_ptr<SyncFileSystemService> sync_service_;
};

TEST_F(SyncFileSystemServiceTest, InitializeForApp) {
  InitializeApp();
}

TEST_F(SyncFileSystemServiceTest, GetConflictFilesWithoutInitialize) {
  EXPECT_EQ(base::PLATFORM_FILE_OK, file_system_->OpenFileSystem());

  {
    base::RunLoop run_loop;
    FileSystemURLSet returned_files;
    SyncStatusCode status = fileapi::SYNC_STATUS_UNKNOWN;
    sync_service_->GetConflictFiles(
        GURL(kOrigin), kServiceName,
        base::Bind(&AssignValueAndQuit<FileSystemURLSet>,
                   &run_loop, &status, &returned_files));
    run_loop.Run();

    EXPECT_EQ(fileapi::SYNC_STATUS_NOT_INITIALIZED, status);
  }

  {
    base::RunLoop run_loop;
    fileapi::ConflictFileInfo actual_file_info;
    SyncStatusCode status = fileapi::SYNC_STATUS_UNKNOWN;
    sync_service_->GetConflictFileInfo(
        GURL(kOrigin), kServiceName, URL("foo"),
        base::Bind(&AssignValueAndQuit<fileapi::ConflictFileInfo>,
                   &run_loop, &status, &actual_file_info));
    run_loop.Run();

    EXPECT_EQ(fileapi::SYNC_STATUS_NOT_INITIALIZED, status);
  }
}

TEST_F(SyncFileSystemServiceTest, GetConflictFiles) {
  InitializeApp();

  // 1. Sets up (conflicting) files.
  struct {
    FileSystemURL url;
    SyncFileMetadata local_metadata;
    SyncFileMetadata remote_metadata;
  } files[] = {
    { URL("file1"),
      SyncFileMetadata(fileapi::SYNC_FILE_TYPE_FILE,
                       10, base::Time::FromDoubleT(1)),
      SyncFileMetadata(fileapi::SYNC_FILE_TYPE_FILE,
                       12, base::Time::FromDoubleT(2)),
    },
    { URL("dir"),
      SyncFileMetadata(fileapi::SYNC_FILE_TYPE_DIRECTORY,
                       0, base::Time::FromDoubleT(3)),
      SyncFileMetadata(fileapi::SYNC_FILE_TYPE_DIRECTORY,
                       0, base::Time::FromDoubleT(4)),
    },
    { URL("dir/foo"),
      SyncFileMetadata(fileapi::SYNC_FILE_TYPE_DIRECTORY,
                       0, base::Time::FromDoubleT(5)),
      SyncFileMetadata(fileapi::SYNC_FILE_TYPE_FILE,
                       200, base::Time::FromDoubleT(6)),
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(files); ++i) {
    // Set up local files/directories.
    switch (files[i].local_metadata.file_type) {
      case fileapi::SYNC_FILE_TYPE_FILE:
        EXPECT_EQ(base::PLATFORM_FILE_OK,
                  file_system_->CreateFile(files[i].url));
        EXPECT_EQ(base::PLATFORM_FILE_OK,
                  file_system_->TruncateFile(files[i].url,
                                             files[i].local_metadata.size));
        break;
      case fileapi::SYNC_FILE_TYPE_DIRECTORY:
        EXPECT_EQ(base::PLATFORM_FILE_OK,
                  file_system_->CreateDirectory(files[i].url));
        break;
      case fileapi::SYNC_FILE_TYPE_UNKNOWN:
        FAIL();
    }
    EXPECT_EQ(base::PLATFORM_FILE_OK,
              file_system_->TouchFile(files[i].url, base::Time(),
                                      files[i].local_metadata.last_modified));

    // Registers remote file information (mock).
    mock_remote_service()->add_conflict_file(
        files[i].url, files[i].remote_metadata);
  }

  // 2. Test GetConflictFiles.
  EXPECT_CALL(*mock_remote_service(),
              GetConflictFiles(GURL(kOrigin), _)).Times(1);

  base::RunLoop run_loop;
  FileSystemURLSet returned_files;
  SyncStatusCode status = fileapi::SYNC_STATUS_UNKNOWN;
  sync_service_->GetConflictFiles(
      GURL(kOrigin), kServiceName,
      base::Bind(&AssignValueAndQuit<FileSystemURLSet>,
                 &run_loop, &status, &returned_files));
  run_loop.Run();

  ASSERT_EQ(ARRAYSIZE_UNSAFE(files), returned_files.size());
  for (size_t i = 0; i < returned_files.size(); ++i)
    ASSERT_TRUE(ContainsKey(returned_files, files[i].url));

  // 3. Test GetConflictFileInfo.
  EXPECT_CALL(*mock_remote_service(),
              GetRemoteFileMetadata(_, _)).Times(3);

  for (size_t i = 0; i < returned_files.size(); ++i) {
    SCOPED_TRACE(testing::Message() << files[i].url.DebugString());

    base::RunLoop run_loop;
    fileapi::ConflictFileInfo actual_file_info;
    SyncStatusCode status = fileapi::SYNC_STATUS_UNKNOWN;
    sync_service_->GetConflictFileInfo(
        GURL(kOrigin), kServiceName, files[i].url,
        base::Bind(&AssignValueAndQuit<fileapi::ConflictFileInfo>,
                   &run_loop, &status, &actual_file_info));
    run_loop.Run();

    EXPECT_EQ(fileapi::SYNC_STATUS_OK, status);

    EXPECT_EQ(files[i].local_metadata.file_type,
              actual_file_info.local_metadata.file_type);
    EXPECT_EQ(files[i].local_metadata.size,
              actual_file_info.local_metadata.size);

    // Touch doesn't change the modified_date or GetMetadata doesn't return
    // correct modified date for directories.
    // TODO(kinuko,tzik): Investigate this.
    if (files[i].local_metadata.file_type == fileapi::SYNC_FILE_TYPE_FILE) {
      EXPECT_EQ(files[i].local_metadata.last_modified,
                actual_file_info.local_metadata.last_modified);
    }

    EXPECT_EQ(files[i].remote_metadata.file_type,
              actual_file_info.remote_metadata.file_type);
    EXPECT_EQ(files[i].remote_metadata.size,
              actual_file_info.remote_metadata.size);
    EXPECT_EQ(files[i].remote_metadata.last_modified,
              actual_file_info.remote_metadata.last_modified);
  }
}

}  // namespace sync_file_system
