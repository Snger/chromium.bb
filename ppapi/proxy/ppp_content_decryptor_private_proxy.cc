// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/proxy/ppp_content_decryptor_private_proxy.h"

#include "base/platform_file.h"
#include "ppapi/c/pp_bool.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/proxy/host_dispatcher.h"
#include "ppapi/proxy/plugin_globals.h"
#include "ppapi/proxy/plugin_resource_tracker.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/proxy/ppb_buffer_proxy.h"
#include "ppapi/proxy/serialized_var.h"
#include "ppapi/shared_impl/var_tracker.h"
#include "ppapi/thunk/enter.h"
#include "ppapi/thunk/ppb_buffer_api.h"
#include "ppapi/thunk/ppb_buffer_trusted_api.h"
#include "ppapi/thunk/ppb_instance_api.h"
#include "ppapi/thunk/thunk.h"

using ppapi::thunk::EnterResourceNoLock;
using ppapi::thunk::PPB_Buffer_API;
using ppapi::thunk::PPB_BufferTrusted_API;
using ppapi::thunk::PPB_Instance_API;

namespace ppapi {
namespace proxy {

namespace {

PP_Bool DescribeHostBufferResource(PP_Resource resource, uint32_t* size) {
  EnterResourceNoLock<PPB_Buffer_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  return enter.object()->Describe(size);
}

// TODO(dmichael): Refactor so this handle sharing code is in one place.
PP_Bool ShareHostBufferResourceToPlugin(
    HostDispatcher* dispatcher,
    PP_Resource resource,
    base::SharedMemoryHandle* shared_mem_handle) {
  if (!dispatcher || resource == 0 || !shared_mem_handle)
    return PP_FALSE;
  EnterResourceNoLock<PPB_BufferTrusted_API> enter(resource, true);
  if (enter.failed())
    return PP_FALSE;
  int handle;
  int32_t result = enter.object()->GetSharedMemory(&handle);
  if (result != PP_OK)
    return PP_FALSE;
  base::PlatformFile platform_file =
  #if defined(OS_WIN)
      reinterpret_cast<HANDLE>(static_cast<intptr_t>(handle));
  #elif defined(OS_POSIX)
      handle;
  #else
  #error Not implemented.
  #endif

  *shared_mem_handle = dispatcher->ShareHandleWithRemote(platform_file, false);
  return PP_TRUE;
}

// SerializedVarReceiveInput will decrement the reference count, but we want
// to give the recipient a reference. This utility function takes care of that
// work for the message handlers defined below.
PP_Var ExtractReceivedVarAndAddRef(Dispatcher* dispatcher,
                                   SerializedVarReceiveInput* serialized_var) {
  PP_Var var = serialized_var->Get(dispatcher);
  PpapiGlobals::Get()->GetVarTracker()->AddRefVar(var);
  return var;
}

PP_Bool GenerateKeyRequest(PP_Instance instance,
                           PP_Var key_system,
                           PP_Var init_data) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return PP_FALSE;
  }

  return PP_FromBool(dispatcher->Send(
      new PpapiMsg_PPPContentDecryptor_GenerateKeyRequest(
          API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
          instance,
          SerializedVarSendInput(dispatcher, key_system),
          SerializedVarSendInput(dispatcher, init_data))));
}

PP_Bool AddKey(PP_Instance instance,
               PP_Var session_id,
               PP_Var key) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return PP_FALSE;
  }

  return PP_FromBool(dispatcher->Send(
      new PpapiMsg_PPPContentDecryptor_AddKey(
          API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
          instance,
          SerializedVarSendInput(dispatcher, session_id),
          SerializedVarSendInput(dispatcher, key))));
}

PP_Bool CancelKeyRequest(PP_Instance instance, PP_Var session_id) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return PP_FALSE;
  }

  return PP_FromBool(dispatcher->Send(
      new PpapiMsg_PPPContentDecryptor_CancelKeyRequest(
          API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
          instance,
          SerializedVarSendInput(dispatcher, session_id))));
}

PP_Bool Decrypt(PP_Instance instance,
                PP_Resource encrypted_block,
                int32_t request_id) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return PP_FALSE;
  }
  const PPB_Core* core = static_cast<const PPB_Core*>(
      dispatcher->local_get_interface()(PPB_CORE_INTERFACE));
  if (!core) {
    NOTREACHED();
    return PP_FALSE;
  }

  // We need to take a ref on the resource now. The browser may drop
  // references once we return from here, but we're sending an asynchronous
  // message. The plugin side takes ownership of that reference.
  core->AddRefResource(encrypted_block);

  HostResource host_resource;
  host_resource.SetHostResource(instance, encrypted_block);

  uint32_t size = 0;
  if (DescribeHostBufferResource(encrypted_block, &size) == PP_FALSE)
    return PP_FALSE;

  base::SharedMemoryHandle handle;
  if (ShareHostBufferResourceToPlugin(dispatcher,
                                      encrypted_block,
                                      &handle) == PP_FALSE)
    return PP_FALSE;

  PPPDecryptor_Buffer buffer;
  buffer.resource = host_resource;
  buffer.handle = handle;
  buffer.size = size;

  return PP_FromBool(dispatcher->Send(
      new PpapiMsg_PPPContentDecryptor_Decrypt(
          API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
          instance,
          buffer,
          request_id)));
}

