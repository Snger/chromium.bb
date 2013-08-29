// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_api_requests.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/location.h"
#include "base/task_runner_util.h"
#include "base/values.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/request_sender.h"
#include "chrome/browser/google_apis/request_util.h"
#include "chrome/browser/google_apis/time_util.h"

namespace google_apis {
namespace {

const char kContentTypeApplicationJson[] = "application/json";
const char kDirectoryMimeType[] = "application/vnd.google-apps.folder";
const char kParentLinkKind[] = "drive#fileLink";

// Parses the JSON value to a resource typed |T| and runs |callback| on the UI
// thread once parsing is done.
template<typename T>
void ParseJsonAndRun(
    const base::Callback<void(GDataErrorCode, scoped_ptr<T>)>& callback,
    GDataErrorCode error,
    scoped_ptr<base::Value> value) {
  DCHECK(!callback.is_null());

  scoped_ptr<T> resource;
  if (value) {
    resource = T::CreateFrom(*value);
    if (!resource) {
      // Failed to parse the JSON value, although the JSON value is available,
      // so let the callback know the parsing error.
      error = GDATA_PARSE_ERROR;
    }
  }

  callback.Run(error, resource.Pass());
}

// Thin adapter of T::CreateFrom.
template<typename T>
scoped_ptr<T> ParseJsonOnBlockingPool(scoped_ptr<base::Value> value) {
  return T::CreateFrom(*value);
}

// Runs |callback| with given |error| and |value|. If |value| is null,
// overwrites |error| to GDATA_PARSE_ERROR.
template<typename T>
void ParseJsonOnBlockingPoolAndRunAfterBlockingPoolTask(
    const base::Callback<void(GDataErrorCode, scoped_ptr<T>)>& callback,
    GDataErrorCode error, scoped_ptr<T> value) {
  if (!value)
    error = GDATA_PARSE_ERROR;
  callback.Run(error, value.Pass());
}

// Parses the JSON value to a resource typed |T| and runs |callback| on
// blocking pool, and then run on the current thread.
// TODO(hidehiko): Move this and ParseJsonAndRun defined above into base with
// merging the tasks running on blocking pool into one.
template<typename T>
void ParseJsonOnBlockingPoolAndRun(
    scoped_refptr<base::TaskRunner> blocking_task_runner,
    const base::Callback<void(GDataErrorCode, scoped_ptr<T>)>& callback,
    GDataErrorCode error,
    scoped_ptr<base::Value> value) {
  DCHECK(!callback.is_null());

  if (!value) {
    callback.Run(error, scoped_ptr<T>());
    return;
  }

  base::PostTaskAndReplyWithResult(
      blocking_task_runner,
      FROM_HERE,
      base::Bind(&ParseJsonOnBlockingPool<T>, base::Passed(&value)),
      base::Bind(&ParseJsonOnBlockingPoolAndRunAfterBlockingPoolTask<T>,
                 callback, error));
}

// Parses the JSON value to FileResource instance and runs |callback| on the
// UI thread once parsing is done.
// This is customized version of ParseJsonAndRun defined above to adapt the
// remaining response type.
void ParseFileResourceWithUploadRangeAndRun(
    const drive::UploadRangeCallback& callback,
    const UploadRangeResponse& response,
    scoped_ptr<base::Value> value) {
  DCHECK(!callback.is_null());

  scoped_ptr<FileResource> file_resource;
  if (value) {
    file_resource = FileResource::CreateFrom(*value);
    if (!file_resource) {
      callback.Run(
          UploadRangeResponse(GDATA_PARSE_ERROR,
                              response.start_position_received,
                              response.end_position_received),
          scoped_ptr<FileResource>());
      return;
    }
  }

  callback.Run(response, file_resource.Pass());
}

}  // namespace

namespace drive {

//=============================== FilesGetRequest =============================

FilesGetRequest::FilesGetRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const FileResourceCallback& callback)
    : GetDataRequest(sender,
                     base::Bind(&ParseJsonAndRun<FileResource>, callback)),
      url_generator_(url_generator) {
  DCHECK(!callback.is_null());
}

FilesGetRequest::~FilesGetRequest() {}

GURL FilesGetRequest::GetURL() const {
  return url_generator_.GetFilesGetUrl(file_id_);
}

//============================== FilesPatchRequest ============================

FilesPatchRequest::FilesPatchRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const FileResourceCallback& callback)
    : GetDataRequest(sender,
                     base::Bind(&ParseJsonAndRun<FileResource>, callback)),
      url_generator_(url_generator),
      set_modified_date_(false),
      update_viewed_date_(true) {
  DCHECK(!callback.is_null());
}

