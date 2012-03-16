// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_

#include <sys/stat.h>

#include <map>
#include <string>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "base/platform_file.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/chromeos/gdata/gdata_files.h"
#include "chrome/browser/chromeos/gdata/gdata_params.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/browser/chromeos/gdata/gdata_uploader.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
class WaitableEvent;
}

namespace gdata {

class DocumentsServiceInterface;
class GDataDownloadObserver;

// Delegate class used to deal with results of virtual directory request
// to FindFileByPath() method. This class is refcounted since we pass it
// around and access it from different threads.
class FindFileDelegate : public base::RefCountedThreadSafe<FindFileDelegate> {
 public:
  virtual ~FindFileDelegate();

  enum FindFileTraversalCommand {
    FIND_FILE_CONTINUES,
    FIND_FILE_TERMINATES,
  };

  // Called when |file| search is completed within the file system.
  virtual void OnFileFound(GDataFile* file) = 0;

  // Called when |directory| is found at |directory_path| within the file
  // system.
  virtual void OnDirectoryFound(const FilePath& directory_path,
                                GDataDirectory* directory) = 0;

  // Called while traversing the virtual file system when |directory|
  // under |directory_path| is encountered. If this function returns
  // FIND_FILE_TERMINATES the current find operation will be terminated.
  virtual FindFileTraversalCommand OnEnterDirectory(
      const FilePath& directory_path, GDataDirectory* directory) = 0;

  // Called when an error occurs while fetching feed content from the server.
  virtual void OnError(base::PlatformFileError error) = 0;

  // Returns true if the delegate had already encountered a terminal state
  // that stops the traversal through the file system (OnError, OnDirectoryFound
  // or OnFileFound).
  virtual bool had_terminated() const = 0;
};

// Callback for completion of cache operation.
typedef base::Callback<void(base::PlatformFileError error,
                            const std::string& resource_id,
                            const std::string& md5)> CacheOperationCallback;

// Callback for GetFromCache.
typedef base::Callback<void(base::PlatformFileError error,
                            const std::string& resource_id,
                            const std::string& md5,
                            const FilePath& gdata_file_path,
                            const FilePath& cache_file_path)>
    GetFromCacheCallback;

// GData file system abstraction layer.
// GDataFileSystem is per-profie, hence inheriting ProfileKeyedService.
class GDataFileSystem : public ProfileKeyedService {
 public:
  struct FindFileParams {
    FindFileParams(const FilePath& in_file_path,
                   bool in_require_content,
                   const FilePath& in_directory_path,
                   const GURL& in_feed_url,
                   bool in_initial_feed,
                   scoped_refptr<FindFileDelegate> in_delegate);
    ~FindFileParams();

    const FilePath file_path;
    const bool require_content;
    const FilePath directory_path;
    const GURL feed_url;
    const bool initial_feed;
    const scoped_refptr<FindFileDelegate> delegate;
  };

  // Used for file operations like removing files.
  typedef base::Callback<void(base::PlatformFileError error)>
      FileOperationCallback;

  // Used to get files from the file system.
  typedef base::Callback<void(base::PlatformFileError error,
                              const FilePath& file_path)>
      GetFileCallback;

  // Used for file operations like removing files.
  typedef base::Callback<void(base::PlatformFileError error,
                              scoped_ptr<base::Value> value)>
    GetJsonDocumentCallback;

  // Used to get available space for the account from GData.
  typedef base::Callback<void(base::PlatformFileError error,
                              int bytes_total,
                              int bytes_used)>
      GetAvailableSpaceCallback;

  // ProfileKeyedService override:
  virtual void Shutdown() OVERRIDE;

  // Authenticates the user by fetching the auth token as
  // needed. |callback| will be run with the error code and the auth
  // token, on the thread this function is run.
  //
  // Must be called on UI thread.
  void Authenticate(const AuthStatusCallback& callback);

  // Finds file info by using virtual |file_path|. If |require_content| is set,
  // the found directory will be pre-populated before passed back to the
  // |delegate|. If |allow_refresh| is not set, directories' content
  // won't be performed.
  //
  // Can be called from any thread.
  void FindFileByPath(const FilePath& file_path,
                      scoped_refptr<FindFileDelegate> delegate);

