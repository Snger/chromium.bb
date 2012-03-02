// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_item_model.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/views/aura/app_list/app_list_model_builder.h"
#include "chrome/browser/extensions/extension_service_unittest.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class AppListModelBuilderTest : public ExtensionServiceTestBase {
 public:
  AppListModelBuilderTest() {}
  virtual ~AppListModelBuilderTest() {}
};

TEST_F(AppListModelBuilderTest, Build) {
  // Load "app_list" extensions test profile. The test profile has 4 extensions:
  // 1 dummy extension, 2 packaged extension apps and 1 hosted extension app.
  FilePath source_install_dir = data_dir_
      .AppendASCII("app_list")
      .AppendASCII("Extensions");
  FilePath pref_path = source_install_dir
      .DirName()
      .AppendASCII("Preferences");
  InitializeInstalledExtensionService(pref_path, source_install_dir);
  service_->Init();

  // There should be 4 extensions in the test profile.
  const ExtensionSet* extensions = service_->extensions();
  ASSERT_EQ(static_cast<size_t>(4),  extensions->size());

  scoped_ptr<ash::AppListModel> model(new ash::AppListModel());
  AppListModelBuilder builder(profile_.get(), model.get());
  builder.Build(std::string());

  // Since we are in unit_tests and there is no browser, the model would have
  // only 3 extension apps in the profile.
  EXPECT_EQ(3, model->item_count());
  EXPECT_EQ("Hosted App", model->GetItem(0)->title());
  EXPECT_EQ("Packaged App 1", model->GetItem(1)->title());
  EXPECT_EQ("Packaged App 2", model->GetItem(2)->title());
}
