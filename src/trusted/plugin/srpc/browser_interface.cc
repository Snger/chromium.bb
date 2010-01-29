/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


// Portable interface for browser interaction - NPAPI implementation

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include <map>
#include <string>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/nacl_elf.h"
#include "native_client/src/include/portability_io.h"
#include "native_client/src/include/portability_string.h"
#include "native_client/src/shared/npruntime/nacl_npapi.h"
#include "native_client/src/trusted/plugin/npinstance.h"
#include "native_client/src/trusted/plugin/srpc/browser_interface.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

using nacl::assert_cast;

bool PortablePluginInterface::identifiers_initialized = false;
uintptr_t PortablePluginInterface::kConnectIdent;
uintptr_t PortablePluginInterface::kHeightIdent;
uintptr_t PortablePluginInterface::kHrefIdent;
uintptr_t PortablePluginInterface::kLengthIdent;
uintptr_t PortablePluginInterface::kLocationIdent;
uintptr_t PortablePluginInterface::kMapIdent;
uintptr_t PortablePluginInterface::kModuleReadyIdent;
uintptr_t PortablePluginInterface::kNaClMultimediaBridgeIdent;
uintptr_t PortablePluginInterface::kNullNpapiMethodIdent;
uintptr_t PortablePluginInterface::kOnfailIdent;
uintptr_t PortablePluginInterface::kOnloadIdent;
uintptr_t PortablePluginInterface::kReadIdent;
uintptr_t PortablePluginInterface::kSetCommandLogIdent;
uintptr_t PortablePluginInterface::kShmFactoryIdent;
uintptr_t PortablePluginInterface::kSignaturesIdent;
uintptr_t PortablePluginInterface::kSrcIdent;
uintptr_t PortablePluginInterface::kToStringIdent;
uintptr_t PortablePluginInterface::kUrlAsNaClDescIdent;
uintptr_t PortablePluginInterface::kValueOfIdent;
uintptr_t PortablePluginInterface::kVideoUpdateModeIdent;
uintptr_t PortablePluginInterface::kWidthIdent;
uintptr_t PortablePluginInterface::kWriteIdent;

uint8_t const PortablePluginInterface::kInvalidAbiVersion = UINT8_MAX;

// TODO(gregoryd): make sure that calls to AddMethodToMap use the same strings
// move the strings to a header file.
void PortablePluginInterface::InitializeIdentifiers() {
  if (!identifiers_initialized) {
    kConnectIdent         = GetStrIdentifierCallback("connect");
    kHeightIdent          = GetStrIdentifierCallback("height");
    kHrefIdent            = GetStrIdentifierCallback("href");
    kLengthIdent          = GetStrIdentifierCallback("length");
    kLocationIdent        = GetStrIdentifierCallback("location");
    kMapIdent             = GetStrIdentifierCallback("map");
    kModuleReadyIdent     = GetStrIdentifierCallback("__moduleReady");
    kNaClMultimediaBridgeIdent =
        GetStrIdentifierCallback("nacl_multimedia_bridge");
    kNullNpapiMethodIdent = GetStrIdentifierCallback("__nullNpapiMethod");
    kOnfailIdent          = GetStrIdentifierCallback("onfail");
    kOnloadIdent          = GetStrIdentifierCallback("onload");
    kReadIdent            = GetStrIdentifierCallback("read");
    kSetCommandLogIdent   = GetStrIdentifierCallback("__setCommandLog");
    kShmFactoryIdent      = GetStrIdentifierCallback("__shmFactory");
    kSignaturesIdent      = GetStrIdentifierCallback("__signatures");
    kSrcIdent             = GetStrIdentifierCallback("src");
    kToStringIdent        = GetStrIdentifierCallback("toString");
    kUrlAsNaClDescIdent   = GetStrIdentifierCallback("__urlAsNaClDesc");
    kValueOfIdent         = GetStrIdentifierCallback("valueOf");
    kVideoUpdateModeIdent = GetStrIdentifierCallback("videoUpdateMode");
    kWidthIdent           = GetStrIdentifierCallback("width");
    kWriteIdent           = GetStrIdentifierCallback("write");

    identifiers_initialized = true;
  }
}


