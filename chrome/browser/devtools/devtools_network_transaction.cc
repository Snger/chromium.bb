// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_network_transaction.h"

#include "chrome/browser/devtools/devtools_network_controller.h"
#include "net/base/net_errors.h"
#include "net/base/upload_progress.h"
#include "net/http/http_network_transaction.h"
#include "net/http/http_request_info.h"

DevToolsNetworkTransaction::DevToolsNetworkTransaction(
    DevToolsNetworkController* controller,
    scoped_ptr<net::HttpTransaction> network_transaction)
    : controller_(controller),
      network_transaction_(network_transaction.Pass()),
      request_(NULL),
      failed_(false),
      throttled_byte_count_(0),
      callback_type_(NONE),
      proxy_callback_(base::Bind(&DevToolsNetworkTransaction::OnCallback,
                                 base::Unretained(this))) {
  DCHECK(controller);
  controller->AddTransaction(this);
}

DevToolsNetworkTransaction::~DevToolsNetworkTransaction() {
  controller_->RemoveTransaction(this);
}

void DevToolsNetworkTransaction::Throttle(int result) {
  throttled_result_ = result;

  if (callback_type_ == START)
    throttled_byte_count_ += network_transaction_->GetTotalReceivedBytes();
  if (result > 0)
    throttled_byte_count_ += result;

  controller_->ThrottleTransaction(this);
}

void DevToolsNetworkTransaction::OnCallback(int rv) {
  if (failed_)
    return;
  DCHECK(!callback_.is_null());
  if (callback_type_ == START || callback_type_ == READ) {
    if (controller_->ShouldThrottle(request_)) {
      Throttle(rv);
      return;
    }
  }
  net::CompletionCallback callback = callback_;
  callback_.Reset();
  callback_type_ = NONE;
  callback.Run(rv);
}

int DevToolsNetworkTransaction::SetupCallback(
    net::CompletionCallback callback,
    int result,
    CallbackType callback_type) {
  DCHECK(callback_type_ == NONE);

  if (result == net::ERR_IO_PENDING) {
    callback_type_ = callback_type;
    callback_ = callback;
    return result;
  }

  if (!controller_->ShouldThrottle(request_))
    return result;

  // Only START and READ operation throttling is supported.
  if (callback_type != START && callback_type != READ)
    return result;

  // In case of error |throttled_byte_count_| is unknown.
  if (result < 0)
    return result;

  // URLRequestJob relies on synchronous end-of-stream notification.
  if (callback_type == READ && result == 0)
    return result;

  callback_type_ = callback_type;
  callback_ = callback;
  Throttle(result);
  return net::ERR_IO_PENDING;
}

void DevToolsNetworkTransaction::Fail() {
  DCHECK(request_);
  DCHECK(!failed_);
  failed_ = true;
  network_transaction_->SetBeforeNetworkStartCallback(
      BeforeNetworkStartCallback());
  if (callback_.is_null())
    return;
  net::CompletionCallback callback = callback_;
  callback_.Reset();
  callback_type_ = NONE;
  callback.Run(net::ERR_INTERNET_DISCONNECTED);
}

int DevToolsNetworkTransaction::Start(
    const net::HttpRequestInfo* request,
    const net::CompletionCallback& callback,
    const net::BoundNetLog& net_log) {
  DCHECK(request);
  request_ = request;

  if (controller_->ShouldFail(request_)) {
    failed_ = true;
    network_transaction_->SetBeforeNetworkStartCallback(
        BeforeNetworkStartCallback());
    return net::ERR_INTERNET_DISCONNECTED;
  }
  int rv = network_transaction_->Start(request, proxy_callback_, net_log);
  return SetupCallback(callback, rv, START);
}

int DevToolsNetworkTransaction::RestartIgnoringLastError(
    const net::CompletionCallback& callback) {
  if (failed_)
    return net::ERR_INTERNET_DISCONNECTED;
  int rv = network_transaction_->RestartIgnoringLastError(proxy_callback_);
  return SetupCallback(callback, rv, RESTART_IGNORING_LAST_ERROR);
}

