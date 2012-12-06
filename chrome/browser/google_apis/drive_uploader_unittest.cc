// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_uploader.h"

#include <algorithm>
#include <cstdlib>
#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/google_apis/drive_service_interface.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kTestDummyId[] = "file:dummy_id";
const char kTestDocumentTitle[] = "Hello world";
const char kTestDrivePath[] = "drive/dummy.txt";
const char kTestInitiateUploadPath[] =
    "http://test/feeds/upload/create-session/default/private/full";
const char kTestMimeType[] = "text/plain";
const char kTestUploadPath[] = "http://test/upload_location";
const int64 kUploadChunkSize = 512 * 1024;

// Mock DriveService that only handles file uploading requests and verifies if
// the uploaded content matches the preset expectation.
class MockDriveService : public DriveServiceInterface {
 public:
  MockDriveService() : received_bytes_(0), resume_upload_call_count_(0) {}

  // Sets up an expected upload content. InitiateUpload and ResumeUpload will
  // verify that the specified data is correctly uploaded.
  void SetExpectation(const std::string& expected_upload_content) {
    expected_upload_content_ = expected_upload_content;
    received_bytes_ = 0;
    resume_upload_call_count_ = 0;
  }

  int64 received_bytes() const { return received_bytes_; }

  int64 resume_upload_call_count() const { return resume_upload_call_count_; }

 private:
  // DriveServiceInterface overrides.
  // Handles a request for obtaining an upload location URL.
  virtual void InitiateUpload(const InitiateUploadParams& params,
                              const InitiateUploadCallback& callback) OVERRIDE {
    const int64 expected_size = expected_upload_content_.size();

    // Verify that the expected parameters are passed.
    if (params.upload_mode == UPLOAD_NEW_FILE)
      EXPECT_EQ(kTestDocumentTitle, params.title);
    else
      EXPECT_EQ("", params.title);
    EXPECT_EQ(kTestMimeType, params.content_type);
    EXPECT_EQ(expected_size, params.content_length);
    EXPECT_EQ(GURL(kTestInitiateUploadPath), params.upload_location);

    // Calls back the upload URL for subsequent ResumeUpload operations.
    callback.Run(HTTP_SUCCESS, GURL(kTestUploadPath));
  }

 // Handles a request for uploading a chunk of bytes.
  virtual void ResumeUpload(const ResumeUploadParams& params,
                            const ResumeUploadCallback& callback) OVERRIDE {
    const int64 expected_size = expected_upload_content_.size();

    // The upload range should start from the current first unreceived byte.
    EXPECT_EQ(received_bytes_, params.start_range);

    // The upload data must be split into 512KB chunks.
    const int64 expected_chunk_end =
        std::min(received_bytes_ + kUploadChunkSize, expected_size);
    EXPECT_EQ(expected_chunk_end - 1, params.end_range);

    const int64 expected_chunk_size =
        expected_chunk_end - received_bytes_;
    const std::string expected_chunk_data(
        expected_upload_content_.substr(received_bytes_,
                                        expected_chunk_size));
    std::string uploading_data(params.buf->data(),
                               params.buf->data() + expected_chunk_size);
    EXPECT_EQ(expected_chunk_data, uploading_data);

    // The upload URL returned by InitiateUpload() must be used.
    EXPECT_EQ(GURL(kTestUploadPath), params.upload_location);

    // Other parameters should be the exact values passed to DriveUploader.
    EXPECT_EQ(expected_size, params.content_length);
    EXPECT_EQ(kTestMimeType, params.content_type);

    // Update the internal status of the current upload session.
    resume_upload_call_count_ ++;
    received_bytes_ = params.end_range + 1;

    // Callback with response.
    if (received_bytes_ == params.content_length) {
      ResumeUploadResponse response(
          params.upload_mode == UPLOAD_NEW_FILE ? HTTP_CREATED : HTTP_SUCCESS,
          -1, -1);

      base::DictionaryValue dict;
      dict.Set("id.$t", new base::StringValue(kTestDummyId));
      callback.Run(response, DocumentEntry::CreateFrom(dict));
    } else {
      ResumeUploadResponse response(HTTP_RESUME_INCOMPLETE,
                                    0, params.end_range);
      callback.Run(response, scoped_ptr<DocumentEntry>());
    }
  }

