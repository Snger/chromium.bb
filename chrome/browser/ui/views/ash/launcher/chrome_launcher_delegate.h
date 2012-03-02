// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_CHROME_LAUNCHER_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_CHROME_LAUNCHER_DELEGATE_H_
#pragma once

#include <map>
#include <string>

#include "ash/launcher/launcher_delegate.h"
#include "ash/launcher/launcher_model_observer.h"
#include "ash/launcher/launcher_types.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace ash {
class LauncherModel;
}

class LauncherUpdater;
class LauncherUpdaterTest;
class PrefService;
class Profile;
class TabContentsWrapper;

// ChromeLauncherDelegate manages the launcher items needed for tabbed browsers
// and apps. It does this by way of LauncherUpdaters.
// TODO: rename this. ChromeLauncherDelegate is a poor name for what it actually
// does.
class ChromeLauncherDelegate : public ash::LauncherDelegate,
                               public ash::LauncherModelObserver,
                               public content::NotificationObserver {
 public:
  // Indicates what should happen when the app is launched.
  enum AppType {
    APP_TYPE_WINDOW,
    APP_TYPE_TAB
  };

  // Interface used to load app icons. This is in it's own class so that it can
  // be mocked.
  class AppIconLoader {
   public:
    virtual ~AppIconLoader() {}

    // Returns the app id of the specified tab, or an empty string if there is
    // no app.
    virtual std::string GetAppID(TabContentsWrapper* tab) = 0;

    // Returns true if |id| is valid. Used during restore to ignore no longer
    // valid extensions.
    virtual bool IsValidID(const std::string& id) = 0;

    // Fetches the image for the specified id. When done (which may be
    // synchronous), this should invoke SetAppImage() on the LauncherUpdater.
    virtual void FetchImage(const std::string& id) = 0;
  };

  ChromeLauncherDelegate(Profile* profile, ash::LauncherModel* model);
  virtual ~ChromeLauncherDelegate();

  // Initializes this ChromeLauncherDelegate.
  void Init();

  // Returns the single ChromeLauncherDelegate instnace.
  static ChromeLauncherDelegate* instance() { return instance_; }

  // Registers the prefs used by ChromeLauncherDelegate.
  static void RegisterUserPrefs(PrefService* user_prefs);

  // Creates a new tabbed item on the launcher for |updater|.
  ash::LauncherID CreateTabbedLauncherItem(LauncherUpdater* updater);

  // Creates a new app item on the launcher for |updater|. If there is an
  // existing pinned app that isn't running on the launcher, its id is returned.
  ash::LauncherID CreateAppLauncherItem(LauncherUpdater* updater,
                                        const std::string& app_id,
                                        AppType app_type);

  // Converts an app item to a tabbed item.
  void ConvertAppToTabbed(ash::LauncherID id);

  // Converts a tabbed item to an app item.
  void ConvertTabbedToApp(ash::LauncherID id,
                          const std::string& app_id,
                          AppType app_type);

  // Invoked when the underlying browser/app is closed. If the item isn't pinned
  // it's removed, otherwise the item says around so that the next time the user
  // launches the app it uses the existing item.
  void LauncherItemClosed(ash::LauncherID id);

  // Invoked when the id of an app changes.
  void AppIDChanged(ash::LauncherID id, const std::string& app_id);

  // Returns true if there is a closed item identified by the specified
  // arguments..
  bool HasClosedAppItem(const std::string& app_id, AppType app_type);

  // Pins the specified id.
  void Pin(ash::LauncherID id);

  // Unpins the specified id, closing if not running.
  void Unpin(ash::LauncherID id);

  // Returns true if the item identified by |id| is pinned.
  bool IsPinned(ash::LauncherID id);

  // Pins/unpins the specified id.
  void TogglePinned(ash::LauncherID id);

  // Returns true if the specified item can be pinned or unpinned. Only apps can
  // be pinned.
  bool IsPinnable(ash::LauncherID id);

  // Opens the specified item.
  void Open(ash::LauncherID id);

  // Closes the specified item.
  void Close(ash::LauncherID id);

  // Returns true if the specified item is open.
  bool IsOpen(ash::LauncherID id);

  // Returns the type of app for the specified id.
  AppType GetAppType(ash::LauncherID id);

  // Returns the id of the app for the specified tab.
  std::string GetAppID(TabContentsWrapper* tab);

  // Sets the image for an app tab. This is intended to be invoked from the
  // AppIconLoader.
  void SetAppImage(const std::string& app_id, SkBitmap* image);

  ash::LauncherModel* model() { return model_; }

  // ash::LauncherDelegate overrides:
  virtual void CreateNewWindow() OVERRIDE;
  virtual void ItemClicked(const ash::LauncherItem& item) OVERRIDE;
  virtual int GetBrowserShortcutResourceId() OVERRIDE;
  virtual string16 GetTitle(const ash::LauncherItem& item) OVERRIDE;
  virtual ui::MenuModel* CreateContextMenu(
      const ash::LauncherItem& item) OVERRIDE;

  // ash::LauncherModelObserver overrides:
  virtual void LauncherItemAdded(int index) OVERRIDE;
  virtual void LauncherItemRemoved(int index, ash::LauncherID id) OVERRIDE;
  virtual void LauncherItemMoved(int start_index, int target_index) OVERRIDE;
  virtual void LauncherItemChanged(int index,
                                   const ash::LauncherItem& old_item) OVERRIDE;
  virtual void LauncherItemWillChange(int index) OVERRIDE;

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
 private:
  friend class LauncherUpdaterTest;

  enum ItemType {
    TYPE_APP,
    TYPE_TABBED_BROWSER
  };

  // Used to identity an item on the launcher.
  struct Item {
    Item();
    ~Item();

    // Type of item.
    ItemType item_type;

    // If |item_type| is |TYPE_APP|, this identifies how the app is launched.
    AppType app_type;

    // ID of the app.
    std::string app_id;

    // The LauncherUpdater this item came from. NULL if pinned and not open.
    LauncherUpdater* updater;

    // Whether the item is pinned.
    bool pinned;
  };

  typedef std::map<ash::LauncherID, Item> IDToItemMap;

  // Updates the pinned pref state. The pinned state consists of a list pref.
  // Each item of the list is a dictionary. The key |kAppIDPath| gives the
  // id of the app. |kAppTypePath| is one of |kAppTypeTab| or |kAppTypeWindow|
  // and indicates how the app is opened.
  void PersistPinnedState();

  // Unpins any app items whose id is |app_id|.
  void UnpinAppsWithID(const std::string& app_id);

  // Sets the AppIconLoader, taking ownership of |loader|. This is intended for
  // testing.
  void SetAppIconLoaderForTest(AppIconLoader* loader);

  // Returns the profile used for new windows.
  Profile* GetProfileForNewWindows();

  static ChromeLauncherDelegate* instance_;

  ash::LauncherModel* model_;

  // Profile used for prefs and loading extensions. This is NOT necessarily the
  // profile new windows are created with.
  Profile* profile_;

  IDToItemMap id_to_item_map_;

  // Used to load the image for an app tab.
  scoped_ptr<AppIconLoader> app_icon_loader_;

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(ChromeLauncherDelegate);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_CHROME_LAUNCHER_DELEGATE_H_
