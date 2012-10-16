// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test_utils.h"
#include "content/shell/shell.h"
#include "content/test/content_browser_test.h"
#include "content/test/content_browser_test_utils.h"
#include "googleurl/src/gurl.h"
#include "webkit/media/crypto/key_systems.h"

// Platform-specific filename relative to the chrome executable.
#if defined(OS_WIN)
static const wchar_t kLibraryName[] = L"clearkeycdmplugin.dll";
#elif defined(OS_MACOSX)
static const char kLibraryName[] = "clearkeycdmplugin.plugin";
#elif defined(OS_POSIX)
static const char kLibraryName[] = "libclearkeycdmplugin.so";
#endif

// Available key systems.
static const char kClearKeyKeySystem[] = "webkit-org.w3.clearkey";
static const char kExternalClearKeyKeySystem[] =
    "org.chromium.externalclearkey";


class EncryptedMediaTest
    : public testing::WithParamInterface<const char*>,
      public content::ContentBrowserTest {
 public:
  void PlayMedia(const char* key_system, const string16 expectation) {
    // TODO(shadi): Add non-HTTP tests once src is supported for EME.
    ASSERT_TRUE(test_server()->Start());

    const string16 kError = ASCIIToUTF16("ERROR");
    const string16 kFailed = ASCIIToUTF16("FAILED");
    GURL player_gurl = test_server()->GetURL(base::StringPrintf(
       "files/media/encrypted_media_player.html?keysystem=%s", key_system));
    content::TitleWatcher title_watcher(shell()->web_contents(), expectation);
    title_watcher.AlsoWaitForTitle(kError);
    title_watcher.AlsoWaitForTitle(kFailed);

    content::NavigateToURL(shell(), player_gurl);

    string16 final_title = title_watcher.WaitAndGetTitle();
    EXPECT_EQ(expectation, final_title);

    if (final_title == kFailed) {
      std::string fail_message;
      EXPECT_TRUE(content::ExecuteJavaScriptAndExtractString(
          shell()->web_contents()->GetRenderViewHost(), L"",
          L"window.domAutomationController.send(failMessage);", &fail_message));
      LOG(INFO) << "Test failed: " << fail_message;
    }
  }

 protected:
  // Registers any CDM plugins not registered by default.
  virtual void SetUpCommandLine(CommandLine* command_line) OVERRIDE {
    command_line->AppendSwitch(switches::kDisableAudio);
    command_line->AppendSwitch(switches::kEnableEncryptedMedia);
    command_line->AppendSwitch(switches::kPpapiOutOfProcess);

    // Append the switch to register the Clear Key CDM plugin.
    FilePath plugin_dir;
    EXPECT_TRUE(PathService::Get(base::DIR_MODULE, &plugin_dir));
    FilePath plugin_lib = plugin_dir.Append(kLibraryName);
    EXPECT_TRUE(file_util::PathExists(plugin_lib));
    FilePath::StringType pepper_plugin = plugin_lib.value();
    pepper_plugin.append(FILE_PATH_LITERAL(
        "#Clear Key CDM#Clear Key CDM 0.1.0.0#0.1.0.0;"));
#if defined(OS_WIN)
      pepper_plugin.append(ASCIIToWide(
          webkit_media::GetPluginType(kExternalClearKeyKeySystem)));
#else
      pepper_plugin.append(
          webkit_media::GetPluginType(kExternalClearKeyKeySystem));
#endif
    command_line->AppendSwitchNative(switches::kRegisterPepperPlugins,
                                     pepper_plugin);
  }
};

// Fails on Linux/ChromeOS with ASan.  http://crbug.com/153231
#if (defined(OS_LINUX) || defined(OS_CHROMEOS)) && defined(ADDRESS_SANITIZER)
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, DISABLED_BasicPlayback) {
  const string16 kExpected = ASCIIToUTF16("ENDED");
  ASSERT_NO_FATAL_FAILURE(PlayMedia(GetParam(), kExpected));
}
#else
IN_PROC_BROWSER_TEST_P(EncryptedMediaTest, BasicPlayback) {
  const string16 kExpected = ASCIIToUTF16("ENDED");
  ASSERT_NO_FATAL_FAILURE(PlayMedia(GetParam(), kExpected));
}
#endif

IN_PROC_BROWSER_TEST_F(EncryptedMediaTest, InvalidKeySystem) {
  const string16 kExpected = ASCIIToUTF16(
      StringToUpperASCII(std::string("GenerateKeyRequestException")));
  ASSERT_NO_FATAL_FAILURE(PlayMedia("com.example.invalid", kExpected));
}

INSTANTIATE_TEST_CASE_P(ClearKey, EncryptedMediaTest,
                        ::testing::Values(kClearKeyKeySystem));

// http://crbug.com/152864
#if !defined(OS_MACOSX)
INSTANTIATE_TEST_CASE_P(ExternalClearKey, EncryptedMediaTest,
                        ::testing::Values(kExternalClearKeyKeySystem));
#endif