  // Other DriveServiceInterface methods should not be used from DriveUploader.
  virtual void Initialize(Profile* profile) OVERRIDE {
    NOTREACHED();
  }
  virtual void AddObserver(DriveServiceObserver* observer) OVERRIDE {
    NOTREACHED();
  }
  virtual void RemoveObserver(DriveServiceObserver* observer) OVERRIDE {
    NOTREACHED();
  }
  virtual bool CanStartOperation() const OVERRIDE {
    NOTREACHED();
    return false;
  }
  virtual void CancelAll() OVERRIDE {
    NOTREACHED();
  }
  virtual bool CancelForFilePath(const FilePath& file_path) OVERRIDE {
    NOTREACHED();
    return false;
  }
  virtual OperationProgressStatusList GetProgressStatusList() const OVERRIDE {
    NOTREACHED();
    return OperationProgressStatusList();
  }
  virtual bool HasAccessToken() const OVERRIDE {
    NOTREACHED();
    return false;
  }
  virtual bool HasRefreshToken() const OVERRIDE {
    NOTREACHED();
    return false;
  }
  virtual void GetDocuments(const GURL& feed_url,
                            int64 start_changestamp,
                            const std::string& search_query,
                            bool shared_with_me,
                            const std::string& directory_resource_id,
                            const GetDataCallback& callback) OVERRIDE {
    NOTREACHED();
  }
  virtual void GetDocumentEntry(const std::string& resource_id,
                                const GetDataCallback& callback) OVERRIDE {
    NOTREACHED();
  }
  virtual void GetAccountMetadata(const GetDataCallback& callback) OVERRIDE {
    NOTREACHED();
  }
  virtual void GetApplicationInfo(const GetDataCallback& callback) OVERRIDE {
    NOTREACHED();
  }
  virtual void DeleteDocument(const GURL& document_url,
                              const EntryActionCallback& callback) OVERRIDE {
    NOTREACHED();
  }
  virtual void DownloadDocument(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& content_url,
      DocumentExportFormat format,
      const DownloadActionCallback& callback) OVERRIDE {
    NOTREACHED();
  }
  virtual void CopyDocument(const std::string& resource_id,
                            const FilePath::StringType& new_name,
                            const GetDataCallback& callback) OVERRIDE {
    NOTREACHED();
  }
  virtual void RenameResource(const GURL& resource_url,
                              const FilePath::StringType& new_name,
                              const EntryActionCallback& callback) OVERRIDE {
    NOTREACHED();
  }
  virtual void AddResourceToDirectory(
      const GURL& parent_content_url,
      const GURL& resource_url,
      const EntryActionCallback& callback) OVERRIDE {
    NOTREACHED();
  }
  virtual void RemoveResourceFromDirectory(
      const GURL& parent_content_url,
      const std::string& resource_id,
      const EntryActionCallback& callback) OVERRIDE {
    NOTREACHED();
  }
  virtual void AddNewDirectory(const GURL& parent_content_url,
                               const FilePath::StringType& directory_name,
                               const GetDataCallback& callback) OVERRIDE {
    NOTREACHED();
  }
  virtual void DownloadFile(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& content_url,
      const DownloadActionCallback& download_action_callback,
      const GetContentCallback& get_content_callback) OVERRIDE {
    NOTREACHED();
  }
  virtual void AuthorizeApp(const GURL& resource_url,
                            const std::string& app_id,
                            const GetDataCallback& callback) OVERRIDE {
    NOTREACHED();
  }

  std::string expected_upload_content_;
  int64 received_bytes_;
  int64 resume_upload_call_count_;
};

class DriveUploaderTest : public testing::Test {
 public:
  DriveUploaderTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        io_thread_(content::BrowserThread::IO) {
  }

