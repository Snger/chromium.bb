// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_NETWORK_MONITOR_PRIVATE_H_
#define PAPPI_TESTS_TEST_NETWORK_MONITOR_PRIVATE_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/tests/test_case.h"

class TestNetworkMonitorPrivate : public TestCase {
 public:
  explicit TestNetworkMonitorPrivate(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestBasic();
  std::string Test2Monitors();
  std::string TestDeleteInCallback();

  std::string VerifyNetworkList(PP_Resource network_resource);
};

#endif  // PAPPI_TESTS_TEST_NETWORK_MONITOR_PRIVATE_H_
