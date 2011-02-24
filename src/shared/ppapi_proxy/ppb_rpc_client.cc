// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
//
// Automatically generated code.  See srpcgen.py
//
// NaCl Simple Remote Procedure Call interface abstractions.

#include "untrusted/srpcgen/ppb_rpc.h"
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

NaClSrpcError NaClFileRpcClient::StreamAsFile(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    char* url,
    int32_t callback_id)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "StreamAsFile:isi:",
      instance,
      url,
      callback_id
  );
  return retval;
}

NaClSrpcError NaClFileRpcClient::GetFileDesc(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    char* url,
    NaClSrpcImcDescType* file_desc)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "GetFileDesc:is:h",
      instance,
      url,
      file_desc
  );
  return retval;
}

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

NaClSrpcError PpbRpcClient::PPB_GetInterface(
    NaClSrpcChannel* channel,
    char* interface_name,
    int32_t* exports_interface_name)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_GetInterface:s:i",
      interface_name,
      exports_interface_name
  );
  return retval;
}

NaClSrpcError PpbAudioRpcClient::PPB_Audio_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource config,
    PP_Resource* out_resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Audio_Create:ii:i",
      instance,
      config,
      out_resource
  );
  return retval;
}

NaClSrpcError PpbAudioRpcClient::PPB_Audio_IsAudio(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* out_bool)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Audio_IsAudio:i:i",
      resource,
      out_bool
  );
  return retval;
}

NaClSrpcError PpbAudioRpcClient::PPB_Audio_GetCurrentConfig(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    PP_Resource* out_resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Audio_GetCurrentConfig:i:i",
      resource,
      out_resource
  );
  return retval;
}

NaClSrpcError PpbAudioRpcClient::PPB_Audio_StopPlayback(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* out_bool)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Audio_StopPlayback:i:i",
      resource,
      out_bool
  );
  return retval;
}

NaClSrpcError PpbAudioRpcClient::PPB_Audio_StartPlayback(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* out_bool)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Audio_StartPlayback:i:i",
      resource,
      out_bool
  );
  return retval;
}

NaClSrpcError PpbAudioConfigRpcClient::PPB_AudioConfig_CreateStereo16Bit(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t sample_rate,
    int32_t sample_frame_count,
    PP_Resource* resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_AudioConfig_CreateStereo16Bit:iii:i",
      instance,
      sample_rate,
      sample_frame_count,
      resource
  );
  return retval;
}

NaClSrpcError PpbAudioConfigRpcClient::PPB_AudioConfig_IsAudioConfig(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* out_bool)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_AudioConfig_IsAudioConfig:i:i",
      resource,
      out_bool
  );
  return retval;
}

NaClSrpcError PpbAudioConfigRpcClient::PPB_AudioConfig_RecommendSampleFrameCount(
    NaClSrpcChannel* channel,
    int32_t request_sample_rate,
    int32_t request_sample_frame_count,
    int32_t* out_sample_frame_count)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_AudioConfig_RecommendSampleFrameCount:ii:i",
      request_sample_rate,
      request_sample_frame_count,
      out_sample_frame_count
  );
  return retval;
}

NaClSrpcError PpbAudioConfigRpcClient::PPB_AudioConfig_GetSampleRate(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* sample_rate)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_AudioConfig_GetSampleRate:i:i",
      resource,
      sample_rate
  );
  return retval;
}

NaClSrpcError PpbAudioConfigRpcClient::PPB_AudioConfig_GetSampleFrameCount(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* sample_frame_count)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_AudioConfig_GetSampleFrameCount:i:i",
      resource,
      sample_frame_count
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::PPB_Core_AddRefResource(
    NaClSrpcChannel* channel,
    PP_Resource resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Core_AddRefResource:i:",
      resource
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::PPB_Core_ReleaseResource(
    NaClSrpcChannel* channel,
    PP_Resource resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Core_ReleaseResource:i:",
      resource
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::ReleaseResourceMultipleTimes(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t count)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "ReleaseResourceMultipleTimes:ii:",
      resource,
      count
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::PPB_Core_GetTime(
    NaClSrpcChannel* channel,
    double* time)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Core_GetTime::d",
      time
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::PPB_Core_GetTimeTicks(
    NaClSrpcChannel* channel,
    double* time_ticks)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Core_GetTimeTicks::d",
      time_ticks
  );
  return retval;
}

NaClSrpcError PpbCoreRpcClient::PPB_Core_CallOnMainThread(
    NaClSrpcChannel* channel,
    int32_t delay_in_milliseconds,
    int32_t callback_id,
    int32_t result)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Core_CallOnMainThread:iii:",
      delay_in_milliseconds,
      callback_id,
      result
  );
  return retval;
}

