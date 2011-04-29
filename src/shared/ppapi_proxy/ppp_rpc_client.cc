// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#include "trusted/srpcgen/ppp_rpc.h"
#ifdef __native_client__
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(P) do { (void) P; } while (0)
#endif  // UNREFERENCED_PARAMETER
#else
#include "native_client/src/include/portability.h"
#endif  // __native_client__
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"

NaClSrpcError ObjectStubRpcClient::HasProperty(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    int32_t* success,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "HasProperty:CCC:iC",
      capability_bytes, capability,
      name_bytes, name,
      exception_in_bytes, exception_in,
      success,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::HasMethod(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    int32_t* success,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "HasMethod:CCC:iC",
      capability_bytes, capability,
      name_bytes, name,
      exception_in_bytes, exception_in,
      success,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::GetProperty(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    nacl_abi_size_t* value_bytes, char* value,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "GetProperty:CCC:CC",
      capability_bytes, capability,
      name_bytes, name,
      exception_in_bytes, exception_in,
      value_bytes, value,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::GetAllPropertyNames(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    int32_t* property_count,
    nacl_abi_size_t* properties_bytes, char* properties,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "GetAllPropertyNames:CC:iCC",
      capability_bytes, capability,
      exception_in_bytes, exception_in,
      property_count,
      properties_bytes, properties,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::SetProperty(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    nacl_abi_size_t value_bytes, char* value,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "SetProperty:CCCC:C",
      capability_bytes, capability,
      name_bytes, name,
      value_bytes, value,
      exception_in_bytes, exception_in,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::RemoveProperty(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "RemoveProperty:CCC:C",
      capability_bytes, capability,
      name_bytes, name,
      exception_in_bytes, exception_in,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::Call(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    nacl_abi_size_t name_bytes, char* name,
    int32_t argc,
    nacl_abi_size_t argv_bytes, char* argv,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    nacl_abi_size_t* ret_bytes, char* ret,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "Call:CCiCC:CC",
      capability_bytes, capability,
      name_bytes, name,
      argc,
      argv_bytes, argv,
      exception_in_bytes, exception_in,
      ret_bytes, ret,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::Construct(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability,
    int32_t argc,
    nacl_abi_size_t argv_bytes, char* argv,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    nacl_abi_size_t* ret_bytes, char* ret,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "Construct:CiCC:CC",
      capability_bytes, capability,
      argc,
      argv_bytes, argv,
      exception_in_bytes, exception_in,
      ret_bytes, ret,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError ObjectStubRpcClient::Deallocate(
    NaClSrpcChannel* channel,
    nacl_abi_size_t capability_bytes, char* capability)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "Deallocate:C:",
      capability_bytes, capability
  );
  return retval;
}

NaClSrpcError CompletionCallbackRpcClient::RunCompletionCallback(
    NaClSrpcChannel* channel,
    int32_t callback_id,
    int32_t result,
    nacl_abi_size_t read_buffer_bytes, char* read_buffer)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "RunCompletionCallback:iiC:",
      callback_id,
      result,
      read_buffer_bytes, read_buffer
  );
  return retval;
}

NaClSrpcError PppRpcClient::PPP_InitializeModule(
    NaClSrpcChannel* channel,
    int32_t pid,
    PP_Module module,
    NaClSrpcImcDescType upcall_channel_desc,
    char* service_description,
    int32_t* nacl_pid,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_InitializeModule:iihs:ii",
      pid,
      module,
      upcall_channel_desc,
      service_description,
      nacl_pid,
      success
  );
  return retval;
}

NaClSrpcError PppRpcClient::PPP_ShutdownModule(
    NaClSrpcChannel* channel)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_ShutdownModule::"
  );
  return retval;
}

NaClSrpcError PppRpcClient::PPP_GetInterface(
    NaClSrpcChannel* channel,
    char* interface_name,
    int32_t* exports_interface_name)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_GetInterface:s:i",
      interface_name,
      exports_interface_name
  );
  return retval;
}

NaClSrpcError PppAudioRpcClient::PPP_Audio_StreamCreated(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    NaClSrpcImcDescType out_shm,
    int32_t out_shm_size,
    NaClSrpcImcDescType out_socket)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Audio_StreamCreated:ihih:",
      instance,
      out_shm,
      out_shm_size,
      out_socket
  );
  return retval;
}

NaClSrpcError PppFindRpcClient::PPP_Find_StartFind(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t text_bytes, char* text,
    int32_t case_sensitive,
    int32_t* supports_find)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Find_StartFind:iCi:i",
      instance,
      text_bytes, text,
      case_sensitive,
      supports_find
  );
  return retval;
}

