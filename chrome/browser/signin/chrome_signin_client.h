// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_
#define CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "components/signin/core/signin_client.h"

class CookieSettings;
class Profile;

class ChromeSigninClient : public SigninClient,
                           public BrowserContextKeyedService {
 public:
  explicit ChromeSigninClient(Profile* profile);
  virtual ~ChromeSigninClient();

  // Utility methods.
  static bool ProfileAllowsSigninCookies(Profile* profile);
  static bool SettingsAllowSigninCookies(CookieSettings* cookie_settings);

  // SigninClient implementation.
  virtual bool AreSigninCookiesAllowed() OVERRIDE;

 private:
  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(ChromeSigninClient);
};

#endif  // CHROME_BROWSER_SIGNIN_CHROME_SIGNIN_CLIENT_H_