uintptr_t PortablePluginInterface::GetStrIdentifierCallback(
    const char *method_name) {
  return reinterpret_cast<uintptr_t>(NPN_GetStringIdentifier(method_name));
}

// TODO(robertm): this is a quick hack to address immediate shortcomings
static void CleanString(std::string* text) {
  for (size_t i = 0; i < text->size(); ++i) {
    if ((*text)[i] == '\'') {
      (*text)[i] = '\"';
    }
  }
}


bool PortablePluginInterface::Alert(
    nacl_srpc::PluginIdentifier plugin_identifier,
    const std::string& text) {
  NPObject* window;
  NPP npp = plugin_identifier;
  NPN_GetValue(npp, NPNVWindowNPObject, &window);

  // usually these messages are important enough to call attention to them
  puts(text.c_str());

  std::string command = text;
  CleanString(&command);
  command = "alert('" + command + "');";
  uint32_t size = assert_cast<uint32_t>(command.size());
  char* buffer = reinterpret_cast<char*>(NPN_MemAlloc(size));
  memcpy(buffer, command.c_str(), command.size());

  // TODO(sehr): write a stand-alone function that converts
  //             between std::string and NPString, and put it in utility.cc.
  NPString script;
  script.UTF8Length = size;
  script.UTF8Characters = buffer;
  NPVariant result;
  bool success = NPN_Evaluate(npp, window, &script, &result);
  NPN_ReleaseVariantValue(&result);
  NPN_MemFree(buffer);

  return success;
}


bool PortablePluginInterface::GetOrigin(
    nacl_srpc::PluginIdentifier plugin_identifier,
    std::string **origin) {
  NPP instance = plugin_identifier;
  NPVariant loc_value;
  NPVariant href_value;

  *origin = NULL;

  VOID_TO_NPVARIANT(loc_value);
  VOID_TO_NPVARIANT(href_value);

  // TODO(gregoryd): consider making this block a function returning origin.
  do {
    NPObject *win_obj;
    if (NPERR_NO_ERROR
      != NPN_GetValue(instance, NPNVWindowNPObject, &win_obj)) {
        dprintf(("GetOrigin: No window object\n"));
        // no window; no URL as NaCl descriptors will be allowed
        break;
    }

    if (!NPN_GetProperty(instance,
                         win_obj,
                         reinterpret_cast<NPIdentifier>(kLocationIdent),
                         &loc_value)) {
        dprintf(("GetOrigin: no location property value\n"));
        break;
    }
    NPObject *loc_obj = NPVARIANT_TO_OBJECT(loc_value);

    if (!NPN_GetProperty(instance,
                         loc_obj,
                         reinterpret_cast<NPIdentifier>(kHrefIdent),
                         &href_value)) {
        dprintf(("GetOrigin: no href property value\n"));
        break;
    }
    std::string *href = new(std::nothrow) std::string(
      NPVARIANT_TO_STRING(href_value).UTF8Characters,
      NPVARIANT_TO_STRING(href_value).UTF8Length);
    dprintf(("GetOrigin: href %s\n", href->c_str()));

    *origin = href;
  } while (0);

  NPN_ReleaseVariantValue(&loc_value);
  NPN_ReleaseVariantValue(&href_value);

  return (NULL != *origin);
}

