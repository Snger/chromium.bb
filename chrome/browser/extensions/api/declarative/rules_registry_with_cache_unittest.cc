// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/declarative/rules_registry.h"

// Here we test the TestRulesRegistry which is the simplest possible
// implementation of RulesRegistryWithCache as a proxy for
// RulesRegistryWithCache.

#include "base/command_line.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/api/declarative/rules_cache_delegate.h"
#include "chrome/browser/extensions/api/declarative/test_rules_registry.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/browser/value_store/testing_value_store.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

using extension_test_util::LoadManifestUnchecked;

namespace {
// The |kExtensionId| needs to pass the Extension::IdIsValid test.
const char kExtensionId[] = "abcdefghijklmnopabcdefghijklmnop";
const char kExtension2Id[] = "ponmlkjihgfedcbaponmlkjihgfedcba";
const char kRuleId[] = "rule";
const char kRule2Id[] = "rule2";
}

namespace extensions {

class RulesRegistryWithCacheTest : public testing::Test {
 public:
  RulesRegistryWithCacheTest()
      : cache_delegate_(/*log_storage_init_delay=*/false ),
        registry_(new TestRulesRegistry(&profile_,
                                        /*event_name=*/"",
                                        content::BrowserThread::UI,
                                        &cache_delegate_,
                                        RulesRegistry::WebViewKey(0, 0))) {}

  virtual ~RulesRegistryWithCacheTest() {}

  std::string AddRule(const std::string& extension_id,
                      const std::string& rule_id,
                      TestRulesRegistry* registry) {
    std::vector<linked_ptr<extensions::RulesRegistry::Rule> > add_rules;
    add_rules.push_back(make_linked_ptr(new extensions::RulesRegistry::Rule));
    add_rules[0]->id.reset(new std::string(rule_id));
    return registry->AddRules(extension_id, add_rules);
  }

  std::string AddRule(const std::string& extension_id,
                      const std::string& rule_id) {
    return AddRule(extension_id, rule_id, registry_.get());
  }

  std::string RemoveRule(const std::string& extension_id,
                         const std::string& rule_id) {
    std::vector<std::string> remove_rules;
    remove_rules.push_back(rule_id);
    return registry_->RemoveRules(extension_id, remove_rules);
  }

  int GetNumberOfRules(const std::string& extension_id,
                       TestRulesRegistry* registry) {
    std::vector<linked_ptr<extensions::RulesRegistry::Rule> > get_rules;
    registry->GetAllRules(extension_id, &get_rules);
    return get_rules.size();
  }

