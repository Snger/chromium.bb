// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/path_service.h"
#include "chrome/browser/extensions/extension_apitest.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/base/mock_host_resolver.h"

// http://crbug.com/111165
#if defined(OS_WIN)
#define MAYBE_WebSocket FAILS_WebSocket
#else
#define MAYBE_WebSocket WebSocket
#endif
IN_PROC_BROWSER_TEST_F(ExtensionApiTest, MAYBE_WebSocket) {
  FilePath websocket_root_dir;
  ASSERT_TRUE(PathService::Get(chrome::DIR_LAYOUT_TESTS, &websocket_root_dir));
  // TODO(toyoshim): Remove following logging after a bug investigation.
  // http://crbug.com/107836 .
  LOG(INFO) << "Assume LayoutTests in " << websocket_root_dir.MaybeAsASCII();

  ui_test_utils::TestWebSocketServer server;
  ASSERT_TRUE(server.Start(websocket_root_dir));
  ASSERT_TRUE(RunExtensionTest("websocket")) << message_;
}
