// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_
#define REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_

#include <list>
#include <string>

#include "base/memory/ref_counted.h"
#include "net/base/x509_certificate.h"
#include "remoting/protocol/content_description.h"
#include "remoting/protocol/jingle_session.h"
#include "remoting/protocol/session_manager.h"
#include "third_party/libjingle/source/talk/p2p/base/session.h"
#include "third_party/libjingle/source/talk/p2p/base/sessionclient.h"

class MessageLoop;

namespace cricket {
class HttpPortAllocator;
class SessionManager;
}  // namespace cricket

namespace remoting {

class JingleInfoRequest;
class JingleSignalingConnector;
class PortAllocatorSessionFactory;

namespace protocol {

// This class implements SessionClient for Chromoting sessions. It acts as a
// server that accepts chromoting connections and can also make new connections
// to other hosts.
class JingleSessionManager
    : public protocol::SessionManager,
      public cricket::SessionClient {
 public:
  JingleSessionManager(
      MessageLoop* message_loop,
      talk_base::NetworkManager* network_manager,
      talk_base::PacketSocketFactory* socket_factory,
      PortAllocatorSessionFactory* port_allocator_session_factory);

  // SessionManager interface.
  virtual void Init(const std::string& local_jid,
                    SignalStrategy* signal_strategy,
                    IncomingSessionCallback* incoming_session_callback,
                    crypto::RSAPrivateKey* private_key,
                    scoped_refptr<net::X509Certificate> certificate) OVERRIDE;
  virtual scoped_refptr<protocol::Session> Connect(
      const std::string& host_jid,
      const std::string& host_public_key,
      const std::string& client_token,
      CandidateSessionConfig* config,
      protocol::Session::StateChangeCallback* state_change_callback) OVERRIDE;
  virtual void Close(Task* closed_task) OVERRIDE;

  void set_allow_local_ips(bool allow_local_ips);

  // cricket::SessionClient interface.
  virtual void OnSessionCreate(cricket::Session* cricket_session,
                               bool received_initiate);
  virtual void OnSessionDestroy(cricket::Session* cricket_session);

  virtual bool ParseContent(cricket::SignalingProtocol protocol,
                            const buzz::XmlElement* elem,
                            const cricket::ContentDescription** content,
                            cricket::ParseError* error);
  virtual bool WriteContent(cricket::SignalingProtocol protocol,
                            const cricket::ContentDescription* content,
                            buzz::XmlElement** elem,
                            cricket::WriteError* error);

 protected:
  virtual ~JingleSessionManager();

 private:
  friend class JingleSession;

  // Message loop that corresponds to the network thread.
  MessageLoop* message_loop();

  // Called by JingleChromotocolConnection when a new connection is initiated.
  void AcceptConnection(JingleSession* jingle_session,
                        cricket::Session* cricket_session);

  void DoConnect(
      scoped_refptr<JingleSession> jingle_session,
      const std::string& host_jid,
      const std::string& host_public_key,
      const std::string& client_token,
      protocol::Session::StateChangeCallback* state_change_callback);

  // Callback for JingleInfoRequest.
  void OnJingleInfo(
      const std::string& token,
      const std::vector<std::string>& relay_hosts,
      const std::vector<talk_base::SocketAddress>& stun_hosts);

  // Creates cricket::SessionManager.
  void DoStartSessionManager();

  // Creates session description for outgoing session.
  cricket::SessionDescription* CreateClientSessionDescription(
      const CandidateSessionConfig* candidate_config,
      const std::string& auth_token,
      const std::string& master_key);
  // Creates session description for incoming session.
  cricket::SessionDescription* CreateHostSessionDescription(
      const CandidateSessionConfig* candidate_config,
      scoped_refptr<net::X509Certificate> certificate);

  MessageLoop* message_loop_;
  scoped_ptr<talk_base::NetworkManager> network_manager_;
  scoped_ptr<talk_base::PacketSocketFactory> socket_factory_;
  scoped_ptr<PortAllocatorSessionFactory> port_allocator_session_factory_;

  std::string local_jid_;  // Full jid for the local side of the session.
  SignalStrategy* signal_strategy_;
  scoped_ptr<IncomingSessionCallback> incoming_session_callback_;
  scoped_refptr<net::X509Certificate> certificate_;
  scoped_ptr<crypto::RSAPrivateKey> private_key_;

  // This must be set to true to enable NAT traversal. STUN/Relay
  // servers are not used when NAT traversal is disabled, so P2P
  // connection will works only when both peers are on the same
  // network.
  bool enable_nat_traversing_;
  bool allow_local_ips_;

  scoped_ptr<cricket::HttpPortAllocator> port_allocator_;
  scoped_ptr<cricket::SessionManager> cricket_session_manager_;
  scoped_ptr<JingleInfoRequest> jingle_info_request_;
  scoped_ptr<JingleSignalingConnector> jingle_signaling_connector_;

  bool closed_;

  std::list<scoped_refptr<JingleSession> > sessions_;

  DISALLOW_COPY_AND_ASSIGN(JingleSessionManager);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_JINGLE_SESSION_MANAGER_H_
