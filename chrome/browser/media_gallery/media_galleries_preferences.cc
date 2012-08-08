// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media_gallery/media_galleries_preferences.h"

#include "base/command_line.h"
#include "base/path_service.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/media_gallery/media_file_system_registry.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"

namespace chrome {

namespace {

const char kMediaGalleriesDeviceIdKey[] = "deviceId";
const char kMediaGalleriesDisplayNameKey[] = "displayName";
const char kMediaGalleriesPathKey[] = "path";
const char kMediaGalleriesPrefIdKey[] = "prefId";
const char kMediaGalleriesTypeKey[] = "type";
const char kMediaGalleriesTypeAutoDetectedValue[] = "autoDetected";
const char kMediaGalleriesTypeUserAddedValue[] = "userAdded";
const char kMediaGalleriesTypeBlackListedValue[] = "blackListed";

bool GetPrefId(const DictionaryValue& dict, MediaGalleryPrefId* value) {
  std::string string_id;
  if (!dict.GetString(kMediaGalleriesPrefIdKey, &string_id) ||
      !base::StringToUint64(string_id, value)) {
    return false;
  }

  return true;
}

bool GetType(const DictionaryValue& dict, MediaGalleryPrefInfo::Type* type) {
  std::string string_type;
  if (!dict.GetString(kMediaGalleriesTypeKey, &string_type))
    return false;

  if (string_type == kMediaGalleriesTypeAutoDetectedValue) {
    *type = MediaGalleryPrefInfo::kAutoDetected;
    return true;
  }
  if (string_type == kMediaGalleriesTypeUserAddedValue) {
    *type = MediaGalleryPrefInfo::kUserAdded;
    return true;
  }
  if (string_type == kMediaGalleriesTypeBlackListedValue) {
    *type = MediaGalleryPrefInfo::kBlackListed;
    return true;
  }

  return false;
}

bool PopulateGalleryPrefInfoFromDictionary(
    const DictionaryValue& dict, MediaGalleryPrefInfo* out_gallery_info) {
  MediaGalleryPrefId pref_id;
  string16 display_name;
  std::string device_id;
  FilePath::StringType path;
  MediaGalleryPrefInfo::Type type = MediaGalleryPrefInfo::kAutoDetected;
  if (!GetPrefId(dict, &pref_id) ||
      !dict.GetString(kMediaGalleriesDisplayNameKey, &display_name) ||
      !dict.GetString(kMediaGalleriesDeviceIdKey, &device_id) ||
      !dict.GetString(kMediaGalleriesPathKey, &path) ||
      !GetType(dict, &type)) {
    return false;
  }

  out_gallery_info->pref_id = pref_id;
  out_gallery_info->display_name = display_name;
  out_gallery_info->device_id = device_id;
  out_gallery_info->path = FilePath(path);
  out_gallery_info->type = type;
  return true;
}

DictionaryValue* CreateGalleryPrefInfoDictionary(
    const MediaGalleryPrefInfo& gallery) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString(kMediaGalleriesPrefIdKey,
                  base::Uint64ToString(gallery.pref_id));
  dict->SetString(kMediaGalleriesDisplayNameKey, gallery.display_name);
  dict->SetString(kMediaGalleriesDeviceIdKey, gallery.device_id);
  dict->SetString(kMediaGalleriesPathKey, gallery.path.value());
  const char* type = NULL;
  switch (gallery.type) {
    case MediaGalleryPrefInfo::kAutoDetected:
      type = kMediaGalleriesTypeAutoDetectedValue;
      break;
    case MediaGalleryPrefInfo::kUserAdded:
      type = kMediaGalleriesTypeUserAddedValue;
      break;
    case MediaGalleryPrefInfo::kBlackListed:
      type = kMediaGalleriesTypeBlackListedValue;
      break;
    default:
      NOTREACHED();
      break;
  }
  dict->SetString(kMediaGalleriesTypeKey, type);
  return dict;
}

bool FindPrefIdFromDeviceId(const MediaGalleriesPrefInfoMap& known_galleries,
                            const std::string& device_id,
                            MediaGalleryPrefId* pref_id)  {
  // TODO(vandebo) Handle multiple galleries that use different paths.
  // TODO(vandebo) Should we keep a second map device_id->pref_id?
  for (MediaGalleriesPrefInfoMap::const_iterator it = known_galleries.begin();
       it != known_galleries.end();
       ++it) {
    if (it->second.device_id == device_id) {
      if (pref_id)
        *pref_id = it->second.pref_id;
      return true;
    }
  }
  return false;
}

FilePath MakePathRelative(FilePath path) {
  if (!path.IsAbsolute())
    return path;

  FilePath relative;
  std::vector<FilePath::StringType> components;
  path.GetComponents(&components);

  // On Windows, the first component may be the drive letter with the second
  // being \\.
  int start = 1;
  if (components[1].size() == 1 && FilePath::IsSeparator(components[1][0]))
    start = 2;

  for (size_t i = start; i < components.size(); i++)
    relative = relative.Append(components[i]);
  return relative;
}

}  // namespace

