// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/services/gcm/gcm_profile_service.h"

#include "base/logging.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/services/gcm/gcm_event_router.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/common/chrome_version_info.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "extensions/common/extension.h"

using extensions::Extension;

namespace gcm {

class GCMProfileService::IOWorker
    : public GCMClient::Delegate,
      public base::RefCountedThreadSafe<GCMProfileService::IOWorker>{
 public:
  explicit IOWorker(const base::WeakPtr<GCMProfileService>& service);

  // Overridden from GCMClient::Delegate:
  // Called from IO thread.
  virtual void OnAddUserFinished(const GCMClient::CheckInInfo& checkin_info,
                                 GCMClient::Result result) OVERRIDE;
  virtual void OnRegisterFinished(const std::string& app_id,
                                  const std::string& registration_id,
                                  GCMClient::Result result) OVERRIDE;
  virtual void OnUnregisterFinished(const std::string& app_id,
                                    GCMClient::Result result) OVERRIDE;
  virtual void OnSendFinished(const std::string& app_id,
                              const std::string& message_id,
                              GCMClient::Result result) OVERRIDE;
  virtual void OnMessageReceived(
      const std::string& app_id,
      const GCMClient::IncomingMessage& message) OVERRIDE;
  virtual void OnMessagesDeleted(const std::string& app_id) OVERRIDE;
  virtual void OnMessageSendError(const std::string& app_id,
                                  const std::string& message_id,
                                  GCMClient::Result result) OVERRIDE;
  virtual GCMClient::CheckInInfo GetCheckInInfo() const OVERRIDE;
  virtual void OnLoadingCompleted() OVERRIDE;
  virtual base::TaskRunner* GetFileTaskRunner() OVERRIDE;

  void CheckIn(const std::string& username);
  void Register(const std::string& username,
                const std::string& app_id,
                const std::vector<std::string>& sender_ids,
                const std::string& cert);
  void Send(const std::string& username,
            const std::string& app_id,
            const std::string& receiver_id,
            const GCMClient::OutgoingMessage& message);

 private:
  friend class base::RefCountedThreadSafe<IOWorker>;
  virtual ~IOWorker();

  const base::WeakPtr<GCMProfileService> service_;

  // The checkin info obtained from the server for the signed in user associated
  // with the profile.
  GCMClient::CheckInInfo checkin_info_;
};

GCMProfileService::IOWorker::IOWorker(
    const base::WeakPtr<GCMProfileService>& service)
    : service_(service) {
}

GCMProfileService::IOWorker::~IOWorker() {
}

void GCMProfileService::IOWorker::OnAddUserFinished(
    const GCMClient::CheckInInfo& checkin_info,
    GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  checkin_info_ = checkin_info;

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::AddUserFinished,
                 service_,
                 checkin_info_,
                 result));
}

void GCMProfileService::IOWorker::OnRegisterFinished(
    const std::string& app_id,
    const std::string& registration_id,
    GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::RegisterFinished,
                 service_,
                 app_id,
                 registration_id,
                 result));
}

void GCMProfileService::IOWorker::OnUnregisterFinished(
    const std::string& app_id,
    GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  // TODO(jianli): to be implemented.
}

void GCMProfileService::IOWorker::OnSendFinished(
    const std::string& app_id,
    const std::string& message_id,
    GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::SendFinished,
                 service_,
                 app_id,
                 message_id,
                 result));
}

void GCMProfileService::IOWorker::OnMessageReceived(
    const std::string& app_id,
    const GCMClient::IncomingMessage& message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::MessageReceived,
                 service_,
                 app_id,
                 message));
}

void GCMProfileService::IOWorker::OnMessagesDeleted(const std::string& app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::MessagesDeleted,
                 service_,
                 app_id));
}

void GCMProfileService::IOWorker::OnMessageSendError(
    const std::string& app_id,
    const std::string& message_id,
    GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  content::BrowserThread::PostTask(
      content::BrowserThread::UI,
      FROM_HERE,
      base::Bind(&GCMProfileService::MessageSendError,
                 service_,
                 app_id,
                 message_id,
                 result));
}

GCMClient::CheckInInfo GCMProfileService::IOWorker::GetCheckInInfo() const {
  return checkin_info_;
}

void GCMProfileService::IOWorker::OnLoadingCompleted() {
  // TODO(jianli): to be implemented.
}

base::TaskRunner* GCMProfileService::IOWorker::GetFileTaskRunner() {
  // TODO(jianli): to be implemented.
  return NULL;
}

void GCMProfileService::IOWorker::CheckIn(const std::string& username) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

  GCMClient::Get()->AddUser(username, this);
}

void GCMProfileService::IOWorker::Register(
    const std::string& username,
    const std::string& app_id,
    const std::vector<std::string>& sender_ids,
    const std::string& cert) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(checkin_info_.IsValid());

  GCMClient::Get()->Register(username, app_id, cert, sender_ids);
}

void GCMProfileService::IOWorker::Send(
    const std::string& username,
    const std::string& app_id,
    const std::string& receiver_id,
    const GCMClient::OutgoingMessage& message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(checkin_info_.IsValid());

  GCMClient::Get()->Send(username, app_id, receiver_id, message);
}

