// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/profile_sync_service_harness.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/live_sync/live_sync_test.h"
#include "chrome/test/live_sync/preferences_helper.h"

class ManyClientPreferencesSyncTest : public LiveSyncTest {
 public:
  ManyClientPreferencesSyncTest() : LiveSyncTest(MANY_CLIENT) {}
  virtual ~ManyClientPreferencesSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ManyClientPreferencesSyncTest);
};

// TODO(rsimha): Enable once http://crbug.com/69604 is fixed.
IN_PROC_BROWSER_TEST_F(ManyClientPreferencesSyncTest, DISABLED_Sanity) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed.";
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kHomePageIsNewTabPage));
  PreferencesHelper::ChangeBooleanPref(0, prefs::kHomePageIsNewTabPage);
  ASSERT_TRUE(GetClient(0)->AwaitGroupSyncCycleCompletion(clients()));
  ASSERT_TRUE(PreferencesHelper::BooleanPrefMatches(
      prefs::kHomePageIsNewTabPage));
}
