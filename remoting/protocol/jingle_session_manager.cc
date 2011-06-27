// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/protocol/jingle_session_manager.h"

#include <limits>

#include "base/message_loop.h"
#include "remoting/base/constants.h"
#include "third_party/libjingle/source/talk/p2p/base/constants.h"
#include "third_party/libjingle/source/talk/p2p/base/transport.h"
#include "third_party/libjingle/source/talk/xmllite/xmlelement.h"

using buzz::XmlElement;

namespace remoting {
namespace protocol {

JingleSessionManager::JingleSessionManager(MessageLoop* message_loop)
    : message_loop_(message_loop),
      cricket_session_manager_(NULL),
      allow_local_ips_(false),
      closed_(false) {
  DCHECK(message_loop);
}

void JingleSessionManager::Init(
    const std::string& local_jid,
    cricket::SessionManager* cricket_session_manager,
    IncomingSessionCallback* incoming_session_callback,
    crypto::RSAPrivateKey* private_key,
    scoped_refptr<net::X509Certificate> certificate) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(
            this, &JingleSessionManager::Init,
            local_jid, cricket_session_manager, incoming_session_callback,
            private_key, certificate));
    return;
  }

  DCHECK(cricket_session_manager);
  DCHECK(incoming_session_callback);

  local_jid_ = local_jid;
  certificate_ = certificate;
  private_key_.reset(private_key);
  incoming_session_callback_.reset(incoming_session_callback);
  cricket_session_manager_ = cricket_session_manager;
  cricket_session_manager_->AddClient(kChromotingXmlNamespace, this);
}

JingleSessionManager::~JingleSessionManager() {
  DCHECK(closed_);
}

void JingleSessionManager::Close(Task* closed_task) {
  if (MessageLoop::current() != message_loop_) {
    message_loop_->PostTask(
        FROM_HERE, NewRunnableMethod(this, &JingleSessionManager::Close,
                                     closed_task));
    return;
  }

  if (!closed_) {
    // Close all connections.
    cricket_session_manager_->RemoveClient(kChromotingXmlNamespace);
    while (!sessions_.empty()) {
      cricket::Session* session = sessions_.front()->ReleaseSession();
      cricket_session_manager_->DestroySession(session);
      sessions_.pop_front();
    }
    closed_ = true;
  }

  closed_task->Run();
  delete closed_task;
}

void JingleSessionManager::set_allow_local_ips(bool allow_local_ips) {
  allow_local_ips_ = allow_local_ips;
}

scoped_refptr<protocol::Session> JingleSessionManager::Connect(
    const std::string& host_jid,
    const std::string& host_public_key,
    const std::string& receiver_token,
    CandidateSessionConfig* candidate_config,
    protocol::Session::StateChangeCallback* state_change_callback) {
  // Can be called from any thread.
  scoped_refptr<JingleSession> jingle_session(
      JingleSession::CreateClientSession(this, host_public_key));
  jingle_session->set_candidate_config(candidate_config);
  jingle_session->set_receiver_token(receiver_token);

  message_loop_->PostTask(
      FROM_HERE, NewRunnableMethod(this, &JingleSessionManager::DoConnect,
                                   jingle_session, host_jid,
                                   host_public_key, receiver_token,
                                   state_change_callback));
  return jingle_session;
}

void JingleSessionManager::DoConnect(
    scoped_refptr<JingleSession> jingle_session,
    const std::string& host_jid,
    const std::string& host_public_key,
    const std::string& receiver_token,
    protocol::Session::StateChangeCallback* state_change_callback) {
  DCHECK_EQ(message_loop_, MessageLoop::current());
  cricket::Session* cricket_session = cricket_session_manager_->CreateSession(
      local_jid_, kChromotingXmlNamespace);

  // Initialize connection object before we send initiate stanza.
  jingle_session->SetStateChangeCallback(state_change_callback);
  jingle_session->Init(cricket_session);
  sessions_.push_back(jingle_session);

  cricket_session->Initiate(host_jid, CreateClientSessionDescription(
      jingle_session->candidate_config()->Clone(), receiver_token,
      jingle_session->GetEncryptedMasterKey()));
}

MessageLoop* JingleSessionManager::message_loop() {
  return message_loop_;
}