NaClSrpcError PpbFileIODevRpcClient::PPB_FileIO_Dev_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource* resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileIO_Dev_Create:i:i",
      instance,
      resource
  );
  return retval;
}

NaClSrpcError PpbFileIODevRpcClient::PPB_FileIO_Dev_IsFileIO(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileIO_Dev_IsFileIO:i:i",
      resource,
      success
  );
  return retval;
}

NaClSrpcError PpbFileIODevRpcClient::PPB_FileIO_Dev_Open(
    NaClSrpcChannel* channel,
    PP_Resource file_io,
    PP_Resource file_ref,
    int32_t open_flags,
    int32_t callback_id,
    int32_t* pp_error)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileIO_Dev_Open:iiii:i",
      file_io,
      file_ref,
      open_flags,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbFileIODevRpcClient::PPB_FileIO_Dev_Read(
    NaClSrpcChannel* channel,
    PP_Resource file_io,
    int64_t offset,
    int32_t bytes_to_read,
    int32_t callback_id,
    nacl_abi_size_t* buffer_bytes, char* buffer,
    int32_t* pp_error_or_bytes)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_FileIO_Dev_Read:ilii:Ci",
      file_io,
      offset,
      bytes_to_read,
      callback_id,
      buffer_bytes, buffer,
      pp_error_or_bytes
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t size_bytes, char* size,
    int32_t is_always_opaque,
    PP_Resource* resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_Create:iCi:i",
      instance,
      size_bytes, size,
      is_always_opaque,
      resource
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_IsGraphics2D(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_IsGraphics2D:i:i",
      resource,
      success
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_Describe(
    NaClSrpcChannel* channel,
    PP_Resource graphics_2d,
    nacl_abi_size_t* size_bytes, char* size,
    int32_t* is_always_opaque,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_Describe:i:Cii",
      graphics_2d,
      size_bytes, size,
      is_always_opaque,
      success
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_PaintImageData(
    NaClSrpcChannel* channel,
    PP_Resource graphics_2d,
    PP_Resource image,
    nacl_abi_size_t top_left_bytes, char* top_left,
    nacl_abi_size_t src_rect_bytes, char* src_rect)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_PaintImageData:iiCC:",
      graphics_2d,
      image,
      top_left_bytes, top_left,
      src_rect_bytes, src_rect
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_Scroll(
    NaClSrpcChannel* channel,
    PP_Resource graphics_2d,
    nacl_abi_size_t clip_rect_bytes, char* clip_rect,
    nacl_abi_size_t amount_bytes, char* amount)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_Scroll:iCC:",
      graphics_2d,
      clip_rect_bytes, clip_rect,
      amount_bytes, amount
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_ReplaceContents(
    NaClSrpcChannel* channel,
    PP_Resource graphics_2d,
    PP_Resource image)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_ReplaceContents:ii:",
      graphics_2d,
      image
  );
  return retval;
}

NaClSrpcError PpbGraphics2DRpcClient::PPB_Graphics2D_Flush(
    NaClSrpcChannel* channel,
    PP_Resource graphics_2d,
    int32_t callback_id,
    int32_t* pp_error)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Graphics2D_Flush:ii:i",
      graphics_2d,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Context3D_BindSurfaces(
    NaClSrpcChannel* channel,
    PP_Resource context,
    PP_Resource draw_surface,
    PP_Resource read_surface,
    int32_t* error_code)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Context3D_BindSurfaces:iii:i",
      context,
      draw_surface,
      read_surface,
      error_code
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Surface3D_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t config,
    nacl_abi_size_t attrib_list_bytes, int32_t* attrib_list,
    PP_Resource* resource_id)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Surface3D_Create:iiI:i",
      instance,
      config,
      attrib_list_bytes, attrib_list,
      resource_id
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Surface3D_SwapBuffers(
    NaClSrpcChannel* channel,
    PP_Resource surface,
    int32_t callback_id,
    int32_t* pp_error)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Surface3D_SwapBuffers:ii:i",
      surface,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Context3DTrusted_CreateRaw(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t config,
    PP_Resource share_context,
    nacl_abi_size_t attrib_list_bytes, int32_t* attrib_list,
    PP_Resource* resource_id)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Context3DTrusted_CreateRaw:iiiI:i",
      instance,
      config,
      share_context,
      attrib_list_bytes, attrib_list,
      resource_id
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Context3DTrusted_Initialize(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    int32_t size,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Context3DTrusted_Initialize:ii:i",
      resource_id,
      size,
      success
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Context3DTrusted_GetRingBuffer(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    NaClSrpcImcDescType* shm_desc,
    int32_t* shm_size)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Context3DTrusted_GetRingBuffer:i:hi",
      resource_id,
      shm_desc,
      shm_size
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Context3DTrusted_GetState(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    nacl_abi_size_t* state_bytes, char* state)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Context3DTrusted_GetState:i:C",
      resource_id,
      state_bytes, state
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Context3DTrusted_Flush(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    int32_t put_offset)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Context3DTrusted_Flush:ii:",
      resource_id,
      put_offset
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Context3DTrusted_FlushSync(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    int32_t put_offset,
    nacl_abi_size_t* state_bytes, char* state)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Context3DTrusted_FlushSync:ii:C",
      resource_id,
      put_offset,
      state_bytes, state
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Context3DTrusted_CreateTransferBuffer(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    int32_t size,
    int32_t* id)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Context3DTrusted_CreateTransferBuffer:ii:i",
      resource_id,
      size,
      id
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Context3DTrusted_DestroyTransferBuffer(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    int32_t id)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Context3DTrusted_DestroyTransferBuffer:ii:",
      resource_id,
      id
  );
  return retval;
}

