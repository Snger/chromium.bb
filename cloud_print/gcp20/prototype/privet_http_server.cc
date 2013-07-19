// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/privet_http_server.h"

#include "base/json/json_writer.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/socket/tcp_listen_socket.h"
#include "url/gurl.h"

namespace {

// {"error":|error_type|}
scoped_ptr<base::DictionaryValue> CreateError(const std::string& error_type) {
  scoped_ptr<base::DictionaryValue> error(new base::DictionaryValue);
  error->SetString("error", error_type);

  return error.Pass();
}

// {"error":|error_type|, "description":|description|}
scoped_ptr<base::DictionaryValue> CreateErrorWithDescription(
    const std::string& error_type,
    const std::string& description) {
  scoped_ptr<base::DictionaryValue> error(CreateError(error_type));
  error->SetString("description", description);
  return error.Pass();
}

// {"error":|error_type|, "timeout":|timout|}
scoped_ptr<base::DictionaryValue> CreateErrorWithTimeout(
    const std::string& error_type,
    int timeout) {
  scoped_ptr<base::DictionaryValue> error(CreateError(error_type));
  error->SetInteger("timeout", timeout);
  return error.Pass();
}

}  // namespace

PrivetHttpServer::DeviceInfo::DeviceInfo() {
}

PrivetHttpServer::DeviceInfo::~DeviceInfo() {
}

PrivetHttpServer::PrivetHttpServer(Delegate* delegate)
    : port_(0),
      delegate_(delegate) {
}

PrivetHttpServer::~PrivetHttpServer() {
  Shutdown();
}

bool PrivetHttpServer::Start(uint16 port) {
  if (server_)
    return true;

  net::TCPListenSocketFactory factory("0.0.0.0", port);
  server_ = new net::HttpServer(factory, this);
  net::IPEndPoint address;

  if (server_->GetLocalAddress(&address) != net::OK) {
    NOTREACHED() << "Cannot start HTTP server";
  } else {
    VLOG(1) << "Address of HTTP server: " << address.ToString();
  }

  return true;
}

void PrivetHttpServer::Shutdown() {
  if (!server_)
    return;

  server_ = NULL;
}

void PrivetHttpServer::OnHttpRequest(int connection_id,
                                     const net::HttpServerRequestInfo& info) {
  VLOG(1) << "Processing HTTP request: " << info.path;
  GURL url("http://host" + info.path);

  // TODO(maksymb): Add checking for X-PrivetToken.
  std::string response;
  net::HttpStatusCode status_code = ProcessHttpRequest(url, &response);

  server_->Send(connection_id, status_code, response, "text/plain");
}

void PrivetHttpServer::OnWebSocketRequest(
    int connection_id, const net::HttpServerRequestInfo& info) {
}

void PrivetHttpServer::OnWebSocketMessage(int connection_id,
                                          const std::string& data) {
}

void PrivetHttpServer::OnClose(int connection_id) {
}

net::HttpStatusCode PrivetHttpServer::ProcessHttpRequest(
    const GURL& url,
    std::string* response) {
  net::HttpStatusCode status_code = net::HTTP_OK;
  scoped_ptr<base::DictionaryValue> json_response;

  if (url.path() == "/privet/info") {
    json_response = ProcessInfo(&status_code);
  } else if (url.path() == "/privet/register") {
    json_response = ProcessRegister(url, &status_code);
  } else {
    response->clear();
    return net::HTTP_NOT_FOUND;
  }

  if (!json_response) {
    response->clear();
    return status_code;
  }

  base::JSONWriter::WriteWithOptions(json_response.get(),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     response);
  return status_code;
}

// Privet API methods:

