// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/cancelable_callback.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/activity_log/counting_policy.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/test_extension_system.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension_builder.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "sql/statement.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

namespace extensions {

class CountingPolicyTest : public testing::Test {
 public:
  CountingPolicyTest()
      : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
        saved_cmdline_(CommandLine::NO_PROGRAM) {
#if defined OS_CHROMEOS
    test_user_manager_.reset(new chromeos::ScopedTestUserManager());
#endif
    CommandLine command_line(CommandLine::NO_PROGRAM);
    saved_cmdline_ = *CommandLine::ForCurrentProcess();
    profile_.reset(new TestingProfile());
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableExtensionActivityLogging);
    extension_service_ = static_cast<TestExtensionSystem*>(
        ExtensionSystem::Get(profile_.get()))->CreateExtensionService
            (&command_line, base::FilePath(), false);
  }

  virtual ~CountingPolicyTest() {
#if defined OS_CHROMEOS
    test_user_manager_.reset();
#endif
    base::RunLoop().RunUntilIdle();
    profile_.reset(NULL);
    base::RunLoop().RunUntilIdle();
    // Restore the original command line and undo the affects of SetUp().
    *CommandLine::ForCurrentProcess() = saved_cmdline_;
  }

  // Wait for the task queue for the specified thread to empty.
  void WaitOnThread(const content::BrowserThread::ID& thread) {
    BrowserThread::PostTaskAndReply(
        thread,
        FROM_HERE,
        base::Bind(&base::DoNothing),
        base::MessageLoop::current()->QuitClosure());
    base::MessageLoop::current()->Run();
  }

  // A helper function to call ReadData on a policy object and wait for the
  // results to be processed.
  void CheckReadData(
      ActivityLogPolicy* policy,
      const std::string& extension_id,
      int day,
      const base::Callback<void(scoped_ptr<Action::ActionVector>)>& checker) {
    // Submit a request to the policy to read back some data, and call the
    // checker function when results are available.  This will happen on the
    // database thread.
    policy->ReadData(
        extension_id,
        day,
        base::Bind(&CountingPolicyTest::CheckWrapper,
                   checker,
                   base::MessageLoop::current()->QuitClosure()));

    // Set up a timeout that will trigger after 5 seconds; if we haven't
    // received any results by then assume that the test is broken.
    base::CancelableClosure timeout(
        base::Bind(&CountingPolicyTest::TimeoutCallback));
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE, timeout.callback(), base::TimeDelta::FromSeconds(5));

    // Wait for results; either the checker or the timeout callbacks should
    // cause the main loop to exit.
    base::MessageLoop::current()->Run();

