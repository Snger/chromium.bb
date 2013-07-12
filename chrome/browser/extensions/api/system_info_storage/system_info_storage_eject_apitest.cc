// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// SystemInfoStorage eject API browser tests.

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/api/system_info_storage/storage_info_provider.h"
#include "chrome/browser/extensions/api/system_info_storage/test_storage_info_provider.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_process_manager.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/storage_monitor/storage_info.h"
#include "chrome/browser/storage_monitor/storage_monitor.h"
#include "chrome/browser/storage_monitor/test_storage_monitor.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/test/test_utils.h"

namespace {

using extensions::TestStorageUnitInfo;
using extensions::TestStorageInfoProvider;

struct TestStorageUnitInfo kRemovableStorageData[] = {
  { "dcim:device:0004", "transient:0004", "/media/usb1",
    extensions::systeminfo::kStorageTypeRemovable, 0, 0, 0}
};

}  // namespace

class SystemInfoStorageEjectApiTest : public ExtensionApiTest {
 public:
  SystemInfoStorageEjectApiTest() {}
  virtual ~SystemInfoStorageEjectApiTest() {}

 protected:
  // ExtensionApiTest overrides.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
  }

  content::RenderViewHost* GetHost() {
    const extensions::Extension* extension =
        LoadExtension(test_data_dir_.AppendASCII("systeminfo/storage_eject"));
    return extensions::ExtensionSystem::Get(browser()->profile())->
        process_manager()->GetBackgroundHostForExtension(extension->id())->
            render_view_host();
  }

  void ExecuteCmdAndCheckReply(content::RenderViewHost* host,
                               const std::string& js_command,
                               const std::string& ok_message) {
    ExtensionTestMessageListener listener(ok_message, false);
    host->ExecuteJavascriptInWebFrame(string16(), ASCIIToUTF16(js_command));
    EXPECT_TRUE(listener.WaitUntilSatisfied());
  }

  void Attach() {
    DCHECK(chrome::StorageMonitor::GetInstance()->IsInitialized());
    chrome::StorageMonitor::GetInstance()->receiver()->ProcessAttach(
        TestStorageInfoProvider::BuildStorageInfo(kRemovableStorageData[0]));
    content::RunAllPendingInMessageLoop();
  }

  void Detach() {
    DCHECK(chrome::StorageMonitor::GetInstance()->IsInitialized());
    chrome::StorageMonitor::GetInstance()->receiver()->ProcessDetach(
        kRemovableStorageData[0].device_id);
    content::RunAllPendingInMessageLoop();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemInfoStorageEjectApiTest);
};


IN_PROC_BROWSER_TEST_F(SystemInfoStorageEjectApiTest, EjectTest) {
  scoped_ptr<chrome::test::TestStorageMonitor> monitor(
      chrome::test::TestStorageMonitor::CreateForBrowserTests());
  monitor->Init();
  monitor->MarkInitialized();

  TestStorageInfoProvider* provider =
      new TestStorageInfoProvider(kRemovableStorageData,
                                  arraysize(kRemovableStorageData));
  extensions::StorageInfoProvider::InitializeForTesting(provider);

  content::RenderViewHost* host = GetHost();
  ExecuteCmdAndCheckReply(host, "addAttachListener()", "add_attach_ok");

  // Attach / detach
  const std::string expect_attach_msg =
      base::StringPrintf("%s,%s", "attach_test_ok",
                         kRemovableStorageData[0].name);
  ExtensionTestMessageListener attach_finished_listener(expect_attach_msg,
                                                        false  /* no reply */);
  Attach();
  EXPECT_TRUE(attach_finished_listener.WaitUntilSatisfied());

  ExecuteCmdAndCheckReply(host, "ejectTest()", "eject_ok");
  EXPECT_EQ(kRemovableStorageData[0].device_id, monitor->ejected_device());

  Detach();
}

IN_PROC_BROWSER_TEST_F(SystemInfoStorageEjectApiTest, EjectBadDeviceTest) {
  scoped_ptr<chrome::test::TestStorageMonitor> monitor(
      chrome::test::TestStorageMonitor::CreateForBrowserTests());
  monitor->Init();
  monitor->MarkInitialized();

  TestStorageInfoProvider* provider =
      new TestStorageInfoProvider(kRemovableStorageData,
                                  arraysize(kRemovableStorageData));
  extensions::StorageInfoProvider::InitializeForTesting(provider);

  ExecuteCmdAndCheckReply(GetHost(), "ejectFailTest()", "eject_no_such_device");

  EXPECT_EQ("", monitor->ejected_device());
}