scoped_ptr<base::DictionaryValue> PrivetHttpServer::ProcessInfo(
    net::HttpStatusCode* status_code) const {

  DeviceInfo info;
  delegate_->CreateInfo(&info);

  scoped_ptr<base::DictionaryValue> response(new base::DictionaryValue);
  response->SetString("version", info.version);
  response->SetString("manufacturer", info.manufacturer);

  base::ListValue api;
  for (size_t i = 0; i < info.api.size(); ++i)
    api.AppendString(info.api[i]);
  response->Set("api", api.DeepCopy());

  *status_code = net::HTTP_OK;
  return response.Pass();
}

scoped_ptr<base::DictionaryValue> PrivetHttpServer::ProcessRegister(
    const GURL& url,
    net::HttpStatusCode* status_code) {
  // TODO(maksymb): Add saving state to drive.

  std::string action;
  if (!net::GetValueForKeyInQuery(url, "action", &action)) {
    *status_code = net::HTTP_BAD_REQUEST;
    return scoped_ptr<base::DictionaryValue>();
  }

  // TODO(maksymb): Is there a possibility |user| to be empty?
  std::string user;
  if (!net::GetValueForKeyInQuery(url, "user", &user) || user.empty()) {
    *status_code = net::HTTP_BAD_REQUEST;
    return scoped_ptr<base::DictionaryValue>();
  }

  RegistrationErrorStatus status = REG_ERROR_NO_RESULT;
  scoped_ptr<base::DictionaryValue> response(new DictionaryValue);
  response->SetString("action", action);
  response->SetString("user", user);

  if (action == "start")
    status = delegate_->RegistrationStart(user);

  if (action == "getClaimToken") {
    std::string token;
    std::string claim_url;
    status = delegate_->RegistrationGetClaimToken(user, &token, &claim_url);
    response->SetString("token", token);
    response->SetString("claim_url", claim_url);
  }

  if (action == "complete") {
    std::string device_id;
    status = delegate_->RegistrationComplete(user, &device_id);
    response->SetString("device_id", device_id);
  }

  if (action == "cancel")
    status = delegate_->RegistrationCancel(user);

  if (status != REG_ERROR_OK)
    response.reset();

  ProcessRegistrationStatus(status, status_code, &response);
  return response.Pass();
}

void PrivetHttpServer::ProcessRegistrationStatus(
    RegistrationErrorStatus status,
    net::HttpStatusCode *status_code,
    scoped_ptr<base::DictionaryValue>* current_response) const {
  switch (status) {
    case REG_ERROR_OK:
      *status_code = net::HTTP_OK;
      DCHECK(*current_response) << "Response shouldn't be empty.";
      break;
    case REG_ERROR_NO_RESULT:
      *status_code = net::HTTP_BAD_REQUEST;
      current_response->reset();
      break;
    case REG_ERROR_REGISTERED:
      *status_code = net::HTTP_NOT_FOUND;
      current_response->reset();
      break;

    case REG_ERROR_DEVICE_BUSY:
      *status_code = net::HTTP_OK;
      *current_response = CreateErrorWithTimeout("device_busy", 30);
      break;
    case REG_ERROR_PENDING_USER_ACTION:
      *status_code = net::HTTP_OK;
      *current_response = CreateErrorWithTimeout("pending_user_action", 30);
      break;
    case REG_ERROR_USER_CANCEL:
      *status_code = net::HTTP_OK;
      *current_response = CreateError("user_cancel");
      break;
    case REG_ERROR_CONFIRMATION_TIMEOUT:
      *status_code = net::HTTP_OK;
      *current_response = CreateError("confirmation_timeout");
      break;
    case REG_ERROR_INVALID_ACTION:
      *status_code = net::HTTP_OK;
      *current_response = CreateError("invalid_action");
      break;
    case REG_ERROR_SERVER_ERROR: {
      *status_code = net::HTTP_OK;
      std::string description;
      delegate_->GetRegistrationServerError(&description);
      *current_response = CreateErrorWithDescription("server_error",
                                                     description);
      break;
    }

    default:
      NOTREACHED();
  };
}


