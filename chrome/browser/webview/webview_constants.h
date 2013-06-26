// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the WebView API.

#ifndef CHROME_BROWSER_WEBVIEW_WEBVIEW_CONSTANTS_H_
#define CHROME_BROWSER_WEBVIEW_WEBVIEW_CONSTANTS_H_

namespace webview {

// Events.
extern const char kEventLoadCommit[];

// Parameters/properties on events.
extern const char kIsTopLevel[];
extern const char kUrl[];

// Internal parameters/properties on events.
extern const char kInternalCurrentEntryIndex[];
extern const char kInternalEntryCount[];

// Attributes.
extern const char kAttributeApi[];

}  // namespace webview

#endif  // CHROME_BROWSER_WEBVIEW_WEBVIEW_CONSTANTS_H_

