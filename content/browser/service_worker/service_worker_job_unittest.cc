// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/service_worker/service_worker_job_coordinator.h"
#include "content/browser/service_worker/service_worker_registration.h"
#include "content/browser/service_worker/service_worker_registration_status.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

// Unit tests for testing all job registration tasks.
namespace content {

namespace {

void SaveRegistrationCallback(
    ServiceWorkerRegistrationStatus expected_status,
    bool* called,
    scoped_refptr<ServiceWorkerRegistration>* registration,
    ServiceWorkerRegistrationStatus status,
    const scoped_refptr<ServiceWorkerRegistration>& result) {
  EXPECT_EQ(expected_status, status);
  *called = true;
  *registration = result;
}

void SaveFoundRegistrationCallback(
    bool expected_found,
    ServiceWorkerRegistrationStatus expected_status,
    bool* called,
    scoped_refptr<ServiceWorkerRegistration>* registration,
    bool found,
    ServiceWorkerRegistrationStatus status,
    const scoped_refptr<ServiceWorkerRegistration>& result) {
  EXPECT_EQ(expected_found, found);
  EXPECT_EQ(expected_status, status);
  *called = true;
  *registration = result;
}

// Creates a callback which both keeps track of if it's been called,
// as well as the resulting registration. Whent the callback is fired,
// it ensures that the resulting status matches the expectation.
// 'called' is useful for making sure a sychronous callback is or
// isn't called.
ServiceWorkerRegisterJob::RegistrationCallback SaveRegistration(
    ServiceWorkerRegistrationStatus expected_status,
    bool* called,
    scoped_refptr<ServiceWorkerRegistration>* registration) {
  *called = false;
  return base::Bind(
      &SaveRegistrationCallback, expected_status, called, registration);
}

ServiceWorkerStorage::FindRegistrationCallback SaveFoundRegistration(
    bool expected_found,
    ServiceWorkerRegistrationStatus expected_status,
    bool* called,
    scoped_refptr<ServiceWorkerRegistration>* registration) {
  *called = false;
  return base::Bind(&SaveFoundRegistrationCallback,
                    expected_found,
                    expected_status,
                    called,
                    registration);
}

void SaveUnregistrationCallback(ServiceWorkerRegistrationStatus expected_status,
                                bool* called,
                                ServiceWorkerRegistrationStatus status) {
  EXPECT_EQ(expected_status, status);
  *called = true;
}

ServiceWorkerRegisterJob::UnregistrationCallback SaveUnregistration(
    ServiceWorkerRegistrationStatus expected_status,
    bool* called) {
  *called = false;
  return base::Bind(&SaveUnregistrationCallback, expected_status, called);
}

}  // namespace

class ServiceWorkerJobTest : public testing::Test {
 public:
  ServiceWorkerJobTest()
      : browser_thread_bundle_(TestBrowserThreadBundle::IO_MAINLOOP) {}

  virtual void SetUp() OVERRIDE {
    storage_.reset(new ServiceWorkerStorage(base::FilePath(), NULL));
    job_coordinator_.reset(new ServiceWorkerJobCoordinator(storage_.get()));
  }

  virtual void TearDown() OVERRIDE { storage_.reset(); }

 protected:
  TestBrowserThreadBundle browser_thread_bundle_;
  scoped_ptr<ServiceWorkerStorage> storage_;
  scoped_ptr<ServiceWorkerJobCoordinator> job_coordinator_;
};

TEST_F(ServiceWorkerJobTest, SameDocumentSameRegistration) {
  scoped_refptr<ServiceWorkerRegistration> original_registration;
  bool called;
  job_coordinator_->Register(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      SaveRegistration(REGISTRATION_OK, &called, &original_registration));
  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  scoped_refptr<ServiceWorkerRegistration> registration1;
  storage_->FindRegistrationForDocument(
      GURL("http://www.example.com/"),
      SaveFoundRegistration(true, REGISTRATION_OK, &called, &registration1));
  scoped_refptr<ServiceWorkerRegistration> registration2;
  storage_->FindRegistrationForDocument(
      GURL("http://www.example.com/"),
      SaveFoundRegistration(true, REGISTRATION_OK, &called, &registration2));

  ServiceWorkerRegistration* null_registration(NULL);
  ASSERT_EQ(null_registration, registration1);
  ASSERT_EQ(null_registration, registration2);
  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  ASSERT_NE(null_registration, registration1);
  ASSERT_NE(null_registration, registration2);

  ASSERT_EQ(registration1, registration2);
}

TEST_F(ServiceWorkerJobTest, SameMatchSameRegistration) {
  bool called;
  scoped_refptr<ServiceWorkerRegistration> original_registration;
  job_coordinator_->Register(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      SaveRegistration(REGISTRATION_OK, &called, &original_registration));
  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);
  ASSERT_NE(static_cast<ServiceWorkerRegistration*>(NULL),
            original_registration.get());

  scoped_refptr<ServiceWorkerRegistration> registration1;
  storage_->FindRegistrationForDocument(
      GURL("http://www.example.com/one"),
      SaveFoundRegistration(true, REGISTRATION_OK, &called, &registration1));

  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  scoped_refptr<ServiceWorkerRegistration> registration2;
  storage_->FindRegistrationForDocument(
      GURL("http://www.example.com/two"),
      SaveFoundRegistration(true, REGISTRATION_OK, &called, &registration2));
  EXPECT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called);

  ASSERT_EQ(registration1, registration2);
}

