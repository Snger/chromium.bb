// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_
#define ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_

#include <jni.h>

#include "base/android/jni_helper.h"
#include "base/memory/scoped_ptr.h"

class TabContents;

namespace content {
class WebContents;
}

namespace android_webview {

class AwContentsContainer;
class AwWebContentsDelegate;

// Native side of java-class of same name.
// Provides the ownership of and access to browser components required for
// WebView functionality; analogous to chrome's TabContents, but with a
// level of indirection provided by the AwContentsContainer abstraction.
class AwContents {
 public:
  AwContents(JNIEnv* env,
             jobject obj,
             jobject web_contents_delegate,
             bool private_browsing);
  ~AwContents();

  jint GetWebContents(JNIEnv* env, jobject obj);
  void Destroy(JNIEnv* env, jobject obj);

 private:
  JavaObjectWeakGlobalRef java_ref_;
  scoped_ptr<AwContentsContainer> contents_container_;
  scoped_ptr<AwWebContentsDelegate> web_contents_delegate_;

  DISALLOW_COPY_AND_ASSIGN(AwContents);
};

bool RegisterAwContents(JNIEnv* env);

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NATIVE_AW_CONTENTS_H_
