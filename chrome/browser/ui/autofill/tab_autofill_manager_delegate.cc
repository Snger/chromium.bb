// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/tab_autofill_manager_delegate.h"

#include "base/logging.h"
#include "chrome/browser/autofill/password_generator.h"
#include "chrome/browser/infobars/infobar_tab_helper.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/url_constants.h"
#include "ui/gfx/rect.h"
#include "webkit/forms/password_form.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(TabAutofillManagerDelegate)

TabAutofillManagerDelegate::TabAutofillManagerDelegate(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {
  DCHECK(web_contents);
}

content::BrowserContext* TabAutofillManagerDelegate::GetBrowserContext() const {
  return web_contents_->GetBrowserContext();
}

content::BrowserContext*
TabAutofillManagerDelegate::GetOriginalBrowserContext() const {
  return GetOriginalProfile();
}

Profile* TabAutofillManagerDelegate::GetOriginalProfile() const {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext())->
      GetOriginalProfile();
}

InfoBarService* TabAutofillManagerDelegate::GetInfoBarService() {
  return TabContents::FromWebContents(web_contents_)->infobar_tab_helper();
}

PrefServiceBase* TabAutofillManagerDelegate::GetPrefs() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext())->
      GetPrefs();
}

ProfileSyncServiceBase* TabAutofillManagerDelegate::GetProfileSyncService() {
  return ProfileSyncServiceFactory::GetForProfile(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
}

bool TabAutofillManagerDelegate::IsSavingPasswordsEnabled() const {
  return TabContents::FromWebContents(web_contents_)->password_manager()->
      IsSavingEnabled();
}

void TabAutofillManagerDelegate::ShowAutofillSettings() {
#if defined(OS_ANDROID)
  NOTIMPLEMENTED();
#else
  Browser* browser = browser::FindBrowserWithWebContents(web_contents_);
  if (browser)
    chrome::ShowSettingsSubPage(browser, chrome::kAutofillSubPage);
#endif  // #if defined(OS_ANDROID)
}

void TabAutofillManagerDelegate::ShowPasswordGenerationBubble(
      const gfx::Rect& bounds,
      const webkit::forms::PasswordForm& form,
      autofill::PasswordGenerator* generator) {
#if defined(OS_ANDROID)
  NOTIMPLEMENTED();
#else
  Browser* browser = browser::FindBrowserWithWebContents(web_contents_);
  browser->window()->ShowPasswordGenerationBubble(bounds, form, generator);
#endif  // #if defined(OS_ANDROID)
}
