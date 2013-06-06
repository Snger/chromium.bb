// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/request_sender.h"

#include "base/bind.h"
#include "chrome/browser/google_apis/auth_service.h"
#include "chrome/browser/google_apis/base_requests.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

namespace google_apis {

RequestSender::RequestSender(
    Profile* profile,
    net::URLRequestContextGetter* url_request_context_getter,
    const std::vector<std::string>& scopes,
    const std::string& custom_user_agent)
    : profile_(profile),
      auth_service_(new AuthService(url_request_context_getter, scopes)),
      operation_registry_(new OperationRegistry()),
      custom_user_agent_(custom_user_agent),
      weak_ptr_factory_(this) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

RequestSender::~RequestSender() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void RequestSender::Initialize() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  auth_service_->Initialize(profile_);
}

void RequestSender::CancelAll() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  operation_registry_->CancelAll();
}

void RequestSender::StartRequestWithRetry(
    AuthenticatedRequestInterface* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!auth_service_->HasAccessToken()) {
    // Fetch OAuth2 access token from the refresh token first.
    auth_service_->StartAuthentication(
        base::Bind(&RequestSender::OnAccessTokenFetched,
                   weak_ptr_factory_.GetWeakPtr(),
                   request->GetWeakPtr()));
    return;
  }

  request->Start(auth_service_->access_token(),
                 custom_user_agent_,
                 base::Bind(&RequestSender::RetryRequest,
                            weak_ptr_factory_.GetWeakPtr()));
}

void RequestSender::OnAccessTokenFetched(
    const base::WeakPtr<AuthenticatedRequestInterface>& request,
    GDataErrorCode code,
    const std::string& /* access_token */) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  // Do nothing if the request is canceled during authentication.
  if (!request.get())
    return;

  if (code == HTTP_SUCCESS) {
    DCHECK(auth_service_->HasAccessToken());
    StartRequestWithRetry(request.get());
  } else {
    request->OnAuthFailed(code);
  }
}

void RequestSender::RetryRequest(
    AuthenticatedRequestInterface* request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  auth_service_->ClearAccessToken();
  // User authentication might have expired - rerun the request to force
  // auth token refresh.
  StartRequestWithRetry(request);
}

}  // namespace google_apis