// static
bool GCMProfileService::IsGCMEnabled() {
  // GCM support is only enabled for Canary/Dev builds.
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  return channel == chrome::VersionInfo::CHANNEL_UNKNOWN ||
         channel == chrome::VersionInfo::CHANNEL_CANARY ||
         channel == chrome::VersionInfo::CHANNEL_DEV;
}

GCMProfileService::GCMProfileService(Profile* profile)
    : profile_(profile),
      testing_delegate_(NULL),
      weak_ptr_factory_(this) {
  // This has to be done first since CheckIn depends on it.
  io_worker_ = new IOWorker(weak_ptr_factory_.GetWeakPtr());

  // In case that the profile has been signed in before GCMProfileService is
  // created.
  SigninManagerBase* manager = SigninManagerFactory::GetForProfile(profile_);
  if (manager)
    username_ = manager->GetAuthenticatedUsername();
  if (!username_.empty())
    CheckIn();

  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
                 content::Source<Profile>(profile_));
  registrar_.Add(this,
                 chrome::NOTIFICATION_GOOGLE_SIGNED_OUT,
                 content::Source<Profile>(profile_));
}

GCMProfileService::~GCMProfileService() {
}

void GCMProfileService::Register(const std::string& app_id,
                                 const std::vector<std::string>& sender_ids,
                                 const std::string& cert,
                                 RegisterCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!app_id.empty() && !sender_ids.empty() && !callback.is_null());

  if (register_callbacks_.find(app_id) != register_callbacks_.end()) {
    callback.Run(std::string(), GCMClient::ASYNC_OPERATION_PENDING);
    return;
  }
  register_callbacks_[app_id] = callback;

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::Register,
                 io_worker_,
                 username_,
                 app_id,
                 sender_ids,
                 cert));
}

void GCMProfileService::Send(const std::string& app_id,
                             const std::string& receiver_id,
                             const GCMClient::OutgoingMessage& message,
                             SendCallback callback) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  DCHECK(!app_id.empty() && !receiver_id.empty() && !callback.is_null());

  std::pair<std::string, std::string> key(app_id, message.id);
  if (send_callbacks_.find(key) != send_callbacks_.end()) {
    callback.Run(message.id, GCMClient::INVALID_PARAMETER);
    return;
  }
  send_callbacks_[key] = callback;

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::Send,
                 io_worker_,
                 username_,
                 app_id,
                 receiver_id,
                 message));
}

void GCMProfileService::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  switch (type) {
    case chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL: {
      const GoogleServiceSigninSuccessDetails* signin_details =
          content::Details<GoogleServiceSigninSuccessDetails>(details).ptr();
      // If re-signin occurs due to password change, there is no need to do
      // check-in again.
      if (username_ != signin_details->username) {
        username_ = signin_details->username;
        DCHECK(!username_.empty());
        CheckIn();
      }
      break;
    }
    case chrome::NOTIFICATION_GOOGLE_SIGNED_OUT:
      username_.clear();
      CheckOut();
      break;
    default:
      NOTREACHED();
  }
}

void GCMProfileService::CheckIn() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  content::BrowserThread::PostTask(
      content::BrowserThread::IO,
      FROM_HERE,
      base::Bind(&GCMProfileService::IOWorker::CheckIn,
                 io_worker_,
                 username_));
}

void GCMProfileService::CheckOut() {
  // TODO(jianli): to be implemented.
}

void GCMProfileService::AddUserFinished(GCMClient::CheckInInfo checkin_info,
                                        GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (testing_delegate_)
    testing_delegate_->CheckInFinished(checkin_info, result);
}

void GCMProfileService::RegisterFinished(std::string app_id,
                                         std::string registration_id,
                                         GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::map<std::string, RegisterCallback>::iterator callback_iter =
      register_callbacks_.find(app_id);
  if (callback_iter == register_callbacks_.end()) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  RegisterCallback callback = callback_iter->second;
  register_callbacks_.erase(callback_iter);
  callback.Run(registration_id, result);
}

void GCMProfileService::SendFinished(std::string app_id,
                                     std::string message_id,
                                     GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  std::map<std::pair<std::string, std::string>, SendCallback>::iterator
      callback_iter = send_callbacks_.find(
          std::pair<std::string, std::string>(app_id, message_id));
  if (callback_iter == send_callbacks_.end()) {
    // The callback could have been removed when the app is uninstalled.
    return;
  }

  SendCallback callback = callback_iter->second;
  send_callbacks_.erase(callback_iter);
  callback.Run(message_id, result);
}

void GCMProfileService::MessageReceived(std::string app_id,
                                        GCMClient::IncomingMessage message) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  GetEventRouter(app_id)->OnMessage(app_id, message);
}

void GCMProfileService::MessagesDeleted(std::string app_id) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  GetEventRouter(app_id)->OnMessagesDeleted(app_id);
}

void GCMProfileService::MessageSendError(std::string app_id,
                                         std::string message_id,
                                         GCMClient::Result result) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  GetEventRouter(app_id)->OnSendError(app_id, message_id, result);
}

GCMEventRouter* GCMProfileService::GetEventRouter(const std::string& app_id) {
  if (testing_delegate_ && testing_delegate_->GetEventRouter())
    return testing_delegate_->GetEventRouter();
  // TODO(fgorski): check and create the event router for JS routing.
  return js_event_router_.get();
}

}  // namespace gcm
