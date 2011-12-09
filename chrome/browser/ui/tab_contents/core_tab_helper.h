// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_H_
#define CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_H_
#pragma once

#include "chrome/browser/prefs/pref_change_registrar.h"
#include "content/browser/tab_contents/tab_contents_observer.h"

class CoreTabHelperDelegate;
class TabContentsWrapper;

// Per-tab class to handle functionality that is core to the operation of tabs.
class CoreTabHelper : public TabContentsObserver,
                      public content::NotificationObserver {
 public:
  explicit CoreTabHelper(TabContentsWrapper* wrapper);
  virtual ~CoreTabHelper();

  CoreTabHelperDelegate* delegate() const { return delegate_; }
  void set_delegate(CoreTabHelperDelegate* d) { delegate_ = d; }

  // Initial title assigned to NavigationEntries from Navigate.
  static string16 GetDefaultTitle();

  // Returns a human-readable description the tab's loading state.
  string16 GetStatusText() const;

 private:
  // TabContentsObserver overrides:
  virtual void DidBecomeSelected() OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Delegate for notifying our owner about stuff. Not owned by us.
  CoreTabHelperDelegate* delegate_;

  // Our owning TabContentsWrapper.
  TabContentsWrapper* wrapper_;

  PrefChangeRegistrar pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(CoreTabHelper);
};

#endif  // CHROME_BROWSER_UI_TAB_CONTENTS_CORE_TAB_HELPER_H_
