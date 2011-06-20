// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_STATUS_OBSERVER_H_
#define REMOTING_HOST_STATUS_OBSERVER_H_

namespace remoting {

class SignalStrategy;

class HostStatusObserver {
 public:
  HostStatusObserver() { }
  virtual ~HostStatusObserver() { }

  // Called on the network thread when status of the XMPP changes.
  virtual void OnSignallingConnected(SignalStrategy* signal_strategy,
                                     const std::string& full_jid) = 0;
  virtual void OnSignallingDisconnected() = 0;

  // Called on the main thread when the host shuts down.
  virtual void OnShutdown() = 0;
};

}  // namespace remoting

#endif  // REMOTING_HOST_STATUS_OBSERVER_H_
