// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/crypto/key_systems_info.h"

#include "base/logging.h"
#include "third_party/WebKit/public/platform/WebString.h"

#include "widevine_cdm_version.h" // In SHARED_INTERMEDIATE_DIR.

// The following must be after widevine_cdm_version.h.

#if defined(WIDEVINE_CDM_AVAILABLE) && \
    defined(OS_LINUX) && !defined(OS_CHROMEOS)
#include <gnu/libc-version.h>
#include "base/logging.h"
#include "base/version.h"
#endif

#if defined(DISABLE_WIDEVINE_CDM_CANPLAYTYPE)
#include "base/command_line.h"
#include "media/base/media_switches.h"
#endif

namespace content {

static const char kClearKeyKeySystem[] = "webkit-org.w3.clearkey";

#if defined(ENABLE_PEPPER_CDMS)
static const char kExternalClearKeyKeySystem[] =
    "org.chromium.externalclearkey";
#endif  // defined(ENABLE_PEPPER_CDMS)

#if defined(WIDEVINE_CDM_AVAILABLE)
// TODO(ddorwin): Automatically support parent systems: http://crbug.com/164303.
static const char kWidevineBaseKeySystem[] = "com.widevine";

#if defined(WIDEVINE_CDM_CENC_SUPPORT_AVAILABLE)
// The supported codecs depend on what the CDM provides.
static const char kWidevineVideoMp4Codecs[] =
#if defined(WIDEVINE_CDM_AVC1_SUPPORT_AVAILABLE) && \
    defined(WIDEVINE_CDM_AAC_SUPPORT_AVAILABLE)
    "avc1,mp4a";
#elif defined(WIDEVINE_CDM_AVC1_SUPPORT_AVAILABLE)
    "avc1";
#else
    "";  // No codec strings are supported.
#endif

static const char kWidevineAudioMp4Codecs[] =
#if defined(WIDEVINE_CDM_AAC_SUPPORT_AVAILABLE)
    "mp4a";
#else
    "";  // No codec strings are supported.
#endif
#endif  // defined(WIDEVINE_CDM_CENC_SUPPORT_AVAILABLE)

static inline bool IsWidevine(const std::string& key_system) {
  return key_system == kWidevineKeySystem ||
         key_system == kWidevineBaseKeySystem;
}
#endif  // defined(WIDEVINE_CDM_AVAILABLE)

const MediaFormatAndKeySystem kSupportedFormatKeySystemCombinations[] = {
  // Clear Key.
  { "video/webm", "vorbis,vp8,vp8.0", kClearKeyKeySystem },
  { "audio/webm", "vorbis", kClearKeyKeySystem },
#if defined(USE_PROPRIETARY_CODECS)
  { "video/mp4", "avc1,mp4a", kClearKeyKeySystem },
  { "audio/mp4", "mp4a", kClearKeyKeySystem },
#endif  // defined(USE_PROPRIETARY_CODECS)

#if defined(ENABLE_PEPPER_CDMS)
  // External Clear Key (used for testing).
  { "video/webm", "vorbis,vp8,vp8.0", kExternalClearKeyKeySystem },
  { "audio/webm", "vorbis", kExternalClearKeyKeySystem },
#if defined(USE_PROPRIETARY_CODECS)
  { "video/mp4", "avc1,mp4a", kExternalClearKeyKeySystem },
  { "audio/mp4", "mp4a", kExternalClearKeyKeySystem },
#endif  // defined(USE_PROPRIETARY_CODECS)
#endif  // defined(ENABLE_PEPPER_CDMS)

#if defined(WIDEVINE_CDM_AVAILABLE)
  // Widevine.
  { "video/webm", "vorbis,vp8,vp8.0", kWidevineKeySystem },
  { "audio/webm", "vorbis", kWidevineKeySystem },
#if defined(USE_PROPRIETARY_CODECS)
#if defined(WIDEVINE_CDM_CENC_SUPPORT_AVAILABLE)
  { "video/mp4", kWidevineVideoMp4Codecs, kWidevineKeySystem },
  { "audio/mp4", kWidevineAudioMp4Codecs, kWidevineKeySystem },
#endif  // defined(WIDEVINE_CDM_CENC_SUPPORT_AVAILABLE)
#endif  // defined(USE_PROPRIETARY_CODECS)
#endif  // WIDEVINE_CDM_AVAILABLE
};

const int kNumSupportedFormatKeySystemCombinations =
    arraysize(kSupportedFormatKeySystemCombinations);

#if defined(ENABLE_PEPPER_CDMS)
const KeySystemPepperTypePair kKeySystemToPepperTypeMapping[] = {
  { kExternalClearKeyKeySystem, "application/x-ppapi-clearkey-cdm"},
#if defined(WIDEVINE_CDM_AVAILABLE)
  { kWidevineKeySystem, kWidevineCdmPluginMimeType}
#endif  // WIDEVINE_CDM_AVAILABLE
};

const int kNumKeySystemToPepperTypeMapping =
    arraysize(kKeySystemToPepperTypeMapping);
#endif  // defined(ENABLE_PEPPER_CDMS)

#if defined(OS_ANDROID)
// TODO(qinmin): add UUIDs for other key systems.
const KeySystemUUIDPair kKeySystemToUUIDMapping[] = {
#if defined(WIDEVINE_CDM_AVAILABLE)
  { kWidevineKeySystem, { 0xED, 0xEF, 0x8B, 0xA9, 0x79, 0xD6, 0x4A, 0xCE,
                          0xA3, 0xC8, 0x27, 0xDC, 0xD5, 0x1D, 0x21, 0xED }
  }
#endif  // defined(WIDEVINE_CDM_AVAILABLE)
};

// arraySize() does not work if the array is empty, so use ARRAYSIZE_UNSAFE().
const int kNumKeySystemToUUIDMapping =
    ARRAYSIZE_UNSAFE(kKeySystemToUUIDMapping);
#endif  // defined(OS_ANDROID)

bool IsOSIncompatible(const std::string& concrete_key_system) {
  DCHECK(IsConcreteKeySystem(concrete_key_system))
      << concrete_key_system << " is not a concrete system";
#if defined(WIDEVINE_CDM_AVAILABLE) && \
    defined(OS_LINUX) && !defined(OS_CHROMEOS)
  if (IsWidevine(concrete_key_system)) {
    Version glibc_version(gnu_get_libc_version());
    DCHECK(glibc_version.IsValid());
    return glibc_version.IsOlderThan(WIDEVINE_CDM_MIN_GLIBC_VERSION);
  }
#endif
  return false;
}

std::string EnsureConcreteKeySystem(const std::string& key_system) {
#if defined(WIDEVINE_CDM_AVAILABLE)
  if (key_system == kWidevineKeySystem || key_system == kWidevineBaseKeySystem)
    return kWidevineKeySystem;
#endif  // WIDEVINE_CDM_AVAILABLE
  // No parent names for Clear Key.
  if (key_system == kClearKeyKeySystem)
    return kClearKeyKeySystem;
#if defined(ENABLE_PEPPER_CDMS)
  // No parent names for External Clear Key.
  if (key_system == kExternalClearKeyKeySystem)
    return kExternalClearKeyKeySystem;
#endif  // defined(ENABLE_PEPPER_CDMS)
  return std::string();
}

bool IsCanPlayTypeSuppressed(const std::string& key_system) {
#if defined(DISABLE_WIDEVINE_CDM_CANPLAYTYPE)
  // See http://crbug.com/237627.
  if (IsWidevine(key_system) &&
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kOverrideEncryptedMediaCanPlayType))
    return true;
#endif
  return false;
}

std::string KeySystemNameForUMAInternal(const WebKit::WebString& key_system) {
  if (key_system == kClearKeyKeySystem)
    return "ClearKey";
#if defined(WIDEVINE_CDM_AVAILABLE)
  if (key_system == kWidevineKeySystem)
    return "Widevine";
#endif  // WIDEVINE_CDM_AVAILABLE
  return "Unknown";
}

bool CanUseAesDecryptorInternal(const std::string& key_system) {
  return  key_system == kClearKeyKeySystem;
}

} // namespace content
