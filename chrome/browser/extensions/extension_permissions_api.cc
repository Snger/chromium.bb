// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_permissions_api.h"

#include "base/json/json_writer.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_permissions_api_constants.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/extensions/extension_permission_set.h"
#include "chrome/common/extensions/url_pattern_set.h"
#include "content/common/notification_service.h"
#include "googleurl/src/gurl.h"


namespace keys = extension_permissions_module_constants;

namespace {

enum AutoConfirmForTest {
  DO_NOT_SKIP = 0,
  PROCEED,
  ABORT
};
AutoConfirmForTest auto_confirm_for_tests = DO_NOT_SKIP;

DictionaryValue* PackPermissionsToValue(const ExtensionPermissionSet* set) {
  DictionaryValue* value = new DictionaryValue();

  // Generate the list of API permissions.
  ListValue* apis = new ListValue();
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  for (ExtensionAPIPermissionSet::const_iterator i = set->apis().begin();
       i != set->apis().end(); ++i)
    apis->Append(Value::CreateStringValue(info->GetByID(*i)->name()));

  // TODO(jstritar): Include hosts once the API supports them. At that point,
  // we could also shared this code with ExtensionPermissionSet methods in
  // ExtensionPrefs.

  value->Set(keys::kApisKey, apis);
  return value;
}

// Creates a new ExtensionPermissionSet from its |value| and passes ownership to
// the caller through |ptr|. Sets |bad_message| to true if the message is badly
// formed. Returns false if the method fails to unpack a permission set.
bool UnpackPermissionsFromValue(DictionaryValue* value,
                                scoped_refptr<ExtensionPermissionSet>* ptr,
                                bool* bad_message,
                                std::string* error) {
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  ExtensionAPIPermissionSet apis;
  if (value->HasKey(keys::kApisKey)) {
    ListValue* api_list = NULL;
    if (!value->GetList(keys::kApisKey, &api_list)) {
      *bad_message = true;
      return false;
    }
    for (size_t i = 0; i < api_list->GetSize(); ++i) {
      std::string api_name;
      if (!api_list->GetString(i, &api_name)) {
        *bad_message = true;
        return false;
      }

      ExtensionAPIPermission* permission = info->GetByName(api_name);
      if (!permission) {
        *error = base::StringPrintf(
            keys::kUnknownPermissionError, api_name.c_str());
        return false;
      }
      apis.insert(permission->id());
    }
  }

  // Ignore host permissions for now.
  URLPatternSet empty_set;
  *ptr = new ExtensionPermissionSet(apis, empty_set, empty_set);
  return true;
}

} // namespace

ExtensionPermissionsManager::ExtensionPermissionsManager(
    ExtensionService* extension_service)
    : extension_service_(extension_service) {
  RegisterWhitelist();
}

ExtensionPermissionsManager::~ExtensionPermissionsManager() {
}

void ExtensionPermissionsManager::AddPermissions(
    const Extension* extension, const ExtensionPermissionSet* permissions) {
  scoped_refptr<const ExtensionPermissionSet> existing(
      extension->GetActivePermissions());
  scoped_refptr<ExtensionPermissionSet> total(
      ExtensionPermissionSet::CreateUnion(existing, permissions));
  scoped_refptr<ExtensionPermissionSet> added(
      ExtensionPermissionSet::CreateDifference(total.get(), existing));

  extension_service_->UpdateActivePermissions(extension, total.get());

  // Update the granted permissions so we don't auto-disable the extension.
  extension_service_->GrantPermissions(extension);

  NotifyPermissionsUpdated(extension, total.get(), added.get(), ADDED);
}

void ExtensionPermissionsManager::RemovePermissions(
    const Extension* extension, const ExtensionPermissionSet* permissions) {
  scoped_refptr<const ExtensionPermissionSet> existing(
      extension->GetActivePermissions());
  scoped_refptr<ExtensionPermissionSet> total(
      ExtensionPermissionSet::CreateDifference(existing, permissions));
  scoped_refptr<ExtensionPermissionSet> removed(
      ExtensionPermissionSet::CreateDifference(existing, total.get()));

  // We update the active permissions, and not the granted permissions, because
  // the extension, not the user, removed the permissions. This allows the
  // extension to add them again without prompting the user.
  extension_service_->UpdateActivePermissions(extension, total.get());

  NotifyPermissionsUpdated(extension, total.get(), removed.get(), REMOVED);
}