  // Copies |src_file_path| to |dest_file_path| on the file system.
  // |src_file_path| can be a hosted document (see limitations below).
  // |dest_file_path| is expected to be of the same type of |src_file_path|
  // (i.e. if |src_file_path| is a file, |dest_file_path| will be created as
  // a file).
  //
  // This method also has the following assumptions/limitations that may be
  // relaxed or addressed later:
  // - |src_file_path| cannot be a regular file (i.e. non-hosted document)
  //   or a directory.
  // - |dest_file_path| must not exist.
  // - The parent of |dest_file_path| must already exist.
  //
  // The file entries represented by |src_file_path| and the parent directory
  // of |dest_file_path| need to be present in the in-memory representation
  // of the file system.
  //
  // Can be called from any thread.
  void Copy(const FilePath& src_file_path,
            const FilePath& dest_file_path,
            const FileOperationCallback& callback);

  // Moves |src_file_path| to |dest_file_path| on the file system.
  // |src_file_path| can be a file (regular or hosted document) or a directory.
  // |dest_file_path| is expected to be of the same type of |src_file_path|
  // (i.e. if |src_file_path| is a file, |dest_file_path| will be created as
  // a file).
  //
  // This method also has the following assumptions/limitations that may be
  // relaxed or addressed later:
  // - |dest_file_path| must not exist.
  // - The parent of |dest_file_path| must already exist.
  //
  // The file entries represented by |src_file_path| and the parent directory
  // of |dest_file_path| need to be present in the in-memory representation
  // of the file system.
  //
  // Can be called from any thread.
  void Move(const FilePath& src_file_path,
            const FilePath& dest_file_path,
            const FileOperationCallback& callback);

  // Removes |file_path| from the file system.  If |is_recursive| is set and
  // |file_path| represents a directory, we will also delete all of its
  // contained children elements. The file entry represented by |file_path|
  // needs to be present in in-memory representation of the file system that
  // in order to be removed.
  //
  // TODO(zelidrag): Wire |is_recursive| through gdata api
  // (find appropriate calls for it).
  //
  // Can be called from any thread. |callback| is run on the calling thread.
  void Remove(const FilePath& file_path,
              bool is_recursive,
              const FileOperationCallback& callback);

  // Creates new directory under |directory_path|. If |is_exclusive| is true,
  // an error is raised in case a directory is already present at the
  // |directory_path|. If |is_recursive| is true, the call creates parent
  // directories as needed just like mkdir -p does.
  //
  // Can be called from any thread. |callback| is run on the calling thread.
  void CreateDirectory(const FilePath& directory_path,
                       bool is_exclusive,
                       bool is_recursive,
                       const FileOperationCallback& callback);

  // Gets |file_path| from the file system. The file entry represented by
  // |file_path| needs to be present in in-memory representation of the file
  // system in order to be retrieved. If the file is not cached, the file
  // will be downloaded through gdata api.
  //
  // Can be called from any thread. |callback| is run on the calling thread.
  void GetFile(const FilePath& file_path, const GetFileCallback& callback);

  // Initiates directory feed fetching operation and continues previously
  // initiated FindFileByPath() attempt upon its completion. Safe to be called
  // from any thread. Internally, it will route content refresh request to
  // DocumentsService::GetDocuments() which will initiated content
  // fetching from UI thread as required by gdata library (UrlFetcher).
  //
  // Can be called from any thread.
  void RefreshDirectoryAndContinueSearch(const FindFileParams& params);

  // Gets absolute path of cache file corresponding to |gdata_file_path|.
  // Upon completion, |callback| is invoked on the same thread where this method
  // was called, with path if it exists and is accessible or empty FilePath
  // otherwise.
  void GetFromCacheForPath(const FilePath& gdata_file_path,
                           const GetFromCacheCallback& callback);

