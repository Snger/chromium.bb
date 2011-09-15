// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLUGIN_PREFS_H_
#define CHROME_BROWSER_PLUGIN_PREFS_H_
#pragma once

#include <map>
#include <set>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/memory/ref_counted.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/common/notification_observer.h"

class NotificationDetails;
class NotificationSource;
class Profile;

namespace content {
class ResourceContext;
}

namespace base {
class DictionaryValue;
class ListValue;
}

namespace webkit {
struct WebPluginInfo;
namespace npapi {
class PluginGroup;
}
}

// This class stores information about whether a plug-in or a plug-in group is
// enabled or disabled.
// Except where otherwise noted, it can be used on every thread.
class PluginPrefs : public base::RefCountedThreadSafe<PluginPrefs>,
                    public NotificationObserver {
 public:
  enum PolicyStatus {
    NO_POLICY = 0,  // Neither enabled or disabled by policy.
    POLICY_ENABLED,
    POLICY_DISABLED,
  };

  // Initializes the factory for this class for dependency tracking.
  // This should be called before the first profile is created.
  static void Initialize();

  // Returns the instance associated with |profile|, creating it if necessary.
  static PluginPrefs* GetForProfile(Profile* profile);

  // Usually the PluginPrefs associated with a TestingProfile is NULL.
  // This method overrides that for a given TestingProfile, returning the newly
  // created PluginPrefs object.
  static PluginPrefs* GetForTestingProfile(Profile* profile);

  // Creates a new instance. This method should only be used for testing.
  PluginPrefs();

  // Associates this instance with |prefs|. This enables or disables
  // plugin groups as defined by the user's preferences.
  // This method should only be called on the UI thread.
  void SetPrefs(PrefService* prefs);

  // Detaches from the PrefService before it is destroyed.
  // As the name says, this method should only be called on the UI thread.
  void ShutdownOnUIThread();

  // Enable or disable a plugin group.
  void EnablePluginGroup(bool enable, const string16& group_name);

  // Enable or disable a specific plugin file.
  void EnablePlugin(bool enable, const FilePath& file_path);

  // Enable or disable a plug-in in all profiles. This sets a default for
  // profiles which are created later as well.
  // This method should only be called on the UI thread.
  static void EnablePluginGlobally(bool enable, const FilePath& file_path);

  // Returns whether there is a policy enabling or disabling plug-ins of the
  // given name.
  PolicyStatus PolicyStatusForPlugin(const string16& name);

  // Returns whether the plugin is enabled or not.
  bool IsPluginEnabled(const webkit::WebPluginInfo& plugin);

  // Registers the preferences used by this class.
  // This method should only be called on the UI thread.
  static void RegisterPrefs(PrefService* prefs);

  // NotificationObserver method override.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<PluginPrefs>;
  friend class PluginPrefsTest;

  class Factory;

  virtual ~PluginPrefs();

  // Allows unit tests to directly set enforced plug-in patterns.
  void SetPolicyEnforcedPluginPatterns(
      const std::set<string16>& disabled_patterns,
      const std::set<string16>& disabled_exception_patterns,
      const std::set<string16>& enabled_patterns);

  // Called on the file thread to get the data necessary to update the saved
  // preferences.
  void GetPreferencesDataOnFileThread();

  // Called on the UI thread with the plugin data to save the preferences.
  void OnUpdatePreferences(std::vector<webkit::npapi::PluginGroup> groups);

  // Sends the notification that plugin data has changed.
  void NotifyPluginStatusChanged();

  static void ListValueToStringSet(const base::ListValue* src,
                                   std::set<string16>* dest);

  // Checks if |name| matches any of the patterns in |pattern_set|.
  static bool IsStringMatchedInSet(const string16& name,
                                   const std::set<string16>& pattern_set);

  // Guards access to the following data structures.
  mutable base::Lock lock_;

  std::map<FilePath, bool> plugin_state_;
  std::map<string16, bool> plugin_group_state_;

  std::set<string16> policy_disabled_plugin_patterns_;
  std::set<string16> policy_disabled_plugin_exception_patterns_;
  std::set<string16> policy_enabled_plugin_patterns_;

  // Weak pointer, owned by the profile (which owns us).
  PrefService* prefs_;

  PrefChangeRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PluginPrefs);
};

#endif  // CHROME_BROWSER_PLUGIN_PREFS_H_
