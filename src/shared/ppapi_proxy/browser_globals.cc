// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/shared/ppapi_proxy/browser_globals.h"
#include <stdio.h>
#include <map>
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/browser_ppp.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"

namespace ppapi_proxy {

// All of these methods are called from the browser main (UI, JavaScript, ...)
// thread.

const PP_Resource kInvalidResourceId = 0;

namespace {

std::map<PP_Instance, BrowserPpp*>* instance_to_ppp_map = NULL;
std::map<NaClSrpcChannel*, PP_Module>* channel_to_module_id_map = NULL;

// The function pointer from the browser
// Set by SetPPBGetInterface().
PPB_GetInterface get_interface;

}  // namespace

void SetBrowserPppForInstance(PP_Instance instance, BrowserPpp* browser_ppp) {
  // If there was no map, create one.
  if (instance_to_ppp_map == NULL) {
    instance_to_ppp_map = new std::map<PP_Instance, BrowserPpp*>;
  }
  // Add the instance to the map.
  (*instance_to_ppp_map)[instance] = browser_ppp;
}

void UnsetBrowserPppForInstance(PP_Instance instance) {
  if (instance_to_ppp_map == NULL) {
    // Something major is wrong here.  We are deleting a map entry
    // when there is no map.
    NACL_NOTREACHED();
    return;
  }
  // Erase the instance from the map.
  instance_to_ppp_map->erase(instance);
  // If there are no more instances alive, remove the map.
  if (instance_to_ppp_map->size() == 0) {
    delete instance_to_ppp_map;
    instance_to_ppp_map = NULL;
  }
}

BrowserPpp* LookupBrowserPppForInstance(PP_Instance instance) {
  if (instance_to_ppp_map == NULL) {
    return NULL;
  }
  return (*instance_to_ppp_map)[instance];
}

void SetModuleIdForSrpcChannel(NaClSrpcChannel* channel, PP_Module module_id) {
  // If there was no map, create one.
  if (channel_to_module_id_map == NULL) {
    channel_to_module_id_map = new std::map<NaClSrpcChannel*, PP_Module>;
  }
  // Add the channel to the map.
  (*channel_to_module_id_map)[channel] = module_id;
}

void UnsetModuleIdForSrpcChannel(NaClSrpcChannel* channel) {
  if (channel_to_module_id_map == NULL) {
    // Something major is wrong here.  We are deleting a map entry
    // when there is no map.
    NACL_NOTREACHED();
    return;
  }
  // Erase the channel from the map.
  channel_to_module_id_map->erase(channel);
  // If there are no more channels alive, remove the map.
  if (channel_to_module_id_map->size() == 0) {
    delete channel_to_module_id_map;
    channel_to_module_id_map = NULL;
  }
}

PP_Module LookupModuleIdForSrpcChannel(NaClSrpcChannel* channel) {
  if (channel_to_module_id_map == NULL) {
    return 0;
  }
  return (*channel_to_module_id_map)[channel];
}

NaClSrpcChannel* GetMainSrpcChannel(NaClSrpcRpc* upcall_rpc) {
  // The upcall channel's server_instance_data member is initialized to point
  // to the main channel for this instance.  Here it is retrieved to use in
  // constructing a RemoteCallbackInfo.
  return reinterpret_cast<NaClSrpcChannel*>(
      upcall_rpc->channel->server_instance_data);
}

NaClSrpcChannel* GetMainSrpcChannel(PP_Instance instance) {
  return LookupBrowserPppForInstance(instance)->main_channel();
}

void SetPPBGetInterface(PPB_GetInterface get_interface_function) {
  get_interface = get_interface_function;
}

const void* GetBrowserInterface(const char* interface_name) {
  return (*get_interface)(interface_name);
}

const void* GetBrowserInterfaceSafe(const char* interface_name) {
  const void* ppb_interface = (*get_interface)(interface_name);
  if (ppb_interface == NULL)
    DebugPrintf("PPB_GetInterface: %s not found\n", interface_name);
  CHECK(ppb_interface != NULL);
  return ppb_interface;
}

const PPB_Context3D_Dev* PPBContext3DInterface() {
  static const PPB_Context3D_Dev* ppb =
      reinterpret_cast<const PPB_Context3D_Dev*>(
          GetBrowserInterfaceSafe(PPB_CONTEXT_3D_DEV_INTERFACE));
  return ppb;
}

const PPB_Context3DTrusted_Dev* PPBContext3DTrustedInterface() {
  static const PPB_Context3DTrusted_Dev* ppb =
      reinterpret_cast<const PPB_Context3DTrusted_Dev*>(
          GetBrowserInterfaceSafe(PPB_CONTEXT_3D_TRUSTED_DEV_INTERFACE));
  return ppb;
}

const PPB_Core* PPBCoreInterface() {
  static const PPB_Core* ppb = reinterpret_cast<const PPB_Core*>(
      GetBrowserInterfaceSafe(PPB_CORE_INTERFACE));
  return ppb;
}

const PPB_Graphics2D* PPBGraphics2DInterface() {
  static const PPB_Graphics2D* ppb = reinterpret_cast<const PPB_Graphics2D*>(
      GetBrowserInterfaceSafe(PPB_GRAPHICS_2D_INTERFACE));
  return ppb;
}

const PPB_ImageData* PPBImageDataInterface() {
  static const PPB_ImageData* ppb = reinterpret_cast<const PPB_ImageData*>(
      GetBrowserInterfaceSafe(PPB_IMAGEDATA_INTERFACE));
  return ppb;
}

const PPB_ImageDataTrusted* PPBImageDataTrustedInterface() {
  static const PPB_ImageDataTrusted* ppb =
      reinterpret_cast<const PPB_ImageDataTrusted*>(
      GetBrowserInterfaceSafe(PPB_IMAGEDATA_TRUSTED_INTERFACE));
  return ppb;
}

const PPB_Instance* PPBInstanceInterface() {
  static const PPB_Instance* ppb = reinterpret_cast<const PPB_Instance*>(
      GetBrowserInterfaceSafe(PPB_INSTANCE_INTERFACE));
  return ppb;
}

const PPB_Surface3D_Dev* PPBSurface3DInterface() {
  static const PPB_Surface3D_Dev* ppb =
      reinterpret_cast<const PPB_Surface3D_Dev*>(
          GetBrowserInterfaceSafe(PPB_SURFACE_3D_DEV_INTERFACE));
  return ppb;
}

const PPB_URLLoader* PPBURLLoaderInterface() {
  static const PPB_URLLoader* ppb =
      reinterpret_cast<const PPB_URLLoader*>(
          GetBrowserInterfaceSafe(PPB_URLLOADER_INTERFACE));
  return ppb;
}

const PPB_URLRequestInfo* PPBURLRequestInfoInterface() {
  static const PPB_URLRequestInfo* ppb =
      reinterpret_cast<const PPB_URLRequestInfo*>(
          GetBrowserInterfaceSafe(PPB_URLREQUESTINFO_INTERFACE));
  return ppb;
}

const PPB_URLResponseInfo* PPBURLResponseInfoInterface() {
  static const PPB_URLResponseInfo* ppb =
      reinterpret_cast<const PPB_URLResponseInfo*>(
          GetBrowserInterfaceSafe(PPB_URLRESPONSEINFO_INTERFACE));
  return ppb;
}

const PPB_Var_Deprecated* PPBVarInterface() {
  static const PPB_Var_Deprecated* ppb =
      reinterpret_cast<const PPB_Var_Deprecated*>(
          GetBrowserInterfaceSafe(PPB_VAR_DEPRECATED_INTERFACE));
  return ppb;
}

const PPB_FileIO_Dev* PPBFileIOInterface() {
  static const PPB_FileIO_Dev* ppb =
      reinterpret_cast<const PPB_FileIO_Dev*>(
        GetBrowserInterfaceSafe(PPB_FILEIO_DEV_INTERFACE));
  return ppb;
}

}  // namespace ppapi_proxy