NaClSrpcError PpbGraphics3DRpcClient::PPB_Context3DTrusted_GetTransferBuffer(
    NaClSrpcChannel* channel,
    PP_Resource resource_id,
    int32_t id,
    NaClSrpcImcDescType* shm_desc,
    int32_t* shm_size)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Context3DTrusted_GetTransferBuffer:ii:hi",
      resource_id,
      id,
      shm_desc,
      shm_size
  );
  return retval;
}

NaClSrpcError PpbImageDataRpcClient::PPB_ImageData_GetNativeImageDataFormat(
    NaClSrpcChannel* channel,
    int32_t* format)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_GetNativeImageDataFormat::i",
      format
  );
  return retval;
}

NaClSrpcError PpbImageDataRpcClient::PPB_ImageData_IsImageDataFormatSupported(
    NaClSrpcChannel* channel,
    int32_t format,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_IsImageDataFormatSupported:i:i",
      format,
      success
  );
  return retval;
}

NaClSrpcError PpbImageDataRpcClient::PPB_ImageData_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t format,
    nacl_abi_size_t size_bytes, char* size,
    int32_t init_to_zero,
    PP_Resource* resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_Create:iiCi:i",
      instance,
      format,
      size_bytes, size,
      init_to_zero,
      resource
  );
  return retval;
}

NaClSrpcError PpbImageDataRpcClient::PPB_ImageData_IsImageData(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_IsImageData:i:i",
      resource,
      success
  );
  return retval;
}

NaClSrpcError PpbImageDataRpcClient::PPB_ImageData_Describe(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    nacl_abi_size_t* desc_bytes, char* desc,
    NaClSrpcImcDescType* shm,
    int32_t* shm_size,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_ImageData_Describe:i:Chii",
      resource,
      desc_bytes, desc,
      shm,
      shm_size,
      success
  );
  return retval;
}

NaClSrpcError PpbInstanceRpcClient::PPB_Instance_GetWindowObject(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t* window_bytes, char* window)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Instance_GetWindowObject:i:C",
      instance,
      window_bytes, window
  );
  return retval;
}

NaClSrpcError PpbInstanceRpcClient::PPB_Instance_GetOwnerElementObject(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t* owner_bytes, char* owner)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Instance_GetOwnerElementObject:i:C",
      instance,
      owner_bytes, owner
  );
  return retval;
}

NaClSrpcError PpbInstanceRpcClient::PPB_Instance_BindGraphics(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource graphics_device,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Instance_BindGraphics:ii:i",
      instance,
      graphics_device,
      success
  );
  return retval;
}

NaClSrpcError PpbInstanceRpcClient::PPB_Instance_IsFullFrame(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    int32_t* is_full_frame)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Instance_IsFullFrame:i:i",
      instance,
      is_full_frame
  );
  return retval;
}

