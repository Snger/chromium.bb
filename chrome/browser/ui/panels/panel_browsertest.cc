// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "chrome/browser/download/download_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/browser/net/url_request_mock_http_job.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_MACOSX)
#include "chrome/browser/ui/cocoa/find_bar/find_bar_bridge.h"
#endif

class PanelBrowserTest : public InProcessBrowserTest {
 public:
  PanelBrowserTest() : InProcessBrowserTest() {
#if defined(OS_MACOSX)
    FindBarBridge::disable_animations_during_testing_ = true;
#endif
  }

  virtual void SetUpCommandLine(CommandLine* command_line) {
    command_line->AppendSwitch(switches::kEnablePanels);
  }

 protected:
  Panel* CreatePanel(const std::string& name, const gfx::Rect& bounds) {
    Browser* panel_browser = Browser::CreateForApp(Browser::TYPE_PANEL,
                                                   name,
                                                   bounds,
                                                   browser()->profile());
    EXPECT_TRUE(panel_browser->is_type_panel());

    TabContentsWrapper* tab_contents =
        new TabContentsWrapper(new TestTabContents(browser()->profile(), NULL));
    panel_browser->AddTab(tab_contents, PageTransition::LINK);

    Panel* panel = static_cast<Panel*>(panel_browser->window());
    panel->Show();
    return panel;
  }

  // Creates a testing extension.
  scoped_refptr<Extension> CreateExtension(const FilePath::StringType& path) {
#if defined(OS_WIN)
    FilePath full_path(FILE_PATH_LITERAL("c:\\"));
#else
    FilePath full_path(FILE_PATH_LITERAL("/"));
#endif
    full_path = full_path.Append(path);
    DictionaryValue input_value;
    input_value.SetString(extension_manifest_keys::kVersion, "1.0.0.0");
    input_value.SetString(extension_manifest_keys::kName, "Sample Extension");
    std::string error;
    scoped_refptr<Extension> extension = Extension::Create(
        full_path,  Extension::INVALID, input_value,
        Extension::STRICT_ERROR_CHECKS, &error);
    EXPECT_TRUE(extension.get());
    EXPECT_STREQ("", error.c_str());
    browser()->GetProfile()->GetExtensionService()->OnLoadSingleExtension(
        extension.get(), false);
    return extension;
  }

  void TestCreatePanelOnOverflow() {
    PanelManager* panel_manager = PanelManager::GetInstance();
    EXPECT_EQ(0, panel_manager->num_panels()); // No panels initially.

    // Specify the work area for testing purpose.
    panel_manager->SetWorkArea(gfx::Rect(0, 0, 800, 600));

    // Create testing extensions.
    scoped_refptr<Extension> extension1 =
        CreateExtension(FILE_PATH_LITERAL("extension1"));
    scoped_refptr<Extension> extension2 =
        CreateExtension(FILE_PATH_LITERAL("extension2"));
    scoped_refptr<Extension> extension3 =
        CreateExtension(FILE_PATH_LITERAL("extension3"));

    // First, create 3 panels.
    Panel* panel1 = CreatePanel(
        web_app::GenerateApplicationNameFromExtensionId(extension1->id()),
        gfx::Rect(0, 0, 250, 200));
    Panel* panel2 = CreatePanel(
        web_app::GenerateApplicationNameFromExtensionId(extension2->id()),
        gfx::Rect(0, 0, 300, 200));
    Panel* panel3 = CreatePanel(
        web_app::GenerateApplicationNameFromExtensionId(extension1->id()),
        gfx::Rect(0, 0, 200, 200));
    ASSERT_EQ(3, panel_manager->num_panels());

    // Test closing the left-most panel that is from same extension.
    ui_test_utils::WindowedNotificationObserver signal(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        Source<Browser>(panel2->browser()));
    Panel* panel4 = CreatePanel(
        web_app::GenerateApplicationNameFromExtensionId(extension2->id()),
        gfx::Rect(0, 0, 280, 200));
    signal.Wait();
    ASSERT_EQ(3, panel_manager->num_panels());
    EXPECT_LT(panel4->GetBounds().right(), panel3->GetBounds().x());
    EXPECT_LT(panel3->GetBounds().right(), panel1->GetBounds().x());

    // Test closing the left-most panel.
    ui_test_utils::WindowedNotificationObserver signal2(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        Source<Browser>(panel4->browser()));
    Panel* panel5 = CreatePanel(
        web_app::GenerateApplicationNameFromExtensionId(extension3->id()),
        gfx::Rect(0, 0, 300, 200));
    signal2.Wait();
    ASSERT_EQ(3, panel_manager->num_panels());
    EXPECT_LT(panel5->GetBounds().right(), panel3->GetBounds().x());
    EXPECT_LT(panel3->GetBounds().right(), panel1->GetBounds().x());

    // Test closing 2 left-most panels.
    ui_test_utils::WindowedNotificationObserver signal3(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        Source<Browser>(panel3->browser()));
    ui_test_utils::WindowedNotificationObserver signal4(
        chrome::NOTIFICATION_BROWSER_CLOSED,
        Source<Browser>(panel5->browser()));
    Panel* panel6 = CreatePanel(
        web_app::GenerateApplicationNameFromExtensionId(extension3->id()),
        gfx::Rect(0, 0, 500, 200));
    signal3.Wait();
    signal4.Wait();
    ASSERT_EQ(2, panel_manager->num_panels());
    EXPECT_LT(panel6->GetBounds().right(), panel1->GetBounds().x());

    panel1->Close();
    panel6->Close();
  }
};

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, CreatePanel) {
  PanelManager* panel_manager = PanelManager::GetInstance();
  EXPECT_EQ(0, panel_manager->num_panels()); // No panels initially.

  Panel* panel = CreatePanel("PanelTest", gfx::Rect());
  EXPECT_EQ(1, panel_manager->num_panels());

  gfx::Rect bounds = panel->GetBounds();
  EXPECT_GT(bounds.x(), 0);
  EXPECT_GT(bounds.y(), 0);
  EXPECT_GT(bounds.width(), 0);
  EXPECT_GT(bounds.height(), 0);

  panel->Close();
  EXPECT_EQ(0, panel_manager->num_panels());
}

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, FindBar) {
  Panel* panel = CreatePanel("PanelTest", gfx::Rect(0, 0, 400, 400));
  Browser* browser = panel->browser();
  browser->ShowFindBar();
  ASSERT_TRUE(browser->GetFindBarController()->find_bar()->IsFindBarVisible());
  panel->Close();
}