FilesPatchRequest::~FilesPatchRequest() {}

net::URLFetcher::RequestType FilesPatchRequest::GetRequestType() const {
  return net::URLFetcher::PATCH;
}

std::vector<std::string> FilesPatchRequest::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(util::kIfMatchAllHeader);
  return headers;
}

GURL FilesPatchRequest::GetURL() const {
  return url_generator_.GetFilesPatchUrl(
      file_id_, set_modified_date_, update_viewed_date_);
}

bool FilesPatchRequest::GetContentData(std::string* upload_content_type,
                                       std::string* upload_content) {
  if (title_.empty() &&
      modified_date_.is_null() &&
      last_viewed_by_me_date_.is_null() &&
      parents_.empty())
    return false;

  *upload_content_type = kContentTypeApplicationJson;

  base::DictionaryValue root;
  if (!title_.empty())
    root.SetString("title", title_);

  if (!modified_date_.is_null())
    root.SetString("modifiedDate", util::FormatTimeAsString(modified_date_));

  if (!last_viewed_by_me_date_.is_null()) {
    root.SetString("lastViewedByMeDate",
                   util::FormatTimeAsString(last_viewed_by_me_date_));
  }

  if (!parents_.empty()) {
    base::ListValue* parents_value = new base::ListValue;
    for (size_t i = 0; i < parents_.size(); ++i) {
      base::DictionaryValue* parent = new base::DictionaryValue;
      parent->SetString("id", parents_[i]);
      parents_value->Append(parent);
    }
    root.Set("parents", parents_value);
  }

  base::JSONWriter::Write(&root, upload_content);
  DVLOG(1) << "FilesPatch data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//============================= FilesListRequest =============================

FilesListRequest::FilesListRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const FileListCallback& callback)
    : GetDataRequest(
          sender,
          base::Bind(&ParseJsonOnBlockingPoolAndRun<FileList>,
                     make_scoped_refptr(sender->blocking_task_runner()),
                     callback)),
      url_generator_(url_generator),
      max_results_(100) {
  DCHECK(!callback.is_null());
}

FilesListRequest::~FilesListRequest() {}

GURL FilesListRequest::GetURL() const {
  return url_generator_.GetFilesListUrl(max_results_, page_token_, q_);
}

//============================== AboutGetRequest =============================

AboutGetRequest::AboutGetRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const AboutResourceCallback& callback)
    : GetDataRequest(sender,
                     base::Bind(&ParseJsonAndRun<AboutResource>, callback)),
      url_generator_(url_generator) {
  DCHECK(!callback.is_null());
}

AboutGetRequest::~AboutGetRequest() {}

GURL AboutGetRequest::GetURL() const {
  return url_generator_.GetAboutGetUrl();
}

//============================ ChangesListRequest ===========================

ChangesListRequest::ChangesListRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const ChangeListCallback& callback)
    : GetDataRequest(
          sender,
          base::Bind(&ParseJsonOnBlockingPoolAndRun<ChangeList>,
                     make_scoped_refptr(sender->blocking_task_runner()),
                     callback)),
      url_generator_(url_generator),
      include_deleted_(true),
      max_results_(100),
      start_change_id_(0) {
  DCHECK(!callback.is_null());
}

