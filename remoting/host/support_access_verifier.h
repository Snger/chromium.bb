// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SUPPORT_ACCESS_VERIFIER_H_
#define REMOTING_HOST_SUPPORT_ACCESS_VERIFIER_H_

#include "remoting/host/access_verifier.h"

#include "base/compiler_specific.h"

namespace remoting {

class HostConfig;

// SupportAccessVerifier is used in Me2Mom scenario to verify that the
// client has access code for the host.
class SupportAccessVerifier : public AccessVerifier {
 public:
  SupportAccessVerifier();
  virtual ~SupportAccessVerifier();

  bool Init();
  const std::string& access_code() const { return access_code_; }

  // AccessVerifier interface.
  virtual bool VerifyPermissions(
      const std::string& client_jid,
      const std::string& encoded_client_token) OVERRIDE;

 private:
  bool initialized_;
  std::string access_code_;

  DISALLOW_COPY_AND_ASSIGN(SupportAccessVerifier);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SUPPORT_ACCESS_VERIFIER_H_