// TODO(jianli): Investigate and enable it for Mac.
#ifdef OS_MACOSX
#define MAYBE_CreatePanelOnOverflow DISABLED_CreatePanelOnOverflow
#else
#define MAYBE_CreatePanelOnOverflow CreatePanelOnOverflow
#endif

IN_PROC_BROWSER_TEST_F(PanelBrowserTest, MAYBE_CreatePanelOnOverflow) {
  TestCreatePanelOnOverflow();
}

class PanelDownloadTest : public PanelBrowserTest {
 public:
  PanelDownloadTest() : PanelBrowserTest() { }

  // Creates a temporary directory for downloads that is auto-deleted
  // on destruction.
  bool CreateDownloadDirectory(Profile* profile) {
    bool created = downloads_directory_.CreateUniqueTempDir();
    if (!created)
      return false;
    profile->GetPrefs()->SetFilePath(
        prefs::kDownloadDefaultDirectory,
        downloads_directory_.path());
    return true;
  }

 private:
  // Location of the downloads directory for download tests.
  ScopedTempDir downloads_directory_;
};

class DownloadObserver : public DownloadManager::Observer {
 public:
  explicit DownloadObserver(Profile* profile)
      : download_manager_(profile->GetDownloadManager()),
        saw_download_(false),
        waiting_(false) {
    download_manager_->AddObserver(this);
  }

  ~DownloadObserver() {
    download_manager_->RemoveObserver(this);
  }

  void WaitForDownload() {
    if (!saw_download_) {
      waiting_ = true;
      ui_test_utils::RunMessageLoop();
      EXPECT_TRUE(saw_download_);
      waiting_ = false;
    }
  }

  // DownloadManager::Observer
  virtual void ModelChanged() {
    std::vector<DownloadItem*> downloads;
    download_manager_->GetCurrentDownloads(FilePath(), &downloads);
    if (downloads.empty())
      return;

    EXPECT_EQ(1U, downloads.size());
    downloads.front()->Cancel(false);  // Don't actually need to download it.

    saw_download_ = true;
    EXPECT_TRUE(waiting_);
    MessageLoopForUI::current()->Quit();
  }

 private:
  DownloadManager* download_manager_;
  bool saw_download_;
  bool waiting_;
};