    timeout.Cancel();
  }

  // A helper function which verifies that the string_ids and url_ids tables in
  // the database have the specified sizes.
  static void CheckStringTableSizes(CountingPolicy* policy,
                                    int string_size,
                                    int url_size) {
    sql::Connection* db = policy->GetDatabaseConnection();
    sql::Statement statement1(db->GetCachedStatement(
        sql::StatementID(SQL_FROM_HERE), "SELECT COUNT(*) FROM string_ids"));
    ASSERT_TRUE(statement1.Step());
    ASSERT_EQ(string_size, statement1.ColumnInt(0));

    sql::Statement statement2(db->GetCachedStatement(
        sql::StatementID(SQL_FROM_HERE), "SELECT COUNT(*) FROM url_ids"));
    ASSERT_TRUE(statement2.Step());
    ASSERT_EQ(url_size, statement2.ColumnInt(0));
  }

  static void CheckWrapper(
      const base::Callback<void(scoped_ptr<Action::ActionVector>)>& checker,
      const base::Closure& done,
      scoped_ptr<Action::ActionVector> results) {
    checker.Run(results.Pass());
    done.Run();
  }

  static void TimeoutCallback() {
    base::MessageLoop::current()->QuitWhenIdle();
    FAIL() << "Policy test timed out waiting for results";
  }

  static void Arguments_Stripped(scoped_ptr<Action::ActionVector> i) {
    scoped_refptr<Action> last = i->front();
    std::string args =
        "ID=odlameecjipmbmbejkplpemijjgpljce CATEGORY=api_call "
        "API=extension.connect ARGS=[\"hello\",\"world\"] COUNT=1";
    ASSERT_EQ(args, last->PrintForDebug());
  }

  static void Arguments_GetTodaysActions(
      scoped_ptr<Action::ActionVector> actions) {
    std::string api_stripped_print =
        "ID=punky CATEGORY=api_call API=brewster COUNT=2";
    std::string api_print =
        "ID=punky CATEGORY=api_call API=extension.sendMessage "
        "ARGS=[\"not\",\"stripped\"] COUNT=1";
    std::string dom_print =
        "ID=punky CATEGORY=dom_access API=lets ARGS=[\"vamoose\"] "
        "PAGE_URL=http://www.google.com/ COUNT=1";
    ASSERT_EQ(3, static_cast<int>(actions->size()));
    ASSERT_EQ(dom_print, actions->at(0)->PrintForDebug());
    ASSERT_EQ(api_print, actions->at(1)->PrintForDebug());
    ASSERT_EQ(api_stripped_print, actions->at(2)->PrintForDebug());
  }

  static void Arguments_GetOlderActions(
      scoped_ptr<Action::ActionVector> actions) {
    std::string api_print =
        "ID=punky CATEGORY=api_call API=brewster COUNT=1";
    std::string dom_print =
        "ID=punky CATEGORY=dom_access API=lets ARGS=[\"vamoose\"] "
        "PAGE_URL=http://www.google.com/ COUNT=1";
    ASSERT_EQ(2, static_cast<int>(actions->size()));
    ASSERT_EQ(dom_print, actions->at(0)->PrintForDebug());
    ASSERT_EQ(api_print, actions->at(1)->PrintForDebug());
  }

  static void Arguments_CheckMergeCount(
      int count,
      scoped_ptr<Action::ActionVector> actions) {
    std::string api_print = base::StringPrintf(
        "ID=punky CATEGORY=api_call API=brewster COUNT=%d", count);
    if (count > 0) {
      ASSERT_EQ(1u, actions->size());
      ASSERT_EQ(api_print, actions->at(0)->PrintForDebug());
    } else {
      ASSERT_EQ(0u, actions->size());
    }
  }

  static void Arguments_CheckMergeCountAndTime(
      int count,
      const base::Time& time,
      scoped_ptr<Action::ActionVector> actions) {
    std::string api_print = base::StringPrintf(
        "ID=punky CATEGORY=api_call API=brewster COUNT=%d", count);
    if (count > 0) {
      ASSERT_EQ(1u, actions->size());
      ASSERT_EQ(api_print, actions->at(0)->PrintForDebug());
      ASSERT_EQ(time, actions->at(0)->time());
    } else {
      ASSERT_EQ(0u, actions->size());
    }
  }

 protected:
  ExtensionService* extension_service_;
  scoped_ptr<TestingProfile> profile_;
  content::TestBrowserThreadBundle thread_bundle_;
  // Used to preserve a copy of the original command line.
  // The test framework will do this itself as well. However, by then,
  // it is too late to call ActivityLog::RecomputeLoggingIsEnabled() in
  // TearDown().
  CommandLine saved_cmdline_;

#if defined OS_CHROMEOS
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  scoped_ptr<chromeos::ScopedTestUserManager> test_user_manager_;
#endif
};

TEST_F(CountingPolicyTest, Construct) {
  ActivityLogPolicy* policy = new CountingPolicy(profile_.get());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                       .Set("name", "Test extension")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2))
          .Build();
  extension_service_->AddExtension(extension.get());
  scoped_ptr<base::ListValue> args(new base::ListValue());
  scoped_refptr<Action> action = new Action(extension->id(),
                                            base::Time::Now(),
                                            Action::ACTION_API_CALL,
                                            "tabs.testMethod");
  action->set_args(args.Pass());
  policy->ProcessAction(action);
  policy->Close();
}

