// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_url_request_info.h"

namespace {

void EmptyCompletionCallback(void* /*data*/, int32_t /*result*/) {
}

// Calls PPB_Core::CallOnMainThread(). To be invoked off the main thread.
void* InvokeCallOnMainThread(void* thread_argument) {
  PPB_Core* ppb_core = reinterpret_cast<PPB_Core*>(thread_argument);
  PP_CompletionCallback callback = MakeTestableCompletionCallback(
      "CallOnMainThreadCallback_FromNonMainThread",
      EmptyCompletionCallback,
      NULL /*user_data*/);
  ppb_core->CallOnMainThread(0 /*delay*/, callback, PP_OK);
  return NULL;
}

const int kStressChecksum = 0x12345678;

struct StressData {
  const PPB_Core* ppb_core_;
  const int callbacks_per_thread_;
  int callback_counter_;
  const int checksum_;
  StressData(const PPB_Core* ppb_core, int callbacks_per_thread, int total)
      : ppb_core_(ppb_core),
        callbacks_per_thread_(callbacks_per_thread),
        callback_counter_(total),
        checksum_(kStressChecksum) {
  }
};

// When passed in stress->callback_counter_ reaches zero, notify JS via
// MakeTestableCompletionCallback.
void ThreadStressCompletionCallback(void* data, int32_t result) {
  if (PP_OK == result) {
    StressData* stress = reinterpret_cast<StressData*>(data);
    CHECK(kStressChecksum == stress->checksum_);
    CHECK(NULL != stress->ppb_core_);
    stress->callback_counter_ -= 1;
    if (0 == stress->callback_counter_) {
      // All the callbacks triggered, so now report back that this test passed.
      PP_CompletionCallback callback = MakeTestableCompletionCallback(
          "CallOnMainThreadCallback_ThreadStress",
          EmptyCompletionCallback, NULL);
      stress->ppb_core_->CallOnMainThread(0, callback, PP_OK);
      // At this point we're done with the structure, so set it to zero.
      // If anyone from here on out tries to access it, either the pointer
      // check or the checksum should trip. It is intentionally left on the
      // heap to prevent re-use of the memory.
      memset(stress, 0, sizeof(*stress));
    }
  }
}

// Calls PPB_Core::CallOnMainThread(). To be invoked off the main thread.
// This is a stess test version.
void* InvokeCallOnMainThreadStress(void* thread_argument) {
  StressData* stress = reinterpret_cast<StressData*>(thread_argument);
  PP_CompletionCallback callback = PP_MakeCompletionCallback(
      ThreadStressCompletionCallback, stress);
  for (int i = 0; i < stress->callbacks_per_thread_; ++i) {
    CHECK(NULL != stress->ppb_core_);
    CHECK(kStressChecksum == stress->checksum_);
    stress->ppb_core_->CallOnMainThread(0, callback, PP_OK);
  }
  return NULL;
}

// Calls PPB_Core::IsMainThread(). To be invoked off the main thread.
void* InvokeIsMainThread(void* thread_argument) {
  PPB_Core* ppb_core = reinterpret_cast<PPB_Core*>(thread_argument);
  return reinterpret_cast<void*>(ppb_core->IsMainThread());
}

// Tests PPB_Core::GetTime().
PP_Var TestGetTime() {
  PP_Time time1 = PPBCore()->GetTime();
  EXPECT(time1 > 0);

  usleep(100000);  // 0.1 second

  PP_Time time2 = PPBCore()->GetTime();
  EXPECT(time2 > time1);

  return TEST_PASSED;
}

// Tests PPB_Core::GetTimeTicks().
PP_Var TestGetTimeTicks() {
  PP_TimeTicks time_ticks1 = PPBCore()->GetTimeTicks();
  EXPECT(time_ticks1 > 0);

  usleep(100000);  // 0.1 second

  PP_TimeTicks time_ticks2 = PPBCore()->GetTimeTicks();
  EXPECT(time_ticks2 > time_ticks1);

  return TEST_PASSED;
}

// Tests PPB_Core::CallOnMainThread() from the main thread.
PP_Var TestCallOnMainThreadFromMainThread() {
  PP_CompletionCallback callback = MakeTestableCompletionCallback(
      "CallOnMainThreadCallback_FromMainThread",
      EmptyCompletionCallback,
      NULL /*user_data*/);
  PPBCore()->CallOnMainThread(0 /*delay*/, callback, PP_OK);

  return TEST_PASSED;
}

// Tests PPB_Core::CallOnMainThread from non-main thread.
PP_Var TestCallOnMainThreadFromNonMainThread() {
  pthread_t tid;
  void* ppb_core = reinterpret_cast<void*>(const_cast<PPB_Core*>(PPBCore()));
  CHECK(pthread_create(&tid, NULL, InvokeCallOnMainThread, ppb_core) == 0);
  // Use a non-joined thread.  This is a more useful test than
  // joining the thread: we want to test CallOnMainThread() when it
  // is called concurrently with the main thread.
  CHECK(pthread_detach(tid) == 0);

  return TEST_PASSED;
}

// Tests PPB_Core::CallOnMainThread from non-main thread.
// This is a stress test version that calls many times from many threads.
PP_Var TestCallOnMainThreadFromNonMainThreadStress() {
  const int kNumThreads = 10;
  const int kNumPerThread = 100;
  const int kNumCallbacks = kNumThreads * kNumPerThread;
  StressData* stress = new StressData(PPBCore(), kNumPerThread, kNumCallbacks);
  for (int i = 0; i < kNumThreads; ++i) {
    pthread_t tid;
    CHECK(pthread_create(
        &tid, NULL, InvokeCallOnMainThreadStress, stress) == 0);
    CHECK(pthread_detach(tid) == 0);
  }
  return TEST_PASSED;
}

// Tests PPB_Core::IsMainThread() from the main thread.
PP_Var TestIsMainThreadFromMainThread() {
  EXPECT(PPBCore()->IsMainThread() == PP_TRUE);
  return TEST_PASSED;
}

// Tests PPB_Core::IsMainThread() from non-main thread.
PP_Var TestIsMainThreadFromNonMainThread() {
  pthread_t tid;
  void* thread_result;
  void* ppb_core = reinterpret_cast<void*>(const_cast<PPB_Core*>(PPBCore()));
  CHECK(pthread_create(&tid, NULL, InvokeIsMainThread, ppb_core) == 0);
  CHECK(pthread_join(tid, &thread_result) == 0);
  EXPECT(reinterpret_cast<int>(thread_result) == PP_FALSE);

  return TEST_PASSED;
}


// Tests PPB_Core::AddRefResource() and PPB_Core::ReleaseResource() with
// a valid resource.
PP_Var TestAddRefAndReleaseResource() {
  PP_Resource valid_resource = PPBURLRequestInfo()->Create(pp_instance());
  EXPECT(valid_resource != kInvalidResource);
  EXPECT(PPBURLRequestInfo()->IsURLRequestInfo(valid_resource) == PP_TRUE);

  // Adjusting ref count should not delete the resource.
  for (size_t j = 0; j < 100; ++j) PPBCore()->AddRefResource(valid_resource);
  EXPECT(PPBURLRequestInfo()->IsURLRequestInfo(valid_resource) == PP_TRUE);
  for (size_t j = 0; j < 100; ++j) PPBCore()->ReleaseResource(valid_resource);
  EXPECT(PPBURLRequestInfo()->IsURLRequestInfo(valid_resource) == PP_TRUE);

  // Releasing the ref count from Create() must delete the resource.
  PPBCore()->ReleaseResource(valid_resource);
  EXPECT(PPBURLRequestInfo()->IsURLRequestInfo(valid_resource) != PP_TRUE);

  return TEST_PASSED;
}

// Tests PPB_Core::AddRefResource() and PPB_Core::ReleaseResource() with
// an invalid resource.
PP_Var TestAddRefAndReleaseInvalidResource() {
  for (size_t j = 0; j < 100; ++j) {
    PPBCore()->AddRefResource(kInvalidResource);
    PPBCore()->ReleaseResource(kInvalidResource);
  }

  return TEST_PASSED;
}

}  // namespace

void SetupScriptableTests() {
  RegisterScriptableTest("testGetTime", TestGetTime);
  RegisterScriptableTest("testGetTimeTicks", TestGetTimeTicks);
  RegisterScriptableTest("testIsMainThread_FromMainThread",
                         TestIsMainThreadFromMainThread);
  RegisterScriptableTest("testIsMainThread_FromNonMainThread",
                         TestIsMainThreadFromNonMainThread);
  RegisterScriptableTest("testAddRefAndReleaseResource",
                         TestAddRefAndReleaseResource);
  RegisterScriptableTest("testAddRefAndReleaseInvalidResource",
                         TestAddRefAndReleaseInvalidResource);
  RegisterScriptableTest("testCallOnMainThread_FromMainThread",
                         TestCallOnMainThreadFromMainThread);
  RegisterScriptableTest("testCallOnMainThread_FromNonMainThread",
                         TestCallOnMainThreadFromNonMainThread);
  RegisterScriptableTest("testCallOnMainThread_FromNonMainThreadStress",
                         TestCallOnMainThreadFromNonMainThreadStress);
}

void SetupPluginInterfaces() {
  // none
}
