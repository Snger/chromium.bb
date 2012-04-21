// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_util.h"

#include <string>
#include <vector>
#include <utility>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/libxml_utils.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_system_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/child_process_security_policy.h"
#include "net/base/escape.h"

namespace gdata {
namespace util {

namespace {

const char kGDataSpecialRootPath[] = "/special";

const char kGDataMountPointPath[] = "/special/gdata";

const FilePath::CharType* kGDataMountPointPathComponents[] = {
  "/", "special", "gdata"
};

const int kReadOnlyFilePermissions = base::PLATFORM_FILE_OPEN |
                                     base::PLATFORM_FILE_READ |
                                     base::PLATFORM_FILE_EXCLUSIVE_READ |
                                     base::PLATFORM_FILE_ASYNC;

class GetFileNameDelegate : public FindEntryDelegate {
 public:
  GetFileNameDelegate() {}
  virtual ~GetFileNameDelegate() {}

  const std::string& file_name() const { return file_name_; }
 private:
  // GDataFileSystem::FindEntryDelegate overrides.
  virtual void OnDone(base::PlatformFileError error,
                      const FilePath& directory_path,
                      GDataEntry* entry) OVERRIDE {
    if (error == base::PLATFORM_FILE_OK && entry && entry->AsGDataFile()) {
      file_name_ = entry->AsGDataFile()->file_name();
    }
  }