ChangesListRequest::~ChangesListRequest() {}

GURL ChangesListRequest::GetURL() const {
  return url_generator_.GetChangesListUrl(
      include_deleted_, max_results_, page_token_, start_change_id_);
}

//============================== AppsListRequest ===========================

AppsListRequest::AppsListRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const AppListCallback& callback)
    : GetDataRequest(sender,
                     base::Bind(&ParseJsonAndRun<AppList>, callback)),
      url_generator_(url_generator) {
  DCHECK(!callback.is_null());
}

AppsListRequest::~AppsListRequest() {}

GURL AppsListRequest::GetURL() const {
  return url_generator_.GetAppsListUrl();
}

//======================= ContinueGetFileListRequest =========================

ContinueGetFileListRequest::ContinueGetFileListRequest(
    RequestSender* sender,
    const GURL& url,
    const GetDataCallback& callback)
    : GetDataRequest(sender, callback),
      url_(url) {
  DCHECK(!callback.is_null());
}

ContinueGetFileListRequest::~ContinueGetFileListRequest() {}

GURL ContinueGetFileListRequest::GetURL() const {
  return url_;
}

//========================== CreateDirectoryRequest ==========================

CreateDirectoryRequest::CreateDirectoryRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const std::string& parent_resource_id,
    const std::string& directory_title,
    const FileResourceCallback& callback)
    : GetDataRequest(sender,
                     base::Bind(&ParseJsonAndRun<FileResource>, callback)),
      url_generator_(url_generator),
      parent_resource_id_(parent_resource_id),
      directory_title_(directory_title) {
  DCHECK(!callback.is_null());
  DCHECK(!parent_resource_id_.empty());
  DCHECK(!directory_title_.empty());
}

CreateDirectoryRequest::~CreateDirectoryRequest() {}

GURL CreateDirectoryRequest::GetURL() const {
  return url_generator_.GetFilesUrl();
}

net::URLFetcher::RequestType CreateDirectoryRequest::GetRequestType() const {
  return net::URLFetcher::POST;
}

bool CreateDirectoryRequest::GetContentData(std::string* upload_content_type,
                                            std::string* upload_content) {
  *upload_content_type = kContentTypeApplicationJson;

  base::DictionaryValue root;
  root.SetString("title", directory_title_);
  {
    base::DictionaryValue* parent_value = new base::DictionaryValue;
    parent_value->SetString("id", parent_resource_id_);
    base::ListValue* parent_list_value = new base::ListValue;
    parent_list_value->Append(parent_value);
    root.Set("parents", parent_list_value);
  }
  root.SetString("mimeType", kDirectoryMimeType);

  base::JSONWriter::Write(&root, upload_content);

  DVLOG(1) << "CreateDirectory data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//=========================== TouchResourceRequest ===========================

TouchResourceRequest::TouchResourceRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const std::string& resource_id,
    const base::Time& modified_date,
    const base::Time& last_viewed_by_me_date,
    const FileResourceCallback& callback)
    : GetDataRequest(sender,
                     base::Bind(&ParseJsonAndRun<FileResource>, callback)),
      url_generator_(url_generator),
      resource_id_(resource_id),
      modified_date_(modified_date),
      last_viewed_by_me_date_(last_viewed_by_me_date) {
  DCHECK(!modified_date.is_null());
  DCHECK(!last_viewed_by_me_date.is_null());
  DCHECK(!callback.is_null());
}

TouchResourceRequest::~TouchResourceRequest() {}

net::URLFetcher::RequestType TouchResourceRequest::GetRequestType() const {
  return net::URLFetcher::PATCH;
}

std::vector<std::string>
TouchResourceRequest::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(util::kIfMatchAllHeader);
  return headers;
}

GURL TouchResourceRequest::GetURL() const {
  return url_generator_.GetFileTouchUrl(resource_id_);
}

