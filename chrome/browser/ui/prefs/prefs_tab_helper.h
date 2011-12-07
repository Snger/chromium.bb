// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PREFS_PREFS_TAB_HELPER_H_
#define CHROME_BROWSER_UI_PREFS_PREFS_TAB_HELPER_H_
#pragma once

#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/browser/tab_contents/tab_contents_observer.h"
#include "content/public/browser/notification_registrar.h"

class PrefService;
class TabContentsWrapper;
struct WebPreferences;

// Per-tab class to handle user preferences.
class PrefsTabHelper : public TabContentsObserver,
                       public content::NotificationObserver {
 public:
  explicit PrefsTabHelper(TabContentsWrapper* tab_contents);
  virtual ~PrefsTabHelper();

  static void RegisterUserPrefs(PrefService* prefs);

  PrefService* per_tab_prefs() { return per_tab_prefs_.get(); }

 protected:
  // Update the RenderView's WebPreferences. Exposed as protected for testing.
  virtual void UpdateWebPreferences();

  // TabContentsObserver overrides, exposed as protected for testing.
  virtual void RenderViewCreated(RenderViewHost* render_view_host) OVERRIDE;

 private:
  // TabContentsObserver overrides:
  virtual void TabContentsDestroyed(TabContents* tab) OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Update the TabContents's RendererPreferences.
  void UpdateRendererPreferences();

  // Our owning TabContentsWrapper.
  TabContentsWrapper* wrapper_;

  content::NotificationRegistrar registrar_;

  scoped_ptr<PrefService> per_tab_prefs_;
  PrefChangeRegistrar pref_change_registrar_;
  PrefChangeRegistrar per_tab_pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(PrefsTabHelper);
};

#endif  // CHROME_BROWSER_UI_PREFS_PREFS_TAB_HELPER_H_