  // Finds file object by |file_path| and returns its |file_info|.
  // Returns true if file was found.
  bool GetFileInfoFromPath(const FilePath& gdata_file_path,
                           base::PlatformFileInfo* file_info);

  // Returns the tmp sub-directory under gdata cache directory, i.e.
  // <user_profile_dir>/GCache/v1/tmp
  FilePath GetGDataCacheTmpDirectory() {
    return cache_paths_[CACHE_TYPE_TMP];
  }

  // Fetches the user's Account Metadata to find out current quota information
  // and returns it to the callback.
  void GetAvailableSpace(const GetAvailableSpaceCallback& callback);

 private:
  friend class GDataUploader;
  friend class GDataFileSystemFactory;
  friend class GDataFileSystemTestBase;
  FRIEND_TEST_ALL_PREFIXES(GDataFileSystemTest,
                           FindFirstMissingParentDirectory);
  FRIEND_TEST_ALL_PREFIXES(GDataFileSystemTest,
                           GetGDataFileInfoFromPath);
  FRIEND_TEST_ALL_PREFIXES(GDataFileSystemTest,
                           GetFromCacheForPath);

  // Defines possible search results of FindFirstMissingParentDirectory().
  enum FindMissingDirectoryResult {
    // Target directory found, it's not a directory.
    FOUND_INVALID,
    // Found missing directory segment while searching for given directory.
    FOUND_MISSING,
    // Found target directory, it already exists.
    DIRECTORY_ALREADY_PRESENT,
  };

  // Defines set of parameters passes to intermediate callbacks during
  // execution of CreateDirectory() method.
  struct CreateDirectoryParams {
    CreateDirectoryParams(const FilePath& created_directory_path,
                          const FilePath& target_directory_path,
                          bool is_exclusive,
                          bool is_recursive,
                          const FileOperationCallback& callback);
    ~CreateDirectoryParams();

    const FilePath created_directory_path;
    const FilePath target_directory_path;
    const bool is_exclusive;
    const bool is_recursive;
    FileOperationCallback callback;
  };

  enum CacheType {  // This indexes into |cache_paths_| vector.
    CACHE_TYPE_BLOBS = 0,
    CACHE_TYPE_META,
    CACHE_TYPE_TMP,
  };

  // Callback similar to FileOperationCallback but with a given
  // |file_path|.
  typedef base::Callback<void(base::PlatformFileError error,
                              const FilePath& file_path)>
      FilePathUpdateCallback;

  GDataFileSystem(Profile* profile,
                  DocumentsServiceInterface* documents_service);
  virtual ~GDataFileSystem();

  // Finds file object by |file_path| and returns the file info.
  // Returns NULL if it does not find the file.
  GDataFileBase* GetGDataFileInfoFromPath(const FilePath& file_path);

  // Initiates upload operation of file defined with |file_name|,
  // |content_type| and |content_length|. The operation will place the newly
  // created file entity into |destination_directory|.
  //
  // Can be called from any thread. |callback| is run on the calling thread.
  void InitiateUpload(const std::string& file_name,
                      const std::string& content_type,
                      int64 content_length,
                      const FilePath& destination_directory,
                      const FilePath& virtual_path,
                      const InitiateUploadCallback& callback);

  // Resumes upload operation for chunk of file defined in |params..
  //
  // Can be called from any thread. |callback| is run on the calling thread.
  void ResumeUpload(const ResumeUploadParams& params,
                    const ResumeUploadCallback& callback);

  // Unsafe (unlocked) version of the function above.
  void UnsafeFindFileByPath(const FilePath& file_path,
                            scoped_refptr<FindFileDelegate> delegate);

  // Starts directory refresh operation as a result of
  // RefreshDirectoryAndContinueSearch call. |feed_list| is used to collect
  // individual parts of document feeds as they are being retrieved from
  // DocumentsService.
  void ContinueDirectoryRefresh(const FindFileParams& params,
                                scoped_ptr<base::ListValue> feed_list);

  // Converts document feed from gdata service into DirectoryInfo. On failure,
  // returns NULL and fills in |error| with an appropriate value.
  GDataDirectory* ParseGDataFeed(GDataErrorCode status,
                                 base::Value* data,
                                 base::PlatformFileError *error);