  virtual void SetUp() OVERRIDE {
    io_thread_.StartIOThread();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() OVERRIDE {
    ASSERT_TRUE(temp_dir_.Delete());
  }

 protected:
  // Creates a |size| byte file and returns its |path|. The file is filled with
  // random bytes so that the test assertions can identify correct
  // portion of the file is being sent.
  void PrepareUploadExpectationOfSpecifiedSize(size_t size, FilePath* path) {
    std::string data(size, ' ');
    for (size_t i = 0; i < size; ++i)
      data[i] = static_cast<char>(rand() % 256);
    ASSERT_TRUE(file_util::CreateTemporaryFileInDir(temp_dir_.path(), path));
    file_util::WriteFile(*path, data.c_str(), static_cast<int>(data.size()));

    mock_service_.SetExpectation(data);
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  base::ScopedTempDir temp_dir_;
  MockDriveService mock_service_;
};

// Records whether UploaderReadyCallback is called or not.
void OnUploaderReady(bool* called, int32 /* upload_id */) {
  *called = true;
}

// Struct for holding the results copied from UploadCompletionCallback.
struct UploadCompletionCallbackResult {
  UploadCompletionCallbackResult() : error(DRIVE_UPLOAD_ERROR_ABORT) {}
  DriveUploadError error;
  FilePath drive_path;
  FilePath file_path;
  scoped_ptr<DocumentEntry> document_entry;
};

// Copies the result from UploadCompletionCallback and quit the message loop.
void CopyResultsFromUploadCompletionCallbackAndQuit(
    UploadCompletionCallbackResult* out,
    DriveUploadError error,
    const FilePath& drive_path,
    const FilePath& file_path,
    scoped_ptr<DocumentEntry> document_entry) {
  out->error = error;
  out->drive_path = drive_path;
  out->file_path = file_path;
  out->document_entry = document_entry.Pass();
  MessageLoop::current()->Quit();
}

}  // namespace

TEST_F(DriveUploaderTest, UploadExisting0KB) {
  FilePath local_path;
  PrepareUploadExpectationOfSpecifiedSize(0, &local_path);

  UploadCompletionCallbackResult out;

  DriveUploader uploader(&mock_service_);
  uploader.UploadExistingFile(
      GURL(kTestInitiateUploadPath),
      FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestMimeType,
      0,  // content length
      base::Bind(&CopyResultsFromUploadCompletionCallbackAndQuit, &out));
  message_loop_.Run();

  EXPECT_EQ(1, mock_service_.resume_upload_call_count());
  EXPECT_EQ(0, mock_service_.received_bytes());
  EXPECT_EQ(DRIVE_UPLOAD_OK, out.error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe(kTestDrivePath), out.drive_path);
  EXPECT_EQ(local_path, out.file_path);
  ASSERT_TRUE(out.document_entry);
  EXPECT_EQ(kTestDummyId, out.document_entry->id());
}

TEST_F(DriveUploaderTest, UploadExisting512KB) {
  FilePath local_path;
  PrepareUploadExpectationOfSpecifiedSize(512 * 1024, &local_path);

  UploadCompletionCallbackResult out;

  DriveUploader uploader(&mock_service_);
  uploader.UploadExistingFile(
      GURL(kTestInitiateUploadPath),
      FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestMimeType,
      512 * 1024,  // content length
      base::Bind(&CopyResultsFromUploadCompletionCallbackAndQuit, &out));
  message_loop_.Run();

  // 512KB upload should not be split into multiple chunks.
  EXPECT_EQ(1, mock_service_.resume_upload_call_count());
  EXPECT_EQ(512 * 1024, mock_service_.received_bytes());
  EXPECT_EQ(DRIVE_UPLOAD_OK, out.error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe(kTestDrivePath), out.drive_path);
  EXPECT_EQ(local_path, out.file_path);
  ASSERT_TRUE(out.document_entry);
  EXPECT_EQ(kTestDummyId, out.document_entry->id());
}

TEST_F(DriveUploaderTest, UploadExisting1234KB) {
  FilePath local_path;
  PrepareUploadExpectationOfSpecifiedSize(1234 * 1024, &local_path);

  UploadCompletionCallbackResult out;

  DriveUploader uploader(&mock_service_);
  uploader.UploadExistingFile(
      GURL(kTestInitiateUploadPath),
      FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestMimeType,
      1234 * 1024,  // content length
      base::Bind(&CopyResultsFromUploadCompletionCallbackAndQuit, &out));
  message_loop_.Run();

  // The file should be split into 3 chunks (1234 = 512 + 512 + 210).
  EXPECT_EQ(3, mock_service_.resume_upload_call_count());
  EXPECT_EQ(1234 * 1024, mock_service_.received_bytes());
  EXPECT_EQ(DRIVE_UPLOAD_OK, out.error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe(kTestDrivePath), out.drive_path);
  EXPECT_EQ(local_path, out.file_path);
  ASSERT_TRUE(out.document_entry);
  EXPECT_EQ(kTestDummyId, out.document_entry->id());
}

TEST_F(DriveUploaderTest, UploadNew1234KB) {
  FilePath local_path;
  PrepareUploadExpectationOfSpecifiedSize(1234 * 1024, &local_path);

  UploadCompletionCallbackResult out;
  bool uploader_ready_called = false;

  DriveUploader uploader(&mock_service_);
  uploader.UploadNewFile(
      GURL(kTestInitiateUploadPath),
      FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestDocumentTitle,
      kTestMimeType,
      1234 * 1024,  // content length
      1234 * 1024,  // current file size
      base::Bind(&CopyResultsFromUploadCompletionCallbackAndQuit, &out),
      base::Bind(&OnUploaderReady, &uploader_ready_called)
  );
  message_loop_.Run();

  EXPECT_TRUE(uploader_ready_called);
  // The file should be split into 3 chunks (1234 = 512 + 512 + 210).
  EXPECT_EQ(3, mock_service_.resume_upload_call_count());
  EXPECT_EQ(1234 * 1024, mock_service_.received_bytes());
  EXPECT_EQ(DRIVE_UPLOAD_OK, out.error);
  EXPECT_EQ(FilePath::FromUTF8Unsafe(kTestDrivePath), out.drive_path);
  EXPECT_EQ(local_path, out.file_path);
  ASSERT_TRUE(out.document_entry);
  EXPECT_EQ(kTestDummyId, out.document_entry->id());
}

// TODO(kinaba): add tests for failing cases.

}  // namespace google_apis