void ExtensionPermissionsManager::DispatchEvent(
    const std::string& extension_id,
    const char* event_name,
    const ExtensionPermissionSet* changed_permissions) {
  Profile* profile = extension_service_->profile();
  if (profile && profile->GetExtensionEventRouter()) {
    ListValue value;
    value.Append(PackPermissionsToValue(changed_permissions));
    std::string json_value;
    base::JSONWriter::Write(&value, false, &json_value);
    profile->GetExtensionEventRouter()->DispatchEventToExtension(
        extension_id, event_name, json_value, profile, GURL());
  }
}

void ExtensionPermissionsManager::NotifyPermissionsUpdated(
    const Extension* extension,
    const ExtensionPermissionSet* active,
    const ExtensionPermissionSet* changed,
    EventType event_type) {
  if (!changed || changed->IsEmpty())
    return;

  UpdatedExtensionPermissionsInfo::Reason reason;
  const char* event_name = NULL;

  if (event_type == REMOVED) {
    reason = UpdatedExtensionPermissionsInfo::REMOVED;
    event_name = keys::kOnRemoved;
  } else {
    CHECK_EQ(ADDED, event_type);
    reason = UpdatedExtensionPermissionsInfo::ADDED;
    event_name = keys::kOnAdded;
  }

  // Notify other APIs or interested parties.
  UpdatedExtensionPermissionsInfo info = UpdatedExtensionPermissionsInfo(
      extension, changed, reason);
  NotificationService::current()->Notify(
      chrome::NOTIFICATION_EXTENSION_PERMISSIONS_UPDATED,
      Source<Profile>(extension_service_->profile()),
      Details<UpdatedExtensionPermissionsInfo>(&info));

  // Trigger the onAdded and onRemoved events in the extension.
  DispatchEvent(extension->id(), event_name, changed);

  // Send the new permissions to the renderers.
  for (RenderProcessHost::iterator i(RenderProcessHost::AllHostsIterator());
       !i.IsAtEnd(); i.Advance()) {
    RenderProcessHost* host = i.GetCurrentValue();
    if (extension_service_->profile()->IsSameProfile(host->profile()))
      host->Send(new ExtensionMsg_UpdatePermissions(
          extension->id(),
          active->apis(),
          active->explicit_hosts(),
          active->scriptable_hosts()));
  }
}

void ExtensionPermissionsManager::RegisterWhitelist() {
  // TODO(jstritar): This could be a field on ExtensionAPIPermission.
  ExtensionAPIPermissionSet api_whitelist;
  api_whitelist.insert(ExtensionAPIPermission::kClipboardRead);
  api_whitelist.insert(ExtensionAPIPermission::kClipboardWrite);
  api_whitelist.insert(ExtensionAPIPermission::kNotification);
  api_whitelist.insert(ExtensionAPIPermission::kBookmark);
  api_whitelist.insert(ExtensionAPIPermission::kContextMenus);
  api_whitelist.insert(ExtensionAPIPermission::kCookie);
  api_whitelist.insert(ExtensionAPIPermission::kDebugger);
  api_whitelist.insert(ExtensionAPIPermission::kHistory);
  api_whitelist.insert(ExtensionAPIPermission::kIdle);
  api_whitelist.insert(ExtensionAPIPermission::kTab);
  api_whitelist.insert(ExtensionAPIPermission::kManagement);
  api_whitelist.insert(ExtensionAPIPermission::kBackground);
  whitelist_ = new ExtensionPermissionSet(
      api_whitelist, URLPatternSet(), URLPatternSet());
}

bool ContainsPermissionsFunction::RunImpl() {
  DictionaryValue* args = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));
  std::string error;
  if (!args)
    return false;

  scoped_refptr<ExtensionPermissionSet> permissions;
  if (!UnpackPermissionsFromValue(args, &permissions, &bad_message_, &error_))
    return false;
  CHECK(permissions.get());

  result_.reset(Value::CreateBooleanValue(
      GetExtension()->GetActivePermissions()->Contains(*permissions)));
  return true;
}

bool GetAllPermissionsFunction::RunImpl() {
  result_.reset(PackPermissionsToValue(
      GetExtension()->GetActivePermissions()));
  return true;
}

