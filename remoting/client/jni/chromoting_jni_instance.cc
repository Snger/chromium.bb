// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/jni/chromoting_jni_instance.h"

#include "base/bind.h"
#include "base/logging.h"
#include "remoting/client/audio_player.h"
#include "remoting/client/jni/chromoting_jni.h"
#include "remoting/protocol/libjingle_transport_factory.h"

// TODO(solb) Move into location shared with client plugin.
const char* const CHAT_SERVER = "talk.google.com";
const int CHAT_PORT = 5222;
const bool CHAT_USE_TLS = true;

namespace remoting {

ChromotingJniInstance::ChromotingJniInstance(const char* username,
                                             const char* auth_token,
                                             const char* host_jid,
                                             const char* host_id,
                                             const char* host_pubkey) {
  DCHECK(ChromotingJni::GetInstance()->
      ui_task_runner()->BelongsToCurrentThread());

  username_ = username;
  auth_token_ = auth_token;
  host_jid_ = host_jid;
  host_id_ = host_id;
  host_pubkey_ = host_pubkey;

  ChromotingJni::GetInstance()->display_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChromotingJniInstance::ConnectToHostOnDisplayThread,
                 this));
}

ChromotingJniInstance::~ChromotingJniInstance() {}

void ChromotingJniInstance::Cleanup() {
  if (!ChromotingJni::GetInstance()->
      display_task_runner()->BelongsToCurrentThread()) {
    ChromotingJni::GetInstance()->display_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::Cleanup, this));
    return;
  }

  // This must be destroyed on the display thread before the producer is gone.
  view_.reset();

  // The weak pointers must be invalidated on the same thread they were used.
  view_weak_factory_->InvalidateWeakPtrs();

  ChromotingJni::GetInstance()->network_task_runner()->PostTask(FROM_HERE,
      base::Bind(&ChromotingJniInstance::DisconnectFromHostOnNetworkThread,
                 this));
}

void ChromotingJniInstance::ProvideSecret(const char* pin) {
  DCHECK(ChromotingJni::GetInstance()->
      ui_task_runner()->BelongsToCurrentThread());
  DCHECK(!pin_callback_.is_null());

  // We invoke the string constructor to ensure |pin| gets copied *before* the
  // asynchronous run, since Java might want it back as soon as we return.
  ChromotingJni::GetInstance()->network_task_runner()->PostTask(FROM_HERE,
                                 base::Bind(pin_callback_, pin));
}

void ChromotingJniInstance::RedrawDesktop() {
  if (!ChromotingJni::GetInstance()->
      display_task_runner()->BelongsToCurrentThread()) {
    ChromotingJni::GetInstance()->display_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::RedrawDesktop, this));
    return;
  }

  ChromotingJni::GetInstance()->RedrawCanvas();
}

void ChromotingJniInstance::PerformMouseAction(
    int x,
    int y,
    protocol::MouseEvent_MouseButton button,
    bool buttonDown) {
  if(!ChromotingJni::GetInstance()->
      network_task_runner()->BelongsToCurrentThread()) {
    ChromotingJni::GetInstance()->network_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::PerformMouseAction,
                   this,
                   x,
                   y,
                   button,
                   buttonDown));
    return;
  }

  protocol::MouseEvent action;
  action.set_x(x);
  action.set_y(y);
  action.set_button(button);
  if (button != protocol::MouseEvent::BUTTON_UNDEFINED)
    action.set_button_down(buttonDown);

  connection_->input_stub()->InjectMouseEvent(action);
}

void ChromotingJniInstance::OnConnectionState(
    protocol::ConnectionToHost::State state,
    protocol::ErrorCode error) {
  if (!ChromotingJni::GetInstance()->
      ui_task_runner()->BelongsToCurrentThread()) {
    ChromotingJni::GetInstance()->
        ui_task_runner()->PostTask(
            FROM_HERE,
            base::Bind(&ChromotingJniInstance::OnConnectionState,
                       this,
                       state,
                       error));
    return;
  }

  ChromotingJni::GetInstance()->ReportConnectionStatus(state, error);
}

void ChromotingJniInstance::OnConnectionReady(bool ready) {
  // We ignore this message, since OnConnectionState() tells us the same thing.
}

void ChromotingJniInstance::SetCapabilities(const std::string& capabilities) {}

void ChromotingJniInstance::SetPairingResponse(
    const protocol::PairingResponse& response) {
  NOTIMPLEMENTED();
}

protocol::ClipboardStub* ChromotingJniInstance::GetClipboardStub() {
  return this;
}