bool TouchResourceRequest::GetContentData(std::string* upload_content_type,
                                          std::string* upload_content) {
  *upload_content_type = kContentTypeApplicationJson;

  base::DictionaryValue root;
  root.SetString("modifiedDate", util::FormatTimeAsString(modified_date_));
  root.SetString("lastViewedByMeDate",
                 util::FormatTimeAsString(last_viewed_by_me_date_));
  base::JSONWriter::Write(&root, upload_content);

  DVLOG(1) << "TouchResource data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//=========================== CopyResourceRequest ============================

CopyResourceRequest::CopyResourceRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const FileResourceCallback& callback)
    : GetDataRequest(sender,
                     base::Bind(&ParseJsonAndRun<FileResource>, callback)),
      url_generator_(url_generator),
      resource_id_(resource_id),
      parent_resource_id_(parent_resource_id),
      new_title_(new_title) {
  DCHECK(!callback.is_null());
}

CopyResourceRequest::~CopyResourceRequest() {
}

net::URLFetcher::RequestType CopyResourceRequest::GetRequestType() const {
  return net::URLFetcher::POST;
}

GURL CopyResourceRequest::GetURL() const {
  return url_generator_.GetFileCopyUrl(resource_id_);
}

bool CopyResourceRequest::GetContentData(std::string* upload_content_type,
                                         std::string* upload_content) {
  *upload_content_type = kContentTypeApplicationJson;

  base::DictionaryValue root;
  root.SetString("title", new_title_);

  if (!parent_resource_id_.empty()) {
    // Set the parent resource (destination directory) of the new resource.
    base::ListValue* parents = new base::ListValue;
    root.Set("parents", parents);
    base::DictionaryValue* parent_value = new base::DictionaryValue;
    parents->Append(parent_value);
    parent_value->SetString("id", parent_resource_id_);
  }

  base::JSONWriter::Write(&root, upload_content);

  DVLOG(1) << "CopyResource data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//=========================== MoveResourceRequest ============================

MoveResourceRequest::MoveResourceRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const std::string& resource_id,
    const std::string& parent_resource_id,
    const std::string& new_title,
    const FileResourceCallback& callback)
    : GetDataRequest(sender,
                     base::Bind(&ParseJsonAndRun<FileResource>, callback)),
      url_generator_(url_generator),
      resource_id_(resource_id),
      parent_resource_id_(parent_resource_id),
      new_title_(new_title) {
  DCHECK(!callback.is_null());
}

MoveResourceRequest::~MoveResourceRequest() {
}

net::URLFetcher::RequestType MoveResourceRequest::GetRequestType() const {
  return net::URLFetcher::PATCH;
}

std::vector<std::string> MoveResourceRequest::GetExtraRequestHeaders() const {
  std::vector<std::string> headers;
  headers.push_back(util::kIfMatchAllHeader);
  return headers;
}

GURL MoveResourceRequest::GetURL() const {
  // TODO(hidehiko): This temporarily shares the URL with "Files: get" method.
  // After the refactoring, this class will be merged with TouchResourceRequest
  // into FilesPatchRequest. Then, url_generator_ will have the method
  // for the new class.
  return url_generator_.GetFilesGetUrl(resource_id_);
}

bool MoveResourceRequest::GetContentData(std::string* upload_content_type,
                                         std::string* upload_content) {
  *upload_content_type = kContentTypeApplicationJson;

  base::DictionaryValue root;
  root.SetString("title", new_title_);

  if (!parent_resource_id_.empty()) {
    // Set the parent resource (destination directory) of the new resource.
    base::ListValue* parents = new base::ListValue;
    root.Set("parents", parents);
    base::DictionaryValue* parent_value = new base::DictionaryValue;
    parents->Append(parent_value);
    parent_value->SetString("id", parent_resource_id_);
  }

  base::JSONWriter::Write(&root, upload_content);

  DVLOG(1) << "MoveResource data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//=========================== TrashResourceRequest ===========================

TrashResourceRequest::TrashResourceRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const std::string& resource_id,
    const EntryActionCallback& callback)
    : EntryActionRequest(sender, callback),
      url_generator_(url_generator),
      resource_id_(resource_id) {
  DCHECK(!callback.is_null());
}