  std::string file_name_;
};

GDataFileSystem* GetGDataFileSystem(Profile* profile) {
  GDataSystemService* system_service =
      GDataSystemServiceFactory::GetForProfile(profile);
  return system_service ? system_service->file_system() : NULL;
}

void GetHostedDocumentURLBlockingThread(const FilePath& gdata_cache_path,
                                        GURL* url) {
  std::string json;
  if (!file_util::ReadFileToString(gdata_cache_path, &json)) {
    NOTREACHED() << "Unable to read file " << gdata_cache_path.value();
    return;
  }
  DVLOG(1) << "Hosted doc content " << json;
  scoped_ptr<base::Value> val(base::JSONReader::Read(json));
  base::DictionaryValue* dict_val;
  if (!val.get() || !val->GetAsDictionary(&dict_val)) {
    NOTREACHED() << "Parse failure for " << json;
    return;
  }
  std::string edit_url;
  if (!dict_val->GetString("url", &edit_url)) {
    NOTREACHED() << "url field doesn't exist in " << json;
    return;
  }
  *url = GURL(edit_url);
  DVLOG(1) << "edit url " << *url;
}

void OpenEditURLUIThread(Profile* profile, GURL* edit_url) {
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile);
  if (browser) {
    browser->OpenURL(content::OpenURLParams(*edit_url, content::Referrer(),
        CURRENT_TAB, content::PAGE_TRANSITION_TYPED, false));
  }
}

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

const FilePath& GetSpecialRemoteRootPath() {
  CR_DEFINE_STATIC_LOCAL(FilePath, gdata_mount_path,
      (FilePath::FromUTF8Unsafe(kGDataSpecialRootPath)));
  return gdata_mount_path;
}

GURL GetFileResourceUrl(const std::string& resource_id,
                        const std::string& file_name) {
  return GURL(base::StringPrintf(
      "%s://%s/%s/%s",
      chrome::kGDataScheme,
      kGDataViewFileHostnameUrl,
      net::EscapePath(resource_id).c_str(),
      net::EscapePath(file_name).c_str()));
}

void ModifyGDataFileResourceUrl(Profile* profile,
                                const FilePath& gdata_cache_path,
                                GURL* url) {
  GDataFileSystem* file_system = GetGDataFileSystem(profile);
  if (!file_system)
    return;

  // Handle hosted documents. The edit url is in the temporary file, so we
  // read it on a blocking thread.
  if (file_system->GetGDataTempDocumentFolderPath().IsParent(
      gdata_cache_path)) {
    GURL* edit_url = new GURL();
    content::BrowserThread::GetBlockingPool()->PostTaskAndReply(FROM_HERE,
        base::Bind(&GetHostedDocumentURLBlockingThread,
                   gdata_cache_path, edit_url),
        base::Bind(&OpenEditURLUIThread, profile, base::Owned(edit_url)));
    *url = GURL();
    return;
  }

  // Handle all other gdata files.
  if (file_system->GetGDataCacheTmpDirectory().IsParent(gdata_cache_path)) {
    const std::string resource_id =
        gdata_cache_path.BaseName().RemoveExtension().AsUTF8Unsafe();
    GetFileNameDelegate delegate;
    file_system->FindEntryByResourceIdSync(resource_id, &delegate);
    *url = gdata::util::GetFileResourceUrl(resource_id, delegate.file_name());
    DVLOG(1) << "ModifyGDataFileResourceUrl " << *url;
  }
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

void InsertGDataCachePathsPermissions(
    Profile* profile,
    const FilePath& gdata_path,
    std::vector<std::pair<FilePath, int> >* cache_paths ) {
  DCHECK(cache_paths);

  GDataFileSystem* file_system = GetGDataFileSystem(profile);
  if (!file_system)
    return;

  GDataFileProperties file_properties;
  file_system->GetFileInfoByPath(gdata_path, &file_properties);

  std::string resource_id = file_properties.resource_id;
  std::string file_md5 = file_properties.file_md5;

  // We check permissions for raw cache file paths only for read-only
  // operations (when fileEntry.file() is called), so read only permissions
  // should be sufficient for all cache paths. For the rest of supported
  // operations the file access check is done for gdata/ paths.
  cache_paths->push_back(std::make_pair(
      file_system->GetCacheFilePath(resource_id, file_md5,
          GDataRootDirectory::CACHE_TYPE_PERSISTENT,
          GDataFileSystem::CACHED_FILE_FROM_SERVER),
      kReadOnlyFilePermissions));
  // TODO(tbarzic): When we start supporting openFile operation, we may have to
  // change permission for localy modified files to match handler's permissions.
  cache_paths->push_back(std::make_pair(
      file_system->GetCacheFilePath(resource_id, file_md5,
          GDataRootDirectory::CACHE_TYPE_PERSISTENT,
          GDataFileSystem::CACHED_FILE_LOCALLY_MODIFIED),
     kReadOnlyFilePermissions));
  cache_paths->push_back(std::make_pair(
      file_system->GetCacheFilePath(resource_id, file_md5,
          GDataRootDirectory::CACHE_TYPE_TMP,
          GDataFileSystem::CACHED_FILE_FROM_SERVER),
      kReadOnlyFilePermissions));

}

void SetPermissionsForGDataCacheFiles(Profile* profile,
                                      int pid,
                                      const FilePath& path) {
  std::vector<std::pair<FilePath, int> > cache_paths;
  InsertGDataCachePathsPermissions(profile, path, &cache_paths);
  for (size_t i = 0; i < cache_paths.size(); i++) {
    content::ChildProcessSecurityPolicy::GetInstance()->GrantPermissionsForFile(
        pid, cache_paths[i].first, cache_paths[i].second);
  }
}

bool IsGDataAvailable(Profile* profile) {
  // We allow GData only in canary and dev channels.  http://crosbug.com/28806
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_DEV ||
      channel == chrome::VersionInfo::CHANNEL_BETA ||
      channel == chrome::VersionInfo::CHANNEL_STABLE)
    return false;

  // Do not allow GData for incognito windows / guest mode.
  if (profile->IsOffTheRecord())
    return false;

  // Disable gdata if preference is set.  This can happen with commandline flag
  // --disable-gdata or enterprise policy, or probably with user settings too
  // in the future.
  if (profile->GetPrefs()->GetBoolean(prefs::kDisableGData))
    return false;

  return true;
}

}  // namespace util
}  // namespace gdata
