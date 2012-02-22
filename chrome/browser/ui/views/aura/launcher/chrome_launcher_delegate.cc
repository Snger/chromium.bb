// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/aura/launcher/chrome_launcher_delegate.h"

#include "ash/launcher/launcher_model.h"
#include "ash/launcher/launcher_types.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/prefs/incognito_mode_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/aura/launcher/launcher_context_menu.h"
#include "chrome/browser/ui/views/aura/launcher/launcher_icon_loader.h"
#include "chrome/browser/ui/views/aura/launcher/launcher_updater.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_resource.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace {

// See description in PersistPinnedState().
const char kAppIDPath[] = "id";
const char kAppTypePath[] = "type";
const char kAppTypeTab[] = "tab";
const char kAppTypeWindow[] = "window";

}  // namespace

// ChromeLauncherDelegate::Item ------------------------------------------------

ChromeLauncherDelegate::Item::Item()
    : item_type(TYPE_TABBED_BROWSER),
      app_type(APP_TYPE_WINDOW),
      updater(NULL),
      pinned(false) {
}

ChromeLauncherDelegate::Item::~Item() {
}

// ChromeLauncherDelegate ------------------------------------------------------

// static
ChromeLauncherDelegate* ChromeLauncherDelegate::instance_ = NULL;

ChromeLauncherDelegate::ChromeLauncherDelegate(Profile* profile,
                                               ash::LauncherModel* model)
    : model_(model),
      profile_(profile) {
  if (!profile_) {
    // Use the original profile as on chromeos we may get a temporary off the
    // record profile.
    profile_ = ProfileManager::GetDefaultProfile()->GetOriginalProfile();
  }
  instance_ = this;
  model_->AddObserver(this);
  app_icon_loader_.reset(new LauncherIconLoader(profile_, this));
  registrar_.Add(this,
                 chrome::NOTIFICATION_EXTENSION_UNLOADED,
                 content::Source<Profile>(profile_));
}

ChromeLauncherDelegate::~ChromeLauncherDelegate() {
  model_->RemoveObserver(this);
  for (IDToItemMap::iterator i = id_to_item_map_.begin();
       i != id_to_item_map_.end(); ++i) {
    model_->RemoveItemAt(model_->ItemIndexByID(i->first));
  }
  if (instance_ == this)
    instance_ = NULL;
}

void ChromeLauncherDelegate::Init() {
  const base::ListValue* pinned_apps =
      profile_->GetPrefs()->GetList(prefs::kPinnedLauncherApps);
  for (size_t i = 0; i < pinned_apps->GetSize(); ++i) {
    DictionaryValue* app = NULL;
    if (pinned_apps->GetDictionary(i, &app)) {
      std::string app_id, type_string;
      if (app->GetString(kAppIDPath, &app_id) &&
          app->GetString(kAppTypePath, &type_string) &&
          app_icon_loader_->IsValidID(app_id)) {
        AppType app_type = (type_string == kAppTypeWindow) ?
            APP_TYPE_WINDOW : APP_TYPE_TAB;
        CreateAppLauncherItem(NULL, app_id, app_type);
      }
    }
  }
}

// static
void ChromeLauncherDelegate::RegisterUserPrefs(PrefService* user_prefs) {
  // TODO: If we want to support multiple profiles this will likely need to be
  // pushed to local state and we'll need to track profile per item.
  user_prefs->RegisterListPref(prefs::kPinnedLauncherApps,
                               PrefService::SYNCABLE_PREF);
}

ash::LauncherID ChromeLauncherDelegate::CreateTabbedLauncherItem(
    LauncherUpdater* updater) {
  // Tabbed items always get a new item. Put the tabbed item before the app
  // tabs. If there are no app tabs put it at the end.
  int index = static_cast<int>(model_->items().size());
  for (IDToItemMap::const_iterator i = id_to_item_map_.begin();
       i != id_to_item_map_.end(); ++i) {
    if (i->second.updater == updater) {
      DCHECK_EQ(TYPE_APP, i->second.item_type);
      index = std::min(index, model_->ItemIndexByID(i->first));
    }
  }
  ash::LauncherID id = model_->next_id();
  ash::LauncherItem item(ash::TYPE_TABBED);
  model_->Add(index, item);
  DCHECK(id_to_item_map_.find(id) == id_to_item_map_.end());
  id_to_item_map_[id].item_type = TYPE_TABBED_BROWSER;
  id_to_item_map_[id].updater = updater;
  return id;
}