  // Renames a file or directory at |file_path| to |new_name|.
  void Rename(const FilePath& file_path,
              const FilePath::StringType& new_name,
              const FilePathUpdateCallback& callback);

  // Adds a file or directory at |file_path| to the directory at |dir_path|.
  void AddFileToDirectory(const FilePath& dir_path,
                          const FileOperationCallback& callback,
                          base::PlatformFileError error,
                          const FilePath& file_path);

  // Removes a file or directory at |file_path| from the directory at
  // |dir_path| and moves it to the root directory.
  void RemoveFileFromDirectory(const FilePath& dir_path,
                               const FilePathUpdateCallback& callback,
                               base::PlatformFileError error,
                               const FilePath& file_path);

  // Removes file under |file_path| from in-memory snapshot of the file system.
  // |resource_id| contains the resource id of the removed file if it was a
  // file.
  // Return PLATFORM_FILE_OK if successful.
  base::PlatformFileError RemoveFileFromGData(const FilePath& file_path,
                                              std::string* resource_id);

  // Callback for handling feed content fetching while searching for file info.
  // This callback is invoked after async feed fetch operation that was
  // invoked by StartDirectoryRefresh() completes. This callback will update
  // the content of the refreshed directory object and continue initially
  // started FindFileByPath() request.
  void OnGetDocuments(const FindFileParams& params,
                      scoped_ptr<base::ListValue> feed_list,
                      GDataErrorCode status,
                      scoped_ptr<base::Value> data);

  // A pass-through callback used for bridging from
  // FilePathUpdateCallback to FileOperationCallback.
  void OnFilePathUpdated(const FileOperationCallback& cllback,
                         base::PlatformFileError error,
                         const FilePath& file_path);

  // Callback for handling resource rename attempt.
  void OnRenameResourceCompleted(const FilePath& file_path,
                                 const FilePath::StringType& new_name,
                                 const FilePathUpdateCallback& callback,
                                 GDataErrorCode status,
                                 const GURL& document_url);

  // Callback for handling document copy attempt.
  void OnCopyDocumentCompleted(const FilePathUpdateCallback& callback,
                               GDataErrorCode status,
                               scoped_ptr<base::Value> data);

  // Callback for handling an attempt to add a file or directory to another
  // directory.
  void OnAddFileToDirectoryCompleted(const FileOperationCallback& callback,
                                     const FilePath& file_path,
                                     const FilePath& dir_path,
                                     GDataErrorCode status,
                                     const GURL& document_url);

  // Callback for handling an attempt to remove a file or directory from
  // another directory.
  void OnRemoveFileFromDirectoryCompleted(
      const FilePathUpdateCallback& callback,
      const FilePath& file_path,
      const FilePath& dir_path,
      GDataErrorCode status,
      const GURL& document_url);

  // Callback for handling account metadata fetch.
  void OnGetAvailableSpace(
      const GetAvailableSpaceCallback& callback,
      GDataErrorCode status,
      scoped_ptr<base::Value> data);

  // Callback for handling document remove attempt.
  void OnRemovedDocument(
      const FileOperationCallback& callback,
      const FilePath& file_path,
      GDataErrorCode status,
      const GURL& document_url);

  // Callback for handling directory create requests.
  void OnCreateDirectoryCompleted(
      const CreateDirectoryParams& params,
      GDataErrorCode status,
      scoped_ptr<base::Value> created_entry);

  // Callback for handling file downloading requests.
  void OnFileDownloaded(
    const GetFileCallback& callback,
    GDataErrorCode status,
    const GURL& content_url,
    const FilePath& temp_file);

  // Callback for handling file upload initialization requests.
  void OnUploadLocationReceived(
      const InitiateUploadCallback& callback,
      scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
      GDataErrorCode code,
      const GURL& upload_location);

  // Callback for handling file upload resume requests.
  void OnResumeUpload(const ResumeUploadCallback& callback,
      scoped_refptr<base::MessageLoopProxy> message_loop_proxy,
      const ResumeUploadResponse& response);