bool RemovePermissionsFunction::RunImpl() {
  DictionaryValue* args = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));
  if (!args)
    return false;

  scoped_refptr<ExtensionPermissionSet> permissions;
  if (!UnpackPermissionsFromValue(args, &permissions, &bad_message_, &error_))
    return false;
  CHECK(permissions.get());

  const Extension* extension = GetExtension();
  ExtensionPermissionsManager* perms_manager =
      profile()->GetExtensionService()->permissions_manager();
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();

  // Make sure they're only trying to remove permissions supported by this API.
  scoped_refptr<ExtensionPermissionSet> unsupported(
      ExtensionPermissionSet::CreateDifference(
          permissions.get(), &perms_manager->whitelist()));
  if (unsupported->apis().size()) {
    std::string api_name = info->GetByID(*unsupported->apis().begin())->name();
    error_ = base::StringPrintf(keys::kNotWhitelistedError, api_name.c_str());
    return false;
  }

  // Make sure we don't remove any required pemissions.
  const ExtensionPermissionSet* required = extension->required_permission_set();
  scoped_refptr<ExtensionPermissionSet> intersection(
      ExtensionPermissionSet::CreateIntersection(permissions.get(), required));
  if (!intersection->IsEmpty()) {
    error_ = keys::kCantRemoveRequiredPermissionsError;
    result_.reset(Value::CreateBooleanValue(false));
    return false;
  }

  perms_manager->RemovePermissions(extension, permissions.get());
  result_.reset(Value::CreateBooleanValue(true));
  return true;
}

// static
void RequestPermissionsFunction::SetAutoConfirmForTests(bool should_proceed) {
  auto_confirm_for_tests = should_proceed ? PROCEED : ABORT;
}

RequestPermissionsFunction::RequestPermissionsFunction() {}
RequestPermissionsFunction::~RequestPermissionsFunction() {}

bool RequestPermissionsFunction::RunImpl() {
  DictionaryValue* args = NULL;
  EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &args));
  if (!args)
    return false;

  if (!UnpackPermissionsFromValue(
          args, &requested_permissions_, &bad_message_, &error_))
    return false;
  CHECK(requested_permissions_.get());

  extension_ = GetExtension();
  ExtensionPermissionsInfo* info = ExtensionPermissionsInfo::GetInstance();
  ExtensionPermissionsManager* perms_manager =
      profile()->GetExtensionService()->permissions_manager();
  ExtensionPrefs* prefs = profile()->GetExtensionService()->extension_prefs();

  // Make sure only white listed permissions have been requested.
  scoped_refptr<ExtensionPermissionSet> unsupported(
      ExtensionPermissionSet::CreateDifference(
          requested_permissions_.get(), &perms_manager->whitelist()));
  if (unsupported->apis().size()) {
    std::string api_name = info->GetByID(*unsupported->apis().begin())->name();
    error_ = base::StringPrintf(keys::kNotWhitelistedError, api_name.c_str());
    return false;
  }

  // The requested permissions must be defined as optional in the manifest.
  if (!extension_->optional_permission_set()->Contains(
          *requested_permissions_)) {
    error_ = keys::kNotInOptionalPermissionsError;
    result_.reset(Value::CreateBooleanValue(false));
    return false;
  }

  // We don't need to prompt the user if the requested permissions are a subset
  // of the granted permissions set.
  const ExtensionPermissionSet* granted =
      prefs->GetGrantedPermissions(extension_->id());
  if (granted && granted->Contains(*requested_permissions_)) {
    perms_manager->AddPermissions(extension_, requested_permissions_.get());
    result_.reset(Value::CreateBooleanValue(true));
    SendResponse(true);
    return true;
  }

  // Filter out the granted permissions so we only prompt for new ones.
  requested_permissions_ = ExtensionPermissionSet::CreateDifference(
      requested_permissions_.get(), granted);

  // Balanced with Release() in InstallUIProceed() and InstallUIAbort().
  AddRef();

  // We don't need to show the prompt if there are no new warnings, or if
  // we're skipping the confirmation UI. All extension types but INTERNAL
  // are allowed to silently increase their permission level.
  if (auto_confirm_for_tests == PROCEED ||
      requested_permissions_->GetWarningMessages().size() == 0) {
    InstallUIProceed();
  } else if (auto_confirm_for_tests == ABORT) {
    // Pretend the user clicked cancel.
    InstallUIAbort(true);
  } else {
    CHECK_EQ(DO_NOT_SKIP, auto_confirm_for_tests);
    install_ui_.reset(new ExtensionInstallUI(profile()));
    install_ui_->ConfirmPermissions(
        this, extension_, requested_permissions_.get());
  }

  return true;
}

void RequestPermissionsFunction::InstallUIProceed() {
  ExtensionPermissionsManager* perms_manager =
      profile()->GetExtensionService()->permissions_manager();

  install_ui_.reset();
  result_.reset(Value::CreateBooleanValue(true));
  perms_manager->AddPermissions(extension_, requested_permissions_.get());

  SendResponse(true);

  Release();
}

void RequestPermissionsFunction::InstallUIAbort(bool user_initiated) {
  install_ui_.reset();
  result_.reset(Value::CreateBooleanValue(false));
  requested_permissions_ = NULL;

  SendResponse(true);
  Release();
}