  int GetNumberOfRules(const std::string& extension_id) {
    return GetNumberOfRules(extension_id, registry_.get());
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  RulesCacheDelegate cache_delegate_;
  // |registry_| needs to be defined after |thread_bundle_| to ensure that it
  // is released before the final spinning of threads.
  scoped_refptr<TestRulesRegistry> registry_;
#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif
};

TEST_F(RulesRegistryWithCacheTest, AddRules) {
  // Check that nothing happens if the concrete RulesRegistry refuses to insert
  // the rules.
  registry_->SetResult("Error");
  EXPECT_EQ("Error", AddRule(kExtensionId, kRuleId));
  EXPECT_EQ(0, GetNumberOfRules(kExtensionId));
  registry_->SetResult(std::string());

  // Check that rules can be inserted.
  EXPECT_EQ("", AddRule(kExtensionId, kRule2Id));
  EXPECT_EQ(1, GetNumberOfRules(kExtensionId));

  // Check that rules cannot be inserted twice with the same kRuleId.
  EXPECT_NE("", AddRule(kExtensionId, kRuleId));
  EXPECT_EQ(1, GetNumberOfRules(kExtensionId));

  // Check that different extensions may use the same kRuleId.
  EXPECT_EQ("", AddRule(kExtension2Id, kRuleId));
  EXPECT_EQ(1, GetNumberOfRules(kExtensionId));
  EXPECT_EQ(1, GetNumberOfRules(kExtension2Id));
}

TEST_F(RulesRegistryWithCacheTest, RemoveRules) {
  // Prime registry.
  EXPECT_EQ("", AddRule(kExtensionId, kRuleId));
  EXPECT_EQ("", AddRule(kExtension2Id, kRuleId));
  EXPECT_EQ(1, GetNumberOfRules(kExtensionId));
  EXPECT_EQ(1, GetNumberOfRules(kExtension2Id));

  // Check that nothing happens if the concrete RuleRegistry refuses to remove
  // the rules.
  registry_->SetResult("Error");
  EXPECT_EQ("Error", RemoveRule(kExtensionId, kRuleId));
  EXPECT_EQ(1, GetNumberOfRules(kExtensionId));
  registry_->SetResult(std::string());

  // Check that nothing happens if a rule does not exist.
  EXPECT_EQ("", RemoveRule(kExtensionId, "unknown_rule"));
  EXPECT_EQ(1, GetNumberOfRules(kExtensionId));

  // Check that rules may be removed and only for the correct extension.
  EXPECT_EQ("", RemoveRule(kExtensionId, kRuleId));
  EXPECT_EQ(0, GetNumberOfRules(kExtensionId));
  EXPECT_EQ(1, GetNumberOfRules(kExtension2Id));
}

TEST_F(RulesRegistryWithCacheTest, RemoveAllRules) {
  // Prime registry.
  EXPECT_EQ("", AddRule(kExtensionId, kRuleId));
  EXPECT_EQ("", AddRule(kExtensionId, kRule2Id));
  EXPECT_EQ("", AddRule(kExtension2Id, kRuleId));
  EXPECT_EQ(2, GetNumberOfRules(kExtensionId));
  EXPECT_EQ(1, GetNumberOfRules(kExtension2Id));

  // Check that nothing happens if the concrete RuleRegistry refuses to remove
  // the rules.
  registry_->SetResult("Error");
  EXPECT_EQ("Error", registry_->RemoveAllRules(kExtensionId));
  EXPECT_EQ(2, GetNumberOfRules(kExtensionId));
  registry_->SetResult(std::string());

  // Check that rules may be removed and only for the correct extension.
  EXPECT_EQ("", registry_->RemoveAllRules(kExtensionId));
  EXPECT_EQ(0, GetNumberOfRules(kExtensionId));
  EXPECT_EQ(1, GetNumberOfRules(kExtension2Id));
}

TEST_F(RulesRegistryWithCacheTest, GetRules) {
  // Prime registry.
  EXPECT_EQ("", AddRule(kExtensionId, kRuleId));
  EXPECT_EQ("", AddRule(kExtensionId, kRule2Id));
  EXPECT_EQ("", AddRule(kExtension2Id, kRuleId));

  // Check that we get the correct rule and unknown rules are ignored.
  std::vector<std::string> rules_to_get;
  rules_to_get.push_back(kRuleId);
  rules_to_get.push_back("unknown_rule");
  std::vector<linked_ptr<extensions::RulesRegistry::Rule> > gotten_rules;
  registry_->GetRules(kExtensionId, rules_to_get, &gotten_rules);
  ASSERT_EQ(1u, gotten_rules.size());
  ASSERT_TRUE(gotten_rules[0]->id.get());
  EXPECT_EQ(kRuleId, *(gotten_rules[0]->id));
}

TEST_F(RulesRegistryWithCacheTest, GetAllRules) {
  // Prime registry.
  EXPECT_EQ("", AddRule(kExtensionId, kRuleId));
  EXPECT_EQ("", AddRule(kExtensionId, kRule2Id));
  EXPECT_EQ("", AddRule(kExtension2Id, kRuleId));

  // Check that we get the correct rules.
  std::vector<linked_ptr<extensions::RulesRegistry::Rule> > gotten_rules;
  registry_->GetAllRules(kExtensionId, &gotten_rules);
  EXPECT_EQ(2u, gotten_rules.size());
  ASSERT_TRUE(gotten_rules[0]->id.get());
  ASSERT_TRUE(gotten_rules[1]->id.get());
  EXPECT_TRUE( (kRuleId == *(gotten_rules[0]->id) &&
                kRule2Id == *(gotten_rules[1]->id)) ||
               (kRuleId == *(gotten_rules[1]->id) &&
                kRule2Id == *(gotten_rules[0]->id)) );
}

TEST_F(RulesRegistryWithCacheTest, OnExtensionUninstalled) {
  // Prime registry.
  EXPECT_EQ("", AddRule(kExtensionId, kRuleId));
  EXPECT_EQ("", AddRule(kExtension2Id, kRuleId));

  // Check that the correct rules are removed.
  registry_->OnExtensionUninstalled(kExtensionId);
  EXPECT_EQ(0, GetNumberOfRules(kExtensionId));
  EXPECT_EQ(1, GetNumberOfRules(kExtension2Id));
}

TEST_F(RulesRegistryWithCacheTest, DeclarativeRulesStored) {
  // TestingProfile::Init makes sure that the factory method for a corresponding
  // extension system creates a TestExtensionSystem.
  extensions::TestExtensionSystem* system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(&profile_));
  ExtensionPrefs* extension_prefs = system->CreateExtensionPrefs(
      CommandLine::ForCurrentProcess(), base::FilePath());
  system->CreateExtensionService(
      CommandLine::ForCurrentProcess(), base::FilePath(), false);
  // The value store is first created during CreateExtensionService.
  TestingValueStore* store = system->value_store();