NaClSrpcError PpbInstanceRpcClient::PPB_Instance_ExecuteScript(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    nacl_abi_size_t script_bytes, char* script,
    nacl_abi_size_t exception_in_bytes, char* exception_in,
    nacl_abi_size_t* result_bytes, char* result,
    nacl_abi_size_t* exception_bytes, char* exception)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_Instance_ExecuteScript:iCC:CC",
      instance,
      script_bytes, script,
      exception_in_bytes, exception_in,
      result_bytes, result,
      exception_bytes, exception
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource* resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_Create:i:i",
      instance,
      resource
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_IsURLLoader(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* is_url_loader)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_IsURLLoader:i:i",
      resource,
      is_url_loader
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_Open(
    NaClSrpcChannel* channel,
    PP_Resource loader,
    PP_Resource request,
    int32_t callback_id,
    int32_t* pp_error)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_Open:iii:i",
      loader,
      request,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_FollowRedirect(
    NaClSrpcChannel* channel,
    PP_Resource loader,
    int32_t callback_id,
    int32_t* pp_error)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_FollowRedirect:ii:i",
      loader,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_GetUploadProgress(
    NaClSrpcChannel* channel,
    PP_Resource loader,
    int64_t* bytes_sent,
    int64_t* total_bytes_to_be_sent,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_GetUploadProgress:i:lli",
      loader,
      bytes_sent,
      total_bytes_to_be_sent,
      success
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_GetDownloadProgress(
    NaClSrpcChannel* channel,
    PP_Resource loader,
    int64_t* bytes_received,
    int64_t* total_bytes_to_be_received,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_GetDownloadProgress:i:lli",
      loader,
      bytes_received,
      total_bytes_to_be_received,
      success
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_GetResponseInfo(
    NaClSrpcChannel* channel,
    PP_Resource loader,
    PP_Resource* response)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_GetResponseInfo:i:i",
      loader,
      response
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_ReadResponseBody(
    NaClSrpcChannel* channel,
    PP_Resource loader,
    int32_t bytes_to_read,
    int32_t callback_id,
    nacl_abi_size_t* buffer_bytes, char* buffer,
    int32_t* pp_error_or_bytes)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_ReadResponseBody:iii:Ci",
      loader,
      bytes_to_read,
      callback_id,
      buffer_bytes, buffer,
      pp_error_or_bytes
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_FinishStreamingToFile(
    NaClSrpcChannel* channel,
    PP_Resource loader,
    int32_t callback_id,
    int32_t* pp_error)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_FinishStreamingToFile:ii:i",
      loader,
      callback_id,
      pp_error
  );
  return retval;
}

NaClSrpcError PpbURLLoaderRpcClient::PPB_URLLoader_Close(
    NaClSrpcChannel* channel,
    PP_Resource loader)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLLoader_Close:i:",
      loader
  );
  return retval;
}

NaClSrpcError PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_Create(
    NaClSrpcChannel* channel,
    PP_Instance instance,
    PP_Resource* resource)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLRequestInfo_Create:i:i",
      instance,
      resource
  );
  return retval;
}

NaClSrpcError PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_IsURLRequestInfo(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLRequestInfo_IsURLRequestInfo:i:i",
      resource,
      success
  );
  return retval;
}

NaClSrpcError PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_SetProperty(
    NaClSrpcChannel* channel,
    PP_Resource request,
    int32_t property,
    nacl_abi_size_t value_bytes, char* value,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLRequestInfo_SetProperty:iiC:i",
      request,
      property,
      value_bytes, value,
      success
  );
  return retval;
}

NaClSrpcError PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_AppendDataToBody(
    NaClSrpcChannel* channel,
    PP_Resource request,
    nacl_abi_size_t data_bytes, char* data,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLRequestInfo_AppendDataToBody:iC:i",
      request,
      data_bytes, data,
      success
  );
  return retval;
}

NaClSrpcError PpbURLRequestInfoRpcClient::PPB_URLRequestInfo_AppendFileToBody(
    NaClSrpcChannel* channel,
    PP_Resource request,
    PP_Resource file_ref,
    int64_t start_offset,
    int64_t number_of_bytes,
    double expected_last_modified_time,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLRequestInfo_AppendFileToBody:iilld:i",
      request,
      file_ref,
      start_offset,
      number_of_bytes,
      expected_last_modified_time,
      success
  );
  return retval;
}

NaClSrpcError PpbURLResponseInfoRpcClient::PPB_URLResponseInfo_IsURLResponseInfo(
    NaClSrpcChannel* channel,
    PP_Resource resource,
    int32_t* success)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLResponseInfo_IsURLResponseInfo:i:i",
      resource,
      success
  );
  return retval;
}

NaClSrpcError PpbURLResponseInfoRpcClient::PPB_URLResponseInfo_GetProperty(
    NaClSrpcChannel* channel,
    PP_Resource response,
    int32_t property,
    nacl_abi_size_t* value_bytes, char* value)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLResponseInfo_GetProperty:ii:C",
      response,
      property,
      value_bytes, value
  );
  return retval;
}

NaClSrpcError PpbURLResponseInfoRpcClient::PPB_URLResponseInfo_GetBodyAsFileRef(
    NaClSrpcChannel* channel,
    PP_Resource response,
    PP_Resource* file_ref)  {
  NaClSrpcError retval;
  retval = NaClSrpcInvokeBySignature(
      channel,
      "PPB_URLResponseInfo_GetBodyAsFileRef:i:i",
      response,
      file_ref
  );
  return retval;
}