  // Renames a file or directory at |file_path| on in-memory snapshot
  // of the file system. Returns PLATFORM_FILE_OK if successful.
  base::PlatformFileError RenameFileOnFilesystem(
      const FilePath& file_path, const FilePath::StringType& new_name,
      FilePath* updated_file_path);

  // Adds a file or directory at |file_path| to another directory at
  // |dir_path| on in-memory snapshot of the file system.
  // Returns PLATFORM_FILE_OK if successful.
  base::PlatformFileError AddFileToDirectoryOnFilesystem(
      const FilePath& file_path, const FilePath& dir_path);

  // Removes a file or directory at |file_path| from another directory at
  // |dir_path| on in-memory snapshot of the file system.
  // Returns PLATFORM_FILE_OK if successful.
  base::PlatformFileError RemoveFileFromDirectoryOnFilesystem(
      const FilePath& file_path, const FilePath& dir_path,
      FilePath* updated_file_path);

  // Removes file under |file_path| from in-memory snapshot of the file system
  // and the corresponding file from cache if it exists.
  // Return PLATFORM_FILE_OK if successful.
  base::PlatformFileError RemoveFileFromFileSystem(const FilePath& file_path);

  // Parses the content of |feed_data| and returns DocumentFeed instance
  // represeting it.
  DocumentFeed* ParseDocumentFeed(base::Value* feed_data);

  // Load the whole directory structure from the root feed.
  base::PlatformFileError UpdateRootWithDocumentFeed(
      base::ListValue* feed_list);

  // Updates content of the directory identified with |directory_path| with
  // feeds collected in |feed_list|.
  // On success, returns PLATFORM_FILE_OK.
  base::PlatformFileError UpdateDirectoryWithDocumentFeed(
      const FilePath& directory_path, base::ListValue* feed_list,
      ContentOrigin origin);

  // Converts |entry_value| into GFileDocument instance and adds it
  // to virtual file system at |directory_path|.
  base::PlatformFileError AddNewDirectory(const FilePath& directory_path,
                                          base::Value* entry_value);

  // Given non-existing |directory_path|, finds the first missing parent
  // directory of |directory_path|.
  FindMissingDirectoryResult FindFirstMissingParentDirectory(
      const FilePath& directory_path,
      GURL* last_dir_content_url,
      FilePath* first_missing_parent_path);

  // Starts root feed load from the cache. If successful, it will continue
  // search given |parms| after it refreshes content from the cache.
  void LoadRootFeed(const GDataFileSystem::FindFileParams& params);

  // Loads root feed content from |file_path| on IO thread pool. Upon
  // completion it will invoke |callback| on thread represented by
  // |relay_proxy|.
  static void LoadRootFeedOnIOThreadPool(
      const FilePath& meta_cache_path,
      scoped_refptr<base::MessageLoopProxy> relay_proxy,
      const GetJsonDocumentCallback& callback);

  // Callback for handling root directory refresh from the cache.
  void OnLoadRootFeed(const GDataFileSystem::FindFileParams& params,
                      base::PlatformFileError error,
                      scoped_ptr<base::Value> feed_list);

  // Saves a collected feed in GCache directory under
  // <user_profile_dir>/GCache/v1/meta/|name| for later reloading when offline.
  void SaveFeed(scoped_ptr<base::Value> feed_vector,
                const FilePath& name);
  static void SaveFeedOnIOThreadPool(
      const FilePath& meta_cache_path,
      scoped_ptr<base::Value> feed_vector,
      const FilePath& name);

  // Finds and returns upload url of a given directory. Returns empty url
  // if directory can't be found.
  GURL GetUploadUrlForDirectory(const FilePath& destination_directory);

  void NotifyDirectoryChanged(const FilePath& directory_path);

  // Cache entry points from within GDataFileSystem.
  // The functionalities of GData blob cache include:
  // - stores downloaded gdata files on disk, indexed by the resource_id and md5
  //   of the gdata file.
  // - provides absolute path for files to be cached or cached.
  // - updates the cached file on disk after user has edited it locally
  // - handles eviction when disk runs out of space
  // - uploads dirty files to gdata server.
  // - etc.

