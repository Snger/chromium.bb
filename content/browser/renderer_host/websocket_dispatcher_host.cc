// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/websocket_dispatcher_host.h"

#include <string>

#include "base/callback.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "content/browser/renderer_host/websocket_host.h"
#include "content/common/websocket_messages.h"

namespace content {

WebSocketDispatcherHost::WebSocketDispatcherHost(
    const GetRequestContextCallback& get_context_callback)
    : get_context_callback_(get_context_callback) {}

bool WebSocketDispatcherHost::OnMessageReceived(const IPC::Message& message,
                                                bool* message_was_ok) {
  switch (message.type()) {
    case WebSocketHostMsg_AddChannelRequest::ID:
    case WebSocketMsg_SendFrame::ID:
    case WebSocketMsg_FlowControl::ID:
    case WebSocketMsg_DropChannel::ID:
      break;

    default:
      // Every message that has not been handled by a previous filter passes
      // through here, so it is good to pass them on as efficiently as possible.
      return false;
  }

  int routing_id = message.routing_id();
  WebSocketHost* host = GetHost(routing_id);
  if (message.type() == WebSocketHostMsg_AddChannelRequest::ID) {
    if (host) {
      DVLOG(1) << "routing_id=" << routing_id << " already in use.";
      // The websocket multiplexing spec says to should drop the physical
      // connection in this case, but there isn't a real physical connection
      // to the renderer, and killing the renderer for this would seem to be a
      // little extreme. So for now just ignore the bogus request.
      return true;  // We handled the message (by ignoring it).
    }
    host = new WebSocketHost(routing_id, this, get_context_callback_.Run());
    hosts_.insert(WebSocketHostTable::value_type(routing_id, host));
  }
  if (!host) {
    DVLOG(1) << "Received invalid routing ID " << routing_id
             << " from renderer.";
    return true;  // We handled the message (by ignoring it).
  }
  return host->OnMessageReceived(message, message_was_ok);
}

WebSocketHost* WebSocketDispatcherHost::GetHost(int routing_id) const {
  WebSocketHostTable::const_iterator it = hosts_.find(routing_id);
  return it == hosts_.end() ? NULL : it->second;
}

void WebSocketDispatcherHost::SendOrDrop(IPC::Message* message) {
  if (!Send(message)) {
    DVLOG(1) << "Sending of message type " << message->type()
             << " failed. Dropping channel.";
    DeleteWebSocketHost(message->routing_id());
  }
}

void WebSocketDispatcherHost::SendAddChannelResponse(
    int routing_id,
    bool fail,
    const std::string& selected_protocol,
    const std::string& extensions) {
  SendOrDrop(new WebSocketMsg_AddChannelResponse(
      routing_id, fail, selected_protocol, extensions));
  if (fail)
    DeleteWebSocketHost(routing_id);
}

void WebSocketDispatcherHost::SendFrame(int routing_id,
                                        bool fin,
                                        WebSocketMessageType type,
                                        const std::vector<char>& data) {
  SendOrDrop(new WebSocketMsg_SendFrame(routing_id, fin, type, data));
}

void WebSocketDispatcherHost::SendFlowControl(int routing_id, int64 quota) {
  SendOrDrop(new WebSocketMsg_FlowControl(routing_id, quota));
}

void WebSocketDispatcherHost::SendClosing(int routing_id) {
  // TODO(ricea): Implement the SendClosing IPC.
}

void WebSocketDispatcherHost::DoDropChannel(int routing_id,
                                            uint16 code,
                                            const std::string& reason) {
  SendOrDrop(new WebSocketMsg_DropChannel(routing_id, code, reason));
  DeleteWebSocketHost(routing_id);
}

WebSocketDispatcherHost::~WebSocketDispatcherHost() {
  STLDeleteContainerPairSecondPointers(hosts_.begin(), hosts_.end());
}

void WebSocketDispatcherHost::DeleteWebSocketHost(int routing_id) {
  WebSocketHostTable::iterator it = hosts_.find(routing_id);
  if (it != hosts_.end()) {
    delete it->second;
    hosts_.erase(it);
  }
}

}  // namespace content
