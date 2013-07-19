// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_GCP20_PROTOTYPE_PRIVET_HTTP_SERVER_H_
#define CLOUD_PRINT_GCP20_PROTOTYPE_PRIVET_HTTP_SERVER_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/values.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"

class GURL;

// HTTP server for receiving .
class PrivetHttpServer: public net::HttpServer::Delegate {
 public:
  // TODO(maksymb): Move this enum to some namespace instead of this class.
  enum RegistrationErrorStatus {
    REG_ERROR_OK,
    REG_ERROR_NO_RESULT,  // default value, never set.
    REG_ERROR_REGISTERED,

    REG_ERROR_DEVICE_BUSY,
    REG_ERROR_PENDING_USER_ACTION,
    REG_ERROR_USER_CANCEL,
    REG_ERROR_CONFIRMATION_TIMEOUT,
    REG_ERROR_INVALID_ACTION,
    REG_ERROR_SERVER_ERROR
  };

  // TODO(maksymb): Move this struct to some namespace instead of this class.
  struct DeviceInfo {
    DeviceInfo();
    ~DeviceInfo();

    std::string version;
    std::string manufacturer;
    std::vector<std::string> api;
  };

  class Delegate {
   public:
    Delegate() {}

    virtual ~Delegate() {}

    // Invoked when registration is starting.
    virtual RegistrationErrorStatus RegistrationStart(
        const std::string& user) = 0;

    // Invoked when claimtoken is needed.
    virtual RegistrationErrorStatus RegistrationGetClaimToken(
        const std::string& user,
        std::string* token,
        std::string* claim_url) = 0;

    // Invoked when registration is going to be completed.
    virtual RegistrationErrorStatus RegistrationComplete(
        const std::string& user,
        std::string* device_id) = 0;

    // Invoked when client asked for cancelling the registration.
    virtual RegistrationErrorStatus RegistrationCancel(
        const std::string& user) = 0;

    // Invoked for receiving server error details.
    virtual void GetRegistrationServerError(std::string* description) = 0;

    // Invoked if /privet/info is called.
    virtual void CreateInfo(DeviceInfo* info) = 0;
  };

  // Constructor doesn't start server.
  explicit PrivetHttpServer(Delegate* delegate);

  // Destroys the object.
  virtual ~PrivetHttpServer();

  // Starts HTTP server: start listening port |port| for HTTP requests.
  bool Start(uint16 port);

  // Stops HTTP server.
  void Shutdown();

 private:
  // net::HttpServer::Delegate methods:
  virtual void OnHttpRequest(
      int connection_id,
      const net::HttpServerRequestInfo& info) OVERRIDE;
  virtual void OnWebSocketRequest(
      int connection_id,
      const net::HttpServerRequestInfo& info) OVERRIDE;
  virtual void OnWebSocketMessage(int connection_id,
                                  const std::string& data) OVERRIDE;
  virtual void OnClose(int connection_id) OVERRIDE;

  // Processes http request after all preparations (XPrivetHeader check,
  // data handling etc.)
  net::HttpStatusCode ProcessHttpRequest(const GURL& url,
                                         std::string* response);

  // Pivet API methods. Return reference to NULL if output should be empty.
  scoped_ptr<base::DictionaryValue> ProcessInfo(
      net::HttpStatusCode* status_code) const;
  scoped_ptr<base::DictionaryValue> ProcessReset(
      net::HttpStatusCode* status_code);
  scoped_ptr<base::DictionaryValue> ProcessRegister(
      const GURL& url,
      net::HttpStatusCode* status_code);

  // Proccesses current status and depending on it replaces (or not)
  // |current_response| with error or empty response.
  void ProcessRegistrationStatus(
      RegistrationErrorStatus status,
      net::HttpStatusCode *status_code,
      scoped_ptr<base::DictionaryValue>* current_response) const;

  // Port for listening.

  uint16 port_;

  // Contains encapsulated object for listening for requests.
  scoped_refptr<net::HttpServer> server_;

  Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(PrivetHttpServer);
};

#endif  // CLOUD_PRINT_GCP20_PROTOTYPE_PRIVET_HTTP_SERVER_H_

