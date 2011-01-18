/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/shared/ppapi_proxy/browser_ppp_instance.h"

#include <stdio.h>
#include <string.h>
#include <limits>

// Include file order cannot be observed because ppp_instance declares a
// structure return type that causes an error on Windows.
// TODO(sehr, brettw): fix the return types and include order in PPAPI.
#include "ppapi/c/pp_input_event.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppp_instance.h"
#include "srpcgen/ppp_rpc.h"
#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/nacl_scoped_ptr.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/shared/ppapi_proxy/object_capability.h"
#include "native_client/src/shared/ppapi_proxy/object_proxy.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"

using nacl::scoped_array;

namespace ppapi_proxy {

namespace {

char* ArgArraySerialize(int argc,
                        const char* array[],
                        uint32_t* serial_size) {
  uint32_t used = 0;
  for (int i = 0; i < argc; ++i) {
    // Note that strlen() cannot ever return SIZE_T_MAX, since that would imply
    // that there were no nulls anywhere in memory, which would lead to
    // strlen() never terminating. So this assignment is safe.
    size_t len = strlen(array[i]) + 1;
    if (len > std::numeric_limits<uint32_t>::max()) {
      // Overflow, input string is too long.
      return NULL;
    }
    if (used > std::numeric_limits<uint32_t>::max() - len) {
      // Overflow, output string is too long.
      return NULL;
    }
    used += static_cast<uint32_t>(len);
  }
  // Note that there is a check against numeric_limits<uint32_t> in the code
  // above, which is why this cast is safe.
  *serial_size = used;
  char* serial_array = new char[used];

  size_t pos = 0;
  for (int i = 0; i < argc; ++i) {
    size_t len = strlen(array[i]) + 1;
    strncpy(serial_array + pos, array[i], len);
    pos += len;
  }
  return serial_array;
}

PP_Bool DidCreate(PP_Instance instance,
                  uint32_t argc,
                  const char* argn[],
                  const char* argv[]) {
  DebugPrintf("PPP_Instance::DidCreate(%"NACL_PRId32")\n", instance);
  uint32_t argn_size;
  scoped_array<char> argn_serial(ArgArraySerialize(argc, argn, &argn_size));
  if (argn_serial.get() == NULL) {
    return PP_FALSE;
  }
  uint32_t argv_size;
  scoped_array<char> argv_serial(ArgArraySerialize(argc, argv, &argv_size));
  if (argv_serial.get() == NULL) {
    return PP_FALSE;
  }
  int32_t int_argc = static_cast<int32_t>(argc);
  int32_t success;
  NaClSrpcError retval =
      PppInstanceRpcClient::PPP_Instance_DidCreate(GetMainSrpcChannel(instance),
                                                   instance,
                                                   int_argc,
                                                   argn_size,
                                                   argn_serial.get(),
                                                   argv_size,
                                                   argv_serial.get(),
                                                   &success);
  if (retval != NACL_SRPC_RESULT_OK) {
    return PP_FALSE;
  }
  return static_cast<PP_Bool>(success != 0);
}

void DidDestroy(PP_Instance instance) {
  DebugPrintf("PPP_Instance::Delete(%"NACL_PRId32")\n", instance);
  (void) PppInstanceRpcClient::PPP_Instance_DidDestroy(
      GetMainSrpcChannel(instance), instance);
}

void DidChangeView(PP_Instance instance,
                   const PP_Rect* position,
                   const PP_Rect* clip) {
  DebugPrintf("PPP_Instance::DidChangeView(%"NACL_PRId32")\n",
              instance);
  int32_t position_array[4];
  const uint32_t kPositionArraySize = NACL_ARRAY_SIZE(position_array);
  position_array[0] = position->point.x;
  position_array[1] = position->point.y;
  position_array[2] = position->size.width;
  position_array[3] = position->size.height;
  int32_t clip_array[4];
  const uint32_t kClipArraySize = NACL_ARRAY_SIZE(clip_array);
  clip_array[0] = clip->point.x;
  clip_array[1] = clip->point.y;
  clip_array[2] = clip->size.width;
  clip_array[3] = clip->size.height;
  (void) PppInstanceRpcClient::PPP_Instance_DidChangeView(
      GetMainSrpcChannel(instance),
      instance,
      kPositionArraySize,
      position_array,
      kClipArraySize,
      clip_array);
}

void DidChangeFocus(PP_Instance instance, PP_Bool has_focus) {
  DebugPrintf("PPP_Instance::DidChangeFocus(%"NACL_PRId32")\n",
              instance);
  // DidChangeFocus() always succeeds, no need to check the SRPC return value.
  (void) PppInstanceRpcClient::PPP_Instance_DidChangeFocus(
      GetMainSrpcChannel(instance),
      instance,
      static_cast<bool>(PP_TRUE == has_focus));
}

PP_Bool HandleInputEvent(PP_Instance instance, const PP_InputEvent* event) {
  DebugPrintf("PPP_Instance::HandleInputEvent(%"NACL_PRId32")\n",
              instance);
  int32_t success;
  char* event_data = const_cast<char*>(reinterpret_cast<const char*>(event));
  NaClSrpcError retval =
      PppInstanceRpcClient::PPP_Instance_HandleInputEvent(
          GetMainSrpcChannel(instance),
          instance,
          sizeof(*event),
          event_data,
          &success);
  if (retval != NACL_SRPC_RESULT_OK) {
    return PP_FALSE;
  }
  return static_cast<PP_Bool>(success != 0);
}

PP_Bool HandleDocumentLoad(PP_Instance instance, PP_Resource url_loader) {
  DebugPrintf("PPP_Instance::HandleDocumentLoad(%"NACL_PRId32")\n",
              instance);
  // TODO(sehr): implement HandleDocumentLoad.
  UNREFERENCED_PARAMETER(instance);
  UNREFERENCED_PARAMETER(url_loader);
  return PP_FALSE;
}

PP_Var GetInstanceObject(PP_Instance instance) {
  DebugPrintf("PPP_Instance::GetInstanceObject(%"NACL_PRId32")\n",
              instance);
  ObjectCapability capability;
  uint32_t capability_bytes = static_cast<uint32_t>(sizeof(capability));
  NaClSrpcChannel* main_channel = GetMainSrpcChannel(instance);
  NaClSrpcError retval =
      PppInstanceRpcClient::PPP_Instance_GetInstanceObject(
          main_channel,
          instance,
          &capability_bytes,
          reinterpret_cast<char*>(&capability));
  if (retval != NACL_SRPC_RESULT_OK) {
    return PP_MakeUndefined();
  }
  return ObjectProxy::New(capability, main_channel);
}

}  // namespace

const PPP_Instance* BrowserInstance::GetInterface() {
  static const PPP_Instance instance_interface = {
    DidCreate,
    DidDestroy,
    DidChangeView,
    DidChangeFocus,
    HandleInputEvent,
    HandleDocumentLoad,
    GetInstanceObject
  };
  return &instance_interface;
}

}  // namespace ppapi_proxy
