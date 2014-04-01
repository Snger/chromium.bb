// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sync/api/sync_data.h"

#include <string>

#include "base/memory/ref_counted_memory.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/time/time.h"
#include "sync/api/attachments/attachment_id.h"
#include "sync/api/attachments/attachment_service.h"
#include "sync/api/attachments/attachment_service_proxy.h"
#include "sync/api/attachments/fake_attachment_service.h"
#include "sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace syncer {

namespace {

const string kSyncTag = "3984729834";
const ModelType kDatatype = syncer::PREFERENCES;
const string kNonUniqueTitle = "my preference";
const int64 kId = 439829;
const base::Time kLastModifiedTime = base::Time();

class SyncDataTest : public testing::Test {
 protected:
  SyncDataTest()
      : attachment_service(FakeAttachmentService::CreateForTest()),
        attachment_service_weak_ptr_factory(attachment_service.get()),
        attachment_service_proxy(
            base::MessageLoopProxy::current(),
            attachment_service_weak_ptr_factory.GetWeakPtr()) {}
  base::MessageLoop loop;
  sync_pb::EntitySpecifics specifics;
  scoped_ptr<AttachmentService> attachment_service;
  base::WeakPtrFactory<AttachmentService> attachment_service_weak_ptr_factory;
  AttachmentServiceProxy attachment_service_proxy;
};

TEST_F(SyncDataTest, NoArgCtor) {
  SyncData data;
  EXPECT_FALSE(data.IsValid());
}

TEST_F(SyncDataTest, CreateLocalDelete) {
  SyncData data = SyncData::CreateLocalDelete(kSyncTag, kDatatype);
  EXPECT_TRUE(data.IsValid());
  EXPECT_TRUE(data.IsLocal());
  EXPECT_EQ(kSyncTag, data.GetTag());
  EXPECT_EQ(kDatatype, data.GetDataType());
}

TEST_F(SyncDataTest, CreateLocalData) {
  specifics.mutable_preference();
  SyncData data =
      SyncData::CreateLocalData(kSyncTag, kNonUniqueTitle, specifics);
  EXPECT_TRUE(data.IsValid());
  EXPECT_TRUE(data.IsLocal());
  EXPECT_EQ(kSyncTag, data.GetTag());
  EXPECT_EQ(kDatatype, data.GetDataType());
  EXPECT_EQ(kNonUniqueTitle, data.GetTitle());
  EXPECT_TRUE(data.GetSpecifics().has_preference());
}

TEST_F(SyncDataTest, CreateLocalDataWithAttachments) {
  specifics.mutable_preference();
  scoped_refptr<base::RefCountedMemory> bytes(new base::RefCountedString);
  AttachmentList attachments;
  attachments.push_back(Attachment::Create(bytes));
  attachments.push_back(Attachment::Create(bytes));
  attachments.push_back(Attachment::Create(bytes));

  SyncData data = SyncData::CreateLocalDataWithAttachments(
      kSyncTag, kNonUniqueTitle, specifics, attachments);
  EXPECT_TRUE(data.IsValid());
  EXPECT_TRUE(data.IsLocal());
  EXPECT_EQ(kSyncTag, data.GetTag());
  EXPECT_EQ(kDatatype, data.GetDataType());
  EXPECT_EQ(kNonUniqueTitle, data.GetTitle());
  EXPECT_TRUE(data.GetSpecifics().has_preference());
  AttachmentIdList attachment_ids = data.GetAttachmentIds();
  EXPECT_EQ(3U, attachment_ids.size());
  EXPECT_EQ(3U, data.GetLocalAttachmentsForUpload().size());
}

TEST_F(SyncDataTest, CreateLocalDataWithAttachments_EmptyListOfAttachments) {
  specifics.mutable_preference();
  AttachmentList attachments;
  SyncData data = SyncData::CreateLocalDataWithAttachments(
      kSyncTag, kNonUniqueTitle, specifics, attachments);
  EXPECT_TRUE(data.IsValid());
  EXPECT_TRUE(data.IsLocal());
  EXPECT_EQ(kSyncTag, data.GetTag());
  EXPECT_EQ(kDatatype, data.GetDataType());
  EXPECT_EQ(kNonUniqueTitle, data.GetTitle());
  EXPECT_TRUE(data.GetSpecifics().has_preference());
  EXPECT_TRUE(data.GetAttachmentIds().empty());
  EXPECT_TRUE(data.GetLocalAttachmentsForUpload().empty());
}

TEST_F(SyncDataTest, CreateRemoteData) {
  specifics.mutable_preference();
  SyncData data = SyncData::CreateRemoteData(kId,
                                             specifics,
                                             kLastModifiedTime,
                                             AttachmentIdList(),
                                             attachment_service_proxy);
  EXPECT_TRUE(data.IsValid());
  EXPECT_FALSE(data.IsLocal());
  EXPECT_EQ(kId, data.GetRemoteId());
  EXPECT_EQ(kLastModifiedTime, data.GetRemoteModifiedTime());
  EXPECT_TRUE(data.GetSpecifics().has_preference());
  EXPECT_TRUE(data.GetAttachmentIds().empty());
}

TEST_F(SyncDataTest, CreateRemoteData_WithoutAttachmentService) {
  specifics.mutable_preference();
  SyncData data = SyncData::CreateRemoteData(kId, specifics, kLastModifiedTime);
  EXPECT_TRUE(data.IsValid());
  EXPECT_FALSE(data.IsLocal());
  EXPECT_EQ(kId, data.GetRemoteId());
  EXPECT_EQ(kLastModifiedTime, data.GetRemoteModifiedTime());
  EXPECT_TRUE(data.GetSpecifics().has_preference());
}

// TODO(maniscalco): Add test cases that verify GetLocalAttachmentsForUpload and
// DropAttachments calls are passed through to the underlying AttachmentService.

}  // namespace

}  // namespace syncer
