// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_SYNC_CLIENT_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_SYNC_CLIENT_H_
#pragma once

#include "chrome/browser/chromeos/gdata/gdata_file_system.h"

namespace gdata {

// The GDataSyncClient is used to synchronize pinned files on gdata and the
// cache on the local drive. The sync client works as follows.
//
// When the user pins files on gdata, this client is notified about the files
// that get pinned, and queues tasks and starts fetching these files in the
// background.
//
// If the user logs out before fetching of the pinned files is complete, this
// client resumes fetching operations next time the user logs in, based on
// the states left in the cache.
//
// TODO(satorux): This client should also upload pinned but dirty (locally
// edited) files to gdata. Will work on this once downloading is done.
// crosbug.com/27836.
//
class GDataSyncClient : public GDataFileSystem::Observer {
 public:
  explicit GDataSyncClient(GDataFileSystem* file_system);
  virtual ~GDataSyncClient();

  // GDataFileSystem::Observer overrides.
  virtual void OnFilePinned(const std::string& resource_id,
                            const std::string& md5) OVERRIDE;
 private:
  GDataFileSystem* file_system_;
  DISALLOW_COPY_AND_ASSIGN(GDataSyncClient);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_SYNC_CLIENT_H_