  // Returns absolute path of the file if it were cached or to be cached.
  FilePath GetCacheFilePath(const std::string& resource_id,
                            const std::string& md5);

  // Stores |source_path| corresponding to |resource_id| and |md5| to cache.
  // Initializes cache if it has not been initialized.
  // Upon completion, |callback| is invoked on the thread where this method was
  // called.
  // TODO(kuan): When URLFetcher can save response to a specified file (as
  // opposed to only temporary file currently), remove |source_path| parameter.
  void StoreToCache(const std::string& resource_id,
                    const std::string& md5,
                    const FilePath& temp_file,
                    const CacheOperationCallback& callback);

  // Checks if file corresponding to |resource_id| and |md5| exist on disk and
  // can be accessed i.e. not corrupted by previous file operations that didn't
  // complete for whatever reasons.
  // Initializes cache if it has not been initialized.
  // Upon completion, |callback| is invoked on the thread where this method was
  // called with the cache file path if it exists and is accessible or empty
  // otherwise.
  void GetFromCache(const std::string& resource_id,
                    const std::string& md5,
                    const GetFromCacheCallback& callback);

  // Removes all files corresponding to |resource_id| from cache.
  // Initializes cache if it has not been initialized.
  // Upon completion, |callback| is invoked on the thread where this method was
  // called.
  void RemoveFromCache(const std::string& resource_id,
                       const CacheOperationCallback& callback);

  // Pin file corresponding to |resource_id| and |md5| by setting the
  // appropriate file attributes.
  // We'll try not to evict pinned cache files unless there's not enough space
  // on disk and pinned files are the only ones left.
  // If the file to be pinned is not stored in the cache,
  // net::ERR_FILE_NOT_FOUND will be passed to the |callback|.
  // Initializes cache if it has not been initialized.
  // Upon completion, |callback| is invoked on the thread where this method was
  // called.
  void Pin(const std::string& resource_id,
           const std::string& md5,
           const CacheOperationCallback& callback);

  // Unpin file corresponding to |resource_id| and |md5| by setting the
  // appropriate file attributes.
  // Unpinned files would be evicted when space on disk runs out.
  // Initializes cache if it has not been initialized.
  // Upon completion, |callback| is invoked on the thread where this method was
  // called.
  void Unpin(const std::string& resource_id,
             const std::string& md5,
             const CacheOperationCallback& callback);

  // Initializes cache if it hasn't been initialized by posting
  // InitializeCacheOnIOThreadPool task to IO thread pool.
  void InitializeCacheIfNecessary();

  // Task posted from InitializeCacheIfNecessary to run on IO thread pool.
  // Creates cache directory and its sub-directories if they don't exist,
  // or scans blobs sub-directory for files and their attributes and updates the
  // info into cache map.
  void InitializeCacheOnIOThreadPool();

  // Cache callbacks from cache tasks that were run on IO thread pool.

  // Callback for StoreToCache that updates the data members with results from
  // StoreToCacheOnIOThreadPool.
  void OnStoredToCache(base::PlatformFileError error,
                       const std::string& resource_id,
                       const std::string& md5,
                       mode_t mode_bits,
                       const CacheOperationCallback& callback);

  // Callback for GetFromCache that checks if file corresponding to
  // |resource_id| and |md5| exist in cache map.
  void OnGottenFromCache(const std::string& resource_id,
                         const std::string& md5,
                         const FilePath& gdata_file_path,
                         const GetFromCacheCallback& callback);

  // Default dummy callback for RemoveFromCache.
  void OnRemovedFromCache(base::PlatformFileError error,
                          const std::string& resource_id,
                          const std::string& md5);

  // Callback for any method that needs to modify cache status, e.g. Pin and
  // Unpin.
  void OnCacheStatusModified(base::PlatformFileError error,
                             const std::string& resource_id,
                             const std::string& md5,
                             mode_t mode_bits,
                             const CacheOperationCallback& callback);

