// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_util.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/file_path.h"
#include "base/logging.h"
#include "chrome/browser/download/download_util.h"
#include "content/public/browser/download_item.h"
#include "content/public/browser/download_manager.h"

using content::DownloadItem;
using content::DownloadManager;

namespace gdata {
namespace util {

namespace {

const char kGDataMountPointPath[] = "/special/gdata";

const FilePath::CharType kGDataDownloadPath[] = ".gdata";

// Key for DownloadItem::ExternalData.
const char kGDataPathKey[] = "GDataPath";

const FilePath::CharType* kGDataMountPointPathComponents[] = {
  "/", "special", "gdata"
};

// External Data stored in DownloadItem for gdata path.
class GDataExternalData : public DownloadItem::ExternalData {
 public:
  explicit GDataExternalData(const FilePath& path) : file_path_(path) {}
  virtual ~GDataExternalData() {}

  const FilePath& file_path() const { return file_path_; }

 private:
  FilePath file_path_;
};

}  // namespace

const FilePath& GetGDataMountPointPath() {
  CR_DEFINE_STATIC_LOCAL(FilePath, gdata_mount_path,
      (FilePath::FromUTF8Unsafe(kGDataMountPointPath)));
  return gdata_mount_path;
}

const std::string& GetGDataMountPointPathAsString() {
  CR_DEFINE_STATIC_LOCAL(std::string, gdata_mount_path_string,
      (kGDataMountPointPath));
  return gdata_mount_path_string;
}

bool IsUnderGDataMountPoint(const FilePath& path) {
  return GetGDataMountPointPath() == path ||
         GetGDataMountPointPath().IsParent(path);
}

FilePath ExtractGDataPath(const FilePath& path) {
  if (!IsUnderGDataMountPoint(path))
    return FilePath();

  std::vector<FilePath::StringType> components;
  path.GetComponents(&components);

  // -1 to include 'gdata'.
  FilePath extracted;
  for (size_t i = arraysize(kGDataMountPointPathComponents) - 1;
       i < components.size(); ++i) {
    extracted = extracted.Append(components[i]);
  }
  return extracted;
}

FilePath GetGDataTempDownloadFolderPath() {
  return download_util::GetDefaultDownloadDirectory().Append(
      kGDataDownloadPath);
}

// Store |path| in DownloadItem external data with key GDataPath.
void SetGDataPath(DownloadItem* download, const FilePath& path) {
  if (download)
      download->SetExternalData(&kGDataPathKey,
                                new GDataExternalData(path));
}

// Return path stored in DownloadItem external data with key GDataPath.
FilePath GetGDataPath(DownloadItem* download) {
  GDataExternalData* data = static_cast<GDataExternalData*>(
      download->GetExternalData(&kGDataPathKey));
  // If data is NULL, we've somehow lost the gdata path selected
  // by the file picker.
  DCHECK(data);
  return data ? ExtractGDataPath(data->file_path()) : FilePath();
}

}  // namespace util
}  // namespace gdata