NaClSrpcError PppFindRpcClient::PPP_Find_SelectFindResult(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t forward)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Find_SelectFindResult:ii:",
      instance,
      forward
  );
  return retval;
}

NaClSrpcError PppFindRpcClient::PPP_Find_StopFind(
    NaClSrpcChannel* channel,
    PP_Instance instance)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Find_StopFind:i:",
      instance
  );
  return retval;
}

NaClSrpcError PppInstanceRpcClient::PPP_Instance_DidCreate(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t argc,
    nacl_abi_size_t argn_bytes, char* argn,
    nacl_abi_size_t argv_bytes, char* argv,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Instance_DidCreate:iiCC:i",
      instance,
      argc,
      argn_bytes, argn,
      argv_bytes, argv,
      success
  );
  return retval;
}

NaClSrpcError PppInstanceRpcClient::PPP_Instance_DidDestroy(
    NaClSrpcChannel* channel,
    PP_Instance instance)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Instance_DidDestroy:i:",
      instance
  );
  return retval;
}

NaClSrpcError PppInstanceRpcClient::PPP_Instance_DidChangeView(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t position_bytes, int32_t* position,
    nacl_abi_size_t clip_bytes, int32_t* clip)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Instance_DidChangeView:iII:",
      instance,
      position_bytes, position,
      clip_bytes, clip
  );
  return retval;
}

NaClSrpcError PppInstanceRpcClient::PPP_Instance_DidChangeFocus(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    bool has_focus)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Instance_DidChangeFocus:ib:",
      instance,
      has_focus
  );
  return retval;
}

NaClSrpcError PppInstanceRpcClient::PPP_Instance_HandleInputEvent(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t event_data_bytes, char* event_data,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Instance_HandleInputEvent:iC:i",
      instance,
      event_data_bytes, event_data,
      success
  );
  return retval;
}

NaClSrpcError PppInstanceRpcClient::PPP_Instance_HandleDocumentLoad(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource url_loader,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Instance_HandleDocumentLoad:ii:i",
      instance,
      url_loader,
      success
  );
  return retval;
}

NaClSrpcError PppInstanceRpcClient::PPP_Instance_GetInstanceObject(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t* capability_bytes, char* capability)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Instance_GetInstanceObject:i:C",
      instance,
      capability_bytes, capability
  );
  return retval;
}

NaClSrpcError PppPrintingRpcClient::PPP_Printing_QuerySupportedFormats(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t* formats_bytes, char* formats,
    int32_t* format_count)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Printing_QuerySupportedFormats:i:Ci",
      instance,
      formats_bytes, formats,
      format_count
  );
  return retval;
}

NaClSrpcError PppPrintingRpcClient::PPP_Printing_Begin(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t print_settings_bytes, char* print_settings,
    int32_t* pages_required)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Printing_Begin:iC:i",
      instance,
      print_settings_bytes, print_settings,
      pages_required
  );
  return retval;
}

NaClSrpcError PppPrintingRpcClient::PPP_Printing_PrintPages(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t page_ranges_bytes, char* page_ranges,
    int32_t page_range_count,
    PP_Resource* image_data)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Printing_PrintPages:iCi:i",
      instance,
      page_ranges_bytes, page_ranges,
      page_range_count,
      image_data
  );
  return retval;
}

NaClSrpcError PppPrintingRpcClient::PPP_Printing_End(
    NaClSrpcChannel* channel,
    PP_Instance instance)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Printing_End:i:",
      instance
  );
  return retval;
}

NaClSrpcError PppScrollbarRpcClient::PPP_Scrollbar_ValueChanged(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource scrollbar,
    int32_t value)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Scrollbar_ValueChanged:iii:",
      instance,
      scrollbar,
      value
  );
  return retval;
}

NaClSrpcError PppSelectionRpcClient::PPP_Selection_GetSelectedText(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t html,
    nacl_abi_size_t* selected_text_bytes, char* selected_text)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Selection_GetSelectedText:ii:C",
      instance,
      html,
      selected_text_bytes, selected_text
  );
  return retval;
}

NaClSrpcError PppWidgetRpcClient::PPP_Widget_Invalidate(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource widget,
    nacl_abi_size_t dirty_rect_bytes, char* dirty_rect)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Widget_Invalidate:iiC:",
      instance,
      widget,
      dirty_rect_bytes, dirty_rect
  );
  return retval;
}

NaClSrpcError PppZoomRpcClient::PPP_Zoom_Zoom(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    double factor,
    int32_t text_only)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPP_Zoom_Zoom:idi:",
      instance,
      factor,
      text_only
  );
  return retval;
}