TEST_F(ServiceWorkerJobTest, DifferentMatchDifferentRegistration) {
  bool called1;
  scoped_refptr<ServiceWorkerRegistration> original_registration1;
  job_coordinator_->Register(
      GURL("http://www.example.com/one/*"),
      GURL("http://www.example.com/service_worker.js"),
      SaveRegistration(REGISTRATION_OK, &called1, &original_registration1));

  bool called2;
  scoped_refptr<ServiceWorkerRegistration> original_registration2;
  job_coordinator_->Register(
      GURL("http://www.example.com/two/*"),
      GURL("http://www.example.com/service_worker.js"),
      SaveRegistration(REGISTRATION_OK, &called2, &original_registration2));

  EXPECT_FALSE(called1);
  EXPECT_FALSE(called2);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called2);
  EXPECT_TRUE(called1);

  scoped_refptr<ServiceWorkerRegistration> registration1;
  storage_->FindRegistrationForDocument(
      GURL("http://www.example.com/one/"),
      SaveFoundRegistration(true, REGISTRATION_OK, &called1, &registration1));
  scoped_refptr<ServiceWorkerRegistration> registration2;
  storage_->FindRegistrationForDocument(
      GURL("http://www.example.com/two/"),
      SaveFoundRegistration(true, REGISTRATION_OK, &called2, &registration2));

  EXPECT_FALSE(called1);
  EXPECT_FALSE(called2);
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(called2);
  EXPECT_TRUE(called1);

  ASSERT_NE(registration1, registration2);
}

// Make sure basic registration is working.
TEST_F(ServiceWorkerJobTest, Register) {
  bool called = false;
  scoped_refptr<ServiceWorkerRegistration> registration;
  job_coordinator_->Register(
      GURL("http://www.example.com/*"),
      GURL("http://www.example.com/service_worker.js"),
      SaveRegistration(REGISTRATION_OK, &called, &registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_NE(scoped_refptr<ServiceWorkerRegistration>(NULL), registration);
}

// Make sure registrations are cleaned up when they are unregistered.
TEST_F(ServiceWorkerJobTest, Unregister) {
  GURL pattern("http://www.example.com/*");

  bool called;
  scoped_refptr<ServiceWorkerRegistration> registration;
  job_coordinator_->Register(
      pattern,
      GURL("http://www.example.com/service_worker.js"),
      SaveRegistration(REGISTRATION_OK, &called, &registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  job_coordinator_->Unregister(pattern,
                               SaveUnregistration(REGISTRATION_OK, &called));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_TRUE(registration->HasOneRef());

  storage_->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(false, REGISTRATION_OK, &called, &registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(scoped_refptr<ServiceWorkerRegistration>(NULL), registration);
}

// Make sure that when a new registration replaces an existing
// registration, that the old one is cleaned up.
TEST_F(ServiceWorkerJobTest, RegisterNewScript) {
  GURL pattern("http://www.example.com/*");

  bool called;
  scoped_refptr<ServiceWorkerRegistration> old_registration;
  job_coordinator_->Register(
      pattern,
      GURL("http://www.example.com/service_worker.js"),
      SaveRegistration(REGISTRATION_OK, &called, &old_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  scoped_refptr<ServiceWorkerRegistration> old_registration_by_pattern;
  storage_->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          true, REGISTRATION_OK, &called, &old_registration_by_pattern));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(old_registration, old_registration_by_pattern);
  old_registration_by_pattern = NULL;

  scoped_refptr<ServiceWorkerRegistration> new_registration;
  job_coordinator_->Register(
      pattern,
      GURL("http://www.example.com/service_worker_new.js"),
      SaveRegistration(REGISTRATION_OK, &called, &new_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_TRUE(old_registration->HasOneRef());

  ASSERT_NE(old_registration, new_registration);

  scoped_refptr<ServiceWorkerRegistration> new_registration_by_pattern;
  storage_->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(true, REGISTRATION_OK, &called, &new_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_NE(new_registration_by_pattern, old_registration);
}

// Make sure that when registering a duplicate pattern+script_url
// combination, that the same registration is used.
TEST_F(ServiceWorkerJobTest, RegisterDuplicateScript) {
  GURL pattern("http://www.example.com/*");
  GURL script_url("http://www.example.com/service_worker.js");

  bool called;
  scoped_refptr<ServiceWorkerRegistration> old_registration;
  job_coordinator_->Register(
      pattern,
      script_url,
      SaveRegistration(REGISTRATION_OK, &called, &old_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  scoped_refptr<ServiceWorkerRegistration> old_registration_by_pattern;
  storage_->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          true, REGISTRATION_OK, &called, &old_registration_by_pattern));
  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_TRUE(old_registration_by_pattern);

  scoped_refptr<ServiceWorkerRegistration> new_registration;
  job_coordinator_->Register(
      pattern,
      script_url,
      SaveRegistration(REGISTRATION_OK, &called, &new_registration));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(old_registration, new_registration);

  ASSERT_FALSE(old_registration->HasOneRef());

  scoped_refptr<ServiceWorkerRegistration> new_registration_by_pattern;
  storage_->FindRegistrationForPattern(
      pattern,
      SaveFoundRegistration(
          true, REGISTRATION_OK, &called, &new_registration_by_pattern));

  ASSERT_FALSE(called);
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(called);

  ASSERT_EQ(new_registration, old_registration);
}

}  // namespace content