TrashResourceRequest::~TrashResourceRequest() {}

GURL TrashResourceRequest::GetURL() const {
  return url_generator_.GetFileTrashUrl(resource_id_);
}

net::URLFetcher::RequestType TrashResourceRequest::GetRequestType() const {
  return net::URLFetcher::POST;
}

//========================== InsertResourceRequest ===========================

InsertResourceRequest::InsertResourceRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback)
    : EntryActionRequest(sender, callback),
      url_generator_(url_generator),
      parent_resource_id_(parent_resource_id),
      resource_id_(resource_id) {
  DCHECK(!callback.is_null());
}

InsertResourceRequest::~InsertResourceRequest() {}

GURL InsertResourceRequest::GetURL() const {
  return url_generator_.GetChildrenUrl(parent_resource_id_);
}

net::URLFetcher::RequestType InsertResourceRequest::GetRequestType() const {
  return net::URLFetcher::POST;
}

bool InsertResourceRequest::GetContentData(std::string* upload_content_type,
                                           std::string* upload_content) {
  *upload_content_type = kContentTypeApplicationJson;

  base::DictionaryValue root;
  root.SetString("id", resource_id_);
  base::JSONWriter::Write(&root, upload_content);

  DVLOG(1) << "InsertResource data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//========================== DeleteResourceRequest ===========================

DeleteResourceRequest::DeleteResourceRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const std::string& parent_resource_id,
    const std::string& resource_id,
    const EntryActionCallback& callback)
    : EntryActionRequest(sender, callback),
      url_generator_(url_generator),
      parent_resource_id_(parent_resource_id),
      resource_id_(resource_id) {
  DCHECK(!callback.is_null());
}

DeleteResourceRequest::~DeleteResourceRequest() {}

GURL DeleteResourceRequest::GetURL() const {
  return url_generator_.GetChildrenUrlForRemoval(
      parent_resource_id_, resource_id_);
}

net::URLFetcher::RequestType DeleteResourceRequest::GetRequestType() const {
  return net::URLFetcher::DELETE_REQUEST;
}

//======================= InitiateUploadNewFileRequest =======================

InitiateUploadNewFileRequest::InitiateUploadNewFileRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const std::string& content_type,
    int64 content_length,
    const std::string& parent_resource_id,
    const std::string& title,
    const InitiateUploadCallback& callback)
    : InitiateUploadRequestBase(sender,
                                callback,
                                content_type,
                                content_length),
      url_generator_(url_generator),
      parent_resource_id_(parent_resource_id),
      title_(title) {
}

InitiateUploadNewFileRequest::~InitiateUploadNewFileRequest() {}

GURL InitiateUploadNewFileRequest::GetURL() const {
  return url_generator_.GetInitiateUploadNewFileUrl();
}

net::URLFetcher::RequestType
InitiateUploadNewFileRequest::GetRequestType() const {
  return net::URLFetcher::POST;
}

bool InitiateUploadNewFileRequest::GetContentData(
    std::string* upload_content_type,
    std::string* upload_content) {
  *upload_content_type = kContentTypeApplicationJson;

  base::DictionaryValue root;
  root.SetString("title", title_);

  // Fill parent link.
  {
    scoped_ptr<base::DictionaryValue> parent(new base::DictionaryValue);
    parent->SetString("kind", kParentLinkKind);
    parent->SetString("id", parent_resource_id_);

    scoped_ptr<base::ListValue> parents(new base::ListValue);
    parents->Append(parent.release());

    root.Set("parents", parents.release());
  }

  base::JSONWriter::Write(&root, upload_content);

  DVLOG(1) << "InitiateUploadNewFile data: " << *upload_content_type << ", ["
           << *upload_content << "]";
  return true;
}

