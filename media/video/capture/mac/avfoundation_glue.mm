// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "media/video/capture/mac/avfoundation_glue.h"

#include <dlfcn.h>

#include "base/mac/mac_util.h"

bool AVFoundationGlue::IsAVFoundationSupported() {
  return (base::mac::IsOSLionOrLater() && [AVFoundationBundle() load]);
}

NSBundle const* AVFoundationGlue::AVFoundationBundle() {
  static NSBundle* bundle = [NSBundle
      bundleWithPath:@"/System/Library/Frameworks/AVFoundation.framework"];
  return bundle;
}

void* AVFoundationGlue::AVFoundationLibraryHandle() {
  const char* library_path =
      [[AVFoundationBundle() executablePath] fileSystemRepresentation];
  if (library_path == NULL) {
    DCHECK(false);
    return NULL;
  }
  static void* library_handle = dlopen(library_path, RTLD_LAZY | RTLD_LOCAL);
  DCHECK(library_handle) << dlerror();
  return library_handle;
}

NSString* AVFoundationGlue::AVCaptureDeviceWasConnectedNotification() {
  return ReadNSStringPtr("AVCaptureDeviceWasConnectedNotification");
}

NSString* AVFoundationGlue::AVCaptureDeviceWasDisconnectedNotification() {
  return ReadNSStringPtr("AVCaptureDeviceWasDisconnectedNotification");
}

NSString* AVFoundationGlue::AVMediaTypeVideo() {
  return ReadNSStringPtr("AVMediaTypeVideo");
}

NSString* AVFoundationGlue::AVMediaTypeAudio() {
  return ReadNSStringPtr("AVMediaTypeAudio");
}

NSString* AVFoundationGlue::AVMediaTypeMuxed() {
  return ReadNSStringPtr("AVMediaTypeMuxed");
}

NSString* AVFoundationGlue::ReadNSStringPtr(const char* symbol) {
  NSString** string_pointer = reinterpret_cast<NSString**>(
      dlsym(AVFoundationLibraryHandle(), symbol));
  DCHECK(string_pointer) << dlerror();
  return *string_pointer;
}

@implementation AVCaptureDeviceGlue

+ (NSArray*)devices {
  Class avcClass =
      [AVFoundationGlue::AVFoundationBundle() classNamed:@"AVCaptureDevice"];
  SEL selectorDevices = NSSelectorFromString(@"devices");
  if ([avcClass respondsToSelector:selectorDevices]) {
    return [avcClass performSelector:selectorDevices];
  }
  return nil;
}

+ (BOOL)hasMediaType:(NSString*)mediaType
    forCaptureDevice:(CrAVCaptureDevice*)device {
  SEL selectorHasMediaType = NSSelectorFromString(@"hasMediaType:");
  if ([device respondsToSelector:selectorHasMediaType]) {
    return [device hasMediaType:mediaType];
  }
  return NO;
}

+ (NSString*)uniqueID:(CrAVCaptureDevice*)device {
  SEL selectorUniqueID = NSSelectorFromString(@"uniqueID");
  if ([device respondsToSelector:selectorUniqueID]) {
    return [device uniqueID];
  }
  return nil;
}

@end  // @implementation AVCaptureDevice
