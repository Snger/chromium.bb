// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_
#pragma once

#include "chrome/browser/ui/webui/chromeos/login/login_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/base_screen_handler.h"
#include "content/browser/webui/web_ui.h"

namespace base {
class DictionaryValue;
class ListValue;
}

namespace chromeos {

// An interface for WebUILoginDisplay to call SigninScreenHandler.
class LoginDisplayWebUIHandler {
 public:
  virtual void ClearAndEnablePassword() = 0;
  virtual void ShowError(const std::string& error_text,
                         const std::string& help_link_text,
                         HelpAppLauncher::HelpTopic help_topic_id) = 0;
};

// An interface for SigninScreenHandler to call WebUILoginDisplay.
class SigninScreenHandlerDelegate {
 public:
  // Confirms sign up by provided |username| and |password| specified.
  // Used for new user login via GAIA extension.
  virtual void CompleteLogin(const std::string& username,
                             const std::string& password) = 0;

  // Sign in using |username| and |password| specified.
  // Used for both known and new users.
  virtual void Login(const std::string& username,
                     const std::string& password) = 0;

  // Sign in into Guest session.
  virtual void LoginAsGuest() = 0;

  // Let the delegate know about the handler it is supposed to be using.
  virtual void SetWebUIHandler(LoginDisplayWebUIHandler* webui_handler) = 0;
};

// A class that handles the WebUI hooks in sign-in screen in OobeDisplay
// and LoginDisplay.
class SigninScreenHandler : public BaseScreenHandler,
                            public LoginDisplayWebUIHandler {
 public:
  SigninScreenHandler();

  // Shows the sign in screen. |oobe_ui| indicates whether the signin
  // screen is for OOBE or usual sign-in flow.
  void Show(bool oobe_ui);

 private:
  // BaseScreenHandler implementation:
  virtual void GetLocalizedStrings(
      base::DictionaryValue* localized_strings) OVERRIDE;
  virtual void Initialize() OVERRIDE;

  // WebUIMessageHandler implementation:
  virtual void RegisterMessages() OVERRIDE;

  // BaseLoginUIHandler implementation.
  virtual void ClearAndEnablePassword() OVERRIDE;
  virtual void ShowError(const std::string& error_text,
                         const std::string& help_link_text,
                         HelpAppLauncher::HelpTopic help_topic_id) OVERRIDE;

  // Handles confirmation message of user authentication that was performed by
  // the authentication extension.
  void HandleCompleteLogin(const base::ListValue* args);

  // Handles get existing user list request when populating account picker.
  void HandleGetUsers(const base::ListValue* args);

  // Handles authentication request when signing in an existing user.
  void HandleAuthenticateUser(const base::ListValue* args);

  // Handles entering bwsi mode request.
  void HandleLaunchIncognito(const base::ListValue* args);

  // Handles system shutdown request.
  void HandleShutdownSystem(const base::ListValue* args);

  // Handles remove user request.
  void HandleRemoveUser(const base::ListValue* args);

  // Handles 'showAddUser' request to show proper sign-in screen.
  void HandleShowAddUser(const base::ListValue* args);

  // Sends user list to account picker.
  void SendUserList();

  // A delegate that glues this handler with backend LoginDisplay.
  SigninScreenHandlerDelegate* delegate_;

  // Whether screen should be shown right after initialization.
  bool show_on_init_;

  // Keeps whether screen should be shown for OOBE.
  bool oobe_ui_;

  // True if new user sign in flow is driven by the extension.
  bool extension_driven_;

  DISALLOW_COPY_AND_ASSIGN(SigninScreenHandler);
};

}  // namespae chromeoc

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_SIGNIN_SCREEN_HANDLER_H_
