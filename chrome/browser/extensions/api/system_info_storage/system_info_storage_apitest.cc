// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "base/command_line.h"
#include "base/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/timer/timer.h"
#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"
#include "chrome/browser/extensions/api/system_info_storage/test_storage_info_provider.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chrome/browser/storage_monitor/test_storage_monitor.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/ui_test_utils.h"

namespace {

using chrome::StorageMonitor;
using chrome::test::TestStorageMonitor;
using extensions::api::experimental_system_info_storage::ParseStorageUnitType;
using extensions::api::experimental_system_info_storage::StorageUnitInfo;
using extensions::StorageInfoProvider;
using extensions::StorageInfo;
using extensions::systeminfo::kStorageTypeFixed;
using extensions::systeminfo::kStorageTypeRemovable;
using extensions::systeminfo::kStorageTypeUnknown;
using extensions::TestStorageUnitInfo;
using extensions::TestStorageInfoProvider;

struct TestStorageUnitInfo kTestingData[] = {
  {"dcim:device:0004", "transient:0004", "0xbeaf", kStorageTypeUnknown,
    4098, 1000, 0},
  {"path:device:002", "transient:002", "/home", kStorageTypeFixed,
    4098, 1000, 10},
  {"path:device:003", "transient:003", "/data", kStorageTypeFixed,
    10000, 1000, 4097}
};

struct TestStorageUnitInfo kRemovableStorageData[] = {
  {"dcim:device:0004", "transient:0004", "/media/usb1",
    kStorageTypeRemovable, 4098, 1000, 1}
};

}  // namespace

class SystemInfoStorageApiTest: public ExtensionApiTest {
 public:
  SystemInfoStorageApiTest() {}
  virtual ~SystemInfoStorageApiTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE {
    ExtensionApiTest::SetUpInProcessBrowserTestFixture();
    message_loop_.reset(new base::MessageLoop(base::MessageLoop::TYPE_UI));
  }

  void AttachRemovableStorage(const std::string& device_id) {
    size_t len = arraysize(kRemovableStorageData);
    for (size_t i = 0; i < len; ++i) {
      if (kRemovableStorageData[i].device_id != device_id)
        continue;

      StorageMonitor::GetInstance()->receiver()->ProcessAttach(
          TestStorageInfoProvider::BuildStorageInfo(kRemovableStorageData[i]));
    }
  }

  void DetachRemovableStorage(const std::string& id) {
    StorageMonitor::GetInstance()->receiver()->ProcessDetach(id);
  }

 private:
  scoped_ptr<base::MessageLoop> message_loop_;
};

IN_PROC_BROWSER_TEST_F(SystemInfoStorageApiTest, Storage) {
  TestStorageInfoProvider* provider =
      new TestStorageInfoProvider(kTestingData, arraysize(kTestingData));
  StorageInfoProvider::InitializeForTesting(provider);
  ASSERT_TRUE(RunPlatformAppTest("systeminfo/storage")) << message_;
}

IN_PROC_BROWSER_TEST_F(SystemInfoStorageApiTest, StorageAttachment) {
  scoped_ptr<TestStorageMonitor> monitor(
      TestStorageMonitor::CreateForBrowserTests());

  TestStorageInfoProvider* provider =
      new TestStorageInfoProvider(kRemovableStorageData,
                                  arraysize(kRemovableStorageData));
  StorageInfoProvider::InitializeForTesting(provider);

  ResultCatcher catcher;
  ExtensionTestMessageListener attach_listener("attach", false);
  ExtensionTestMessageListener detach_listener("detach", false);

  EXPECT_TRUE(LoadExtension(
      test_data_dir_.AppendASCII("systeminfo/storage_attachment")));

  // Simulate triggering onAttached event.
  ASSERT_TRUE(attach_listener.WaitUntilSatisfied());
  AttachRemovableStorage(kRemovableStorageData[0].device_id);
  // Simulate triggering onDetached event.
  ASSERT_TRUE(detach_listener.WaitUntilSatisfied());
  DetachRemovableStorage(kRemovableStorageData[0].device_id);

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
