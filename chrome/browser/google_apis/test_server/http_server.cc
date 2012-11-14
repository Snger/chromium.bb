// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/test_server/http_server.h"

#include "base/bind.h"
#include "base/file_util.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "chrome/browser/google_apis/gdata_test_util.h"
#include "chrome/browser/google_apis/test_server/http_request.h"
#include "chrome/browser/google_apis/test_server/http_response.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_utils.h"
#include "net/tools/fetch/http_listen_socket.h"

namespace drive {
namespace test_server {

using content::BrowserThread;

namespace {

const int kPort = 8040;
const char kIp[] = "127.0.0.1";
const int kRetries = 10;

// Callback to handle requests with default predefined response for requests
// matching the address |url|.
scoped_ptr<HttpResponse> HandleDefaultRequest(const GURL& url,
                                              const HttpResponse& response,
                                              const HttpRequest& request) {
  if (url.path() != request.url.path())
    return scoped_ptr<HttpResponse>(NULL);
  return scoped_ptr<HttpResponse>(new HttpResponse(response));
}

}  // namespace

HttpListenSocket::HttpListenSocket(const SocketDescriptor socket_descriptor,
                                   net::StreamListenSocket::Delegate* delegate)
    : net::TCPListenSocket(socket_descriptor, delegate) {
}

void HttpListenSocket::Listen() {
  net::TCPListenSocket::Listen();
}

HttpListenSocket::~HttpListenSocket() {
}

bool HttpServer::InitializeAndWaitUntilReady() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(&HttpServer::InitializeOnIOThread,
                 base::Unretained(this),
                 InitializeCallback()));

  // Wait for the task completion.
  content::RunAllPendingInMessageLoop(BrowserThread::IO);
  content::RunAllPendingInMessageLoop();

  return Started();
}

void HttpServer::InitializeOnIOThread(const InitializeCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(!Started());

  int retries_left = kRetries + 1;
  int try_port = kPort;

  while (retries_left > 0) {
    SocketDescriptor socket_descriptor = net::TCPListenSocket::CreateAndBind(
        kIp,
        try_port);
    if (socket_descriptor != net::TCPListenSocket::kInvalidSocket) {
      listen_socket_ = new HttpListenSocket(socket_descriptor, this);
      listen_socket_->Listen();
      base_url_ = GURL(base::StringPrintf("http://%s:%d", kIp, try_port));
      port_ = try_port;
      break;
    }
    retries_left--;
    try_port++;
  }

  if (!callback.is_null())
    callback.Run(listen_socket_.get() != NULL);
}

HttpServer::HttpServer()
    : port_(-1),
      weak_factory_(ALLOW_THIS_IN_INITIALIZER_LIST(this)) {
}

HttpServer::~HttpServer() {
  STLDeleteContainerPairSecondPointers(connections_.begin(),
                                       connections_.end());
}

void HttpServer::HandleRequest(HttpConnection* connection,
                               scoped_ptr<HttpRequest> request) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  for (size_t i = 0; i < request_handlers_.size(); ++i) {
    scoped_ptr<HttpResponse> response =
        request_handlers_[i].Run(*request.get());
    if (response.get()) {
      connection->SendResponse(response.Pass());
      return;
    }
  }

  LOG(WARNING) << "Request not handled. Returning 404.";
  scoped_ptr<HttpResponse> not_found_response(new HttpResponse());
  not_found_response->set_code(NOT_FOUND);
  connection->SendResponse(not_found_response.Pass());

  // Drop the connection, since we do not support multiple requests per
  // connection.
  connections_.erase(connection->socket_.get());
  delete connection;
}

GURL HttpServer::GetBaseURL() {
  return base_url_;
}

void HttpServer::RegisterRequestHandler(
    const HandleRequestCallback& callback) {
  request_handlers_.push_back(callback);
}

GURL HttpServer::RegisterDefaultResponse(
    const std::string& relative_path,
    const HttpResponse& default_response) {
  GURL request_url = base_url_.Resolve(relative_path);
  const HandleRequestCallback callback =
      base::Bind(&HandleDefaultRequest,
                 request_url,
                 default_response);
  request_handlers_.push_back(callback);

  return request_url;
}

GURL HttpServer::RegisterTextResponse(
     const std::string& relative_path,
     const std::string& content,
     const std::string& content_type,
     const ResponseCode response_code) {
  HttpResponse default_response;
  default_response.set_content(content);
  default_response.set_content_type(content_type);
  default_response.set_code(response_code);

  return RegisterDefaultResponse(relative_path, default_response);
}

GURL HttpServer::RegisterFileResponse(
     const std::string& relative_path,
     const FilePath& file_path,
     const std::string& content_type,
     const ResponseCode response_code) {
  HttpResponse default_response;

  std::string content;
  const bool success = file_util::ReadFileToString(
      file_path, &content);
  default_response.set_content(content);
  DCHECK(success) << "Failed to open the file: " << file_path.value();

  default_response.set_content_type(content_type);
  default_response.set_code(response_code);

  return RegisterDefaultResponse(relative_path, default_response);
}

void HttpServer::DidAccept(net::StreamListenSocket* server,
                           net::StreamListenSocket* connection) {
  HttpConnection* http_connection = new HttpConnection(
      connection,
      base::Bind(&HttpServer::HandleRequest, weak_factory_.GetWeakPtr()));
  connections_[connection] = http_connection;
}

void HttpServer::DidRead(net::StreamListenSocket* connection,
                         const char* data,
                         int length) {
  HttpConnection* http_connection = FindConnection(connection);
  if (http_connection == NULL) {
    LOG(WARNING) << "Unknown connection.";
    return;
  }
  http_connection->ReceiveData(std::string(data, length));
}

void HttpServer::DidClose(net::StreamListenSocket* connection) {
  HttpConnection* http_connection = FindConnection(connection);
  if (http_connection == NULL) {
    LOG(WARNING) << "Unknown connection.";
    return;
  }
  delete http_connection;
  connections_.erase(connection);
}

HttpConnection* HttpServer::FindConnection(
    net::StreamListenSocket* socket) {
  std::map<net::StreamListenSocket*, HttpConnection*>::iterator it =
      connections_.find(socket);
  if (it == connections_.end()) {
    return NULL;
  }
  return it->second;
}

}  // namespace test_server
}  // namespace drive
