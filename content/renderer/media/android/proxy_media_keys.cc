// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/android/proxy_media_keys.h"

#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "content/renderer/media/android/renderer_media_player_manager.h"
#include "content/renderer/media/crypto/key_systems.h"

namespace content {

ProxyMediaKeys::ProxyMediaKeys(
    RendererMediaPlayerManager* manager,
    int media_keys_id,
    const media::SessionCreatedCB& session_created_cb,
    const media::SessionMessageCB& session_message_cb,
    const media::SessionReadyCB& session_ready_cb,
    const media::SessionClosedCB& session_closed_cb,
    const media::SessionErrorCB& session_error_cb)
    : manager_(manager),
      media_keys_id_(media_keys_id),
      session_created_cb_(session_created_cb),
      session_message_cb_(session_message_cb),
      session_ready_cb_(session_ready_cb),
      session_closed_cb_(session_closed_cb),
      session_error_cb_(session_error_cb) {
  DCHECK(manager_);
}

ProxyMediaKeys::~ProxyMediaKeys() {
}

void ProxyMediaKeys::InitializeCDM(const std::string& key_system,
                                   const GURL& frame_url) {
#if defined(ENABLE_PEPPER_CDMS)
  NOTIMPLEMENTED();
#elif defined(OS_ANDROID)
  std::vector<uint8> uuid = GetUUID(key_system);
  DCHECK(!uuid.empty());
  manager_->InitializeCDM(media_keys_id_, this, uuid, frame_url);
#endif
}

bool ProxyMediaKeys::CreateSession(uint32 reference_id,
                                   const std::string& type,
                                   const uint8* init_data,
                                   int init_data_length) {
  manager_->GenerateKeyRequest(
      media_keys_id_,
      reference_id,
      type,
      std::vector<uint8>(init_data, init_data + init_data_length));
  return true;
}

void ProxyMediaKeys::UpdateSession(uint32 reference_id,
                                   const uint8* response,
                                   int response_length) {
  manager_->AddKey(media_keys_id_,
                   reference_id,
                   std::vector<uint8>(response, response + response_length),
                   std::vector<uint8>());
}

void ProxyMediaKeys::ReleaseSession(uint32 reference_id) {
  manager_->CancelKeyRequest(media_keys_id_, reference_id);
}

void ProxyMediaKeys::OnSessionCreated(uint32 reference_id,
                                      const std::string& session_id) {
  session_created_cb_.Run(reference_id, session_id);
}

void ProxyMediaKeys::OnSessionMessage(uint32 reference_id,
                                      const std::vector<uint8>& message,
                                      const std::string& destination_url) {
  session_message_cb_.Run(reference_id, message, destination_url);
}

void ProxyMediaKeys::OnSessionReady(uint32 reference_id) {
  session_ready_cb_.Run(reference_id);
}

void ProxyMediaKeys::OnSessionClosed(uint32 reference_id) {
  session_closed_cb_.Run(reference_id);
}

void ProxyMediaKeys::OnSessionError(uint32 reference_id,
                                    media::MediaKeys::KeyError error_code,
                                    int system_code) {
  session_error_cb_.Run(reference_id, error_code, system_code);
}

}  // namespace content