ash::LauncherID ChromeLauncherDelegate::CreateAppLauncherItem(
    LauncherUpdater* updater,
    const std::string& app_id,
    AppType app_type) {
  // See if we have a closed item that matches the app.
  if (updater) {
    for (IDToItemMap::iterator i = id_to_item_map_.begin();
         i != id_to_item_map_.end(); ++i) {
      if (i->second.updater == NULL && i->second.app_id == app_id &&
          i->second.app_type == app_type) {
        i->second.updater = updater;
        return i->first;
      }
    }
  }

  // Newly created apps go after all existing apps. If there are no apps put it
  // at after the tabbed item, and if there is no tabbed item put it at the end.
  int item_count = static_cast<int>(model_->items().size());
  int min_app_index = item_count;
  int min_tab_index = min_app_index;
  if (updater) {
    for (IDToItemMap::const_iterator i = id_to_item_map_.begin();
         i != id_to_item_map_.end(); ++i) {
      if (i->second.updater == updater) {
        if (i->second.item_type == TYPE_APP) {
          min_app_index =
              std::min(min_app_index, model_->ItemIndexByID(i->first));
        } else {
          min_tab_index =
              std::min(min_app_index, model_->ItemIndexByID(i->first));
        }
      }
    }
  }
  int insert_index = min_app_index != item_count ?
      min_app_index : std::min(item_count, min_tab_index + 1);
  ash::LauncherID id = model_->next_id();
  ash::LauncherItem item(ash::TYPE_APP);
  model_->Add(insert_index, item);
  DCHECK(id_to_item_map_.find(id) == id_to_item_map_.end());
  id_to_item_map_[id].item_type = TYPE_APP;
  id_to_item_map_[id].app_type = app_type;
  id_to_item_map_[id].app_id = app_id;
  id_to_item_map_[id].updater = updater;
  id_to_item_map_[id].pinned = updater == NULL;

  app_icon_loader_->FetchImage(app_id);
  return id;
}

void ChromeLauncherDelegate::ConvertAppToTabbed(ash::LauncherID id) {
  DCHECK(id_to_item_map_.find(id) != id_to_item_map_.end());
  DCHECK_EQ(TYPE_APP, id_to_item_map_[id].item_type);
  DCHECK(!id_to_item_map_[id].pinned);
  id_to_item_map_[id].item_type = TYPE_TABBED_BROWSER;
  id_to_item_map_[id].app_id.clear();
}

void ChromeLauncherDelegate::ConvertTabbedToApp(ash::LauncherID id,
                                                const std::string& app_id,
                                                AppType app_type) {
  DCHECK(id_to_item_map_.find(id) != id_to_item_map_.end());
  DCHECK_EQ(TYPE_TABBED_BROWSER, id_to_item_map_[id].item_type);
  DCHECK(!id_to_item_map_[id].pinned);
  id_to_item_map_[id].item_type = TYPE_APP;
  id_to_item_map_[id].app_type = app_type;
  id_to_item_map_[id].app_id = app_id;

  ash::LauncherItem item(ash::TYPE_APP);
  item.id = id;
  model_->Set(model_->ItemIndexByID(id), item);

  app_icon_loader_->FetchImage(app_id);
}

void ChromeLauncherDelegate::LauncherItemClosed(ash::LauncherID id) {
  DCHECK(id_to_item_map_.find(id) != id_to_item_map_.end());
  if (id_to_item_map_[id].pinned) {
    // The item is pinned, leave it in the launcher.
    id_to_item_map_[id].updater = NULL;
  } else {
    id_to_item_map_.erase(id);
    model_->RemoveItemAt(model_->ItemIndexByID(id));
  }
}

void ChromeLauncherDelegate::AppIDChanged(ash::LauncherID id,
                                          const std::string& app_id) {
  DCHECK(id_to_item_map_.find(id) != id_to_item_map_.end());
  id_to_item_map_[id].app_id = app_id;
  PersistPinnedState();

  app_icon_loader_->FetchImage(app_id);
}

bool ChromeLauncherDelegate::HasClosedAppItem(const std::string& app_id,
                                              AppType app_type) {
  for (IDToItemMap::const_iterator i = id_to_item_map_.begin();
       i != id_to_item_map_.end(); ++i) {
    if (!i->second.updater && i->second.item_type == TYPE_APP &&
        i->second.app_type == app_type && i->second.app_id == app_id)
      return true;
  }
  return false;
}