int DevToolsNetworkTransaction::RestartWithCertificate(
    net::X509Certificate* client_cert,
    const net::CompletionCallback& callback) {
  if (failed_)
    return net::ERR_INTERNET_DISCONNECTED;
  int rv = network_transaction_->RestartWithCertificate(
      client_cert, proxy_callback_);
  return SetupCallback(callback, rv, RESTART_WITH_CERTIFICATE);
}

int DevToolsNetworkTransaction::RestartWithAuth(
    const net::AuthCredentials& credentials,
    const net::CompletionCallback& callback) {
  if (failed_)
    return net::ERR_INTERNET_DISCONNECTED;
  int rv = network_transaction_->RestartWithAuth(credentials, proxy_callback_);
  return SetupCallback(callback, rv, RESTART_WITH_AUTH);
}

bool DevToolsNetworkTransaction::IsReadyToRestartForAuth() {
  return network_transaction_->IsReadyToRestartForAuth();
}

int DevToolsNetworkTransaction::Read(
    net::IOBuffer* buf,
    int buf_len,
    const net::CompletionCallback& callback) {
  if (failed_)
    return net::ERR_INTERNET_DISCONNECTED;
  int rv = network_transaction_->Read(buf, buf_len, proxy_callback_);
  return SetupCallback(callback, rv, READ);
}

void DevToolsNetworkTransaction::StopCaching() {
  network_transaction_->StopCaching();
}

bool DevToolsNetworkTransaction::GetFullRequestHeaders(
    net::HttpRequestHeaders* headers) const {
  return network_transaction_->GetFullRequestHeaders(headers);
}

int64 DevToolsNetworkTransaction::GetTotalReceivedBytes() const {
  return network_transaction_->GetTotalReceivedBytes();
}

void DevToolsNetworkTransaction::DoneReading() {
  network_transaction_->DoneReading();
}

const net::HttpResponseInfo*
DevToolsNetworkTransaction::GetResponseInfo() const {
  return network_transaction_->GetResponseInfo();
}

net::LoadState DevToolsNetworkTransaction::GetLoadState() const {
  return network_transaction_->GetLoadState();
}

net::UploadProgress DevToolsNetworkTransaction::GetUploadProgress() const {
  return network_transaction_->GetUploadProgress();
}

void DevToolsNetworkTransaction::SetQuicServerInfo(
    net::QuicServerInfo* quic_server_info) {
  network_transaction_->SetQuicServerInfo(quic_server_info);
}

bool DevToolsNetworkTransaction::GetLoadTimingInfo(
    net::LoadTimingInfo* load_timing_info) const {
  return network_transaction_->GetLoadTimingInfo(load_timing_info);
}

void DevToolsNetworkTransaction::SetPriority(net::RequestPriority priority) {
  network_transaction_->SetPriority(priority);
}

void DevToolsNetworkTransaction::SetWebSocketHandshakeStreamCreateHelper(
    net::WebSocketHandshakeStreamBase::CreateHelper* create_helper) {
  network_transaction_->SetWebSocketHandshakeStreamCreateHelper(create_helper);
}

void DevToolsNetworkTransaction::SetBeforeNetworkStartCallback(
    const BeforeNetworkStartCallback& callback) {
  network_transaction_->SetBeforeNetworkStartCallback(callback);
}

int DevToolsNetworkTransaction::ResumeNetworkStart() {
  if (failed_)
    return net::ERR_INTERNET_DISCONNECTED;
  return network_transaction_->ResumeNetworkStart();
}

void DevToolsNetworkTransaction::FireThrottledCallback() {
  DCHECK(!callback_.is_null());
  DCHECK(callback_type_ == READ || callback_type_ == START);
  net::CompletionCallback callback = callback_;
  callback_.Reset();
  callback_type_ = NONE;
  callback.Run(throttled_result_);
}
