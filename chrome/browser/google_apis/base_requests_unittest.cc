// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/base_requests.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/google_apis/request_sender.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kValidJsonString[] = "{ \"test\": 123 }";
const char kInvalidJsonString[] = "$$$";

class FakeGetDataRequest : public GetDataRequest {
 public:
  explicit FakeGetDataRequest(RequestSender* sender,
                              const GetDataCallback& callback)
      : GetDataRequest(sender, callback) {
  }

  virtual ~FakeGetDataRequest() {
  }

 protected:
  virtual GURL GetURL() const OVERRIDE {
    NOTREACHED();  // This method is not called in tests.
    return GURL();
  }
};

}  // namespace

class BaseRequestsTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);
    sender_.reset(new RequestSender(profile_.get(),
                                    NULL /* url_request_context_getter */,
                                    std::vector<std::string>() /* scopes */,
                                    std::string() /* custom user agent */));
    sender_->Initialize();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  scoped_ptr<RequestSender> sender_;
};

TEST_F(BaseRequestsTest, ParseValidJson) {
  scoped_ptr<base::Value> json;
  ParseJson(kValidJsonString,
            base::Bind(test_util::CreateCopyResultCallback(&json)));
  // Should wait for a blocking pool task, as the JSON parsing is done in the
  // blocking pool.
  test_util::RunBlockingPoolTask();

  DictionaryValue* root_dict = NULL;
  ASSERT_TRUE(json);
  ASSERT_TRUE(json->GetAsDictionary(&root_dict));

  int int_value = 0;
  ASSERT_TRUE(root_dict->GetInteger("test", &int_value));
  EXPECT_EQ(123, int_value);
}

TEST_F(BaseRequestsTest, ParseInvalidJson) {
  // Initialize with a valid pointer to verify that null is indeed assigned.
  scoped_ptr<base::Value> json(base::Value::CreateNullValue());
  ParseJson(kInvalidJsonString,
            base::Bind(test_util::CreateCopyResultCallback(&json)));
  // Should wait for a blocking pool task, as the JSON parsing is done in the
  // blocking pool.
  test_util::RunBlockingPoolTask();

  EXPECT_FALSE(json);
}

TEST_F(BaseRequestsTest, GetDataRequestParseValidResponse) {
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> value;
  FakeGetDataRequest* get_data_request =
      new FakeGetDataRequest(
          sender_.get(),
          base::Bind(test_util::CreateCopyResultCallback(&error, &value)));

  get_data_request->ParseResponse(HTTP_SUCCESS, kValidJsonString);
  // Should wait for a blocking pool task, as the JSON parsing is done in the
  // blocking pool.
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_TRUE(value);
}

TEST_F(BaseRequestsTest, GetDataRequestParseInvalidResponse) {
  GDataErrorCode error = GDATA_OTHER_ERROR;
  scoped_ptr<base::Value> value;
  FakeGetDataRequest* get_data_request =
      new FakeGetDataRequest(
          sender_.get(),
          base::Bind(test_util::CreateCopyResultCallback(&error, &value)));

  get_data_request->ParseResponse(HTTP_SUCCESS, kInvalidJsonString);
  // Should wait for a blocking pool task, as the JSON parsing is done in the
  // blocking pool.
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(GDATA_PARSE_ERROR, error);
  EXPECT_FALSE(value);
}

}  // namespace google_apis
