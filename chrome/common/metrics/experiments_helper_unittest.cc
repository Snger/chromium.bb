// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests for the Experiment Helpers.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/metrics/field_trial.h"
#include "chrome/common/metrics/experiments_helper.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Convenience helper to retrieve the GoogleExperimentID for a FieldTrial. Note
// that this will do the group assignment in |trial| if not already done.
experiments_helper::GoogleExperimentID GetIDForTrial(base::FieldTrial* trial) {
  return experiments_helper::GetGoogleExperimentID(
      base::FieldTrial::MakeNameGroupId(trial->name(),
                                        trial->group_name()));
}

}  // namespace

class ExperimentsHelperTest : public ::testing::Test {
 public:
  ExperimentsHelperTest() {
    // Since the API can only be called on the UI thread, we have to fake that
    // we're on it.
    ui_thread_.reset(new content::TestBrowserThread(
        content::BrowserThread::UI, &message_loop_));
  }

  MessageLoop message_loop_;
  scoped_ptr<content::TestBrowserThread> ui_thread_;
};

// Test that if the trial is immediately disabled, GetGoogleExperimentID just
// returns the empty ID.
TEST_F(ExperimentsHelperTest, DisableImmediately) {
  int default_group_number = -1;
  base::FieldTrial* trial = base::FieldTrialList::FactoryGetFieldTrial(
      "trial", 100, "default", 2199, 12, 12, &default_group_number);
  trial->Disable();

  ASSERT_EQ(default_group_number, trial->group());
  ASSERT_EQ(experiments_helper::kEmptyGoogleExperimentID, GetIDForTrial(trial));
}

// Test that successfully associating the FieldTrial with some ID, and then
// disabling the FieldTrial actually makes GetGoogleExperimentID correctly
// return the empty ID.
TEST_F(ExperimentsHelperTest, DisableAfterInitialization) {
  const std::string default_name = "default";
  const std::string non_default_name = "non_default";

  base::FieldTrial* trial = base::FieldTrialList::FactoryGetFieldTrial(
      "trial", 100, default_name, 2199, 12, 12, NULL);
  trial->AppendGroup(non_default_name, 100);
  experiments_helper::AssociateGoogleExperimentID(
      base::FieldTrial::MakeNameGroupId(trial->name(), default_name), 123);
  experiments_helper::AssociateGoogleExperimentID(
      base::FieldTrial::MakeNameGroupId(trial->name(), non_default_name), 456);
  ASSERT_EQ(non_default_name, trial->group_name());
  ASSERT_EQ(456U, GetIDForTrial(trial));
  trial->Disable();
  ASSERT_EQ(default_name, trial->group_name());
  ASSERT_EQ(123U, GetIDForTrial(trial));
}

// Test various successful association cases.
TEST_F(ExperimentsHelperTest, AssociateGoogleExperimentID) {
  const std::string default_name1 = "default1";
  base::FieldTrial* trial_true = base::FieldTrialList::FactoryGetFieldTrial(
      "d1", 10, default_name1, 2199, 12, 31, NULL);
  const std::string winner = "TheWinner";
  int winner_group = trial_true->AppendGroup(winner, 10);

  // Set GoogleExperimentIDs so we can verify that they were chosen correctly.
  experiments_helper::AssociateGoogleExperimentID(
      base::FieldTrial::MakeNameGroupId(trial_true->name(), default_name1),
      123);
  experiments_helper::AssociateGoogleExperimentID(
      base::FieldTrial::MakeNameGroupId(trial_true->name(), winner),
      456);

  EXPECT_EQ(winner_group, trial_true->group());
  EXPECT_EQ(winner, trial_true->group_name());
  EXPECT_EQ(456U, GetIDForTrial(trial_true));

  const std::string default_name2 = "default2";
  base::FieldTrial* trial_false = base::FieldTrialList::FactoryGetFieldTrial(
      "d2", 10, default_name2, 2199, 12, 31, NULL);
  const std::string loser = "ALoser";
  int loser_group = trial_false->AppendGroup(loser, 0);

  experiments_helper::AssociateGoogleExperimentID(
      base::FieldTrial::MakeNameGroupId(trial_false->name(), default_name2),
      123);
  experiments_helper::AssociateGoogleExperimentID(
      base::FieldTrial::MakeNameGroupId(trial_false->name(), loser),
      456);

  EXPECT_NE(loser_group, trial_false->group());
  EXPECT_EQ(123U, GetIDForTrial(trial_false));
}

// Test that not associating a FieldTrial with any IDs ensure that the empty ID
// will be returned.
TEST_F(ExperimentsHelperTest, NoAssociation) {
  const std::string default_name = "default";
  base::FieldTrial* no_id_trial = base::FieldTrialList::FactoryGetFieldTrial(
      "d3", 10, default_name, 2199, 12, 31, NULL);
  const std::string winner = "TheWinner";
  int winner_group = no_id_trial->AppendGroup(winner, 10);

  // Ensure that despite the fact that a normal winner is elected, it does not
  // have a valid GoogleExperimentID associated with it.
  EXPECT_EQ(winner_group, no_id_trial->group());
  EXPECT_EQ(winner, no_id_trial->group_name());
  EXPECT_EQ(experiments_helper::kEmptyGoogleExperimentID,
            GetIDForTrial(no_id_trial));
}