  // Cache internal helper functions.

  void GetFromCacheInternal(const std::string& resource_id,
                            const std::string& md5,
                            const FilePath& gdata_file_path,
                            const GetFromCacheCallback& callback);

  scoped_ptr<GDataRootDirectory> root_;

  base::Lock lock_;

  // The profile hosts the GDataFileSystem.
  Profile* profile_;

  // The document service for the GDataFileSystem.
  scoped_ptr<DocumentsServiceInterface> documents_service_;

  // File content uploader.
  scoped_ptr<GDataUploader> gdata_uploader_;
  // Downloads observer.
  scoped_ptr<GDataDownloadObserver> gdata_download_observer_;

  // Base path for GData cache, e.g. <user_profile_dir>/user/GCache/v1.
  FilePath gdata_cache_path_;

  // Paths for all subdirectories of GCache, one for each CacheType enum.
  std::vector<FilePath> cache_paths_;

  scoped_ptr<base::WaitableEvent> on_cache_initialized_;

  // True if cache initialization has started, is in progress or has completed,
  // we only want to initialize cache once.
  bool cache_initialization_started_;

  base::WeakPtrFactory<GDataFileSystem> weak_ptr_factory_;
};

// Singleton that owns all GDataFileSystems and associates them with
// Profiles.
class GDataFileSystemFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the GDataFileSystem for |profile|, creating it if it is not
  // yet created.
  static GDataFileSystem* GetForProfile(Profile* profile);

  // Returns the GDataFileSystemFactory instance.
  static GDataFileSystemFactory* GetInstance();

 private:
  friend struct DefaultSingletonTraits<GDataFileSystemFactory>;

  GDataFileSystemFactory();
  virtual ~GDataFileSystemFactory();

  // ProfileKeyedServiceFactory:
  virtual ProfileKeyedService* BuildServiceInstanceFor(
      Profile* profile) const OVERRIDE;
};

// Base class for find delegates that require content refreshed.
// Also, keeps the track of the calling thread message loop proxy to ensure its
// specializations can provide reply on it.
class FindFileDelegateReplyBase : public FindFileDelegate {
 public:
  FindFileDelegateReplyBase(
      GDataFileSystem* file_system,
      const FilePath& search_file_path,
      bool require_content);
  virtual ~FindFileDelegateReplyBase();

  // FindFileDelegate overrides.
  virtual FindFileTraversalCommand OnEnterDirectory(
      const FilePath& current_directory_path,
      GDataDirectory* current_dir) OVERRIDE;

 protected:
  // Checks if the content of the |directory| under |directory_path| needs to be
  // refreshed. Returns true if directory content is fresh, otherwise it kicks
  // off content request request. After feed content content is received and
  // processed in GDataFileSystem::OnGetDocuments(), that function will also
  // restart the initiated FindFileByPath() request.
  bool CheckAndRefreshContent(const FilePath& directory_path,
                              GDataDirectory* directory);

 protected:
  GDataFileSystem* file_system_;
  // Search file path.
  FilePath search_file_path_;
  // True if the final directory content is required.
  bool require_content_;
  scoped_refptr<base::MessageLoopProxy> reply_message_proxy_;
};

// Delegate used to find a directory element for file system updates.
class ReadOnlyFindFileDelegate : public FindFileDelegate {
 public:
  ReadOnlyFindFileDelegate();

  // Returns found file.
  GDataFileBase* file() { return file_; }

 private:
  // GDataFileSystem::FindFileDelegate overrides.
  virtual void OnFileFound(gdata::GDataFile* file) OVERRIDE;
  virtual void OnDirectoryFound(const FilePath&,
                                GDataDirectory* dir) OVERRIDE;
  virtual FindFileTraversalCommand OnEnterDirectory(const FilePath&,
                                                    GDataDirectory*) OVERRIDE;
  virtual void OnError(base::PlatformFileError) OVERRIDE;
  virtual bool had_terminated() const OVERRIDE;

  // File entry that was found.
  GDataFileBase* file_;
};

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_FILE_SYSTEM_H_
