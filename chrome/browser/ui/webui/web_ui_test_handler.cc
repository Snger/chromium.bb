// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/web_ui_test_handler.h"

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_details.h"
#include "content/common/notification_registrar.h"

void WebUITestHandler::PreloadJavaScript(const string16& js_text,
                                         RenderViewHost* preload_host) {
  DCHECK(preload_host);
  preload_host->Send(new ViewMsg_WebUIJavaScript(
      preload_host->routing_id(), string16(), js_text, 0,
      false));
}

void WebUITestHandler::RunJavaScript(const string16& js_text) {
  web_ui_->tab_contents()->render_view_host()->ExecuteJavascriptInWebFrame(
      string16(), js_text);
}

bool WebUITestHandler::RunJavaScriptTestWithResult(const string16& js_text) {
  RenderViewHost* rvh = web_ui_->tab_contents()->render_view_host();
  NotificationRegistrar notification_registrar;
  notification_registrar.Add(
      this, content::NOTIFICATION_EXECUTE_JAVASCRIPT_RESULT,
      Source<RenderViewHost>(rvh));
  rvh->ExecuteJavascriptInWebFrameNotifyResult(string16(), js_text);
  return WaitForResult();
}

void WebUITestHandler::Observe(int type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  // Quit the message loop if we were waiting so Waiting process can get result
  // or error. To ensure this gets done, do this before ASSERT* calls.
  if (is_waiting_)
    MessageLoopForUI::current()->Quit();

  SCOPED_TRACE("WebUITestHandler::Observe");
  Value* value = Details<std::pair<int, Value*> >(details)->second;
  ListValue* list_value;
  ASSERT_TRUE(value->GetAsList(&list_value));
  ASSERT_TRUE(list_value->GetBoolean(0, &test_succeeded_));
  if (!test_succeeded_) {
    std::string message;
    ASSERT_TRUE(list_value->GetString(1, &message));
    LOG(ERROR) << message;
  }
}

bool WebUITestHandler::WaitForResult() {
  is_waiting_ = true;
  ui_test_utils::RunMessageLoop();
  is_waiting_ = false;
  return test_succeeded_;
}
