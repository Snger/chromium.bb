// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_API_SERVICE_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_API_SERVICE_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/chromeos/gdata/auth_service.h"
#include "chrome/browser/chromeos/gdata/drive_service_interface.h"
#include "chrome/browser/chromeos/gdata/gdata_operations.h"

class FilePath;
class GURL;
class Profile;

namespace gdata {

class OperationRunner;

// This class provides documents feed service calls for Drive V2 API.
// Details of API call are abstracted in each operation class and this class
// works as a thin wrapper for the API.
class DriveAPIService : public DriveServiceInterface {
 public:
  // Instance is usually created by GDataSystemServiceFactory and owned by
  // DriveFileSystem.
  DriveAPIService();
  virtual ~DriveAPIService();

  // DriveServiceInterface Overrides
  virtual void Initialize(Profile* profile) OVERRIDE;
  virtual OperationRegistry* operation_registry() const OVERRIDE;
  virtual void CancelAll() OVERRIDE;
  virtual void Authenticate(const AuthStatusCallback& callback) OVERRIDE;
  virtual bool HasAccessToken() const OVERRIDE;
  virtual bool HasRefreshToken() const OVERRIDE;
  virtual void GetDocuments(const GURL& feed_url,
                            int64 start_changestamp,
                            const std::string& search_query,
                            const std::string& directory_resource_id,
                            const GetDataCallback& callback) OVERRIDE;
  virtual void GetDocumentEntry(const std::string& resource_id,
                                const GetDataCallback& callback) OVERRIDE;

  virtual void GetAccountMetadata(const GetDataCallback& callback) OVERRIDE;
  virtual void GetApplicationInfo(const GetDataCallback& callback) OVERRIDE;
  virtual void DeleteDocument(const GURL& document_url,
                              const EntryActionCallback& callback) OVERRIDE;
  virtual void DownloadDocument(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& content_url,
      DocumentExportFormat format,
      const DownloadActionCallback& callback) OVERRIDE;
  virtual void DownloadFile(
      const FilePath& virtual_path,
      const FilePath& local_cache_path,
      const GURL& content_url,
      const DownloadActionCallback& download_action_callback,
      const GetContentCallback& get_content_callback) OVERRIDE;
  virtual void CopyDocument(const std::string& resource_id,
                            const FilePath::StringType& new_name,
                            const GetDataCallback& callback) OVERRIDE;
  virtual void RenameResource(const GURL& document_url,
                              const FilePath::StringType& new_name,
                              const EntryActionCallback& callback) OVERRIDE;
  virtual void AddResourceToDirectory(
      const GURL& parent_content_url,
      const GURL& resource_url,
      const EntryActionCallback& callback) OVERRIDE;
  virtual void RemoveResourceFromDirectory(
      const GURL& parent_content_url,
      const GURL& resource_url,
      const std::string& resource_id,
      const EntryActionCallback& callback) OVERRIDE;
  virtual void CreateDirectory(const GURL& parent_content_url,
                               const FilePath::StringType& directory_name,
                               const GetDataCallback& callback) OVERRIDE;
  virtual void InitiateUpload(const InitiateUploadParams& params,
                              const InitiateUploadCallback& callback) OVERRIDE;
  virtual void ResumeUpload(const ResumeUploadParams& params,
                            const ResumeUploadCallback& callback) OVERRIDE;
  virtual void AuthorizeApp(const GURL& resource_url,
                            const std::string& app_id,
                            const GetDataCallback& callback) OVERRIDE;

 private:
  // Fetches a changelist from |url| with |start_changestamp|, using Drive V2
  // API. If this URL is empty the call will use the default URL. Specify |url|
  // when pagenated request should be issued.
  // |start_changestamp| specifies the starting point of change list or 0 if
  // all changes are necessary.
  // Upon completion, invokes |callback| with results on calling thread.
  virtual void GetChangelist(const GURL& url,
                             int64 start_changestamp,
                             const GetDataCallback& callback);

  // Fetches a filelist from |url| with |search_query|, using Drive V2 API. If
  // this URL is empty the call will use the default URL. Specify |url| when
  // pagenated request should be issued.
  // |search_query| specifies query string, whose syntax is described at
  // https://developers.google.com/drive/search-parameters
  virtual void GetFilelist(const GURL& url,
                           const std::string& search_query,
                           const GetDataCallback& callback);

  Profile* profile_;
  scoped_ptr<OperationRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(DriveAPIService);
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_DRIVE_API_SERVICE_H_
