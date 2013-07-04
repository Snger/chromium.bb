// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Constants used for the WebView API.

#ifndef CHROME_BROWSER_GUESTVIEW_WEBVIEW_WEBVIEW_CONSTANTS_H_
#define CHROME_BROWSER_GUESTVIEW_WEBVIEW_WEBVIEW_CONSTANTS_H_

namespace webview {

// Events.
extern const char kEventClose[];
extern const char kEventConsoleMessage[];
extern const char kEventContentLoad[];
extern const char kEventLoadCommit[];
extern const char kEventLoadRedirect[];
extern const char kEventLoadStart[];
extern const char kEventLoadStop[];

// Parameters/properties on events.
extern const char kLevel[];
extern const char kMessage[];
extern const char kLine[];
extern const char kSourceId[];

// Internal parameters/properties on events.
extern const char kInternalCurrentEntryIndex[];
extern const char kInternalEntryCount[];
extern const char kInternalProcessId[];

// Parameters/properties on events.
extern const char kNewURL[];
extern const char kOldURL[];

}  // namespace webview

#endif  // CHROME_BROWSER_GUESTVIEW_WEBVIEW_WEBVIEW_CONSTANTS_H_