void ChromeLauncherDelegate::Pin(ash::LauncherID id) {
  DCHECK(id_to_item_map_.find(id) != id_to_item_map_.end());
  id_to_item_map_[id].pinned = true;
  PersistPinnedState();
}

void ChromeLauncherDelegate::Unpin(ash::LauncherID id) {
  DCHECK(id_to_item_map_.find(id) != id_to_item_map_.end());
  id_to_item_map_[id].pinned = false;
  if (!id_to_item_map_[id].updater)
    LauncherItemClosed(id);
  PersistPinnedState();
}

bool ChromeLauncherDelegate::IsPinned(ash::LauncherID id) {
  DCHECK(id_to_item_map_.find(id) != id_to_item_map_.end());
  return id_to_item_map_[id].pinned;
}

void ChromeLauncherDelegate::TogglePinned(ash::LauncherID id) {
  if (id_to_item_map_.find(id) == id_to_item_map_.end())
    return;  // May happen if item closed with menu open.

  if (IsPinned(id))
    Unpin(id);
  else
    Pin(id);
}

bool ChromeLauncherDelegate::IsPinnable(ash::LauncherID id) {
  return id_to_item_map_.find(id) != id_to_item_map_.end() &&
      id_to_item_map_[id].item_type == TYPE_APP;
}

void ChromeLauncherDelegate::Open(ash::LauncherID id) {
  if (id_to_item_map_.find(id) == id_to_item_map_.end())
    return;  // In case invoked from menu and item closed while menu up.

  LauncherUpdater* updater = id_to_item_map_[id].updater;
  if (updater) {
    updater->window()->Show();
    ash::ActivateWindow(updater->window());
    TabContentsWrapper* tab = updater->GetTab(id);
    if (tab) {
      updater->tab_model()->ActivateTabAt(
          updater->tab_model()->GetIndexOfTabContents(tab), true);
    }
  } else {
    DCHECK_EQ(TYPE_APP, id_to_item_map_[id].item_type);
    if (id_to_item_map_[id].app_type == APP_TYPE_TAB) {
      const Extension* extension =
          profile_->GetExtensionService()->GetInstalledExtension(
              id_to_item_map_[id].app_id);
      DCHECK(extension);
      Browser::OpenApplicationTab(GetProfileForNewWindows(), extension, GURL(),
                                  NEW_FOREGROUND_TAB);
      if (id_to_item_map_[id].updater)
        id_to_item_map_[id].updater->window()->Show();
    } else {
      std::string app_name = web_app::GenerateApplicationNameFromExtensionId(
          id_to_item_map_[id].app_id);
      Browser* browser = Browser::CreateForApp(
          Browser::TYPE_POPUP, app_name, gfx::Rect(),
          GetProfileForNewWindows());
      browser->window()->Show();
    }
  }
}

void ChromeLauncherDelegate::Close(ash::LauncherID id) {
  if (id_to_item_map_.find(id) == id_to_item_map_.end())
    return;  // May happen if menu closed.

  if (!id_to_item_map_[id].updater)
    return;  // TODO: maybe should treat as unpin?

  TabContentsWrapper* tab = id_to_item_map_[id].updater->GetTab(id);
  if (tab) {
    content::WebContentsDelegate* delegate =
        tab->web_contents()->GetDelegate();
    if (delegate)
      delegate->CloseContents(tab->web_contents());
    else
      delete tab;
  } else {
    views::Widget* widget = views::Widget::GetWidgetForNativeView(
        id_to_item_map_[id].updater->window());
    if (widget)
      widget->Close();
  }
}

bool ChromeLauncherDelegate::IsOpen(ash::LauncherID id) {
  return id_to_item_map_.find(id) != id_to_item_map_.end() &&
      id_to_item_map_[id].updater != NULL;
}

ChromeLauncherDelegate::AppType ChromeLauncherDelegate::GetAppType(
    ash::LauncherID id) {
  DCHECK(id_to_item_map_.find(id) != id_to_item_map_.end());
  return id_to_item_map_[id].app_type;
}

std::string ChromeLauncherDelegate::GetAppID(TabContentsWrapper* tab) {
  return app_icon_loader_->GetAppID(tab);
}

void ChromeLauncherDelegate::SetAppImage(const std::string& id,
                                         SkBitmap* image) {
  for (IDToItemMap::const_iterator i = id_to_item_map_.begin();
       i != id_to_item_map_.end(); ++i) {
    if (i->second.app_id == id) {
      int index = model_->ItemIndexByID(i->first);
      ash::LauncherItem item = model_->items()[index];
      item.image = image ? *image : Extension::GetDefaultIcon(true);
      model_->Set(index, item);
      // It's possible we're waiting on more than one item, so don't break.
    }
  }
}