namespace {
bool RunHandler(
    nacl_srpc::PluginIdentifier plugin_identifier,
    uintptr_t handler_identifier) {
  NPP instance = plugin_identifier;
  NPVariant attr_value;
  NPVariant dummy_return;
  NPIdentifier attr_id = reinterpret_cast<NPIdentifier>(handler_identifier);

  VOID_TO_NPVARIANT(attr_value);
  VOID_TO_NPVARIANT(dummy_return);

  do {
    NPObject* element_obj;
    if (NPERR_NO_ERROR !=
        NPN_GetValue(instance, NPNVPluginElementNPObject, &element_obj)) {
      break;
    }
    if (!NPN_GetProperty(instance, element_obj, attr_id, &attr_value)) {
      break;
    }
    if (!NPVARIANT_IS_VOID(attr_value) && !NPVARIANT_IS_OBJECT(attr_value)) {
      break;
    }
    if (!NPN_InvokeDefault(instance,
                           NPVARIANT_TO_OBJECT(attr_value),
                           NULL,
                           0,
                           &dummy_return)) {
      break;
    }
  } while (0);

  NPN_ReleaseVariantValue(&attr_value);
  NPN_ReleaseVariantValue(&dummy_return);

  return true;
}
}  // namespace

bool PortablePluginInterface::RunOnloadHandler(
    nacl_srpc::PluginIdentifier plugin_identifier) {
  return RunHandler(plugin_identifier, kOnloadIdent);
}

bool PortablePluginInterface::RunOnfailHandler(
    nacl_srpc::PluginIdentifier plugin_identifier) {
  return RunHandler(plugin_identifier, kOnfailIdent);
}

void* PortablePluginInterface::BrowserAlloc(int size) {
  return NPN_MemAlloc(size);
}


void PortablePluginInterface::BrowserRelease(void* ptr) {
  NPN_MemFree(ptr);
}


const char* PortablePluginInterface::IdentToString(uintptr_t ident) {
  NPIdentifier npident = reinterpret_cast<NPIdentifier>(ident);
  if (NPN_IdentifierIsString(npident)) {
    return reinterpret_cast<char*>(NPN_UTF8FromIdentifier(npident));
  } else {
    static char buf[10];
    SNPRINTF(buf, sizeof(buf), "%d", NPN_IntFromIdentifier(npident));
    return buf;
  }
}

bool PortablePluginInterface::CheckExecutableVersionCommon(
    nacl_srpc::PluginIdentifier instance,
    const char *version) {
  if ((NULL != version) && (EF_NACL_ABIVERSION == *version)) {
    return true;
  }

  if (NULL == version) {
    Alert(instance, "Load failed: Unknown error");
  } else {
    char alert[256];
    SNPRINTF(alert,
      sizeof alert,
      "Load failed: ABI version mismatch: expected %d, got %d",
      EF_NACL_ABIVERSION,
      *version);
    Alert(instance, alert);
  }
  return false;
}

// TODO(gregoryd): consider refactoring and moving the code to service_runtime
bool PortablePluginInterface::CheckExecutableVersion(
    nacl_srpc::PluginIdentifier instance,
    const char *filename) {
  FILE *f;
  uint8_t nacl_abi_version = kInvalidAbiVersion;
  bool success = false;
  f = fopen(filename, "rb");
  if (NULL != f) {
    if (0 == fseek(f, EI_ABIVERSION, SEEK_SET)) {
      if (1 == fread(&nacl_abi_version, 1, 1, f)) {
        success = true;
      }
    }
    fclose(f);
  }

  if (success) {
    return CheckExecutableVersionCommon(
      instance,
      reinterpret_cast<char*>(&nacl_abi_version));
  } else {
    Alert(instance, "Load failed: Generic file error");
    return false;
  }
}

bool PortablePluginInterface::CheckExecutableVersion(
    nacl_srpc::PluginIdentifier instance,
    const void* buffer,
    int32_t size) {
  if ((EI_ABIVERSION + sizeof(char)) > static_cast<uint32_t>(size)) {
    return false;
  }

  const char *nacl_abi_version =
    reinterpret_cast<const char*>(buffer) + EI_ABIVERSION;
  return CheckExecutableVersionCommon(instance, nacl_abi_version);
}

char *PortablePluginInterface::MemAllocStrdup(const char *str) {
  size_t lenz = strlen(str) + 1;
  char *dup = static_cast<char *>(malloc(lenz));
  if (NULL != dup) {
    strncpy(dup, str, lenz);
  }
  // else abort();
  return dup;
}
