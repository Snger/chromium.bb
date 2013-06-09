// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>
#include <vector>
#include "base/command_line.h"
#include "base/json/json_string_value_serializer.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/extensions/activity_log/activity_log.h"
#include "chrome/browser/extensions/activity_log/api_actions.h"
#include "chrome/browser/extensions/activity_log/blocked_actions.h"
#include "chrome/browser/extensions/api/activity_log_private/activity_log_private_api.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/extensions/extension_system_factory.h"
#include "chrome/browser/extensions/install_tracker_factory.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "third_party/re2/re2/re2.h"

namespace {

// Concatenate arguments.
std::string MakeArgList(const ListValue* args) {
  std::string call_signature;
  ListValue::const_iterator it = args->begin();
  for (; it != args->end(); ++it) {
    std::string arg;
    JSONStringValueSerializer serializer(&arg);
    if (serializer.SerializeAndOmitBinaryValues(**it)) {
      if (it != args->begin())
        call_signature += ", ";
      call_signature += arg;
    }
  }
  return call_signature;
}

// This is a hack for AL callers who don't have access to a profile object
// when deciding whether or not to do the work required for logging. The state
// is accessed through the static ActivityLog::IsLogEnabledOnAnyProfile()
// method. It returns true if --enable-extension-activity-logging is set on the
// command line OR *ANY* profile has the activity log whitelisted extension
// installed.
class LogIsEnabled {
 public:
  LogIsEnabled() : any_profile_enabled_(false) {
    ComputeIsFlagEnabled();
  }

  void ComputeIsFlagEnabled() {
    base::AutoLock auto_lock(lock_);
    cmd_line_enabled_ = CommandLine::ForCurrentProcess()->
        HasSwitch(switches::kEnableExtensionActivityLogging);
  }

  static LogIsEnabled* GetInstance() {
    return Singleton<LogIsEnabled>::get();
  }

  bool IsEnabled() {
    base::AutoLock auto_lock(lock_);
    return cmd_line_enabled_ || any_profile_enabled_;
  }

  void SetProfileEnabled(bool any_profile_enabled) {
    base::AutoLock auto_lock(lock_);
    any_profile_enabled_ = any_profile_enabled;
  }

 private:
  base::Lock lock_;
  bool any_profile_enabled_;
  bool cmd_line_enabled_;
};

}  // namespace

namespace extensions {

// static
bool ActivityLog::IsLogEnabledOnAnyProfile() {
  return LogIsEnabled::GetInstance()->IsEnabled();
}

// static
void ActivityLog::RecomputeLoggingIsEnabled(bool profile_enabled) {
  LogIsEnabled::GetInstance()->ComputeIsFlagEnabled();
  LogIsEnabled::GetInstance()->SetProfileEnabled(profile_enabled);
}

// ActivityLogFactory

ActivityLogFactory* ActivityLogFactory::GetInstance() {
  return Singleton<ActivityLogFactory>::get();
}

BrowserContextKeyedService* ActivityLogFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new ActivityLog(static_cast<Profile*>(profile));
}

content::BrowserContext* ActivityLogFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}

ActivityLogFactory::ActivityLogFactory()
    : BrowserContextKeyedServiceFactory(
        "ActivityLog",
        BrowserContextDependencyManager::GetInstance()) {
  DependsOn(ExtensionSystemFactory::GetInstance());
  DependsOn(InstallTrackerFactory::GetInstance());
}

ActivityLogFactory::~ActivityLogFactory() {
}

// ActivityLog

// Use GetInstance instead of directly creating an ActivityLog.
ActivityLog::ActivityLog(Profile* profile)
    : profile_(profile),
      first_time_checking_(true),
      has_threads_(true) {
  enabled_ = IsLogEnabledOnAnyProfile();

  // enable-extension-activity-log-testing
  // This controls whether arguments are collected.
  // It also controls whether logging statements are printed.
  testing_mode_ = CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableExtensionActivityLogTesting);
  if (!testing_mode_) {
    for (int i = 0; i < APIAction::kSizeAlwaysLog; i++) {
      arg_whitelist_api_.insert(std::string(APIAction::kAlwaysLog[i]));
    }
  }

  // Check that the right threads exist. If not, we shouldn't try to do things
  // that require them.
  if (!BrowserThread::IsMessageLoopValid(BrowserThread::DB) ||
      !BrowserThread::IsMessageLoopValid(BrowserThread::FILE) ||
      !BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
    LOG(ERROR) << "Missing threads, disabling Activity Logging!";
    has_threads_ = false;
  }

  observers_ = new ObserverListThreadSafe<Observer>;

  // We initialize the database whether or not the AL is enabled, since we might
  // be enabled later on. If the database cannot be initialized for some
  // reason, we keep chugging along but nothing will get recorded. If the UI is
  // available, things will still get sent to the UI even if nothing
  // is being written to the database.
  db_ = new ActivityDatabase();
  if (!has_threads_) return;
  base::FilePath base_dir = profile->GetPath();
  base::FilePath database_name = base_dir.Append(
      chrome::kExtensionActivityLogFilename);
  ScheduleAndForget(&ActivityDatabase::Init, database_name);
}

