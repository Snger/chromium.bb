// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENTERPRISE_OAUTH_ENROLLMENT_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENTERPRISE_OAUTH_ENROLLMENT_SCREEN_HANDLER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/login/enterprise_enrollment_screen_actor.h"
#include "chrome/browser/net/gaia/gaia_oauth_consumer.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"

class GaiaOAuthFetcher;

namespace chromeos {

// WebUIMessageHandler implementation which handles events occurring on the
// page, such as the user pressing the signin button.
class EnterpriseOAuthEnrollmentScreenHandler
    : public BaseScreenHandler,
      public EnterpriseEnrollmentScreenActor,
      public GaiaOAuthConsumer {
 public:
  EnterpriseOAuthEnrollmentScreenHandler();
  virtual ~EnterpriseOAuthEnrollmentScreenHandler();

  // Implements WebUIMessageHandler:
  virtual void RegisterMessages() OVERRIDE;

  // Implements EnterpriseEnrollmentScreenActor:
  virtual void SetController(Controller* controller);
  virtual void PrepareToShow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void SetEditableUser(bool editable) OVERRIDE;
  virtual void ShowConfirmationScreen() OVERRIDE;
  virtual void ShowAuthError(const GoogleServiceAuthError& error) OVERRIDE;
  virtual void ShowAccountError() OVERRIDE;
  virtual void ShowFatalAuthError() OVERRIDE;
  virtual void ShowFatalEnrollmentError() OVERRIDE;
  virtual void ShowNetworkEnrollmentError() OVERRIDE;

  // Implements BaseScreenHandler:
  virtual void GetLocalizedStrings(
      base::DictionaryValue* localized_strings) OVERRIDE;

  // Implements GaiaOAuthConsumer:
  virtual void OnGetOAuthTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnOAuthGetAccessTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnOAuthWrapBridgeSuccess(const std::string& service_scope,
                                        const std::string& token,
                                        const std::string& expires_in) OVERRIDE;
  virtual void OnOAuthWrapBridgeFailure(
      const std::string& service_scope,
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnUserInfoSuccess(const std::string& email) OVERRIDE;
  virtual void OnUserInfoFailure(const GoogleServiceAuthError& error) OVERRIDE;

 protected:
  // Implements BaseScreenHandler:
  virtual void Initialize() OVERRIDE;

  // Keeps the controller for this actor.
  Controller* controller_;

 private:
  // Handlers for WebUI messages.
  void HandleClose(const base::ListValue* args);
  void HandleCompleteLogin(const base::ListValue* args);
  void HandleRetry(const base::ListValue* args);

  // Shows a given enrollment step.
  void ShowStep(const char* step);

  // Display the given i18n string as error message.
  void ShowError(int message_id, bool retry);

  // Resets the authentication machinery and clears cookies, so other screens
  // (like the actual login screen) find a clean slate and don't pick up our
  // auth state.
  void ResetAuth();

  bool editable_user_;
  bool show_on_init_;

  // Username of the user signing in.
  std::string user_;

  // This intentionally lives here and not in the controller, since it needs to
  // execute requests in the context of the profile that displays the webui.
  scoped_ptr<GaiaOAuthFetcher> oauth_fetcher_;

  DISALLOW_COPY_AND_ASSIGN(EnterpriseOAuthEnrollmentScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_ENTERPRISE_OAUTH_ENROLLMENT_SCREEN_HANDLER_H_
