// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_settings/content_settings_policy_provider.h"

#include <string>

#include "base/auto_reset.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/content_settings/content_settings_mock_observer.h"
#include "chrome/browser/content_settings/content_settings_rule.h"
#include "chrome/browser/content_settings/content_settings_utils.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/testing_pref_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "googleurl/src/gurl.h"

using ::testing::_;

namespace content_settings {

typedef std::vector<Rule> Rules;

class PolicyProviderTest : public testing::Test {
 public:
  PolicyProviderTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {
  }

 protected:
  // TODO(markusheintz): Check if it's possible to derive the provider class
  // from NonThreadSafe and to use native thread identifiers instead of
  // BrowserThread IDs. Then we could get rid of the message_loop and ui_thread
  // fields.
  MessageLoop message_loop_;
  BrowserThread ui_thread_;
};

TEST_F(PolicyProviderTest, DefaultGeolocationContentSetting) {
  TestingProfile profile;
  TestingPrefService* prefs = profile.GetTestingPrefService();
  PolicyProvider provider(prefs);

  Rules rules;

  provider.GetAllContentSettingsRules(
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      &rules);
  EXPECT_EQ(0U, rules.size());

  prefs->SetInteger(prefs::kGeolocationDefaultContentSetting,
                    CONTENT_SETTING_ALLOW);
  provider.GetAllContentSettingsRules(
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      &rules);
  EXPECT_EQ(0U, rules.size());

  prefs->SetManagedPref(prefs::kGeolocationDefaultContentSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  provider.GetAllContentSettingsRules(
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      &rules);
  EXPECT_EQ(0U, rules.size());

  // Change the managed value of the default geolocation setting
  prefs->SetManagedPref(prefs::kManagedDefaultGeolocationSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));

  provider.GetAllContentSettingsRules(
      CONTENT_SETTINGS_TYPE_GEOLOCATION,
      std::string(),
      &rules);
  EXPECT_EQ(1U, rules.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rules[0].primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rules[0].secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, rules[0].content_setting);

  provider.ShutdownOnUIThread();
}

TEST_F(PolicyProviderTest, ManagedDefaultContentSettings) {
  TestingProfile profile;
  TestingPrefService* prefs = profile.GetTestingPrefService();
  PolicyProvider provider(prefs);

  prefs->SetManagedPref(prefs::kManagedDefaultPluginsSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));

  Rules rules;
  provider.GetAllContentSettingsRules(
      CONTENT_SETTINGS_TYPE_PLUGINS,
      std::string(),
      &rules);
  EXPECT_EQ(1U, rules.size());
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rules[0].primary_pattern);
  EXPECT_EQ(ContentSettingsPattern::Wildcard(), rules[0].secondary_pattern);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, rules[0].content_setting);

  provider.ShutdownOnUIThread();
}

// When a default-content-setting is set to a managed setting a
// CONTENT_SETTINGS_CHANGED notification should be fired. The same should happen
// if the managed setting is removed.
TEST_F(PolicyProviderTest, ObserveManagedSettingsChange) {
  TestingProfile profile;
  TestingPrefService* prefs = profile.GetTestingPrefService();
  PolicyProvider provider(prefs);

  MockObserver mock_observer;
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(_,
                                      _,
                                      CONTENT_SETTINGS_TYPE_DEFAULT,
                                      ""));
  provider.AddObserver(&mock_observer);

  // Set the managed default-content-setting.
  prefs->SetManagedPref(prefs::kManagedDefaultImagesSetting,
                        Value::CreateIntegerValue(CONTENT_SETTING_BLOCK));
  ::testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_CALL(mock_observer,
              OnContentSettingChanged(_,
                                      _,
                                      CONTENT_SETTINGS_TYPE_DEFAULT,
                                      ""));
  // Remove the managed default-content-setting.
  prefs->RemoveManagedPref(prefs::kManagedDefaultImagesSetting);
  provider.ShutdownOnUIThread();
}

