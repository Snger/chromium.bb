// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/parallel_download_job.h"

#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "content/browser/download/download_item_impl_delegate.h"
#include "content/browser/download/mock_download_item_impl.h"
#include "content/browser/download/parallel_download_utils.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::NiceMock;

namespace content {

namespace {

class MockDownloadRequestHandle : public DownloadRequestHandleInterface {
 public:
  MOCK_CONST_METHOD0(GetWebContents, WebContents*());
  MOCK_CONST_METHOD0(GetDownloadManager, DownloadManager*());
  MOCK_CONST_METHOD0(PauseRequest, void());
  MOCK_CONST_METHOD0(ResumeRequest, void());
  MOCK_CONST_METHOD0(CancelRequest, void());
  MOCK_CONST_METHOD0(DebugString, std::string());
};

}  // namespace

class ParallelDownloadJobForTest : public ParallelDownloadJob {
 public:
  ParallelDownloadJobForTest(
      DownloadItemImpl* download_item,
      std::unique_ptr<DownloadRequestHandleInterface> request_handle,
      const DownloadCreateInfo& create_info,
      int request_count)
      : ParallelDownloadJob(download_item,
                            std::move(request_handle),
                            create_info),
        request_count_(request_count) {}

  void CreateRequest(int64_t offset, int64_t length) override {
    std::unique_ptr<DownloadWorker> worker =
        base::MakeUnique<DownloadWorker>(this, offset, length);

    DCHECK(workers_.find(offset) == workers_.end());
    workers_[offset] = std::move(worker);
  }

  ParallelDownloadJob::WorkerMap& workers() { return workers_; }

  int GetParallelRequestCount() const override { return request_count_; }

  void OnByteStreamReady(
      DownloadWorker* worker,
      std::unique_ptr<ByteStreamReader> stream_reader) override {
    CountOnByteStreamReady();
  }

  MOCK_METHOD0(CountOnByteStreamReady, void());

 private:
  int request_count_;
  DISALLOW_COPY_AND_ASSIGN(ParallelDownloadJobForTest);
};

class ParallelDownloadJobTest : public testing::Test {
 public:
  void CreateParallelJob(int64_t offset,
                         int64_t content_length,
                         const DownloadItem::ReceivedSlices& slices,
                         int request_count) {
    item_delegate_ = base::MakeUnique<DownloadItemImplDelegate>();
    download_item_ = base::MakeUnique<NiceMock<MockDownloadItemImpl>>(
        item_delegate_.get(), slices);
    DownloadCreateInfo info;
    info.offset = offset;
    info.total_bytes = content_length;
    std::unique_ptr<MockDownloadRequestHandle> request_handle =
        base::MakeUnique<MockDownloadRequestHandle>();
    mock_request_handle_ = request_handle.get();
    job_ = base::MakeUnique<ParallelDownloadJobForTest>(
        download_item_.get(), std::move(request_handle), info, request_count);
  }

  void DestroyParallelJob() {
    job_.reset();
    download_item_.reset();
    item_delegate_.reset();
    mock_request_handle_ = nullptr;
  }

  void BuildParallelRequests() { job_->BuildParallelRequests(); }

  bool IsJobCanceled() const { return job_->is_canceled_; };

  void MakeWorkerReady(
      DownloadWorker* worker,
      std::unique_ptr<MockDownloadRequestHandle> request_handle) {
    UrlDownloader::Delegate* delegate =
        static_cast<UrlDownloader::Delegate*>(worker);
    std::unique_ptr<DownloadCreateInfo> create_info =
        base::MakeUnique<DownloadCreateInfo>();
    create_info->request_handle = std::move(request_handle);
    delegate->OnUrlDownloaderStarted(
        std::move(create_info), std::unique_ptr<ByteStreamReader>(),
        DownloadUrlParameters::OnStartedCallback());
  }

  void VerifyWorker(int64_t offset, int64_t length) const {
    EXPECT_TRUE(job_->workers_.find(offset) != job_->workers_.end());
    EXPECT_EQ(offset, job_->workers_[offset]->offset());
    EXPECT_EQ(length, job_->workers_[offset]->length());
  }

  content::TestBrowserThreadBundle browser_threads_;
  std::unique_ptr<DownloadItemImplDelegate> item_delegate_;
  std::unique_ptr<MockDownloadItemImpl> download_item_;
  std::unique_ptr<ParallelDownloadJobForTest> job_;
  // Request handle for the original request.
  MockDownloadRequestHandle* mock_request_handle_;
};

// Test if parallel requests can be built correctly for a new download.
TEST_F(ParallelDownloadJobTest, CreateNewDownloadRequests) {
  // Totally 2 requests for 100 bytes.
  // Original request:  Range:0-49, for 50 bytes.
  // Task 1:  Range:50-, for 50 bytes.
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 2);
  BuildParallelRequests();
  EXPECT_EQ(1, static_cast<int>(job_->workers().size()));
  VerifyWorker(50, 0);
  DestroyParallelJob();

