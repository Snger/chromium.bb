// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_SHELL_WEBKIT_TEST_RUNNER_HOST_H_
#define CONTENT_SHELL_WEBKIT_TEST_RUNNER_HOST_H_

#include <string>

#include "base/cancelable_callback.h"
#include "base/threading/non_thread_safe.h"
#include "content/public/browser/render_view_host_observer.h"
#include "content/public/browser/web_contents_observer.h"

class SkBitmap;

namespace content {

class Shell;

class WebKitTestController : public base::NonThreadSafe,
                             public WebContentsObserver {
 public:
  static WebKitTestController* Get();

  WebKitTestController();
  virtual ~WebKitTestController();

  void PrepareForLayoutTest(const GURL& test_url,
                            const std::string& expected_pixel_hash);
  // True if the controller was reset successfully.
  bool ResetAfterLayoutTest();

  const std::string& expected_pixel_hash() const {
    return expected_pixel_hash_;
  }
  bool should_stay_on_page_after_handling_before_unload() const {
    return should_stay_on_page_after_handling_before_unload_;
  }
  void set_should_stay_on_page_after_handling_before_unload(
      bool should_stay_on_page_after_handling_before_unload) {
    should_stay_on_page_after_handling_before_unload_ =
        should_stay_on_page_after_handling_before_unload;
  }
  bool dump_as_text() const { return dump_as_text_; }
  void set_dump_as_text(bool dump_as_text) { dump_as_text_ = dump_as_text; }
  bool dump_child_frames() const { return dump_child_frames_; }
  void set_dump_child_frames(bool dump_child_frames) {
    dump_child_frames_ = dump_child_frames;
  }
  bool is_printing() const { return is_printing_; }
  void set_is_printing(bool is_printing) { is_printing_ = is_printing; }

  void LoadFinished(Shell* window);
  void NotifyDone();
  void WaitUntilDone();
  void NotImplemented(const std::string& object_name,
                      const std::string& method_name);

 private:
  static WebKitTestController* instance_;

  // WebContentsObserver implementation.
  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE;

  void CaptureDump();
  void TimeoutHandler();

  Shell* main_window_;

  std::string expected_pixel_hash_;

  bool captured_dump_;

  bool dump_as_text_;
  bool dump_child_frames_;
  bool is_printing_;
  bool should_stay_on_page_after_handling_before_unload_;
  bool wait_until_done_;

  base::CancelableClosure watchdog_;

  DISALLOW_COPY_AND_ASSIGN(WebKitTestController);
};

class WebKitTestRunnerHost : public RenderViewHostObserver {
 public:
  explicit WebKitTestRunnerHost(RenderViewHost* render_view_host);
  virtual ~WebKitTestRunnerHost();

  // RenderViewHostObserver implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // Message handlers.
  void OnDidFinishLoad();
  void OnTextDump(const std::string& dump);
  void OnImageDump(const std::string& actual_pixel_hash, const SkBitmap& image);

  // testRunner handlers.
  void OnNotifyDone();
  void OnDumpAsText();
  void OnDumpChildFramesAsText();
  void OnSetPrinting();
  void OnSetShouldStayOnPageAfterHandlingBeforeUnload(bool should_stay_on_page);
  void OnWaitUntilDone();

  void OnNotImplemented(const std::string& object_name,
                        const std::string& method_name);

  DISALLOW_COPY_AND_ASSIGN(WebKitTestRunnerHost);
};

}  // namespace content

#endif  // CONTENT_SHELL_WEBKIT_TEST_RUNNER_HOST_H_
