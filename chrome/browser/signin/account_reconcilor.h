// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_SIGNIN_ACCOUNT_RECONCILOR_H_
#define CHROME_BROWSER_SIGNIN_ACCOUNT_RECONCILOR_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "google_apis/gaia/oauth2_token_service.h"

class Profile;

class AccountReconcilor : public BrowserContextKeyedService,
                                 OAuth2TokenService::Observer {
 public:
  AccountReconcilor(Profile* profile);
  virtual ~AccountReconcilor();

  // BrowserContextKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  Profile* profile() { return profile_; }

 private:
  // The profile that this reconcilor belongs to.
  Profile* profile_;

  void PerformMergeAction(const std::string& account_id);
  void PerformRemoveAction(const std::string& account_id);
  void PerformReconcileAction();

  // Overriden from OAuth2TokenService::Observer
  virtual void OnRefreshTokenAvailable(const std::string& account_id) OVERRIDE;
  virtual void OnRefreshTokenRevoked(const std::string& account_id) OVERRIDE;
  virtual void OnRefreshTokensLoaded() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AccountReconcilor);
};

#endif  // CHROME_BROWSER_SIGNIN_ACCOUNT_RECONCILOR_H_