  // Totally 3 requests for 100 bytes.
  // Original request:  Range:0-32, for 33 bytes.
  // Task 1:  Range:33-65, for 33 bytes.
  // Task 2:  Range:66-, for 34 bytes.
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 3);
  BuildParallelRequests();
  EXPECT_EQ(2, static_cast<int>(job_->workers().size()));
  VerifyWorker(33, 33);
  VerifyWorker(66, 0);
  DestroyParallelJob();

  // Totally 3 requests for 100 bytes. Start from the 17th byte.
  // Original request:  Range:17-43, for 27 bytes.
  // Task 1:  Range:44-70, for 27 bytes.
  // Task 2:  Range:71-99, for 29 bytes.
  CreateParallelJob(17, 83, DownloadItem::ReceivedSlices(), 3);
  BuildParallelRequests();
  EXPECT_EQ(2, static_cast<int>(job_->workers().size()));
  VerifyWorker(44, 27);
  VerifyWorker(71, 0);
  DestroyParallelJob();

  // Less than 2 requests, do nothing.
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 1);
  BuildParallelRequests();
  EXPECT_TRUE(job_->workers().empty());
  DestroyParallelJob();

  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 0);
  BuildParallelRequests();
  EXPECT_TRUE(job_->workers().empty());
  DestroyParallelJob();

  // Content-length is 0, do nothing.
  CreateParallelJob(100, 0, DownloadItem::ReceivedSlices(), 3);
  BuildParallelRequests();
  EXPECT_TRUE(job_->workers().empty());
  DestroyParallelJob();

  CreateParallelJob(0, 0, DownloadItem::ReceivedSlices(), 3);
  BuildParallelRequests();
  EXPECT_TRUE(job_->workers().empty());
  DestroyParallelJob();

  // 2 bytes left for 3 additional requests. Only 1 are built.
  // Original request:  Range:98-98, for 1 byte.
  // Task 1:  Range:99-, for 1 byte.
  CreateParallelJob(98, 2, DownloadItem::ReceivedSlices(), 4);
  BuildParallelRequests();
  EXPECT_EQ(1, static_cast<int>(job_->workers().size()));
  VerifyWorker(99, 0);
  DestroyParallelJob();
}

// Pause, cancel, resume can be called before or after the worker establish
// the byte stream.
// These tests ensure the states consistency between the job and workers.

// Ensure cancel before building the requests will result in no requests are
// built.
TEST_F(ParallelDownloadJobTest, EarlyCancelBeforeBuildRequests) {
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 2);
  EXPECT_CALL(*mock_request_handle_, CancelRequest());

  // Job is canceled before building parallel requests.
  job_->Cancel(true);
  EXPECT_TRUE(IsJobCanceled());

  BuildParallelRequests();
  EXPECT_TRUE(job_->workers().empty());

  DestroyParallelJob();
}

// Ensure cancel before adding the byte stream will result in workers being
// canceled.
TEST_F(ParallelDownloadJobTest, EarlyCancelBeforeByteStreamReady) {
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 2);
  EXPECT_CALL(*mock_request_handle_, CancelRequest());

  BuildParallelRequests();
  VerifyWorker(50, 0);

  // Job is canceled after building parallel requests and before byte streams
  // are added to the file sink.
  job_->Cancel(true);
  EXPECT_TRUE(IsJobCanceled());

  for (auto& worker : job_->workers()) {
    std::unique_ptr<MockDownloadRequestHandle> mock_handle =
        base::MakeUnique<MockDownloadRequestHandle>();
    EXPECT_CALL(*mock_handle.get(), CancelRequest());
    MakeWorkerReady(worker.second.get(), std::move(mock_handle));
  }

  DestroyParallelJob();
}

// Ensure pause before adding the byte stream will result in workers being
// paused.
TEST_F(ParallelDownloadJobTest, EarlyPauseBeforeByteStreamReady) {
  CreateParallelJob(0, 100, DownloadItem::ReceivedSlices(), 2);
  EXPECT_CALL(*mock_request_handle_, PauseRequest());

  BuildParallelRequests();
  VerifyWorker(50, 0);

  // Job is paused after building parallel requests and before adding the byte
  // stream to the file sink.
  job_->Pause();
  EXPECT_TRUE(job_->is_paused());

  for (auto& worker : job_->workers()) {
    EXPECT_CALL(*job_.get(), CountOnByteStreamReady());
    std::unique_ptr<MockDownloadRequestHandle> mock_handle =
        base::MakeUnique<MockDownloadRequestHandle>();
    EXPECT_CALL(*mock_handle.get(), PauseRequest());
    MakeWorkerReady(worker.second.get(), std::move(mock_handle));
  }

  DestroyParallelJob();
}

}  // namespace content