  const std::string event_name("testEvent");
  const std::string rules_stored_key(
      RulesCacheDelegate::GetRulesStoredKey(
          event_name, profile_.IsOffTheRecord()));
  scoped_ptr<RulesCacheDelegate> cache_delegate(new RulesCacheDelegate(false));
  scoped_refptr<RulesRegistry> registry(new TestRulesRegistry(
      &profile_, event_name, content::BrowserThread::UI,
      cache_delegate.get(),
      RulesRegistry::WebViewKey(0, 0)));

  // 1. Test the handling of preferences.
  // Default value is always true.
  EXPECT_TRUE(cache_delegate->GetDeclarativeRulesStored(kExtensionId));

  extension_prefs->UpdateExtensionPref(
      kExtensionId, rules_stored_key, new base::FundamentalValue(false));
  EXPECT_FALSE(cache_delegate->GetDeclarativeRulesStored(kExtensionId));

  extension_prefs->UpdateExtensionPref(
      kExtensionId, rules_stored_key, new base::FundamentalValue(true));
  EXPECT_TRUE(cache_delegate->GetDeclarativeRulesStored(kExtensionId));

  // 2. Test writing behavior.
  int write_count = store->write_count();

  scoped_ptr<base::ListValue> value(new base::ListValue);
  value->AppendBoolean(true);
  cache_delegate->WriteToStorage(kExtensionId, value.PassAs<base::Value>());
  EXPECT_TRUE(cache_delegate->GetDeclarativeRulesStored(kExtensionId));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(write_count + 1, store->write_count());
  write_count = store->write_count();

  value.reset(new base::ListValue);
  cache_delegate->WriteToStorage(kExtensionId, value.PassAs<base::Value>());
  EXPECT_FALSE(cache_delegate->GetDeclarativeRulesStored(kExtensionId));
  base::RunLoop().RunUntilIdle();
  // No rules currently, but previously there were, so we expect a write.
  EXPECT_EQ(write_count + 1, store->write_count());
  write_count = store->write_count();

  value.reset(new base::ListValue);
  cache_delegate->WriteToStorage(kExtensionId, value.PassAs<base::Value>());
  EXPECT_FALSE(cache_delegate->GetDeclarativeRulesStored(kExtensionId));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(write_count, store->write_count());

  // 3. Test reading behavior.
  int read_count = store->read_count();

  cache_delegate->SetDeclarativeRulesStored(kExtensionId, false);
  cache_delegate->ReadFromStorage(kExtensionId);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(read_count, store->read_count());
  read_count = store->read_count();

  cache_delegate->SetDeclarativeRulesStored(kExtensionId, true);
  cache_delegate->ReadFromStorage(kExtensionId);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(read_count + 1, store->read_count());
}

// Test that each registry has its own "are some rules stored" flag.
TEST_F(RulesRegistryWithCacheTest, RulesStoredFlagMultipleRegistries) {
  // TestingProfile::Init makes sure that the factory method for a corresponding
  // extension system creates a TestExtensionSystem.
  extensions::TestExtensionSystem* system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(&profile_));
  ExtensionPrefs* extension_prefs = system->CreateExtensionPrefs(
      CommandLine::ForCurrentProcess(), base::FilePath());

  const std::string event_name1("testEvent1");
  const std::string event_name2("testEvent2");
  const std::string rules_stored_key1(
      RulesCacheDelegate::GetRulesStoredKey(
          event_name1, profile_.IsOffTheRecord()));
  const std::string rules_stored_key2(
      RulesCacheDelegate::GetRulesStoredKey(
          event_name2, profile_.IsOffTheRecord()));
  scoped_ptr<RulesCacheDelegate> cache_delegate1(new RulesCacheDelegate(false));
  scoped_refptr<RulesRegistry> registry1(new TestRulesRegistry(
      &profile_, event_name1, content::BrowserThread::UI,
      cache_delegate1.get(),
      RulesRegistry::WebViewKey(0, 0)));