// Verify that the download shelf is opened in the existing tabbed browser
// when a download is started in a Panel.
IN_PROC_BROWSER_TEST_F(PanelDownloadTest, Download) {
  Profile* profile = browser()->profile();
  ASSERT_TRUE(CreateDownloadDirectory(profile));
  Browser* panel_browser = Browser::CreateForApp(Browser::TYPE_PANEL,
                                                 "PanelTest",
                                                 gfx::Rect(),
                                                 profile);
  EXPECT_EQ(2U, BrowserList::size());
  ASSERT_FALSE(browser()->window()->IsDownloadShelfVisible());
  ASSERT_FALSE(panel_browser->window()->IsDownloadShelfVisible());

  scoped_ptr<DownloadObserver> observer(new DownloadObserver(profile));
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL download_url(URLRequestMockHTTPJob::GetMockUrl(file));
  ui_test_utils::NavigateToURLWithDisposition(
      panel_browser,
      download_url,
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  observer->WaitForDownload();

#if defined(OS_CHROMEOS)
  // ChromeOS uses a download panel instead of a download shelf.
  EXPECT_EQ(3U, BrowserList::size());
  ASSERT_FALSE(browser()->window()->IsDownloadShelfVisible());

  std::set<Browser*> original_browsers;
  original_browsers.insert(browser());
  original_browsers.insert(panel_browser);
  Browser* added = ui_test_utils::GetBrowserNotInSet(original_browsers);
  ASSERT_TRUE(added->is_type_panel());
  ASSERT_FALSE(added->window()->IsDownloadShelfVisible());
#else
  EXPECT_EQ(2U, BrowserList::size());
  ASSERT_TRUE(browser()->window()->IsDownloadShelfVisible());
#endif

  EXPECT_EQ(1, browser()->tab_count());
  EXPECT_EQ(1, panel_browser->tab_count());
  ASSERT_FALSE(panel_browser->window()->IsDownloadShelfVisible());

  panel_browser->CloseWindow();
  browser()->CloseWindow();
}

// Verify that a new tabbed browser is created to display a download
// shelf when a download is started in a Panel and there is no existing
// tabbed browser.
IN_PROC_BROWSER_TEST_F(PanelDownloadTest, DownloadNoTabbedBrowser) {
  Profile* profile = browser()->profile();
  ASSERT_TRUE(CreateDownloadDirectory(profile));
  Browser* panel_browser = Browser::CreateForApp(Browser::TYPE_PANEL,
                                                 "PanelTest",
                                                 gfx::Rect(),
                                                 profile);
  EXPECT_EQ(2U, BrowserList::size());
  ASSERT_FALSE(browser()->window()->IsDownloadShelfVisible());
  ASSERT_FALSE(panel_browser->window()->IsDownloadShelfVisible());

  ui_test_utils::WindowedNotificationObserver signal(
      chrome::NOTIFICATION_BROWSER_CLOSED,
      Source<Browser>(browser()));
  browser()->CloseWindow();
  signal.Wait();
  ASSERT_EQ(1U, BrowserList::size());
  ASSERT_EQ(NULL, Browser::GetTabbedBrowser(profile, false));

  scoped_ptr<DownloadObserver> observer(new DownloadObserver(profile));
  FilePath file(FILE_PATH_LITERAL("download-test1.lib"));
  GURL download_url(URLRequestMockHTTPJob::GetMockUrl(file));
  ui_test_utils::NavigateToURLWithDisposition(
      panel_browser,
      download_url,
      CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  observer->WaitForDownload();

  EXPECT_EQ(2U, BrowserList::size());

#if defined(OS_CHROMEOS)
  // ChromeOS uses a download panel instead of a download shelf.
  std::set<Browser*> original_browsers;
  original_browsers.insert(panel_browser);
  Browser* added = ui_test_utils::GetBrowserNotInSet(original_browsers);
  ASSERT_TRUE(added->is_type_panel());
  ASSERT_FALSE(added->window()->IsDownloadShelfVisible());
#else
  Browser* tabbed_browser = Browser::GetTabbedBrowser(profile, false);
  EXPECT_EQ(1, tabbed_browser->tab_count());
  ASSERT_TRUE(tabbed_browser->window()->IsDownloadShelfVisible());
  tabbed_browser->CloseWindow();
#endif

  EXPECT_EQ(1, panel_browser->tab_count());
  ASSERT_FALSE(panel_browser->window()->IsDownloadShelfVisible());

  panel_browser->CloseWindow();
}
