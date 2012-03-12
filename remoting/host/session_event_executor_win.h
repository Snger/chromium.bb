// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_SESSION_EVENT_EXECUTOR_WIN_H_
#define REMOTING_HOST_SESSION_EVENT_EXECUTOR_WIN_H_

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ipc/ipc_channel.h"

#include "remoting/host/event_executor.h"
#include "remoting/protocol/input_stub.h"

class MessageLoop;

namespace base {
class MessageLoopProxy;
} // namespace base

namespace IPC {
class ChannelProxy;
} // namespace IPC

namespace remoting {

class SessionEventExecutorWin : public protocol::InputStub,
                                public IPC::Channel::Listener {
 public:
  SessionEventExecutorWin(MessageLoop* message_loop,
                          base::MessageLoopProxy* io_message_loop,
                          scoped_ptr<protocol::InputStub> nested_executor);
  ~SessionEventExecutorWin();

  // protocol::InputStub implementation.
  virtual void InjectKeyEvent(const protocol::KeyEvent& event) OVERRIDE;
  virtual void InjectMouseEvent(const protocol::MouseEvent& event) OVERRIDE;

  // IPC::Channel::Listener implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

 private:
  // Pointer to the next event executor.
  scoped_ptr<protocol::InputStub> nested_executor_;

  MessageLoop* message_loop_;

  // The Chromoting IPC channel connecting the host with the service.
  scoped_ptr<IPC::ChannelProxy> chromoting_channel_;

  bool scroll_pressed_;

  DISALLOW_COPY_AND_ASSIGN(SessionEventExecutorWin);
};

}  // namespace remoting

#endif  // REMOTING_HOST_SESSION_EVENT_EXECUTOR_WIN_H_
