// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/managed_user_import_handler.h"

#include <set>

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/values.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/managed_mode/managed_user_sync_service.h"
#include "chrome/browser/managed_mode/managed_user_sync_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/web_ui.h"
#include "grit/generated_resources.h"

namespace options {

ManagedUserImportHandler::ManagedUserImportHandler() {}

ManagedUserImportHandler::~ManagedUserImportHandler() {}

void ManagedUserImportHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
      { "managedUserImportText", IDS_IMPORT_EXISTING_MANAGED_USER_TEXT },
      { "createNewUserLink", IDS_CREATE_NEW_USER_LINK },
      { "managedUserImportOk", IDS_IMPORT_EXISTING_MANAGED_USER_OK },
      { "managedUserAlreadyOnThisDevice",
          IDS_MANAGED_USER_ALREADY_ON_THIS_DEVICE },
      { "noExistingManagedUsers", IDS_MANAGED_USER_NO_EXISTING_ERROR },
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "managedUserImport",
                IDS_IMPORT_EXISTING_MANAGED_USER_TITLE);
}

void ManagedUserImportHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("requestExistingManagedUsers",
      base::Bind(&ManagedUserImportHandler::RequestExistingManagedUsers,
                 base::Unretained(this)));
}

void ManagedUserImportHandler::RequestExistingManagedUsers(
    const ListValue* args) {
  Profile* profile = Profile::FromWebUI(web_ui());
  if (profile->IsManaged())
    return;

  const ProfileInfoCache& cache =
      g_browser_process->profile_manager()->GetProfileInfoCache();
  std::set<std::string> managed_user_ids;
  for (size_t i = 0; i < cache.GetNumberOfProfiles(); ++i)
    managed_user_ids.insert(cache.GetManagedUserIdOfProfileAtIndex(i));

  const DictionaryValue* dict =
      ManagedUserSyncServiceFactory::GetForProfile(profile)->GetManagedUsers();
  ListValue managed_users;
  for (DictionaryValue::Iterator it(*dict); !it.IsAtEnd(); it.Advance()) {
    const DictionaryValue* value = NULL;
    bool success = it.value().GetAsDictionary(&value);
    DCHECK(success);
    std::string name;
    value->GetString("name", &name);

    DictionaryValue* managed_user = new DictionaryValue;
    managed_user->SetString("id", it.key());
    managed_user->SetString("name", name);

    // TODO(ibraaaa): Update this to use the correct avatar
    // when avatar syncing is implemented: http://crbug.com/278083
    std::string avatar_url = ProfileInfoCache::GetDefaultAvatarIconUrl(0);
    managed_user->SetString("iconURL", avatar_url);
    bool on_current_device =
        managed_user_ids.find(it.key()) != managed_user_ids.end();
    managed_user->SetBoolean("onCurrentDevice", on_current_device);

    managed_users.Append(managed_user);
  }

  web_ui()->CallJavascriptFunction(
      "ManagedUserImportOverlay.receiveExistingManagedUsers",
      managed_users);
}

}  // namespace options
