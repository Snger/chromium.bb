// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/extensions/api/serial/serial_api.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/extensions/extension_test_message_listener.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

using namespace extension_function_test_utils;

namespace {

class SerialApiTest : public ExtensionApiTest {
 public:
  SerialApiTest() {}

  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    ExtensionApiTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kEnableExperimentalExtensionApis);
    command_line->AppendSwitch(switches::kEnablePlatformApps);
  }
};

}  // namespace

IN_PROC_BROWSER_TEST_F(SerialApiTest, SerialExtension) {
  ResultCatcher catcher;
  catcher.RestrictToProfile(browser()->profile());

  ExtensionTestMessageListener listener("serial_port", true);

  ASSERT_TRUE(LoadExtension(test_data_dir_.AppendASCII("serial/api")));
  EXPECT_TRUE(listener.WaitUntilSatisfied());

#if 0
  // Enable this path only if all the following are true:
  //
  // 1. You're running Linux.
  //
  // 2. You have an Adafruit ATmega32u4 breakout board attached to your machine
  // via USB with the Arduino Leonardo bootloader flashed to the board. Other
  // devices will work; this is the only one tested.
  //
  // 3. Your user has permission to read/write the /dev/ttyACM0 device.
  //
  // 4. You have uploaded a program to the '32u4 that does a byte-for-byte echo
  // on the virtual serial port at 57600 bps. Here is an example (built using
  // the Arduino IDE):
  //
  // void setup() {
  //   Serial.begin(57600);
  // }
  //
  // void loop() {
  //   while (true) {
  //     while (Serial.available() > 0) {
  //       Serial.print((char)Serial.read());
  //     }
  //   }
  // }
  //
  // TODO(miket): Enable a more forgiving set of test conditions, specifically
  // by mocking SerialConnection.
  listener.Reply("/dev/ttyACM0");
#else
  listener.Reply("none");
#endif

  EXPECT_TRUE(catcher.GetNextResult()) << catcher.message();
}
