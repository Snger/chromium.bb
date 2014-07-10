// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/login/login_prompt.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/test_data_directory.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "url/gurl.h"

namespace {

class WebSocketBrowserTest : public InProcessBrowserTest {
 public:
  WebSocketBrowserTest()
      : ws_server_(net::SpawnedTestServer::TYPE_WS,
                   net::SpawnedTestServer::kLocalhost,
                   net::GetWebSocketTestDataDirectory()),
        wss_server_(net::SpawnedTestServer::TYPE_WSS,
                    SSLOptions(SSLOptions::CERT_OK),
                    net::GetWebSocketTestDataDirectory()) {}

 protected:
  // Prepare the title watcher.
  virtual void SetUpOnMainThread() OVERRIDE {
    watcher_.reset(new content::TitleWatcher(
        browser()->tab_strip_model()->GetActiveWebContents(),
        base::ASCIIToUTF16("PASS")));
    watcher_->AlsoWaitForTitle(base::ASCIIToUTF16("FAIL"));
  }

  virtual void CleanUpOnMainThread() OVERRIDE { watcher_.reset(); }

  std::string WaitAndGetTitle() {
    return base::UTF16ToUTF8(watcher_->WaitAndGetTitle());
  }

  net::SpawnedTestServer ws_server_;
  net::SpawnedTestServer wss_server_;

 private:
  typedef net::SpawnedTestServer::SSLOptions SSLOptions;
  scoped_ptr<content::TitleWatcher> watcher_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketBrowserTest);
};

// Framework for tests using the connect_to.html page served by a separate HTTP
// server.
class WebSocketBrowserConnectToTest : public WebSocketBrowserTest {
 protected:
  WebSocketBrowserConnectToTest()
      : http_server_(net::SpawnedTestServer::TYPE_HTTP,
                     net::SpawnedTestServer::kLocalhost,
                     net::GetWebSocketTestDataDirectory()) {}

  // The title watcher and HTTP server are set up automatically by the test
  // framework. Each test case still needs to configure and start the
  // WebSocket server(s) it needs.
  virtual void SetUpOnMainThread() OVERRIDE {
    WebSocketBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(http_server_.StartInBackground());
  }

  // Supply a ws: or wss: URL to connect to.
  void ConnectTo(GURL url) {
    ASSERT_TRUE(http_server_.BlockUntilStarted());
    std::string query("url=" + url.spec());
    GURL::Replacements replacements;
    replacements.SetQueryStr(query);
    ui_test_utils::NavigateToURL(browser(),
                                 http_server_.GetURL("files/connect_to.html")
                                     .ReplaceComponents(replacements));
  }

 private:
  net::SpawnedTestServer http_server_;
};

// Automatically fill in any login prompts that appear with the supplied
// credentials.
class AutoLogin : public content::NotificationObserver {
 public:
  AutoLogin(const std::string& username,
            const std::string& password,
            content::NavigationController* navigation_controller)
      : username_(base::UTF8ToUTF16(username)),
        password_(base::UTF8ToUTF16(password)),
        logged_in_(false) {
    registrar_.Add(
        this,
        chrome::NOTIFICATION_AUTH_NEEDED,
        content::Source<content::NavigationController>(navigation_controller));
  }

  // NotificationObserver implementation
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK_EQ(chrome::NOTIFICATION_AUTH_NEEDED, type);
    scoped_refptr<LoginHandler> login_handler =
        content::Details<LoginNotificationDetails>(details)->handler();
    login_handler->SetAuth(username_, password_);
    logged_in_ = true;
  }

  bool logged_in() const { return logged_in_; }

 private:
  const base::string16 username_;
  const base::string16 password_;
  bool logged_in_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(AutoLogin);
};