void ChromeLauncherDelegate::CreateNewWindow() {
  Browser::OpenEmptyWindow(GetProfileForNewWindows());
}

void ChromeLauncherDelegate::ItemClicked(const ash::LauncherItem& item) {
  DCHECK(id_to_item_map_.find(item.id) != id_to_item_map_.end());
  Open(item.id);
}

int ChromeLauncherDelegate::GetBrowserShortcutResourceId() {
  return IDR_PRODUCT_LOGO_32;
}

string16 ChromeLauncherDelegate::GetTitle(const ash::LauncherItem& item) {
  DCHECK(id_to_item_map_.find(item.id) != id_to_item_map_.end());
  LauncherUpdater* updater = id_to_item_map_[item.id].updater;
  if (updater) {
    if (id_to_item_map_[item.id].item_type == TYPE_TABBED_BROWSER) {
      return updater->tab_model()->GetActiveTabContents() ?
          updater->tab_model()->GetActiveTabContents()->web_contents()->
          GetTitle() : string16();
    }
    // Fall through to get title from extension.
  }
  const Extension* extension = profile_->GetExtensionService()->
      GetInstalledExtension(id_to_item_map_[item.id].app_id);
  return extension ? UTF8ToUTF16(extension->name()) : string16();
}

ui::MenuModel* ChromeLauncherDelegate::CreateContextMenu(
    const ash::LauncherItem& item) {
  return new LauncherContextMenu(this, item.id);
}

void ChromeLauncherDelegate::LauncherItemAdded(int index) {
}

void ChromeLauncherDelegate::LauncherItemRemoved(int index,
                                                 ash::LauncherID id) {
}

void ChromeLauncherDelegate::LauncherItemMoved(
    int start_index,
    int target_index) {
  ash::LauncherID id = model_->items()[target_index].id;
  if (id_to_item_map_.find(id) != id_to_item_map_.end() &&
      id_to_item_map_[id].pinned) {
    PersistPinnedState();
  }
}

void ChromeLauncherDelegate::LauncherItemChanged(
    int index,
    const ash::LauncherItem& old_item) {
}

void ChromeLauncherDelegate::LauncherItemWillChange(int index) {
}

void ChromeLauncherDelegate::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_EXTENSION_UNLOADED);
  const Extension* extension =
      content::Details<UnloadedExtensionInfo>(details)->extension;
  UnpinAppsWithID(extension->id());
}

void ChromeLauncherDelegate::PersistPinnedState() {
  ListPrefUpdate updater(profile_->GetPrefs(), prefs::kPinnedLauncherApps);
  updater.Get()->Clear();
  for (size_t i = 0; i < model_->items().size(); ++i) {
    if (model_->items()[i].type == ash::TYPE_APP) {
      ash::LauncherID id = model_->items()[i].id;
      if (id_to_item_map_.find(id) != id_to_item_map_.end() &&
          id_to_item_map_[id].pinned) {
        base::DictionaryValue* app_value = new base::DictionaryValue;
        app_value->SetString(kAppIDPath, id_to_item_map_[id].app_id);
        const char* app_type_string =
            id_to_item_map_[id].app_type == APP_TYPE_WINDOW ?
            kAppTypeWindow : kAppTypeTab;
        app_value->SetString(kAppTypePath, app_type_string);
        updater.Get()->Append(app_value);
      }
    }
  }
}

void ChromeLauncherDelegate::UnpinAppsWithID(const std::string& app_id) {
  for (IDToItemMap::iterator i = id_to_item_map_.begin();
       i != id_to_item_map_.end(); ) {
    IDToItemMap::iterator current(i);
    ++i;
    if (current->second.app_id == app_id && current->second.pinned)
      Unpin(current->first);
  }
}

void ChromeLauncherDelegate::SetAppIconLoaderForTest(AppIconLoader* loader) {
  app_icon_loader_.reset(loader);
}

Profile* ChromeLauncherDelegate::GetProfileForNewWindows() {
  Profile* profile = ProfileManager::GetDefaultProfile();
  if (browser_defaults::kAlwaysOpenIncognitoWindow &&
      IncognitoModePrefs::ShouldLaunchIncognito(
          *CommandLine::ForCurrentProcess(),
          profile->GetPrefs())) {
    profile = profile->GetOffTheRecordProfile();
  }
  return profile;
}