void ActivityLog::Shutdown() {
  if (tracker_ && !first_time_checking_) tracker_->RemoveObserver(this);
}

ActivityLog::~ActivityLog() {
  if (has_threads_)
    ScheduleAndForget(&ActivityDatabase::Close);
  else
    db_->Close();
}

// We can't register for the InstallTrackerFactory events or talk to the
// extension service in the constructor, so we do that here the first time
// this is called (as identified by first_time_checking_).
bool ActivityLog::IsLogEnabled() {
  if (!first_time_checking_) return enabled_;
  if (!has_threads_) return false;
  tracker_ = InstallTrackerFactory::GetForProfile(profile_);
  tracker_->AddObserver(this);
  const ExtensionService* extension_service =
      ExtensionSystem::Get(profile_)->extension_service();
  if (extension_service->IsExtensionEnabled(
      kActivityLogExtensionId)) {
    enabled_ = true;
    LogIsEnabled::GetInstance()->SetProfileEnabled(true);
  }
  first_time_checking_ = false;
  return enabled_;
}

void ActivityLog::OnExtensionInstalled(const Extension* extension) {
  if (extension->id() != kActivityLogExtensionId) return;
  enabled_ = true;
  LogIsEnabled::GetInstance()->SetProfileEnabled(true);
}

void ActivityLog::OnExtensionUninstalled(const Extension* extension) {
  if (extension->id() != kActivityLogExtensionId) return;
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableExtensionActivityLogging))
    enabled_ = false;
}

// Counter-intuitively, OnExtensionInstalled is also called when an extension
// is reenabled.
void ActivityLog::OnExtensionDisabled(const Extension* extension) {
  if (extension->id() != kActivityLogExtensionId) return;
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableExtensionActivityLogging))
    enabled_ = false;
}

void ActivityLog::SetArgumentLoggingForTesting(bool log_arguments) {
  testing_mode_ = log_arguments;
}

// static
ActivityLog* ActivityLog::GetInstance(Profile* profile) {
  return ActivityLogFactory::GetForProfile(profile);
}

void ActivityLog::AddObserver(ActivityLog::Observer* observer) {
  observers_->AddObserver(observer);
}

void ActivityLog::RemoveObserver(ActivityLog::Observer* observer) {
  observers_->RemoveObserver(observer);
}

void ActivityLog::LogAPIActionInternal(const std::string& extension_id,
                                       const std::string& api_call,
                                       ListValue* args,
                                       const std::string& extra,
                                       const APIAction::Type type) {
  std::string verb, manager;
  bool matches = RE2::FullMatch(api_call, "(.*?)\\.(.*)", &manager, &verb);
  if (matches) {
    if (!args->empty() && manager == "tabs") {
      APIAction::LookupTabId(api_call, args, profile_);
    }
    scoped_refptr<APIAction> action = new APIAction(
        extension_id,
        base::Time::Now(),
        type,
        api_call,
        MakeArgList(args),
        extra);
    ScheduleAndForget(&ActivityDatabase::RecordAction, action);
    observers_->Notify(&Observer::OnExtensionActivity, action);
    if (testing_mode_) LOG(INFO) << action->PrintForDebug();
  } else {
    LOG(ERROR) << "Unknown API call! " << api_call;
  }
}

// A wrapper around LogAPIActionInternal, but we know it's an API call.
void ActivityLog::LogAPIAction(const std::string& extension_id,
                               const std::string& api_call,
                               ListValue* args,
                               const std::string& extra) {
  if (!IsLogEnabled() ||
      ActivityLogAPI::IsExtensionWhitelisted(extension_id)) return;
  if (!testing_mode_ &&
      arg_whitelist_api_.find(api_call) == arg_whitelist_api_.end())
    args->Clear();
  LogAPIActionInternal(extension_id,
                       api_call,
                       args,
                       extra,
                       APIAction::CALL);
}

// A wrapper around LogAPIActionInternal, but we know it's actually an event
// being fired and triggering extension code. Having the two separate methods
// (LogAPIAction vs LogEventAction) lets us hide how we actually choose to
// handle them. Right now they're being handled almost the same.
void ActivityLog::LogEventAction(const std::string& extension_id,
                                 const std::string& api_call,
                                 ListValue* args,
                                 const std::string& extra) {
  if (!IsLogEnabled() ||
      ActivityLogAPI::IsExtensionWhitelisted(extension_id)) return;
  if (!testing_mode_ &&
      arg_whitelist_api_.find(api_call) == arg_whitelist_api_.end())
    args->Clear();
  LogAPIActionInternal(extension_id,
                       api_call,
                       args,
                       extra,
                       APIAction::EVENT_CALLBACK);
}

