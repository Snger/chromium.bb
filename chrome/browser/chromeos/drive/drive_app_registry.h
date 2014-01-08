// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_APP_REGISTRY_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_APP_REGISTRY_H_

#include <map>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/memory/weak_ptr.h"
#include "google_apis/drive/gdata_errorcode.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "url/gurl.h"

namespace google_apis {
class AppList;
}  // namespace google_apis

namespace drive {

class JobScheduler;

// Data structure that defines Drive app. See
// https://chrome.google.com/webstore/category/collection/drive_apps for
// Drive apps available on the webstore.
struct DriveAppInfo {
  DriveAppInfo();
  DriveAppInfo(const std::string& app_id,
               const google_apis::InstalledApp::IconList& app_icons,
               const google_apis::InstalledApp::IconList& document_icons,
               const std::string& app_name,
               const GURL& create_url);
  ~DriveAppInfo();

  // Drive app id.
  std::string app_id;
  // Drive application icon URLs for this app, paired with their size (length of
  // a side in pixels).
  google_apis::InstalledApp::IconList app_icons;
  // Drive document icon URLs for this app, paired with their size (length of
  // a side in pixels).
  google_apis::InstalledApp::IconList document_icons;
  // App name.
  std::string app_name;
  // URL for opening a new file in the app.
  GURL create_url;
};

// Keeps the track of installed drive applications in-memory.
class DriveAppRegistry {
 public:
  explicit DriveAppRegistry(JobScheduler* scheduler);
  ~DriveAppRegistry();

  // Returns a list of Drive app information for the |file_extension| with
  // |mime_type|.
  void GetAppsForFile(const base::FilePath::StringType& file_extension,
                      const std::string& mime_type,
                      ScopedVector<DriveAppInfo>* apps) const;

  // Updates this registry by fetching the data from the server.
  void Update();

  // Updates this registry from the |app_list|.
  void UpdateFromAppList(const google_apis::AppList& app_list);

 private:
  // Part of Update(). Runs upon the completion of fetching the Drive apps
  // data from the server.
  void UpdateAfterGetAppList(google_apis::GDataErrorCode gdata_error,
                             scoped_ptr<google_apis::AppList> app_list);

  // Map of application id to each app's info.
  std::map<std::string, DriveAppInfo> all_apps_;

  // Defines mapping between file content type selectors (extensions, MIME
  // types) and corresponding app.
  typedef std::multimap<std::string, std::string> DriveAppFileSelectorMap;
  DriveAppFileSelectorMap extension_map_;
  DriveAppFileSelectorMap mimetype_map_;

  JobScheduler* scheduler_;

  bool is_updating_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate the weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DriveAppRegistry> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DriveAppRegistry);
};

namespace util {

// The preferred icon size, which should usually be used for FindPreferredIcon;
const int kPreferredIconSize = 16;

// Finds an icon in the list of icons. If unable to find an icon of the exact
// size requested, returns one with the next larger size. If all icons are
// smaller than the preferred size, we'll return the largest one available.
// Icons do not have to be sorted by the icon size. If there are no icons in
// the list, returns an empty URL.
GURL FindPreferredIcon(const google_apis::InstalledApp::IconList& icons,
                       int preferred_size);

}  // namespace util

}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DRIVE_APP_REGISTRY_H_
