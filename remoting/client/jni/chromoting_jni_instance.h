// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_CLIENT_CHROMOTING_JNI_INSTANCE_H_
#define REMOTING_CLIENT_CHROMOTING_JNI_INSTANCE_H_

#include <jni.h>
#include <string>

#include "base/at_exit.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "net/url_request/url_request_context_getter.h"
#include "remoting/base/auto_thread.h"
#include "remoting/client/client_user_interface.h"

template<typename T> struct DefaultSingletonTraits;

// Class and package name of the Java class supporting the methods we call.
const char* const JAVA_CLASS="org/chromium/chromoting/jni/JNIInterface";

namespace remoting {

// ClientUserInterface that makes and (indirectly) receives JNI calls.
class ChromotingJNIInstance : public ClientUserInterface {
 public:
  static ChromotingJNIInstance* GetInstance();

  // Call from UI thread.
  void ConnectToHost(
      jstring username,
      jstring auth_token,
      jstring host_jid,
      jstring host_id,
      jstring host_pubkey);

  // Call from UI thread.
  void DisconnectFromHost();

  // ClientUserInterface implementation:
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
  ChromotingJNIInstance();
  virtual ~ChromotingJNIInstance();

  // Reusable between sessions:
  jclass class_;  // Reference to the Java class into which we make JNI calls.
  scoped_ptr<base::AtExitManager> collector_;
  scoped_ptr<base::MessageLoopForUI> ui_loop_;
  scoped_refptr<AutoThreadTaskRunner> ui_runner_;
  scoped_refptr<AutoThreadTaskRunner> net_runner_;
  scoped_refptr<AutoThreadTaskRunner> disp_runner_;
  scoped_refptr<net::URLRequestContextGetter> url_requester_;

  // Java string handles:
  jstring username_jstr_;
  jstring auth_token_jstr_;
  jstring host_jid_jstr_;
  jstring host_id_jstr_;
  jstring host_pubkey_jstr_;
  jstring pin_jstr_;

  // C string pointers:
  const char* username_cstr_;
  const char* auth_token_cstr_;
  const char* host_jid_cstr_;
  const char* host_id_cstr_;
  const char* host_pubkey_cstr_;
  const char* pin_cstr_;

  friend struct DefaultSingletonTraits<ChromotingJNIInstance>;

  DISALLOW_COPY_AND_ASSIGN(ChromotingJNIInstance);
};

}  // namespace remoting

#endif
