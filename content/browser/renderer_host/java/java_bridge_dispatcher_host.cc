// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/java/java_bridge_dispatcher_host.h"

#include "base/bind.h"
#include "content/browser/renderer_host/browser_render_process_host.h"
#include "content/browser/renderer_host/java/java_bridge_channel_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/common/child_process.h"
#include "content/common/java_bridge_messages.h"
#include "content/common/npobject_stub.h"
#include "content/common/npobject_util.h"  // For CreateNPVariantParam()
#include "content/public/browser/browser_thread.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebBindings.h"

using content::BrowserThread;

JavaBridgeDispatcherHost::JavaBridgeDispatcherHost(
    RenderViewHost* render_view_host)
    : RenderViewHostObserver(render_view_host),
      is_renderer_initialized_(false) {
}

JavaBridgeDispatcherHost::~JavaBridgeDispatcherHost() {
}

void JavaBridgeDispatcherHost::AddNamedObject(const string16& name,
                                              NPObject* object) {
  NPVariant_Param variant_param;
  CreateNPVariantParam(object, &variant_param);

  if (!is_renderer_initialized_) {
    is_renderer_initialized_ = true;
    Send(new JavaBridgeMsg_Init(routing_id()));
  }
  Send(new JavaBridgeMsg_AddNamedObject(routing_id(), name, variant_param));
}

void JavaBridgeDispatcherHost::RemoveNamedObject(const string16& name) {
  // On receipt of this message, the JavaBridgeDispatcher will drop its
  // reference to the corresponding proxy object. When the last reference is
  // removed, the proxy object will delete its NPObjectProxy, which will cause
  // the NPObjectStub to be deleted, which will drop its reference to the
  // original NPObject.
  Send(new JavaBridgeMsg_RemoveNamedObject(routing_id(), name));
}

bool JavaBridgeDispatcherHost::Send(IPC::Message* msg) {
  return RenderViewHostObserver::Send(msg);
}

bool JavaBridgeDispatcherHost::OnMessageReceived(const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(JavaBridgeDispatcherHost, msg)
    IPC_MESSAGE_HANDLER_DELAY_REPLY(JavaBridgeHostMsg_GetChannelHandle,
                                    OnGetChannelHandle)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void JavaBridgeDispatcherHost::OnGetChannelHandle(IPC::Message* reply_msg) {
  if (RenderProcessHost::run_renderer_in_process()) {
    // TODO(steveblock): Fix Java Bridge with in-process renderer. See
    // http://code.google.com/p/chromium/issues/detail?id=106838
    CHECK(false) << "Java Bridge does not support in-process renderer";
  }
  BrowserThread::PostTask(
      BrowserThread::WEBKIT,
      FROM_HERE,
      base::Bind(&JavaBridgeDispatcherHost::GetChannelHandle, this, reply_msg));
}

void JavaBridgeDispatcherHost::GetChannelHandle(IPC::Message* reply_msg) {
  // The channel creates the channel handle based on the renderer ID we passed
  // to GetJavaBridgeChannelHost() and, on POSIX, the file descriptor used by
  // the underlying channel.
  JavaBridgeHostMsg_GetChannelHandle::WriteReplyParams(
      reply_msg,
      channel_->channel_handle());
  Send(reply_msg);
}

void JavaBridgeDispatcherHost::CreateNPVariantParam(NPObject* object,
                                                    NPVariant_Param* param) {
  // The JavaBridgeChannelHost needs to be created on the WEBKIT thread, as
  // that is where Java objects will live, and CreateNPVariantParam() needs the
  // channel to create the NPObjectStub. To avoid blocking here until the
  // channel is ready, create the NPVariant_Param by hand, then post a message
  // to the WEBKIT thread to set up the channel and create the corresponding
  // NPObjectStub. Post that message before doing any IPC, to make sure that
  // the channel and object proxies are ready before responses are received
  // from the renderer.

  // Create an NPVariantParam suitable for serialization over IPC from our
  // NPVariant. See CreateNPVariantParam() in npobject_utils.
  param->type = NPVARIANT_PARAM_SENDER_OBJECT_ROUTING_ID;
  int route_id = JavaBridgeChannelHost::ThreadsafeGenerateRouteID();
  param->npobject_routing_id = route_id;

  WebKit::WebBindings::retainObject(object);
  BrowserThread::PostTask(
      BrowserThread::WEBKIT,
      FROM_HERE,
      base::Bind(&JavaBridgeDispatcherHost::CreateObjectStub, this, object,
                 route_id));
}

void JavaBridgeDispatcherHost::CreateObjectStub(NPObject* object,
                                                int route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));

  if (!channel_) {
    channel_ = JavaBridgeChannelHost::GetJavaBridgeChannelHost(
        render_view_host()->process()->id(),
        BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO));
  }

  // We don't need the containing window or the page URL, as we don't do
  // re-entrant sync IPC.
  new NPObjectStub(object, channel_, route_id, 0, GURL());
  // The NPObjectStub takes a reference to the NPObject. Release the ref added
  // in CreateNPVariantParam().
  WebKit::WebBindings::releaseObject(object);
}
