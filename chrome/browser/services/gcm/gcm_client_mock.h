// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SERVICES_GCM_GCM_CLIENT_MOCK_H_
#define CHROME_BROWSER_SERVICES_GCM_GCM_CLIENT_MOCK_H_

#include <map>

#include "base/compiler_specific.h"
#include "google_apis/gcm/gcm_client.h"

namespace gcm {

class GCMClientMock : public GCMClient {
 public:
  enum Status {
    NOT_READY,
    READY
  };

  explicit GCMClientMock(Status status);
  virtual ~GCMClientMock();

  // Overridden from GCMClient:
  // Called on IO thread.
  virtual void Initialize(
      const checkin_proto::ChromeBuildProto& chrome_build_proto,
      const base::FilePath& store_path,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_task_runner,
      const scoped_refptr<net::URLRequestContextGetter>&
          url_request_context_getter) OVERRIDE;
  virtual void SetUserDelegate(const std::string& username,
                               Delegate* delegate) OVERRIDE;
  virtual void CheckIn(const std::string& username) OVERRIDE;
  virtual void Register(const std::string& username,
                        const std::string& app_id,
                        const std::string& cert,
                        const std::vector<std::string>& sender_ids) OVERRIDE;
  virtual void Unregister(const std::string& username,
                          const std::string& app_id) OVERRIDE;
  virtual void Send(const std::string& username,
                    const std::string& app_id,
                    const std::string& receiver_id,
                    const OutgoingMessage& message) OVERRIDE;
  virtual bool IsReady() const OVERRIDE;

  // Simulate receiving something from the server.
  // Called on UI thread.
  void ReceiveMessage(const std::string& username,
                      const std::string& app_id,
                      const IncomingMessage& message);
  void DeleteMessages(const std::string& username, const std::string& app_id);

  void set_simulate_server_error(bool simulate_server_error) {
    simulate_server_error_ = simulate_server_error;
  }

  // Can only transition from non-ready to ready.
  void SetReady();

  static CheckinInfo GetCheckinInfoFromUsername(const std::string& username);
  static std::string GetRegistrationIdFromSenderIds(
      const std::vector<std::string>& sender_ids);

 private:
  Delegate* GetDelegate(const std::string& username) const;

  // Called on IO thread.
  // TODO(fgorski): Update parameters to const ref.
  void CheckInFinished(std::string username, CheckinInfo checkin_info);
  void RegisterFinished(std::string username,
                        std::string app_id,
                        std::string registrion_id);
  void SendFinished(std::string username,
                    std::string app_id,
                    std::string message_id);
  void MessageReceived(std::string username,
                       std::string app_id,
                       IncomingMessage message);
  void MessagesDeleted(std::string username, std::string app_id);
  void MessageSendError(std::string username,
                        std::string app_id,
                        std::string message_id);
  void SetReadyOnIO();

  std::map<std::string, Delegate*> delegates_;

  Status status_;

  // The testing code could set this to simulate the server error in order to
  // test the error scenario.
  bool simulate_server_error_;

  DISALLOW_COPY_AND_ASSIGN(GCMClientMock);
};

}  // namespace gcm

#endif  // CHROME_BROWSER_SERVICES_GCM_GCM_CLIENT_MOCK_H_
