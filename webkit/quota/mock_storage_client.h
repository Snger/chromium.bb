// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_QUOTA_MOCK_STORAGE_CLIENT_H_
#define WEBKIT_QUOTA_MOCK_STORAGE_CLIENT_H_

#include <map>

#include "base/compiler_specific.h"
#include "base/task.h"
#include "googleurl/src/gurl.h"
#include "webkit/quota/quota_client.h"

namespace quota {

class QuotaManagerProxy;

// Mock storage class for testing.
class MockStorageClient : public QuotaClient {
 public:
  MockStorageClient(QuotaManagerProxy* quota_manager_proxy);
  virtual ~MockStorageClient();

  // To add or modify mock data in this client.
  void AddMockOriginData(const GURL& origin_url, StorageType type, int64 size);
  void ModifyMockOriginDataSize(
      const GURL& origin_url, StorageType type, int64 delta);

  // QuotaClient methods.
  virtual QuotaClient::ID id() const OVERRIDE;
  virtual void OnQuotaManagerDestroyed() OVERRIDE;
  virtual void GetOriginUsage(const GURL& origin_url,
                              StorageType type,
                              GetUsageCallback* callback) OVERRIDE;
  virtual void GetOriginsForType(StorageType type,
                                 GetOriginsCallback* callback) OVERRIDE;
  virtual void GetOriginsForHost(StorageType type, const std::string& host,
                                 GetOriginsCallback* callback) OVERRIDE;

 private:
  void RunGetOriginUsage(const GURL& origin_url,
                         StorageType type,
                         GetUsageCallback* callback);
  void RunGetOriginsForType(StorageType type,
                            GetOriginsCallback* callback);
  void RunGetOriginsForHost(StorageType type,
                            const std::string& host,
                            GetOriginsCallback* callback);

  scoped_refptr<QuotaManagerProxy> quota_manager_proxy_;
  const ID id_;

  struct MockOriginData {
    MockOriginData(StorageType type, int64 usage) : type(type), usage(usage) { }
    StorageType type;
    int64 usage;
  };
  std::map<GURL, MockOriginData> origin_data_;

  std::set<GetUsageCallback*> usage_callbacks_;
  std::set<GetOriginsCallback*> origins_callbacks_;

  ScopedRunnableMethodFactory<MockStorageClient> runnable_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockStorageClient);
};

}  // namespace quota

#endif  // WEBKIT_QUOTA_MOCK_STORAGE_H_
