// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_ANDROID_PROTOCOL_HANDLER_H_
#define ANDROID_WEBVIEW_NATIVE_ANDROID_PROTOCOL_HANDLER_H_

#include "base/android/jni_android.h"

namespace net {

class URLRequestJobFactory;

}

// This class adds support for Android WebView-specific protocol schemes:
//
//  - "content:" scheme is used for accessing data from Android content
//    providers, see http://developer.android.com/guide/topics/providers/
//      content-provider-basics.html#ContentURIs
//
//  - "file:" scheme extension for accessing application assets and resources
//    (file:///android_asset/ and file:///android_res/), see
//    http://developer.android.com/reference/android/webkit/
//      WebSettings.html#setAllowFileAccess(boolean)
//
class AndroidProtocolHandler {
 public:
  // Register handlers for all supported Android protocol schemes.
  static void RegisterProtocolsOnIOThread(
      net::URLRequestJobFactory* context_getter);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(AndroidProtocolHandler);
};

bool RegisterAndroidProtocolHandler(JNIEnv* env);

#endif  // ANDROID_WEBVIEW_NATIVE_ANDROID_PROTOCOL_HANDLER_H_
