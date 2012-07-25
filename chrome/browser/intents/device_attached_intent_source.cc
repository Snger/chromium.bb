// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/intents/device_attached_intent_source.h"

#include "base/file_path.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "content/public/browser/web_intents_dispatcher.h"
#include "content/public/browser/web_contents_delegate.h"
#include "webkit/fileapi/file_system_types.h"
#include "webkit/fileapi/isolated_context.h"
#include "webkit/glue/web_intent_data.h"

using base::SystemMonitor;
using content::WebContentsDelegate;

DeviceAttachedIntentSource::DeviceAttachedIntentSource(
    Browser* browser, WebContentsDelegate* delegate)
    : browser_(browser), delegate_(delegate) {
  SystemMonitor* sys_monitor = SystemMonitor::Get();
  if (sys_monitor)
    sys_monitor->AddDevicesChangedObserver(this);
}

DeviceAttachedIntentSource::~DeviceAttachedIntentSource() {
  SystemMonitor* sys_monitor = SystemMonitor::Get();
  if (sys_monitor)
    sys_monitor->RemoveDevicesChangedObserver(this);
}

void DeviceAttachedIntentSource::OnMediaDeviceAttached(
    const std::string& id,
    const string16& name,
    base::SystemMonitor::MediaDeviceType type,
    const FilePath::StringType& location) {
  if (!browser_->window()->IsActive())
    return;

  // Only handle FilePaths for now.
  if (type != SystemMonitor::TYPE_PATH)
    return;

  // Sanity checks for |device_path|.
  const FilePath device_path(location);
  if (!device_path.IsAbsolute() || device_path.ReferencesParent())
    return;

  std::string device_name;

  // Register device path as an isolated file system.
  // TODO(kinuko, kmadhusu): Use a different file system type for MTP.
  const std::string filesystem_id =
      fileapi::IsolatedContext::GetInstance()->RegisterFileSystemForPath(
          fileapi::kFileSystemTypeIsolated, device_path, &device_name);

  CHECK(!filesystem_id.empty());
  webkit_glue::WebIntentData intent(
      ASCIIToUTF16("chrome-extension://attach"),
      ASCIIToUTF16("chrome-extension://filesystem"),
      device_name,
      filesystem_id);

  delegate_->WebIntentDispatch(NULL  /* no WebContents */,
                               content::WebIntentsDispatcher::Create(intent));
}