void JingleSessionManager::OnSessionCreate(
    cricket::Session* cricket_session, bool incoming) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Allow local connections if neccessary.
  cricket_session->set_allow_local_ips(allow_local_ips_);

  // If this is an outcoming session the session object is already created.
  if (incoming) {
    DCHECK(certificate_);
    DCHECK(private_key_.get());

    JingleSession* jingle_session = JingleSession::CreateServerSession(
        this, certificate_, private_key_.get());
    sessions_.push_back(make_scoped_refptr(jingle_session));
    jingle_session->Init(cricket_session);
  }
}

void JingleSessionManager::OnSessionDestroy(cricket::Session* cricket_session) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  std::list<scoped_refptr<JingleSession> >::iterator it;
  for (it = sessions_.begin(); it != sessions_.end(); ++it) {
    if ((*it)->HasSession(cricket_session)) {
      (*it)->ReleaseSession();
      sessions_.erase(it);
      return;
    }
  }
}

void JingleSessionManager::AcceptConnection(
    JingleSession* jingle_session,
    cricket::Session* cricket_session) {
  DCHECK_EQ(message_loop_, MessageLoop::current());

  // Reject connection if we are closed.
  if (closed_) {
    cricket_session->Reject(cricket::STR_TERMINATE_DECLINE);
    return;
  }

  const cricket::SessionDescription* session_description =
      cricket_session->remote_description();
  const cricket::ContentInfo* content =
      session_description->FirstContentByType(kChromotingXmlNamespace);

  CHECK(content);

  const ContentDescription* content_description =
      static_cast<const ContentDescription*>(content->description);
  jingle_session->set_candidate_config(content_description->config()->Clone());
  jingle_session->set_initiator_token(content_description->auth_token());

  // Always reject connection if there is no callback.
  IncomingSessionResponse response = protocol::SessionManager::DECLINE;

  // Use the callback to generate a response.
  if (incoming_session_callback_.get())
    incoming_session_callback_->Run(jingle_session, &response);

  switch (response) {
    case protocol::SessionManager::ACCEPT: {
      // Connection must be configured by the callback.
      DCHECK(jingle_session->config());
      CandidateSessionConfig* candidate_config =
          CandidateSessionConfig::CreateFrom(jingle_session->config());
      cricket_session->Accept(
          CreateHostSessionDescription(candidate_config,
                                       jingle_session->local_certificate()));
      break;
    }

    case protocol::SessionManager::INCOMPATIBLE: {
      cricket_session->Reject(cricket::STR_TERMINATE_INCOMPATIBLE_PARAMETERS);
      break;
    }

    case protocol::SessionManager::DECLINE: {
      cricket_session->Reject(cricket::STR_TERMINATE_DECLINE);
      break;
    }

    default: {
      NOTREACHED();
    }
  }
}

// Parse content description generated by WriteContent().
bool JingleSessionManager::ParseContent(
    cricket::SignalingProtocol protocol,
    const XmlElement* element,
    const cricket::ContentDescription** content,
    cricket::ParseError* error) {
  *content = ContentDescription::ParseXml(element);
  return *content != NULL;
}

bool JingleSessionManager::WriteContent(
    cricket::SignalingProtocol protocol,
    const cricket::ContentDescription* content,
    XmlElement** elem,
    cricket::WriteError* error) {
  const ContentDescription* desc =
      static_cast<const ContentDescription*>(content);

  *elem = desc->ToXml();
  return true;
}

cricket::SessionDescription*
JingleSessionManager::CreateClientSessionDescription(
    const CandidateSessionConfig* config,
    const std::string& auth_token,
    const std::string& master_key) {
  cricket::SessionDescription* desc = new cricket::SessionDescription();
  desc->AddContent(
      JingleSession::kChromotingContentName, kChromotingXmlNamespace,
      new ContentDescription(config, auth_token, master_key, NULL));
  return desc;
}

cricket::SessionDescription* JingleSessionManager::CreateHostSessionDescription(
    const CandidateSessionConfig* config,
    scoped_refptr<net::X509Certificate> certificate) {
  cricket::SessionDescription* desc = new cricket::SessionDescription();
  desc->AddContent(
      JingleSession::kChromotingContentName, kChromotingXmlNamespace,
      new ContentDescription(config, "", "", certificate));
  return desc;
}

}  // namespace protocol
}  // namespace remoting
