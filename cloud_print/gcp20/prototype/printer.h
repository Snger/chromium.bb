// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GCP20_PROTOTYPE_PRINTER_H_
#define GCP20_PROTOTYPE_PRINTER_H_

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "cloud_print/gcp20/prototype/cloud_print_requester.h"
#include "cloud_print/gcp20/prototype/dns_sd_server.h"
#include "cloud_print/gcp20/prototype/print_job_handler.h"
#include "cloud_print/gcp20/prototype/privet_http_server.h"
#include "cloud_print/gcp20/prototype/x_privet_token.h"

extern const base::FilePath::CharType kPrinterStatePath[];

// This class maintains work of DNS-SD server, HTTP server and others.
class Printer : public base::SupportsWeakPtr<Printer>,
                public PrivetHttpServer::Delegate,
                public CloudPrintRequester::Delegate {
 public:
  // Constructs uninitialized object.
  Printer();

  // Destroys the object.
  virtual ~Printer();

  // Starts all servers.
  bool Start();

  // Returns true if printer was started.
  bool IsOnline() const;

  // Method for trying to reconnecting to server.
  void WakeUp();

  // Stops all servers.
  void Stop();

 private:
  struct RegistrationInfo {
    enum RegistrationState {
      DEV_REG_UNREGISTERED,
      DEV_REG_REGISTRATION_STARTED,  // |action=start| was called,
                                     // request to CloudPrint was sent.
      DEV_REG_REGISTRATION_CLAIM_TOKEN_READY,  // The same as previous,
                                               // but request reply is already
                                               // received.
      DEV_REG_REGISTRATION_COMPLETING,  // |action=complete| was called,
                                        // |complete| request was sent.
      DEV_REG_REGISTRATION_ERROR,  // Is set when server error was occurred.
      DEV_REG_REGISTERED,
    };

    enum ConfirmationState {
      CONFIRMATION_PENDING,
      CONFIRMATION_CONFIRMED,
      CONFIRMATION_DISCARDED,
      CONFIRMATION_TIMEOUT,
    };

    RegistrationInfo();
    ~RegistrationInfo();

    std::string user;
    std::string refresh_token;
    std::string device_id;
    RegistrationState state;
    ConfirmationState confirmation_state;

    std::string registration_token;
    std::string complete_invite_url;

    // Contains error response if |DEV_REG_REGISTRATION_ERROR| is set.
    std::string error_description;
  };

  enum RegistrationAction {
    REG_ACTION_START,
    REG_ACTION_GET_CLAIM_TOKEN,
    REG_ACTION_COMPLETE,
    REG_ACTION_CANCEL
  };

  enum ConnectionState {
    NOT_CONFIGURED,
    OFFLINE,
    ONLINE,
    CONNECTING
  };

  // PrivetHttpServer::Delegate methods:
  virtual PrivetHttpServer::RegistrationErrorStatus RegistrationStart(
      const std::string& user) OVERRIDE;
  virtual PrivetHttpServer::RegistrationErrorStatus RegistrationGetClaimToken(
      const std::string& user,
      std::string* token,
      std::string* claim_url) OVERRIDE;
  virtual PrivetHttpServer::RegistrationErrorStatus RegistrationComplete(
      const std::string& user,
      std::string* device_id) OVERRIDE;
  virtual PrivetHttpServer::RegistrationErrorStatus RegistrationCancel(
      const std::string& user) OVERRIDE;
  virtual void GetRegistrationServerError(std::string* description) OVERRIDE;
  virtual void CreateInfo(PrivetHttpServer::DeviceInfo* info) OVERRIDE;
  virtual bool IsRegistered() const OVERRIDE;
  virtual bool CheckXPrivetTokenHeader(const std::string& token) const OVERRIDE;

  // CloudRequester::Delegate methods:
  virtual void OnRegistrationStartResponseParsed(
      const std::string& registration_token,
      const std::string& complete_invite_url,
      const std::string& device_id) OVERRIDE;
  virtual void OnGetAuthCodeResponseParsed(
      const std::string& refresh_token) OVERRIDE;
  virtual void OnRegistrationError(const std::string& description) OVERRIDE;
  virtual void OnServerError(const std::string& description) OVERRIDE;
  virtual void OnNetworkError() OVERRIDE;
  virtual void OnPrintJobsAvailable(
      const std::vector<cloud_print_response_parser::Job>& jobs) OVERRIDE;
  virtual void OnPrintJobDownloaded(
      const cloud_print_response_parser::Job& job) OVERRIDE;
  virtual void OnPrintJobDone() OVERRIDE;

  // Checks if register call is called correctly (|user| is correct,
  // error is no set etc). Returns |false| if error status is put into |status|.
  // Otherwise no error was occurred.
  PrivetHttpServer::RegistrationErrorStatus CheckCommonRegErrors(
      const std::string& user) const;

  // Checks if confirmation was received.
  void WaitUserConfirmation(base::Time valid_until);

  // Generates ProxyId for this device.
  std::string GenerateProxyId() const;

  // Creates data for DNS TXT respond.
  std::vector<std::string> CreateTxt() const;

  // Ask CloudPrint server for printjobs.
  void FetchPrintJobs();

  // Saving and loading registration info from file.
  void SaveToFile(const base::FilePath& file_path) const;
  bool LoadFromFile(const base::FilePath& file_path);

  // Adds |WakeUp| method to the MessageLoop.
  void PostWakeUp();

  // Adds |WakeUp| method to the MessageLoop with certain |delay|.
  void PostDelayedWakeUp(const base::TimeDelta& delay);

  // Converts errors.
  PrivetHttpServer::RegistrationErrorStatus ConfirmationToRegistrationError(
      RegistrationInfo::ConfirmationState state);

  std::string ConnectionStateToString(ConnectionState state) const;

  // Changes state and update info in DNS server.
  bool ChangeState(ConnectionState new_state);

  RegistrationInfo reg_info_;

  // Contains DNS-SD server.
  DnsSdServer dns_server_;

  // Contains Privet HTTP server.
  PrivetHttpServer http_server_;

  // Connection state of device.
  ConnectionState connection_state_;

  // Contains CloudPrint client.
  scoped_ptr<CloudPrintRequester> requester_;

  XPrivetToken xtoken_;

  scoped_ptr<PrintJobHandler> print_job_handler_;

  // Uses for calculating uptime.
  base::Time starttime_;

  DISALLOW_COPY_AND_ASSIGN(Printer);
};

#endif  // GCP20_PROTOTYPE_PRINTER_H_

