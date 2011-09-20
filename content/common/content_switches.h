// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines all the "content" command-line switches.

#ifndef CONTENT_COMMON_CONTENT_SWITCHES_H_
#define CONTENT_COMMON_CONTENT_SWITCHES_H_
#pragma once

#include "build/build_config.h"
#include "content/common/content_export.h"

namespace switches {

CONTENT_EXPORT extern const char kAllowFileAccessFromFiles[];
CONTENT_EXPORT extern const char kAllowRunningInsecureContent[];
extern const char kAllowSandboxDebugging[];
extern const char kAuditHandles[];
extern const char kAuditAllHandles[];
CONTENT_EXPORT extern const char kBrowserAssertTest[];
CONTENT_EXPORT extern const char kBrowserCrashTest[];
extern const char kBrowserSubprocessPath[];
// TODO(jam): this doesn't belong in content.
CONTENT_EXPORT extern const char kChromeFrame[];
CONTENT_EXPORT extern const char kDisable3DAPIs[];
CONTENT_EXPORT extern const char kDisableAccelerated2dCanvas[];
CONTENT_EXPORT extern const char kDisableAcceleratedCompositing[];
CONTENT_EXPORT extern const char kDisableAcceleratedLayers[];
CONTENT_EXPORT extern const char kDisableAcceleratedPlugins[];
CONTENT_EXPORT extern const char kDisableAcceleratedVideo[];
CONTENT_EXPORT extern const char kDisableAltWinstation[];
CONTENT_EXPORT extern const char kDisableApplicationCache[];
extern const char kDisableAudio[];
extern const char kDisableBackingStoreLimit[];
CONTENT_EXPORT extern const char kDisableDatabases[];
extern const char kDisableDataTransferItems[];
extern const char kDisableDesktopNotifications[];
extern const char kDisableDeviceOrientation[];
CONTENT_EXPORT extern const char kDisableExperimentalWebGL[];
extern const char kDisableFileSystem[];
extern const char kDisableGeolocation[];
CONTENT_EXPORT extern const char kDisableGLMultisampling[];
extern const char kDisableGLSLTranslator[];
extern const char kDisableGpuDriverBugWorkarounds[];
extern const char kDisableGpuSandbox[];
extern const char kDisableGpuWatchdog[];
CONTENT_EXPORT extern const char kDisableHangMonitor[];
extern const char kDisableIndexedDatabase[];
CONTENT_EXPORT extern const char kDisableJava[];
CONTENT_EXPORT extern const char kDisableJavaScript[];
extern const char kDisableJavaScriptI18NAPI[];
CONTENT_EXPORT extern const char kDisableLocalStorage[];
CONTENT_EXPORT extern const char kDisableLogging[];
CONTENT_EXPORT extern const char kDisableSmoothScrolling[];
CONTENT_EXPORT extern const char kDisablePlugins[];
CONTENT_EXPORT extern const char kDisablePopupBlocking[];
extern const char kDisableRendererAccessibility[];
extern const char kDisableSSLFalseStart[];
extern const char kDisableSeccompSandbox[];
extern const char kDisableSessionStorage[];
extern const char kDisableSharedWorkers[];
extern const char kDisableSpeechInput[];
extern const char kDisableSpellcheckAPI[];
CONTENT_EXPORT extern const char kDisableWebAudio[];
extern const char kDisableWebSockets[];
extern const char kEnableAccelerated2dCanvas[];
CONTENT_EXPORT extern const char kEnableAcceleratedDrawing[];
extern const char kEnableAccessibility[];
extern const char kEnableAccessibilityLogging[];
CONTENT_EXPORT extern const char kEnableDNSCertProvenanceChecking[];
CONTENT_EXPORT extern const char kEnableDeviceMotion[];
CONTENT_EXPORT extern const char kDisableFullScreen[];
extern const char kEnableGPUPlugin[];
CONTENT_EXPORT extern const char kEnableLogging[];
extern const char kEnableMediaStream[];
extern const char kEnableMonitorProfile[];
extern const char kEnableOriginBoundCerts[];
extern const char kEnablePreparsedJsCaching[];
extern const char kEnableSSLCachedInfo[];
extern const char kEnableSandboxLogging[];
extern const char kEnableSeccompSandbox[];
CONTENT_EXPORT extern const char kEnableStatsTable[];
CONTENT_EXPORT extern const char kEnableTcpFastOpen[];
extern const char kEnableVideoFullscreen[];
extern const char kEnableVideoLogging[];
CONTENT_EXPORT extern const char kEnableWebIntents[];
CONTENT_EXPORT extern const char kExperimentalLocationFeatures[];
// TODO(jam): this doesn't belong in content.
CONTENT_EXPORT extern const char kExtensionProcess[];
extern const char kExtraPluginDir[];
extern const char kForceFieldTestNameAndValue[];
extern const char kForceRendererAccessibility[];
extern const char kGpuLauncher[];
CONTENT_EXPORT extern const char kGpuProcess[];
extern const char kGpuStartupDialog[];
CONTENT_EXPORT extern const char kIgnoreGpuBlacklist[];
extern const char kInProcessGPU[];
extern const char kInProcessPlugins[];
CONTENT_EXPORT extern const char kInProcessWebGL[];
extern const char kJavaScriptFlags[];
extern const char kLoadPlugin[];
CONTENT_EXPORT extern const char kLoggingLevel[];
extern const char kLogPluginMessages[];
extern const char kLowLatencyAudio[];
// TODO(jam): this doesn't belong in content.
CONTENT_EXPORT extern const char kNaClBrokerProcess[];
CONTENT_EXPORT extern const char kNaClLoaderProcess[];
// TODO(bradchen): remove kNaClLinuxHelper switch.
// This switch enables the experimental lightweight nacl_helper for Linux.
// It will be going away soon, when the helper is enabled permanently.
extern const char kNaClLinuxHelper[];
CONTENT_EXPORT extern const char kNoDisplayingInsecureContent[];
extern const char kNoJsRandomness[];
CONTENT_EXPORT extern const char kNoReferrers[];
CONTENT_EXPORT extern const char kNoSandbox[];
CONTENT_EXPORT extern const char kPlaybackMode[];
extern const char kPluginLauncher[];
CONTENT_EXPORT extern const char kPluginPath[];
CONTENT_EXPORT extern const char kPluginProcess[];
extern const char kPluginStartupDialog[];
CONTENT_EXPORT extern const char kPpapiBrokerProcess[];
CONTENT_EXPORT extern const char kPpapiFlashArgs[];
CONTENT_EXPORT extern const char kPpapiFlashPath[];
CONTENT_EXPORT extern const char kPpapiFlashVersion[];
extern const char kPpapiOutOfProcess[];
extern const char kPpapiPluginLauncher[];
CONTENT_EXPORT extern const char kPpapiPluginProcess[];
extern const char kPpapiStartupDialog[];
extern const char kProcessPerSite[];
CONTENT_EXPORT extern const char kProcessPerTab[];
CONTENT_EXPORT extern const char kProcessType[];
// TODO(jam): this doesn't belong in content.
extern const char kProfileImportProcess[];
CONTENT_EXPORT extern const char kRecordMode[];
extern const char kRegisterPepperPlugins[];
CONTENT_EXPORT extern const char kRemoteShellPort[];
extern const char kRendererAssertTest[];
extern const char kRendererCmdPrefix[];
extern const char kRendererCrashTest[];
CONTENT_EXPORT extern const char kRendererProcess[];
extern const char kRendererStartupDialog[];
// TODO(jam): this doesn't belong in content.
CONTENT_EXPORT extern const char kServiceProcess[];
extern const char kShowPaintRects[];
extern const char kSimpleDataSource[];
CONTENT_EXPORT extern const char kSingleProcess[];
extern const char kSQLiteIndexedDatabase[];
extern const char kTestSandbox[];
extern const char kUnlimitedQuotaForFiles[];
CONTENT_EXPORT extern const char kUserAgent[];
extern const char kUtilityCmdPrefix[];
CONTENT_EXPORT extern const char kUtilityProcess[];
extern const char kUtilityProcessAllowedDir[];
CONTENT_EXPORT extern const char kWaitForDebuggerChildren[];
extern const char kWebCoreLogChannels[];
extern const char kWebWorkerProcessPerCore[];
extern const char kWebWorkerShareProcesses[];
CONTENT_EXPORT extern const char kWorkerProcess[];
CONTENT_EXPORT extern const char kZygoteCmdPrefix[];
CONTENT_EXPORT extern const char kZygoteProcess[];

#if defined(OS_POSIX) && !defined(OS_MACOSX)
extern const char kScrollPixels[];
#endif

#if defined(OS_MACOSX) || defined(OS_WIN)
extern const char kUseSystemSSL[];
#endif

#if !defined(OFFICIAL_BUILD)
extern const char kRendererCheckFalseTest[];
#endif

}  // namespace switches

#endif  // CONTENT_COMMON_CONTENT_SWITCHES_H_
