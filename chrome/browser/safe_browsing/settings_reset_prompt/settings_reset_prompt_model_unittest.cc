// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_model.h"

#include <memory>
#include <string>
#include <unordered_set>
#include <utility>

#include "base/callback_forward.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/extensions/extension_service_test_base.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_test_utils.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/prefs/pref_service.h"
#include "components/search_engines/search_terms_data.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_client.h"
#include "content/public/browser/browser_context.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {
namespace {

using testing::_;
using testing::Bool;
using testing::Combine;
using testing::NiceMock;
using testing::Return;
using testing::StrictMock;

const char kHomepage[] = "http://myhomepage.com";
const char kDefaultSearch[] = "http://testsearch.com/?q={searchTerms}";

// |ResettableSettingsSnapshot| needs to get a |TemplateURLService| for the
// profile it takes a snapshot for. This will create one for the testing profile
// similar to how it is done in |ProfileResetterTest|.
//
// TODO(crbug.com/685702): Break this out so it can be used both here and in the
// |ProfileResetter| tests.
std::unique_ptr<KeyedService> CreateTemplateURLService(
    content::BrowserContext* context) {
  Profile* profile = static_cast<Profile*>(context);
  return base::MakeUnique<TemplateURLService>(
      profile->GetPrefs(),
      std::unique_ptr<SearchTermsData>(new UIThreadSearchTermsData(profile)),
      WebDataServiceFactory::GetKeywordWebDataForProfile(
          profile, ServiceAccessType::EXPLICIT_ACCESS),
      std::unique_ptr<TemplateURLServiceClient>(), nullptr, nullptr,
      base::Closure());
}

class SettingsResetPromptModelTest
    : public extensions::ExtensionServiceTestBase {
 protected:
  using ModelPointer = std::unique_ptr<SettingsResetPromptModel>;

  void SetUp() override {
    extensions::ExtensionServiceTestBase::SetUp();
    InitializeEmptyExtensionService();

    profile_->CreateWebDataService();
    TemplateURLServiceFactory::GetInstance()->SetTestingFactory(
        profile(), CreateTemplateURLService);

    prefs_ = profile()->GetPrefs();
    ASSERT_TRUE(prefs_);
  }

  void SetShowHomeButton(bool show_home_button) {
    prefs_->SetBoolean(prefs::kShowHomeButton, show_home_button);
  }

  void SetHomepageIsNTP(bool homepage_is_ntp) {
    prefs_->SetBoolean(prefs::kHomePageIsNewTabPage, homepage_is_ntp);
  }

  void SetHomepage(const std::string homepage) {
    prefs_->SetString(prefs::kHomePage, homepage);
  }

  void SetDefaultSearch(const std::string default_search) {
    TemplateURLService* template_url_service =
        TemplateURLServiceFactory::GetForProfile(profile());
    ASSERT_TRUE(template_url_service);

    TemplateURLData data;
    data.SetShortName(base::ASCIIToUTF16("TestEngine"));
    data.SetKeyword(base::ASCIIToUTF16("TestEngine"));
    data.SetURL(default_search);
    TemplateURL* template_url =
        template_url_service->Add(base::MakeUnique<TemplateURL>(data));
    template_url_service->SetUserSelectedDefaultSearchProvider(template_url);
  }

  // Returns a model with a mock config that will return negative IDs for every
  // URL. positive IDs for each URL in |reset_urls_|.
  ModelPointer CreateModel() {
    return CreateModelForTesting(profile(), std::unordered_set<std::string>());
  }

  // Returns a model with a mock config that will return positive IDs for each
  // URL in |reset_urls|.
  ModelPointer CreateModel(std::unordered_set<std::string> reset_urls) {
    return CreateModelForTesting(profile(), reset_urls);
  }

  PrefService* prefs_;
};

class ResetStatesTest
    : public SettingsResetPromptModelTest,
      public testing::WithParamInterface<testing::tuple<bool, bool>> {
 protected:
  void SetUp() override {
    SettingsResetPromptModelTest::SetUp();
    homepage_reset_enabled_ = testing::get<0>(GetParam());
    default_search_reset_enabled_ = testing::get<1>(GetParam());
    should_prompt_ = homepage_reset_enabled_ || default_search_reset_enabled_;

    if (homepage_reset_enabled_) {
      SetShowHomeButton(true);
      SetHomepageIsNTP(false);
      SetHomepage(kHomepage);
    }

    if (default_search_reset_enabled_)
      SetDefaultSearch(kDefaultSearch);
  }

  bool homepage_reset_enabled_;
  bool default_search_reset_enabled_;
  bool should_prompt_;
};

TEST_F(SettingsResetPromptModelTest, Homepage) {
  SetHomepage(kHomepage);
  ModelPointer model = CreateModel();
  EXPECT_EQ(model->homepage(), kHomepage);
}

TEST_F(SettingsResetPromptModelTest, HomepageResetState) {
  SetHomepage(kHomepage);

  for (bool homepage_is_ntp : {false, true}) {
    for (bool show_home_button : {false, true}) {
      SetShowHomeButton(show_home_button);
      SetHomepageIsNTP(homepage_is_ntp);
      // Should return |DISABLED_DUE_TO_DOMAIN_NOT_MATCHED| when
      // |UrlToResetDomainId()| returns a negative integer.
      {
        ModelPointer model = CreateModel();
        EXPECT_EQ(model->homepage_reset_state(),
                  SettingsResetPromptModel::DISABLED_DUE_TO_DOMAIN_NOT_MATCHED);
      }

      // Should return |ENABLED| when |UrlToResetDomainId()| returns a positive
      // integer and the home button is visible and homepage is not set to the
      // New Tab page, and |DISABLED_DUE_TO_DOMAIN_NOT_MATCHED| otherwise.
      {
        ModelPointer model = CreateModel({kHomepage});
        EXPECT_EQ(
            model->homepage_reset_state(),
            show_home_button && !homepage_is_ntp
                ? SettingsResetPromptModel::ENABLED
                : SettingsResetPromptModel::DISABLED_DUE_TO_DOMAIN_NOT_MATCHED);
      }
    }
  }
}

TEST_F(SettingsResetPromptModelTest, DefaultSearch) {
  SetDefaultSearch(kDefaultSearch);
  ModelPointer model = CreateModel();
  EXPECT_EQ(model->default_search(), kDefaultSearch);
}

TEST_F(SettingsResetPromptModelTest, DefaultSearchResetState) {
  SetDefaultSearch(kDefaultSearch);

  // Should return |DISABLED_DUE_TO_DOMAIN_NOT_MATCHED| when
  // |UrlToResetDomainId()| is negative.
  {
    ModelPointer model = CreateModel();
    EXPECT_EQ(model->default_search_reset_state(),
              SettingsResetPromptModel::DISABLED_DUE_TO_DOMAIN_NOT_MATCHED);
  }

  // Should return |ENABLED| when |UrlToResetDomainId()| is non-negative.
  {
    ModelPointer model = CreateModel({kDefaultSearch});
    EXPECT_EQ(model->default_search_reset_state(),
              SettingsResetPromptModel::ENABLED);
  }
}

TEST_P(ResetStatesTest, ShouldPromptForReset) {
  std::unordered_set<std::string> reset_urls;
  if (homepage_reset_enabled_)
    reset_urls.insert(kHomepage);
  if (default_search_reset_enabled_)
    reset_urls.insert(kDefaultSearch);

  ModelPointer model = CreateModel(reset_urls);
  ASSERT_EQ(model->homepage_reset_state() == SettingsResetPromptModel::ENABLED,
            homepage_reset_enabled_);
  ASSERT_EQ(
      model->default_search_reset_state() == SettingsResetPromptModel::ENABLED,
      default_search_reset_enabled_);
  EXPECT_EQ(model->ShouldPromptForReset(), should_prompt_);
}

INSTANTIATE_TEST_CASE_P(SettingsResetPromptModel,
                        ResetStatesTest,
                        Combine(Bool(), Bool()));

}  // namespace
}  // namespace safe_browsing
