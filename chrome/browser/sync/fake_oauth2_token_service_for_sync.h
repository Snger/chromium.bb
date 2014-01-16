// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FAKE_OAUTH2_TOKEN_SERVICE_FOR_SYNC_H_
#define CHROME_BROWSER_SYNC_FAKE_OAUTH2_TOKEN_SERVICE_FOR_SYNC_H_

#include "chrome/browser/signin/profile_oauth2_token_service.h"

namespace content {
class BrowserContext;
}

class FakeOAuth2TokenServiceForSync : public ProfileOAuth2TokenService {
 public:
  static BrowserContextKeyedService* BuildTokenService(
      content::BrowserContext* context);

  virtual bool RefreshTokenIsAvailable(
      const std::string& account_id) OVERRIDE;

  virtual void UpdateCredentials(const std::string& account_id,
                                 const std::string& refresh_token) OVERRIDE;

 protected:
  virtual void FetchOAuth2Token(
      OAuth2TokenService::RequestImpl* request,
      const std::string& account_id,
      net::URLRequestContextGetter* getter,
      const std::string& client_id,
      const std::string& client_secret,
      const OAuth2TokenService::ScopeSet& scopes) OVERRIDE;

 private:
  std::string account_id_;
  std::string refresh_token_;
};

#endif  // CHROME_BROWSER_SYNC_FAKE_OAUTH2_TOKEN_SERVICE_FOR_SYNC_H_