  scoped_ptr<RulesCacheDelegate> cache_delegate2(new RulesCacheDelegate(false));
  scoped_refptr<RulesRegistry> registry2(new TestRulesRegistry(
      &profile_, event_name2, content::BrowserThread::UI,
      cache_delegate2.get(),
      RulesRegistry::WebViewKey(0, 0)));

  // Checkt the correct default values.
  EXPECT_TRUE(cache_delegate1->GetDeclarativeRulesStored(kExtensionId));
  EXPECT_TRUE(cache_delegate2->GetDeclarativeRulesStored(kExtensionId));

  // Update the flag for the first registry.
  extension_prefs->UpdateExtensionPref(
      kExtensionId, rules_stored_key1, new base::FundamentalValue(false));
  EXPECT_FALSE(cache_delegate1->GetDeclarativeRulesStored(kExtensionId));
  EXPECT_TRUE(cache_delegate2->GetDeclarativeRulesStored(kExtensionId));
}

TEST_F(RulesRegistryWithCacheTest, RulesPreservedAcrossRestart) {
  // This test makes sure that rules are restored from the rule store
  // on registry (in particular, browser) restart.
  extensions::TestExtensionSystem* system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(&profile_));
  ExtensionService* extension_service = system->CreateExtensionService(
      CommandLine::ForCurrentProcess(), base::FilePath(), false);

  // 1. Add an extension, before rules registry gets created.
  std::string error;
  scoped_refptr<Extension> extension(
      LoadManifestUnchecked("permissions",
                            "web_request_all_host_permissions.json",
                            Manifest::INVALID_LOCATION,
                            Extension::NO_FLAGS,
                            kExtensionId,
                            &error));
  ASSERT_TRUE(error.empty());
  extension_service->AddExtension(extension.get());
  system->SetReady();

  // 2. First run, adding a rule for the extension.
  scoped_ptr<RulesCacheDelegate> cache_delegate(new RulesCacheDelegate(false));
  scoped_refptr<TestRulesRegistry> registry(new TestRulesRegistry(
      &profile_,
      "testEvent",
      content::BrowserThread::UI,
      cache_delegate.get(),
      RulesRegistry::WebViewKey(0, 0)));

  AddRule(kExtensionId, kRuleId, registry.get());
  base::RunLoop().RunUntilIdle();  // Posted tasks store the added rule.
  EXPECT_EQ(1, GetNumberOfRules(kExtensionId, registry.get()));

  // 3. Restart the TestRulesRegistry and see the rule still there.
  cache_delegate.reset(new RulesCacheDelegate(false));
  registry = new TestRulesRegistry(
      &profile_,
      "testEvent",
      content::BrowserThread::UI,
      cache_delegate.get(),
      RulesRegistry::WebViewKey(0, 0));

  base::RunLoop().RunUntilIdle();  // Posted tasks retrieve the stored rule.
  EXPECT_EQ(1, GetNumberOfRules(kExtensionId, registry.get()));
}

TEST_F(RulesRegistryWithCacheTest, ConcurrentStoringOfRules) {
  // When an extension updates its rules, the new set of rules is stored to disk
  // with some delay. While it is acceptable for a quick series of updates for a
  // single extension to only write the last one, we should never forget to
  // write a rules update for extension A, just because it is immediately
  // followed by a rules update for extension B.
  extensions::TestExtensionSystem* system =
      static_cast<extensions::TestExtensionSystem*>(
          extensions::ExtensionSystem::Get(&profile_));
  system->CreateExtensionPrefs(CommandLine::ForCurrentProcess(),
                               base::FilePath());
  system->CreateExtensionService(
      CommandLine::ForCurrentProcess(), base::FilePath(), false);
  TestingValueStore* store = system->value_store();

  int write_count = store->write_count();
  EXPECT_EQ("", AddRule(kExtensionId, kRuleId));
  EXPECT_EQ("", AddRule(kExtension2Id, kRule2Id));
  system->SetReady();
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(write_count + 2, store->write_count());
}

}  //  namespace extensions