PP_Bool DecryptAndDecode(PP_Instance instance,
                         PP_Resource encrypted_block,
                         int32_t request_id) {
  HostDispatcher* dispatcher = HostDispatcher::GetForInstance(instance);
  if (!dispatcher) {
    NOTREACHED();
    return PP_FALSE;
  }

  HostResource host_resource;
  host_resource.SetHostResource(instance, encrypted_block);

  return PP_FromBool(dispatcher->Send(
      new PpapiMsg_PPPContentDecryptor_DecryptAndDecode(
          API_ID_PPP_CONTENT_DECRYPTOR_PRIVATE,
          instance,
          host_resource,
          request_id)));
}

static const PPP_ContentDecryptor_Private content_decryptor_interface = {
  &GenerateKeyRequest,
  &AddKey,
  &CancelKeyRequest,
  &Decrypt,
  &DecryptAndDecode
};

InterfaceProxy* CreateContentDecryptorPPPProxy(Dispatcher* dispatcher) {
  return new PPP_ContentDecryptor_Private_Proxy(dispatcher);
}

}  // namespace

PPP_ContentDecryptor_Private_Proxy::PPP_ContentDecryptor_Private_Proxy(
    Dispatcher* dispatcher)
    : InterfaceProxy(dispatcher),
      ppp_decryptor_impl_(NULL) {
  if (dispatcher->IsPlugin()) {
    ppp_decryptor_impl_ = static_cast<const PPP_ContentDecryptor_Private*>(
        dispatcher->local_get_interface()(
            PPP_CONTENTDECRYPTOR_PRIVATE_INTERFACE));
  }
}

PPP_ContentDecryptor_Private_Proxy::~PPP_ContentDecryptor_Private_Proxy() {
}

// static
const PPP_ContentDecryptor_Private*
    PPP_ContentDecryptor_Private_Proxy::GetProxyInterface() {
  return &content_decryptor_interface;
}

bool PPP_ContentDecryptor_Private_Proxy::OnMessageReceived(
    const IPC::Message& msg) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(PPP_ContentDecryptor_Private_Proxy, msg)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPContentDecryptor_GenerateKeyRequest,
                        OnMsgGenerateKeyRequest)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPContentDecryptor_AddKey,
                        OnMsgAddKey)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPContentDecryptor_CancelKeyRequest,
                        OnMsgCancelKeyRequest)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPContentDecryptor_Decrypt,
                        OnMsgDecrypt)
    IPC_MESSAGE_HANDLER(PpapiMsg_PPPContentDecryptor_DecryptAndDecode,
                        OnMsgDecryptAndDecode)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  DCHECK(handled);
  return handled;
}

void PPP_ContentDecryptor_Private_Proxy::OnMsgGenerateKeyRequest(
    PP_Instance instance,
    SerializedVarReceiveInput key_system,
    SerializedVarReceiveInput init_data) {
  if (ppp_decryptor_impl_) {
    CallWhileUnlocked(ppp_decryptor_impl_->GenerateKeyRequest,
                      instance,
                      ExtractReceivedVarAndAddRef(dispatcher(), &key_system),
                      ExtractReceivedVarAndAddRef(dispatcher(), &init_data));
  }
}

void PPP_ContentDecryptor_Private_Proxy::OnMsgAddKey(
    PP_Instance instance,
    SerializedVarReceiveInput session_id,
    SerializedVarReceiveInput key) {
  if (ppp_decryptor_impl_) {
    CallWhileUnlocked(ppp_decryptor_impl_->AddKey,
                      instance,
                      ExtractReceivedVarAndAddRef(dispatcher(), &session_id),
                      ExtractReceivedVarAndAddRef(dispatcher(), &key));
  }
}

void PPP_ContentDecryptor_Private_Proxy::OnMsgCancelKeyRequest(
    PP_Instance instance,
    SerializedVarReceiveInput session_id) {
  if (ppp_decryptor_impl_) {
    CallWhileUnlocked(ppp_decryptor_impl_->CancelKeyRequest,
                      instance,
                      ExtractReceivedVarAndAddRef(dispatcher(), &session_id));
  }
}

void PPP_ContentDecryptor_Private_Proxy::OnMsgDecrypt(
    PP_Instance instance,
    const PPPDecryptor_Buffer& encrypted_buffer,
    int32_t request_id) {
  if (ppp_decryptor_impl_) {
    PP_Resource plugin_resource =
        PPB_Buffer_Proxy::AddProxyResource(encrypted_buffer.resource,
                                           encrypted_buffer.handle,
                                           encrypted_buffer.size);
    CallWhileUnlocked(ppp_decryptor_impl_->Decrypt,
                      instance,
                      plugin_resource,
                      request_id);
  }
}

void PPP_ContentDecryptor_Private_Proxy::OnMsgDecryptAndDecode(
    PP_Instance instance,
    const HostResource& encrypted_block,
    int32_t request_id) {
  if (ppp_decryptor_impl_) {
    PP_Resource plugin_resource =
        PluginGlobals::Get()->plugin_resource_tracker()->
            PluginResourceForHostResource(encrypted_block);
    CallWhileUnlocked(ppp_decryptor_impl_->DecryptAndDecode,
                      instance,
                      plugin_resource,
                      request_id);
  }
}

}  // namespace proxy
}  // namespace ppapi