// Test that the browser can handle a WebSocket frame split into multiple TCP
// segments.
IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, WebSocketSplitSegments) {
  // Launch a WebSocket server.
  ASSERT_TRUE(ws_server_.Start());

  // Visit a HTTP page for testing.
  std::string scheme("http");
  GURL::Replacements replacements;
  replacements.SetSchemeStr(scheme);
  ui_test_utils::NavigateToURL(
      browser(),
      ws_server_.GetURL(
          "split_packet_check.html").ReplaceComponents(replacements));

  EXPECT_EQ("PASS", WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, SecureWebSocketSplitRecords) {
  // Launch a secure WebSocket server.
  ASSERT_TRUE(wss_server_.Start());

  // Visit a HTTPS page for testing.
  std::string scheme("https");
  GURL::Replacements replacements;
  replacements.SetSchemeStr(scheme);
  ui_test_utils::NavigateToURL(
      browser(),
      wss_server_.GetURL(
          "split_packet_check.html").ReplaceComponents(replacements));

  EXPECT_EQ("PASS", WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, WebSocketBasicAuthInHTTPURL) {
  // Launch a basic-auth-protected WebSocket server.
  ws_server_.set_websocket_basic_auth(true);
  ASSERT_TRUE(ws_server_.Start());

  // Visit a HTTP page for testing.
  std::string scheme("http");
  GURL::Replacements replacements;
  replacements.SetSchemeStr(scheme);
  ui_test_utils::NavigateToURL(
      browser(),
      ws_server_.GetURLWithUserAndPassword("connect_check.html", "test", "test")
          .ReplaceComponents(replacements));

  EXPECT_EQ("PASS", WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, WebSocketBasicAuthInHTTPSURL) {
  // Launch a basic-auth-protected secure WebSocket server.
  wss_server_.set_websocket_basic_auth(true);
  ASSERT_TRUE(wss_server_.Start());

  // Visit a HTTPS page for testing.
  std::string scheme("https");
  GURL::Replacements replacements;
  replacements.SetSchemeStr(scheme);
  ui_test_utils::NavigateToURL(
      browser(),
      wss_server_.GetURLWithUserAndPassword(
                      "connect_check.html", "test", "test")
          .ReplaceComponents(replacements));

  EXPECT_EQ("PASS", WaitAndGetTitle());
}

// This test verifies that login details entered by the user into the login
// prompt to authenticate the main page are re-used for WebSockets from the same
// origin.
IN_PROC_BROWSER_TEST_F(WebSocketBrowserTest, WebSocketBasicAuthPrompt) {
  // Launch a basic-auth-protected WebSocket server.
  ws_server_.set_websocket_basic_auth(true);
  ASSERT_TRUE(ws_server_.Start());

  content::NavigationController* navigation_controller =
      &browser()->tab_strip_model()->GetActiveWebContents()->GetController();
  AutoLogin auto_login("test", "test", navigation_controller);

  // Visit a HTTP page for testing.
  std::string scheme("http");
  GURL::Replacements replacements;
  replacements.SetSchemeStr(scheme);
  ui_test_utils::NavigateToURL(
      browser(),
      ws_server_.GetURL("connect_check.html").ReplaceComponents(replacements));

  EXPECT_TRUE(auto_login.logged_in());
  EXPECT_EQ("PASS", WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(WebSocketBrowserConnectToTest,
                       WebSocketBasicAuthInWSURL) {
  // Launch a basic-auth-protected WebSocket server.
  ws_server_.set_websocket_basic_auth(true);
  ASSERT_TRUE(ws_server_.Start());

  ConnectTo(ws_server_.GetURLWithUserAndPassword(
      "echo-with-no-extension", "test", "test"));

  EXPECT_EQ("PASS", WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(WebSocketBrowserConnectToTest,
                       WebSocketBasicAuthInWSURLBadCreds) {
  // Launch a basic-auth-protected WebSocket server.
  ws_server_.set_websocket_basic_auth(true);
  ASSERT_TRUE(ws_server_.Start());

  ConnectTo(ws_server_.GetURLWithUserAndPassword(
      "echo-with-no-extension", "wrong-user", "wrong-password"));

  EXPECT_EQ("FAIL", WaitAndGetTitle());
}

IN_PROC_BROWSER_TEST_F(WebSocketBrowserConnectToTest,
                       WebSocketBasicAuthNoCreds) {
  // Launch a basic-auth-protected WebSocket server.
  ws_server_.set_websocket_basic_auth(true);
  ASSERT_TRUE(ws_server_.Start());

  ConnectTo(ws_server_.GetURL("echo-with-no-extension"));

  EXPECT_EQ("FAIL", WaitAndGetTitle());
}

}  // namespace