TEST_F(PolicyProviderTest, GettingManagedContentSettings) {
  TestingProfile profile;
  TestingPrefService* prefs = profile.GetTestingPrefService();

  ListValue* value = new ListValue();
  value->Append(Value::CreateStringValue("[*.]google.com"));
  prefs->SetManagedPref(prefs::kManagedImagesBlockedForUrls,
                        value);

  PolicyProvider provider(prefs);

  ContentSettingsPattern yt_url_pattern =
      ContentSettingsPattern::FromString("www.youtube.com");
  GURL youtube_url("http://www.youtube.com");
  GURL google_url("http://mail.google.com");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            provider.GetContentSetting(
                youtube_url, youtube_url, CONTENT_SETTINGS_TYPE_COOKIES, ""));
  EXPECT_EQ(NULL,
            provider.GetContentSettingValue(
                youtube_url, youtube_url, CONTENT_SETTINGS_TYPE_COOKIES, ""));

  EXPECT_EQ(CONTENT_SETTING_BLOCK,
            provider.GetContentSetting(
                google_url, google_url, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  scoped_ptr<Value> value_ptr(provider.GetContentSettingValue(
                google_url, google_url, CONTENT_SETTINGS_TYPE_IMAGES, ""));
  int int_value = -1;
  value_ptr->GetAsInteger(&int_value);
  EXPECT_EQ(CONTENT_SETTING_BLOCK, IntToContentSetting(int_value));

  // The PolicyProvider does not allow setting content settings as they are
  // enforced via policies and not set by the user or extension. So a call to
  // SetContentSetting does nothing.
  provider.SetContentSetting(
      yt_url_pattern,
      yt_url_pattern,
      CONTENT_SETTINGS_TYPE_COOKIES,
      "",
      CONTENT_SETTING_BLOCK);
  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            provider.GetContentSetting(
                youtube_url, youtube_url, CONTENT_SETTINGS_TYPE_COOKIES, ""));

  provider.ShutdownOnUIThread();
}

TEST_F(PolicyProviderTest, ResourceIdentifier) {
  TestingProfile profile;
  TestingPrefService* prefs = profile.GetTestingPrefService();

  ListValue* value = new ListValue();
  value->Append(Value::CreateStringValue("[*.]google.com"));
  prefs->SetManagedPref(prefs::kManagedPluginsAllowedForUrls,
                        value);

  PolicyProvider provider(prefs);

  GURL youtube_url("http://www.youtube.com");
  GURL google_url("http://mail.google.com");

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            provider.GetContentSetting(
                youtube_url,
                youtube_url,
                CONTENT_SETTINGS_TYPE_PLUGINS,
                "someplugin"));

  // There is currently no policy support for resource content settings.
  // Resource identifiers are simply ignored by the PolicyProvider.
  EXPECT_EQ(CONTENT_SETTING_ALLOW,
            provider.GetContentSetting(
                google_url,
                google_url,
                CONTENT_SETTINGS_TYPE_PLUGINS,
                ""));

  EXPECT_EQ(CONTENT_SETTING_DEFAULT,
            provider.GetContentSetting(
                google_url,
                google_url,
                CONTENT_SETTINGS_TYPE_PLUGINS,
                "someplugin"));

  provider.ShutdownOnUIThread();
}

TEST_F(PolicyProviderTest, AutoSelectCertificateList) {
  TestingProfile profile;
  TestingPrefService* prefs = profile.GetTestingPrefService();

  PolicyProvider provider(prefs);
  GURL google_url("https://mail.google.com");
  // Tests the default setting for auto selecting certificates
  EXPECT_EQ(NULL,
            provider.GetContentSettingValue(
                google_url,
                google_url,
                CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
                std::string()));

  // Set the content settings pattern list for origins to auto select
  // certificates.
  std::string pattern_str("\"pattern\":\"[*.]google.com\"");
  std::string filter_str("\"filter\":{\"ISSUER\":{\"CN\":\"issuer name\"}}");
  ListValue* value = new ListValue();
  value->Append(Value::CreateStringValue(
      "{" + pattern_str + "," + filter_str + "}"));
  prefs->SetManagedPref(prefs::kManagedAutoSelectCertificateForUrls,
                        value);
  GURL youtube_url("https://www.youtube.com");
  EXPECT_EQ(NULL,
            provider.GetContentSettingValue(
                youtube_url,
                youtube_url,
                CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
                std::string()));
  scoped_ptr<Value> cert_filter(provider.GetContentSettingValue(
      google_url,
      google_url,
      CONTENT_SETTINGS_TYPE_AUTO_SELECT_CERTIFICATE,
      std::string()));

  ASSERT_EQ(Value::TYPE_DICTIONARY, cert_filter->GetType());
  DictionaryValue* dict_value =
      static_cast<DictionaryValue*>(cert_filter.get());
  std::string actual_common_name;
  ASSERT_TRUE(dict_value->GetString("ISSUER.CN", &actual_common_name));
  EXPECT_EQ("issuer name", actual_common_name);
  provider.ShutdownOnUIThread();
}

}  // namespace content_settings