MediaGalleryPrefInfo::MediaGalleryPrefInfo()
    : pref_id(kInvalidMediaGalleryPrefId) {
}
MediaGalleryPrefInfo::~MediaGalleryPrefInfo() {}

MediaGalleriesPreferences::MediaGalleriesPreferences(Profile* profile)
    : profile_(profile) {
  DCHECK(UserInteractionIsEnabled());

  // Populate the default galleries if this is a fresh profile.
  MediaGalleryPrefId current_id =
      profile_->GetPrefs()->GetUint64(prefs::kMediaGalleriesUniqueId);
  if (current_id == kInvalidMediaGalleryPrefId + 1) {
    FilePath pictures_path;
    if (PathService::Get(chrome::DIR_USER_PICTURES, &pictures_path)) {
      std::string device_id = MediaFileSystemRegistry::GetInstance()->
          GetDeviceIdFromPath(pictures_path);
      string16 display_name = ComputeDisplayName(pictures_path);
      AddGallery(device_id, display_name, pictures_path, false /*user added*/);
    }
  }
  InitFromPrefs();
}

MediaGalleriesPreferences::~MediaGalleriesPreferences() {}

void MediaGalleriesPreferences::InitFromPrefs() {
  known_galleries_.clear();

  PrefService* prefs = profile_->GetPrefs();
  const ListValue* list = prefs->GetList(
      prefs::kMediaGalleriesRememberedGalleries);
  if (!list)
    return;

  for (ListValue::const_iterator it = list->begin(); it != list->end(); ++it) {
    const DictionaryValue* dict = NULL;
    if (!(*it)->GetAsDictionary(&dict))
      continue;

    MediaGalleryPrefInfo gallery_info;
    if (PopulateGalleryPrefInfoFromDictionary(*dict, &gallery_info))
      known_galleries_[gallery_info.pref_id] = gallery_info;
  }
}

bool MediaGalleriesPreferences::LookUpGalleryByPath(
    const FilePath& path,
    MediaGalleryPrefInfo* gallery_info) const {
  std::string device_id =
      MediaFileSystemRegistry::GetInstance()->GetDeviceIdFromPath(path);
  MediaGalleryPrefId pref_id;
  if (!FindPrefIdFromDeviceId(known_galleries_, device_id, &pref_id)) {
    if (gallery_info) {
      gallery_info->pref_id = kInvalidMediaGalleryPrefId;
      gallery_info->display_name = ComputeDisplayName(path);
      gallery_info->device_id = device_id;
      gallery_info->path = MakePathRelative(path);
      gallery_info->type = MediaGalleryPrefInfo::kUserAdded;
    }
    return false;
  }

  if (gallery_info) {
    MediaGalleriesPrefInfoMap::const_iterator it =
        known_galleries_.find(pref_id);
    DCHECK(it != known_galleries_.end());
    *gallery_info = it->second;
  }
  return true;
}

MediaGalleryPrefId MediaGalleriesPreferences::AddGallery(
    const std::string& device_id, const string16& display_name,
    const FilePath& path, bool user_added) {
  DCHECK(display_name.length() > 0);
  MediaGalleryPrefId existing_id;
  if (FindPrefIdFromDeviceId(known_galleries_, device_id, &existing_id)) {
    const MediaGalleryPrefInfo& existing = known_galleries_[existing_id];
    if (existing.type == MediaGalleryPrefInfo::kBlackListed) {
      PrefService* prefs = profile_->GetPrefs();
      ListPrefUpdate update(prefs, prefs::kMediaGalleriesRememberedGalleries);
      ListValue* list = update.Get();

      for (ListValue::const_iterator it = list->begin();
           it != list->end();
           ++it) {
        DictionaryValue* dict;
        MediaGalleryPrefId iter_id;
        if ((*it)->GetAsDictionary(&dict) &&
            GetPrefId(*dict, &iter_id) &&
            existing_id == iter_id) {
          dict->SetString(kMediaGalleriesTypeKey,
                          kMediaGalleriesTypeAutoDetectedValue);
          InitFromPrefs();
          break;
        }
      }
    }
    return existing_id;
  }

  FilePath relative_path = MakePathRelative(path);
  PrefService* prefs = profile_->GetPrefs();

  MediaGalleryPrefInfo gallery_info;
  gallery_info.pref_id = prefs->GetUint64(prefs::kMediaGalleriesUniqueId);
  prefs->SetUint64(prefs::kMediaGalleriesUniqueId, gallery_info.pref_id + 1);
  gallery_info.display_name = display_name;
  gallery_info.device_id = device_id;
  gallery_info.path = relative_path;
  gallery_info.type = MediaGalleryPrefInfo::kAutoDetected;
  if (user_added)
    gallery_info.type = MediaGalleryPrefInfo::kUserAdded;

  ListPrefUpdate update(prefs, prefs::kMediaGalleriesRememberedGalleries);
  ListValue* list = update.Get();
  list->Append(CreateGalleryPrefInfoDictionary(gallery_info));
  InitFromPrefs();

  return gallery_info.pref_id;
}

