// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/google_auto_login_helper.h"

#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "content/public/browser/notification_service.h"
#include "google_apis/gaia/gaia_auth_fetcher.h"
#include "google_apis/gaia/gaia_constants.h"

GoogleAutoLoginHelper::GoogleAutoLoginHelper(Profile* profile,
                                             Observer* observer)
    : profile_(profile) {
  if (observer)
    AddObserver(observer);
}

GoogleAutoLoginHelper::~GoogleAutoLoginHelper() {
  DCHECK(accounts_.empty());
}

void GoogleAutoLoginHelper::LogIn() {
  ProfileOAuth2TokenService* token_service =
      ProfileOAuth2TokenServiceFactory::GetForProfile(profile_);
  LogIn(token_service->GetPrimaryAccountId());
}

void GoogleAutoLoginHelper::LogIn(const std::string& account_id) {
  accounts_.push_back(account_id);
  if (accounts_.size() == 1)
    StartFetching();
}

void GoogleAutoLoginHelper::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void GoogleAutoLoginHelper::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void GoogleAutoLoginHelper::CancelAll() {
  gaia_auth_fetcher_.reset();
  uber_token_fetcher_.reset();
  accounts_.clear();
}

void GoogleAutoLoginHelper::OnUbertokenSuccess(const std::string& uber_token) {
  VLOG(1) << "GoogleAutoLoginHelper::OnUbertokenSuccess"
          << " account=" << accounts_.front();
  gaia_auth_fetcher_.reset(
      new GaiaAuthFetcher(this, GaiaConstants::kChromeSource,
                          profile_->GetRequestContext()));
  gaia_auth_fetcher_->StartMergeSession(uber_token);
}

void GoogleAutoLoginHelper::OnUbertokenFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "Failed to retrieve ubertoken"
          << " account=" << accounts_.front()
          << " error=" << error.ToString();
  const std::string account_id = accounts_.front();
  MergeNextAccount();
  SignalComplete(account_id, error);
}

void GoogleAutoLoginHelper::OnMergeSessionSuccess(const std::string& data) {
  DVLOG(1) << "MergeSession successful account=" << accounts_.front();
  const std::string account_id = accounts_.front();
  MergeNextAccount();
  SignalComplete(account_id, GoogleServiceAuthError::AuthErrorNone());
}

void GoogleAutoLoginHelper::OnMergeSessionFailure(
    const GoogleServiceAuthError& error) {
  VLOG(1) << "Failed MergeSession"
          << " account=" << accounts_.front()
          << " error=" << error.ToString();
  const std::string account_id = accounts_.front();
  MergeNextAccount();
  SignalComplete(account_id, error);
}

void GoogleAutoLoginHelper::StartFetching() {
  uber_token_fetcher_.reset(new UbertokenFetcher(profile_, this));
  uber_token_fetcher_->StartFetchingToken(accounts_.front());
}

void GoogleAutoLoginHelper::SignalComplete(
    const std::string& account_id,
    const GoogleServiceAuthError& error) {
  // Its possible for the observer to delete |this| object.  Don't access
  // access any members after this calling the observer.  This method should
  // be the last call in any other method.
  FOR_EACH_OBSERVER(Observer, observer_list_,
                    MergeSessionCompleted(account_id, error));
}

void GoogleAutoLoginHelper::MergeNextAccount() {
  gaia_auth_fetcher_.reset();
  accounts_.pop_front();
  if (accounts_.empty()) {
    uber_token_fetcher_.reset();
  } else {
    StartFetching();
  }
}