void ActivityLog::LogBlockedAction(const std::string& extension_id,
                                   const std::string& blocked_call,
                                   ListValue* args,
                                   BlockedAction::Reason reason,
                                   const std::string& extra) {
  if (!IsLogEnabled() ||
      ActivityLogAPI::IsExtensionWhitelisted(extension_id)) return;
  if (!testing_mode_ &&
      arg_whitelist_api_.find(blocked_call) == arg_whitelist_api_.end())
    args->Clear();
  scoped_refptr<BlockedAction> action = new BlockedAction(extension_id,
                                                          base::Time::Now(),
                                                          blocked_call,
                                                          MakeArgList(args),
                                                          reason,
                                                          extra);
  ScheduleAndForget(&ActivityDatabase::RecordAction, action);
  observers_->Notify(&Observer::OnExtensionActivity, action);
  if (testing_mode_) LOG(INFO) << action->PrintForDebug();
}

void ActivityLog::LogDOMAction(const std::string& extension_id,
                               const GURL& url,
                               const string16& url_title,
                               const std::string& api_call,
                               const ListValue* args,
                               DomActionType::Type call_type,
                               const std::string& extra) {
  if (!IsLogEnabled() ||
      ActivityLogAPI::IsExtensionWhitelisted(extension_id)) return;
  if (call_type == DomActionType::METHOD && api_call == "XMLHttpRequest.open")
    call_type = DomActionType::XHR;
  scoped_refptr<DOMAction> action = new DOMAction(
      extension_id,
      base::Time::Now(),
      call_type,
      url,
      url_title,
      api_call,
      MakeArgList(args),
      extra);
  ScheduleAndForget(&ActivityDatabase::RecordAction, action);
  observers_->Notify(&Observer::OnExtensionActivity, action);
  if (testing_mode_) LOG(INFO) << action->PrintForDebug();
}

void ActivityLog::LogWebRequestAction(const std::string& extension_id,
                                      const GURL& url,
                                      const std::string& api_call,
                                      scoped_ptr<DictionaryValue> details,
                                      const std::string& extra) {
  string16 null_title;
  if (!IsLogEnabled() ||
      ActivityLogAPI::IsExtensionWhitelisted(extension_id)) return;

  // Strip details of the web request modifications (for privacy reasons),
  // unless testing is enabled.
  if (!testing_mode_) {
    DictionaryValue::Iterator details_iterator(*details);
    while (!details_iterator.IsAtEnd()) {
      details->SetBoolean(details_iterator.key(), true);
      details_iterator.Advance();
    }
  }
  std::string details_string;
  JSONStringValueSerializer serializer(&details_string);
  serializer.SerializeAndOmitBinaryValues(*details);

  scoped_refptr<DOMAction> action = new DOMAction(
      extension_id,
      base::Time::Now(),
      DomActionType::WEBREQUEST,
      url,
      null_title,
      api_call,
      details_string,
      extra);
  ScheduleAndForget(&ActivityDatabase::RecordAction, action);
  observers_->Notify(&Observer::OnExtensionActivity, action);
  if (testing_mode_) LOG(INFO) << action->PrintForDebug();
}

void ActivityLog::GetActions(
    const std::string& extension_id,
    const int day,
    const base::Callback
        <void(scoped_ptr<std::vector<scoped_refptr<Action> > >)>& callback) {
  if (!has_threads_) return;
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::DB,
      FROM_HERE,
      base::Bind(&ActivityDatabase::GetActions,
                 base::Unretained(db_),
                 extension_id,
                 day),
      callback);
}

void ActivityLog::OnScriptsExecuted(
    const content::WebContents* web_contents,
    const ExecutingScriptsMap& extension_ids,
    int32 on_page_id,
    const GURL& on_url) {
  if (!IsLogEnabled()) return;
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  const ExtensionService* extension_service =
      ExtensionSystem::Get(profile)->extension_service();
  const ExtensionSet* extensions = extension_service->extensions();

  for (ExecutingScriptsMap::const_iterator it = extension_ids.begin();
       it != extension_ids.end(); ++it) {
    const Extension* extension = extensions->GetByID(it->first);
    if (!extension || ActivityLogAPI::IsExtensionWhitelisted(extension->id()))
      continue;

    // If OnScriptsExecuted is fired because of tabs.executeScript, the list
    // of content scripts will be empty.  We don't want to log it because
    // the call to tabs.executeScript will have already been logged anyway.
    if (!it->second.empty()) {
      std::string ext_scripts_str;
      for (std::set<std::string>::const_iterator it2 = it->second.begin();
           it2 != it->second.end();
           ++it2) {
        ext_scripts_str += *it2;
        ext_scripts_str += " ";
      }
      scoped_ptr<ListValue> script_names(new ListValue());
      script_names->Set(0, new StringValue(ext_scripts_str));
      LogDOMAction(extension->id(),
                   on_url,
                   web_contents->GetTitle(),
                   std::string(),  // no api call here
                   script_names.get(),
                   DomActionType::INSERTED,
                   std::string());  // no extras either
    }
  }
}

}  // namespace extensions
