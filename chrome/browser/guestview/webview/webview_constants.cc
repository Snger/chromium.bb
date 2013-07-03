// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/guestview/webview/webview_constants.h"

namespace webview {

// Events.
const char kEventContentLoad[] = "webview.onContentLoad";
const char kEventLoadCommit[] = "webview.onLoadCommit";
const char kEventLoadRedirect[] = "webview.onLoadRedirect";
const char kEventLoadStart[] = "webview.onLoadStart";
const char kEventLoadStop[] = "webview.onLoadStop";

// Internal parameters/properties on events.
const char kInternalCurrentEntryIndex[] = "currentEntryIndex";
const char kInternalEntryCount[] = "entryCount";
const char kInternalProcessId[] = "processId";

// Parameters/properties on events.
extern const char kNewURL[];
extern const char kOldURL[];

}  // namespace webview
