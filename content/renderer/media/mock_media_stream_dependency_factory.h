// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DEPENDENCY_FACTORY_H_
#define CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DEPENDENCY_FACTORY_H_

#include "base/compiler_specific.h"
#include "content/renderer/media/media_stream_dependency_factory.h"

// A mock factory for creating different objects for MediaStreamImpl.
class MockMediaStreamDependencyFactory : public MediaStreamDependencyFactory {
 public:
  MockMediaStreamDependencyFactory();
  virtual ~MockMediaStreamDependencyFactory();

  virtual cricket::WebRtcMediaEngine* CreateWebRtcMediaEngine() OVERRIDE;
  virtual bool CreatePeerConnectionFactory(
      cricket::MediaEngineInterface* media_engine,
      talk_base::Thread* worker_thread) OVERRIDE;
  virtual void DeletePeerConnectionFactory() OVERRIDE;
  virtual bool PeerConnectionFactoryCreated() OVERRIDE;
  virtual cricket::PortAllocator* CreatePortAllocator(
      content::P2PSocketDispatcher* socket_dispatcher,
      talk_base::NetworkManager* network_manager,
      talk_base::PacketSocketFactory* socket_factory,
      const webkit_glue::P2PTransport::Config& config) OVERRIDE;
  virtual webrtc::PeerConnection* CreatePeerConnection(
      cricket::PortAllocator* port_allocator,
      talk_base::Thread* signaling_thread) OVERRIDE;

 private:
  bool mock_pc_factory_created_;
  scoped_ptr<cricket::MediaEngineInterface> media_engine_;

  DISALLOW_COPY_AND_ASSIGN(MockMediaStreamDependencyFactory);
};

#endif  // CONTENT_RENDERER_MEDIA_MOCK_MEDIA_STREAM_DEPENDENCY_FACTORY_H_
