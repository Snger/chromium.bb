// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/mock_download_manager_delegate.h"

MockDownloadManagerDelegate::~MockDownloadManagerDelegate() {
}

void MockDownloadManagerDelegate::GetSaveDir(
    TabContents* tab_contents,
    FilePath* website_save_dir,
    FilePath* download_save_dir) {
}

void MockDownloadManagerDelegate::ChooseSavePath(
    const base::WeakPtr<SavePackage>& save_package,
    const FilePath& suggested_path,
    bool can_save_as_complete) {
}

void MockDownloadManagerDelegate::ChooseDownloadPath(
    DownloadManager* download_manager,
    TabContents* tab_contents,
    const FilePath& suggested_path,
    void* data) {
}

TabContents*
    MockDownloadManagerDelegate::GetAlternativeTabContentsToNotifyForDownload(
        DownloadManager* download_manager) {
  return NULL;
}