//===================== InitiateUploadExistingFileRequest ====================

InitiateUploadExistingFileRequest::InitiateUploadExistingFileRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const std::string& content_type,
    int64 content_length,
    const std::string& resource_id,
    const std::string& etag,
    const InitiateUploadCallback& callback)
    : InitiateUploadRequestBase(sender,
                                callback,
                                content_type,
                                content_length),
      url_generator_(url_generator),
      resource_id_(resource_id),
      etag_(etag) {
}

InitiateUploadExistingFileRequest::~InitiateUploadExistingFileRequest() {}

GURL InitiateUploadExistingFileRequest::GetURL() const {
  return url_generator_.GetInitiateUploadExistingFileUrl(resource_id_);
}

net::URLFetcher::RequestType
InitiateUploadExistingFileRequest::GetRequestType() const {
  return net::URLFetcher::PUT;
}

std::vector<std::string>
InitiateUploadExistingFileRequest::GetExtraRequestHeaders() const {
  std::vector<std::string> headers(
      InitiateUploadRequestBase::GetExtraRequestHeaders());
  headers.push_back(util::GenerateIfMatchHeader(etag_));
  return headers;
}

//============================ ResumeUploadRequest ===========================

ResumeUploadRequest::ResumeUploadRequest(
    RequestSender* sender,
    const GURL& upload_location,
    int64 start_position,
    int64 end_position,
    int64 content_length,
    const std::string& content_type,
    const base::FilePath& local_file_path,
    const UploadRangeCallback& callback,
    const ProgressCallback& progress_callback)
    : ResumeUploadRequestBase(sender,
                              upload_location,
                              start_position,
                              end_position,
                              content_length,
                              content_type,
                              local_file_path),
      callback_(callback),
      progress_callback_(progress_callback) {
  DCHECK(!callback_.is_null());
}

ResumeUploadRequest::~ResumeUploadRequest() {}

void ResumeUploadRequest::OnRangeRequestComplete(
    const UploadRangeResponse& response,
    scoped_ptr<base::Value> value) {
  DCHECK(CalledOnValidThread());
  ParseFileResourceWithUploadRangeAndRun(callback_, response, value.Pass());
}

void ResumeUploadRequest::OnURLFetchUploadProgress(
    const net::URLFetcher* source, int64 current, int64 total) {
  if (!progress_callback_.is_null())
    progress_callback_.Run(current, total);
}

//========================== GetUploadStatusRequest ==========================

GetUploadStatusRequest::GetUploadStatusRequest(
    RequestSender* sender,
    const GURL& upload_url,
    int64 content_length,
    const UploadRangeCallback& callback)
    : GetUploadStatusRequestBase(sender,
                                 upload_url,
                                 content_length),
      callback_(callback) {
  DCHECK(!callback.is_null());
}

GetUploadStatusRequest::~GetUploadStatusRequest() {}

void GetUploadStatusRequest::OnRangeRequestComplete(
    const UploadRangeResponse& response,
    scoped_ptr<base::Value> value) {
  DCHECK(CalledOnValidThread());
  ParseFileResourceWithUploadRangeAndRun(callback_, response, value.Pass());
}

//========================== DownloadFileRequest ==========================

DownloadFileRequest::DownloadFileRequest(
    RequestSender* sender,
    const DriveApiUrlGenerator& url_generator,
    const std::string& resource_id,
    const base::FilePath& output_file_path,
    const DownloadActionCallback& download_action_callback,
    const GetContentCallback& get_content_callback,
    const ProgressCallback& progress_callback)
    : DownloadFileRequestBase(
          sender,
          download_action_callback,
          get_content_callback,
          progress_callback,
          url_generator.GenerateDownloadFileUrl(resource_id),
          output_file_path) {
}

DownloadFileRequest::~DownloadFileRequest() {
}

}  // namespace drive
}  // namespace google_apis