protocol::CursorShapeStub* ChromotingJniInstance::GetCursorShapeStub() {
  return this;
}

scoped_ptr<protocol::ThirdPartyClientAuthenticator::TokenFetcher>
    ChromotingJniInstance::GetTokenFetcher(const std::string& host_public_key) {
  // Return null to indicate that third-party authentication is unsupported.
  return scoped_ptr<protocol::ThirdPartyClientAuthenticator::TokenFetcher>();
}

void ChromotingJniInstance::InjectClipboardEvent(
    const protocol::ClipboardEvent& event) {
  NOTIMPLEMENTED();
}

void ChromotingJniInstance::SetCursorShape(
    const protocol::CursorShapeInfo& shape) {
  NOTIMPLEMENTED();
}

void ChromotingJniInstance::ConnectToHostOnDisplayThread() {
  DCHECK(ChromotingJni::GetInstance()->
      display_task_runner()->BelongsToCurrentThread());

  frame_consumer_ = new FrameConsumerProxy(
      ChromotingJni::GetInstance()->display_task_runner());
  view_.reset(new JniFrameConsumer());
  view_weak_factory_.reset(new base::WeakPtrFactory<JniFrameConsumer>(
      view_.get()));
  frame_consumer_->Attach(view_weak_factory_->GetWeakPtr());

  ChromotingJni::GetInstance()->network_task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&ChromotingJniInstance::ConnectToHostOnNetworkThread,
                 this));
}

void ChromotingJniInstance::ConnectToHostOnNetworkThread() {
  DCHECK(ChromotingJni::GetInstance()->
      network_task_runner()->BelongsToCurrentThread());

  client_config_.reset(new ClientConfig());
  client_config_->host_jid = host_jid_;
  client_config_->host_public_key = host_pubkey_;

  client_config_->fetch_secret_callback = base::Bind(
      &ChromotingJniInstance::FetchSecret,
      this);
  client_config_->authentication_tag = host_id_;

  client_config_->authentication_methods.push_back(
      protocol::AuthenticationMethod::FromString("spake2_hmac"));
  client_config_->authentication_methods.push_back(
      protocol::AuthenticationMethod::FromString("spake2_plain"));

  client_context_.reset(new ClientContext(
      ChromotingJni::GetInstance()->network_task_runner().get()));
  client_context_->Start();

  connection_.reset(new protocol::ConnectionToHost(true));

  client_.reset(new ChromotingClient(*client_config_,
                                     client_context_.get(),
                                     connection_.get(),
                                     this,
                                     frame_consumer_,
                                     scoped_ptr<AudioPlayer>()));

  view_->set_frame_producer(client_->GetFrameProducer());

  signaling_config_.reset(new XmppSignalStrategy::XmppServerConfig());
  signaling_config_->host = CHAT_SERVER;
  signaling_config_->port = CHAT_PORT;
  signaling_config_->use_tls = CHAT_USE_TLS;

  signaling_.reset(new XmppSignalStrategy(
      ChromotingJni::GetInstance()->url_requester(),
      username_,
      auth_token_,
      "oauth2",
      *signaling_config_));

  network_settings_.reset(new NetworkSettings(
      NetworkSettings::NAT_TRAVERSAL_OUTGOING));
  scoped_ptr<protocol::TransportFactory> fact(
      protocol::LibjingleTransportFactory::Create(
          *network_settings_,
          ChromotingJni::GetInstance()->url_requester()));

  client_->Start(signaling_.get(), fact.Pass());
}

void ChromotingJniInstance::DisconnectFromHostOnNetworkThread() {
  DCHECK(ChromotingJni::GetInstance()->
      network_task_runner()->BelongsToCurrentThread());

  username_ = "";
  auth_token_ = "";
  host_jid_ = "";
  host_id_ = "";
  host_pubkey_ = "";

  // |client_| must be torn down before |signaling_|.
  connection_.reset();
  client_.reset();
}

void ChromotingJniInstance::FetchSecret(
    bool pairable,
    const protocol::SecretFetchedCallback& callback) {
  if (!ChromotingJni::GetInstance()->
      ui_task_runner()->BelongsToCurrentThread()) {
    ChromotingJni::GetInstance()->ui_task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&ChromotingJniInstance::FetchSecret,
                   this,
                   pairable,
                   callback));
    return;
  }

  pin_callback_ = callback;
  ChromotingJni::GetInstance()->DisplayAuthenticationPrompt();
}

}  // namespace remoting
