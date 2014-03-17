// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/error_screen.h"

#include "chrome/browser/chromeos/login/screens/error_screen_actor.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/net/network_portal_detector.h"

namespace chromeos {

ErrorScreen::ErrorScreen(ScreenObserver* screen_observer,
                         ErrorScreenActor* actor)
    : WizardScreen(screen_observer),
      actor_(actor),
      parent_screen_(OobeDisplay::SCREEN_UNKNOWN) {
  DCHECK(actor_);
  actor_->SetDelegate(this);
  AddObserver(NetworkPortalDetector::Get());
}

ErrorScreen::~ErrorScreen() {
  actor_->SetDelegate(NULL);
  RemoveObserver(NetworkPortalDetector::Get());
}

void ErrorScreen::AddObserver(Observer* observer) {
  if (observer)
    observers_.AddObserver(observer);
}

void ErrorScreen::RemoveObserver(Observer* observer) {
  if (observer)
    observers_.RemoveObserver(observer);
}

void ErrorScreen::PrepareToShow() {
}

void ErrorScreen::Show() {
  DCHECK(actor_);
  actor_->Show(parent_screen(), NULL);
}

void ErrorScreen::Hide() {
  DCHECK(actor_);
  actor_->Hide();
}

std::string ErrorScreen::GetName() const {
  return WizardController::kErrorScreenName;
}

void ErrorScreen::OnErrorShow() {
  FOR_EACH_OBSERVER(Observer, observers_, OnErrorScreenShow());
}

void ErrorScreen::OnErrorHide() {
  FOR_EACH_OBSERVER(Observer, observers_, OnErrorScreenHide());
}

void ErrorScreen::FixCaptivePortal() {
  DCHECK(actor_);
  actor_->FixCaptivePortal();
}

void ErrorScreen::ShowCaptivePortal() {
  DCHECK(actor_);
  actor_->ShowCaptivePortal();
}

void ErrorScreen::HideCaptivePortal() {
  DCHECK(actor_);
  actor_->HideCaptivePortal();
}

void ErrorScreen::SetUIState(UIState ui_state) {
  DCHECK(actor_);
  actor_->SetUIState(ui_state);
}

ErrorScreen::UIState ErrorScreen::GetUIState() const {
  DCHECK(actor_);
  return actor_->ui_state();
}

void ErrorScreen::SetErrorState(ErrorState error_state,
                                const std::string& network) {
  DCHECK(actor_);
  actor_->SetErrorState(error_state, network);
}

void ErrorScreen::ShowConnectingIndicator(bool show) {
  DCHECK(actor_);
  actor_->ShowConnectingIndicator(show);
}

}  // namespace chromeos