MediaGalleryPrefId MediaGalleriesPreferences::AddGalleryByPath(
    const FilePath& path) {
  std::string device_id =
      MediaFileSystemRegistry::GetInstance()->GetDeviceIdFromPath(path);
  string16 display_name = ComputeDisplayName(path);
  return AddGallery(device_id, display_name, path, true);
}

void MediaGalleriesPreferences::ForgetGalleryById(MediaGalleryPrefId pref_id) {
  PrefService* prefs = profile_->GetPrefs();
  ListPrefUpdate update(prefs, prefs::kMediaGalleriesRememberedGalleries);
  ListValue* list = update.Get();

  for (ListValue::iterator iter = list->begin(); iter != list->end(); ++iter) {
    DictionaryValue* dict;
    MediaGalleryPrefId iter_id;
    if ((*iter)->GetAsDictionary(&dict) && GetPrefId(*dict, &iter_id) &&
        pref_id == iter_id) {
      GetExtensionPrefs()->RemoveMediaGalleryPermissions(pref_id);
      MediaGalleryPrefInfo::Type type;
      if (GetType(*dict, &type) &&
          type == MediaGalleryPrefInfo::kAutoDetected) {
        dict->SetString(kMediaGalleriesTypeKey,
                        kMediaGalleriesTypeBlackListedValue);
      } else {
        list->Erase(iter, NULL);
      }
      InitFromPrefs();
      return;
    }
  }
}

std::set<MediaGalleryPrefId>
MediaGalleriesPreferences::GalleriesForExtension(
    const extensions::Extension& extension) const {
  std::set<MediaGalleryPrefId> result;
  if (extension.HasAPIPermission(
          extensions::APIPermission::kMediaGalleriesAllGalleries)) {
    for (MediaGalleriesPrefInfoMap::const_iterator it =
             known_galleries_.begin(); it != known_galleries_.end(); ++it) {
      if (it->second.type == MediaGalleryPrefInfo::kAutoDetected)
        result.insert(it->second.pref_id);
    }
  }

  std::vector<MediaGalleryPermission> stored_permissions =
      GetExtensionPrefs()->GetMediaGalleryPermissions(extension.id());

  for (std::vector<MediaGalleryPermission>::const_iterator it =
           stored_permissions.begin(); it != stored_permissions.end(); ++it) {
    if (!it->has_permission) {
      result.erase(it->pref_id);
    } else {
      MediaGalleriesPrefInfoMap::const_iterator gallery =
          known_galleries_.find(it->pref_id);
      DCHECK(gallery != known_galleries_.end());
      if (gallery->second.type != MediaGalleryPrefInfo::kBlackListed) {
        result.insert(it->pref_id);
      } else {
        NOTREACHED() << gallery->second.device_id;
      }
    }
  }
  return result;
}

void MediaGalleriesPreferences::SetGalleryPermissionForExtension(
    const extensions::Extension& extension,
    MediaGalleryPrefId pref_id,
    bool has_permission) {
  bool all_permission = extension.HasAPIPermission(
      extensions::APIPermission::kMediaGalleriesAllGalleries);
  if (has_permission && all_permission) {
    MediaGalleriesPrefInfoMap::iterator gallery_info =
        known_galleries_.find(pref_id);
    DCHECK(gallery_info != known_galleries_.end());
    if (gallery_info->second.type == MediaGalleryPrefInfo::kAutoDetected) {
      GetExtensionPrefs()->UnsetMediaGalleryPermission(extension.id(), pref_id);
      return;
    }
  }

  if (!has_permission && !all_permission) {
    GetExtensionPrefs()->UnsetMediaGalleryPermission(extension.id(), pref_id);
  } else {
    GetExtensionPrefs()->SetMediaGalleryPermission(extension.id(), pref_id,
                                                   has_permission);
  }
}

void MediaGalleriesPreferences::Shutdown() {
  profile_ = NULL;
}

// static
bool MediaGalleriesPreferences::UserInteractionIsEnabled() {
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kEnableMediaGalleryUI);
}

// static
string16 MediaGalleriesPreferences::ComputeDisplayName(const FilePath& path) {
  // Assumes that path is a directory and not a file.
  return path.BaseName().LossyDisplayName();
}

// static
void MediaGalleriesPreferences::RegisterUserPrefs(PrefService* prefs) {
  if (!UserInteractionIsEnabled())
    return;

  prefs->RegisterListPref(prefs::kMediaGalleriesRememberedGalleries,
                          PrefService::UNSYNCABLE_PREF);
  prefs->RegisterUint64Pref(prefs::kMediaGalleriesUniqueId,
                            kInvalidMediaGalleryPrefId + 1,
                            PrefService::UNSYNCABLE_PREF);
}

extensions::ExtensionPrefs*
MediaGalleriesPreferences::GetExtensionPrefs() const {
  ExtensionService* extension_service =
      extensions::ExtensionSystem::Get(profile_)->extension_service();
  return extension_service->extension_prefs();
}

} // namespace chrome
