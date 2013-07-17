// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CHROMOTING_JNI_INSTANCE_H_
#define REMOTING_CLIENT_CHROMOTING_JNI_INSTANCE_H_

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "remoting/client/chromoting_client.h"
#include "remoting/client/client_config.h"
#include "remoting/client/client_context.h"
#include "remoting/client/client_user_interface.h"
#include "remoting/client/frame_consumer_proxy.h"
#include "remoting/jingle_glue/network_settings.h"
#include "remoting/jingle_glue/xmpp_signal_strategy.h"
#include "remoting/protocol/connection_to_host.h"

namespace remoting {
class ChromotingJni;

// ClientUserInterface that indirectly makes and receives JNI calls.
class ChromotingJniInstance
  : public ClientUserInterface,
    public base::RefCountedThreadSafe<ChromotingJniInstance> {
 public:
  // Initiates a connection with the specified host. Call from the UI thread.
  ChromotingJniInstance(
      const char* username,
      const char* auth_token,
      const char* host_jid,
      const char* host_id,
      const char* host_pubkey);

  // Terminates the current connection (if it hasn't already failed) and cleans
  // up. Must be called before destruction.
  void Cleanup();

  // Provides the user's PIN and resumes the host authentication attempt. Call
  // on the UI thread once the user has finished entering this PIN into the UI,
  // but only after the UI has been asked to provide a PIN (via FetchSecret()).
  void ProvideSecret(const char* pin);

  // ClientUserInterface implementation.
  virtual void OnConnectionState(
      protocol::ConnectionToHost::State state,
      protocol::ErrorCode error) OVERRIDE;
  virtual void OnConnectionReady(bool ready) OVERRIDE;
  virtual void SetCapabilities(const std::string& capabilities) OVERRIDE;
  virtual void SetPairingResponse(
      const protocol::PairingResponse& response) OVERRIDE;
  virtual protocol::ClipboardStub* GetClipboardStub() OVERRIDE;
  virtual protocol::CursorShapeStub* GetCursorShapeStub() OVERRIDE;
  virtual scoped_ptr<protocol::ThirdPartyClientAuthenticator::TokenFetcher>
      GetTokenFetcher(const std::string& host_public_key) OVERRIDE;

 private:
  // This object is ref-counted, so it cleans itself up.
  virtual ~ChromotingJniInstance();

  void ConnectToHostOnDisplayThread();
  void ConnectToHostOnNetworkThread();

  // Notifies the user interface that the user needs to enter a PIN. The
  // current authentication attempt is put on hold until |callback| is invoked.
  void FetchSecret(bool pairable,
                   const protocol::SecretFetchedCallback& callback);

  scoped_refptr<FrameConsumerProxy> frame_consumer_;

  // This group of variables is to be used on the network thread.
  scoped_ptr<ClientConfig> client_config_;
  scoped_ptr<ClientContext> client_context_;
  scoped_ptr<protocol::ConnectionToHost> connection_;
  scoped_ptr<ChromotingClient> client_;
  scoped_ptr<XmppSignalStrategy::XmppServerConfig> signaling_config_;
  scoped_ptr<XmppSignalStrategy> signaling_;  // Must outlive client_
  scoped_ptr<NetworkSettings> network_settings_;

  // Pass this the user's PIN once we have it. To be assigned and accessed on
  // the UI thread, but must be posted to the network thread to call it.
  protocol::SecretFetchedCallback pin_callback_;

  // These strings describe the current connection, and are not reused. They
  // are initialized in ConnectionToHost(), but thereafter are only to be used
  // on the network thread. (This is safe because ConnectionToHost()'s finishes
  // using them on the UI thread before they are ever touched from network.)
  std::string username_;
  std::string auth_token_;
  std::string host_jid_;
  std::string host_id_;
  std::string host_pubkey_;

  friend class base::RefCountedThreadSafe<ChromotingJniInstance>;

  DISALLOW_COPY_AND_ASSIGN(ChromotingJniInstance);
};

}  // namespace remoting

#endif