TEST_F(CountingPolicyTest, LogWithStrippedArguments) {
  ActivityLogPolicy* policy = new CountingPolicy(profile_.get());
  scoped_refptr<const Extension> extension =
      ExtensionBuilder()
          .SetManifest(DictionaryBuilder()
                       .Set("name", "Test extension")
                       .Set("version", "1.0.0")
                       .Set("manifest_version", 2))
          .Build();
  extension_service_->AddExtension(extension.get());

  scoped_ptr<base::ListValue> args(new base::ListValue());
  args->Set(0, new base::StringValue("hello"));
  args->Set(1, new base::StringValue("world"));
  scoped_refptr<Action> action = new Action(extension->id(),
                                            base::Time::Now(),
                                            Action::ACTION_API_CALL,
                                            "extension.connect");
  action->set_args(args.Pass());

  policy->ProcessAction(action);
  CheckReadData(policy,
                extension->id(),
                0,
                base::Bind(&CountingPolicyTest::Arguments_Stripped));
  policy->Close();
}

TEST_F(CountingPolicyTest, GetTodaysActions) {
  CountingPolicy* policy = new CountingPolicy(profile_.get());
  // Disable row expiration for this test by setting a time before any actions
  // we generate.
  policy->set_retention_time(base::TimeDelta::FromDays(14));

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.  Note: Ownership is passed
  // to the policy, but we still keep a pointer locally.  The policy will take
  // care of destruction; this is safe since the policy outlives all our
  // accesses to the mock clock.
  base::SimpleTestClock* mock_clock = new base::SimpleTestClock();
  mock_clock->SetNow(base::Time::Now().LocalMidnight() +
                     base::TimeDelta::FromHours(12));
  policy->SetClockForTesting(scoped_ptr<base::Clock>(mock_clock));

  // Record some actions
  scoped_refptr<Action> action =
      new Action("punky",
                 mock_clock->Now() - base::TimeDelta::FromMinutes(40),
                 Action::ACTION_API_CALL,
                 "brewster");
  action->mutable_args()->AppendString("woof");
  policy->ProcessAction(action);

  action = new Action("punky",
                      mock_clock->Now() - base::TimeDelta::FromMinutes(30),
                      Action::ACTION_API_CALL,
                      "brewster");
  action->mutable_args()->AppendString("meow");
  policy->ProcessAction(action);

  action = new Action("punky",
                      mock_clock->Now() - base::TimeDelta::FromMinutes(20),
                      Action::ACTION_API_CALL,
                      "extension.sendMessage");
  action->mutable_args()->AppendString("not");
  action->mutable_args()->AppendString("stripped");
  policy->ProcessAction(action);

  action =
      new Action("punky", mock_clock->Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  action = new Action(
      "scoobydoo", mock_clock->Now(), Action::ACTION_DOM_ACCESS, "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  CheckReadData(
      policy,
      "punky",
      0,
      base::Bind(&CountingPolicyTest::Arguments_GetTodaysActions));
  policy->Close();
}

// Check that we can read back less recent actions in the db.
TEST_F(CountingPolicyTest, GetOlderActions) {
  CountingPolicy* policy = new CountingPolicy(profile_.get());
  policy->set_retention_time(base::TimeDelta::FromDays(14));

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  base::SimpleTestClock* mock_clock = new base::SimpleTestClock();
  mock_clock->SetNow(base::Time::Now().LocalMidnight() +
                     base::TimeDelta::FromHours(12));
  policy->SetClockForTesting(scoped_ptr<base::Clock>(mock_clock));

  // Record some actions
  scoped_refptr<Action> action =
      new Action("punky",
                 mock_clock->Now() - base::TimeDelta::FromDays(3) -
                     base::TimeDelta::FromMinutes(40),
                 Action::ACTION_API_CALL,
                 "brewster");
  action->mutable_args()->AppendString("woof");
  policy->ProcessAction(action);

  action = new Action("punky",
                      mock_clock->Now() - base::TimeDelta::FromDays(3),
                      Action::ACTION_DOM_ACCESS,
                      "lets");
  action->mutable_args()->AppendString("vamoose");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  action = new Action("punky",
                      mock_clock->Now(),
                      Action::ACTION_DOM_ACCESS,
                      "lets");
  action->mutable_args()->AppendString("too new");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  action = new Action("punky",
                      mock_clock->Now() - base::TimeDelta::FromDays(7),
                      Action::ACTION_DOM_ACCESS,
                      "lets");
  action->mutable_args()->AppendString("too old");
  action->set_page_url(GURL("http://www.google.com"));
  policy->ProcessAction(action);

  CheckReadData(
      policy,
      "punky",
      3,
      base::Bind(&CountingPolicyTest::Arguments_GetOlderActions));

  policy->Close();
}

// Check that merging of actions only occurs within the same day, not across
// days, and that old data can be expired from the database.
TEST_F(CountingPolicyTest, MergingAndExpiring) {
  CountingPolicy* policy = new CountingPolicy(profile_.get());
  // Initially disable expiration by setting a retention time before any
  // actions we generate.
  policy->set_retention_time(base::TimeDelta::FromDays(14));

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  base::SimpleTestClock* mock_clock = new base::SimpleTestClock();
  mock_clock->SetNow(base::Time::Now().LocalMidnight() +
                    base::TimeDelta::FromHours(12));
  policy->SetClockForTesting(scoped_ptr<base::Clock>(mock_clock));

  // The first two actions should be merged; the last one is on a separate day
  // and should not be.
  scoped_refptr<Action> action =
      new Action("punky",
                 mock_clock->Now() - base::TimeDelta::FromDays(3) -
                     base::TimeDelta::FromMinutes(40),
                 Action::ACTION_API_CALL,
                 "brewster");
  policy->ProcessAction(action);

  action = new Action("punky",
                      mock_clock->Now() - base::TimeDelta::FromDays(3) -
                          base::TimeDelta::FromMinutes(20),
                      Action::ACTION_API_CALL,
                      "brewster");
  policy->ProcessAction(action);

  action = new Action("punky",
                      mock_clock->Now() - base::TimeDelta::FromDays(2) -
                          base::TimeDelta::FromMinutes(20),
                      Action::ACTION_API_CALL,
                      "brewster");
  policy->ProcessAction(action);

  CheckReadData(policy,
                "punky",
                3,
                base::Bind(&CountingPolicyTest::Arguments_CheckMergeCount, 2));
  CheckReadData(policy,
                "punky",
                2,
                base::Bind(&CountingPolicyTest::Arguments_CheckMergeCount, 1));

  // Clean actions before midnight two days ago.  Force expiration to run by
  // clearing last_database_cleaning_time_ and submitting a new action.
  policy->set_retention_time(base::TimeDelta::FromDays(2));
  policy->last_database_cleaning_time_ = base::Time();
  action = new Action("punky",
                      mock_clock->Now(),
                      Action::ACTION_API_CALL,
                      "brewster");
  policy->ProcessAction(action);

  CheckReadData(policy,
                "punky",
                3,
                base::Bind(&CountingPolicyTest::Arguments_CheckMergeCount, 0));
  CheckReadData(policy,
                "punky",
                2,
                base::Bind(&CountingPolicyTest::Arguments_CheckMergeCount, 1));

  policy->Close();
}

// Test cleaning of old data in the string and URL tables.
TEST_F(CountingPolicyTest, StringTableCleaning) {
  CountingPolicy* policy = new CountingPolicy(profile_.get());
  // Initially disable expiration by setting a retention time before any
  // actions we generate.
  policy->set_retention_time(base::TimeDelta::FromDays(14));

  base::SimpleTestClock* mock_clock = new base::SimpleTestClock();
  mock_clock->SetNow(base::Time::Now());
  policy->SetClockForTesting(scoped_ptr<base::Clock>(mock_clock));

  // Insert an action; this should create entries in both the string table (for
  // the extension and API name) and the URL table (for page_url).
  scoped_refptr<Action> action =
      new Action("punky",
                 mock_clock->Now() - base::TimeDelta::FromDays(7),
                 Action::ACTION_API_CALL,
                 "brewster");
  action->set_page_url(GURL("http://www.google.com/"));
  policy->ProcessAction(action);

  // Add an action which will not be expired, so that some strings will remain
  // in use.
  action = new Action(
      "punky", mock_clock->Now(), Action::ACTION_API_CALL, "tabs.create");
  policy->ProcessAction(action);

  // There should now be three strings ("punky", "brewster", "tabs.create") and
  // one URL in the tables.
  policy->Flush();
  policy->ScheduleAndForget(policy,
                            &CountingPolicyTest::CheckStringTableSizes,
                            3,
                            1);
  WaitOnThread(BrowserThread::DB);

  // Trigger a cleaning.  The oldest action is expired when we submit a
  // duplicate of the newer action.  After this, there should be two strings
  // and no URLs.
  policy->set_retention_time(base::TimeDelta::FromDays(2));
  policy->last_database_cleaning_time_ = base::Time();
  policy->ProcessAction(action);
  policy->Flush();
  policy->ScheduleAndForget(policy,
                            &CountingPolicyTest::CheckStringTableSizes,
                            2,
                            0);
  WaitOnThread(BrowserThread::DB);

  policy->Close();
}

// A stress test for memory- and database-based merging of actions.  Submit
// multiple items, not in chronological order, spanning a few days.  Check that
// items are merged properly and final timestamps are correct.
TEST_F(CountingPolicyTest, MoreMerging) {
  CountingPolicy* policy = new CountingPolicy(profile_.get());
  policy->set_retention_time(base::TimeDelta::FromDays(14));

  // Use a mock clock to ensure that events are not recorded on the wrong day
  // when the test is run close to local midnight.
  base::SimpleTestClock* mock_clock = new base::SimpleTestClock();
  mock_clock->SetNow(base::Time::Now().LocalMidnight() +
                    base::TimeDelta::FromHours(12));
  policy->SetClockForTesting(scoped_ptr<base::Clock>(mock_clock));

  // Create an action 2 days ago, then 1 day ago, then 2 days ago.  Make sure
  // that we end up with two merged records (one for each day), and each has
  // the appropriate timestamp.  These merges should happen in the database
  // since the date keeps changing.
  base::Time time1 =
      mock_clock->Now() - base::TimeDelta::FromDays(2) -
      base::TimeDelta::FromMinutes(40);
  base::Time time2 =
      mock_clock->Now() - base::TimeDelta::FromDays(1) -
      base::TimeDelta::FromMinutes(40);
  base::Time time3 =
      mock_clock->Now() - base::TimeDelta::FromDays(2) -
      base::TimeDelta::FromMinutes(20);

  scoped_refptr<Action> action =
      new Action("punky", time1, Action::ACTION_API_CALL, "brewster");
  policy->ProcessAction(action);

  action = new Action("punky", time2, Action::ACTION_API_CALL, "brewster");
  policy->ProcessAction(action);

  action = new Action("punky", time3, Action::ACTION_API_CALL, "brewster");
  policy->ProcessAction(action);

  CheckReadData(
      policy,
      "punky",
      2,
      base::Bind(
          &CountingPolicyTest::Arguments_CheckMergeCountAndTime, 2, time3));
  CheckReadData(
      policy,
      "punky",
      1,
      base::Bind(
          &CountingPolicyTest::Arguments_CheckMergeCountAndTime, 1, time2));

  // Create three actions today, where the merges should happen in memory.
  // Again these are not chronological; timestamp time5 should win out since it
  // is the latest.
  base::Time time4 = mock_clock->Now() - base::TimeDelta::FromMinutes(60);
  base::Time time5 = mock_clock->Now() - base::TimeDelta::FromMinutes(20);
  base::Time time6 = mock_clock->Now() - base::TimeDelta::FromMinutes(40);

  action = new Action("punky", time4, Action::ACTION_API_CALL, "brewster");
  policy->ProcessAction(action);

  action = new Action("punky", time5, Action::ACTION_API_CALL, "brewster");
  policy->ProcessAction(action);

  action = new Action("punky", time6, Action::ACTION_API_CALL, "brewster");
  policy->ProcessAction(action);

  CheckReadData(
      policy,
      "punky",
      0,
      base::Bind(
          &CountingPolicyTest::Arguments_CheckMergeCountAndTime, 3, time5));
  policy->Close();
}

}  // namespace extensions
