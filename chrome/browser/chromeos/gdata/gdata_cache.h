// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_H_
#pragma once

#include <map>
#include <string>

#include "base/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "base/platform_file.h"
#include "base/threading/sequenced_worker_pool.h"

class Profile;

namespace gdata {

// GDataCache is used to maintain cache states of GDataFileSystem.
//
// All non-static public member functions, unless mentioned otherwise (see
// GetCacheFilePath() for example), should be called from the sequenced
// worker pool with the sequence token set by CreateGDataCache(). This
// threading model is enforced by AssertOnSequencedWorkerPool().
class GDataCache {
 public:
  // Enum defining GCache subdirectory location.
  // This indexes into |GDataCache::cache_paths_| vector.
  enum CacheSubDirectoryType {
    CACHE_TYPE_META = 0,       // Downloaded feeds.
    CACHE_TYPE_PINNED,         // Symlinks to files in persistent dir that are
                               // pinned, or to /dev/null for non-existent
                               // files.
    CACHE_TYPE_OUTGOING,       // Symlinks to files in persistent or tmp dir to
                               // be uploaded.
    CACHE_TYPE_PERSISTENT,     // Files that are pinned or modified locally,
                               // not evictable, hopefully.
    CACHE_TYPE_TMP,            // Files that don't meet criteria to be in
                               // persistent dir, and hence evictable.
    CACHE_TYPE_TMP_DOWNLOADS,  // Downloaded files.
    CACHE_TYPE_TMP_DOCUMENTS,  // Temporary JSON files for hosted documents.
    NUM_CACHE_TYPES,           // This must be at the end.
  };

  // This is used as a bitmask for the cache state.
  enum CacheState {
    CACHE_STATE_NONE    = 0x0,
    CACHE_STATE_PINNED  = 0x1 << 0,
    CACHE_STATE_PRESENT = 0x1 << 1,
    CACHE_STATE_DIRTY   = 0x1 << 2,
    CACHE_STATE_MOUNTED = 0x1 << 3,
  };

  // Enum defining origin of a cached file.
  enum CachedFileOrigin {
    CACHED_FILE_FROM_SERVER = 0,
    CACHED_FILE_LOCALLY_MODIFIED,
    CACHED_FILE_MOUNTED,
  };

  // Enum defining type of file operation e.g. copy or move, etc.
  enum FileOperationType {
    FILE_OPERATION_MOVE = 0,
    FILE_OPERATION_COPY,
  };

  // Structure to store information of an existing cache file.
  struct CacheEntry {
    CacheEntry(const std::string& md5,
               CacheSubDirectoryType sub_dir_type,
               int cache_state)
    : md5(md5),
      sub_dir_type(sub_dir_type),
      cache_state(cache_state) {
    }

    bool IsPresent() const { return IsCachePresent(cache_state); }
    bool IsPinned() const { return IsCachePinned(cache_state); }
    bool IsDirty() const { return IsCacheDirty(cache_state); }
    bool IsMounted() const  { return IsCacheMounted(cache_state); }

    // For debugging purposes.
    std::string ToString() const;

    std::string md5;
    CacheSubDirectoryType sub_dir_type;
    int cache_state;
  };

  static bool IsCachePresent(int cache_state) {
    return cache_state & CACHE_STATE_PRESENT;
  }
  static bool IsCachePinned(int cache_state) {
    return cache_state & CACHE_STATE_PINNED;
  }
  static bool IsCacheDirty(int cache_state) {
    return cache_state & CACHE_STATE_DIRTY;
  }
  static bool IsCacheMounted(int cache_state) {
    return cache_state & CACHE_STATE_MOUNTED;
  }
  static int SetCachePresent(int cache_state) {
    return cache_state |= CACHE_STATE_PRESENT;
  }
  static int SetCachePinned(int cache_state) {
    return cache_state |= CACHE_STATE_PINNED;
  }
  static int SetCacheDirty(int cache_state) {
    return cache_state |= CACHE_STATE_DIRTY;
  }
  static int SetCacheMounted(int cache_state) {
    return cache_state |= CACHE_STATE_MOUNTED;
  }
  static int ClearCachePresent(int cache_state) {
    return cache_state &= ~CACHE_STATE_PRESENT;
  }
  static int ClearCachePinned(int cache_state) {
    return cache_state &= ~CACHE_STATE_PINNED;
  }
  static int ClearCacheDirty(int cache_state) {
    return cache_state &= ~CACHE_STATE_DIRTY;
  }
  static int ClearCacheMounted(int cache_state) {
    return cache_state &= ~CACHE_STATE_MOUNTED;
  }

  // A map table of cache file's resource id to its CacheEntry* entry.
  typedef std::map<std::string, CacheEntry> CacheMap;

  virtual ~GDataCache();

  // Returns the sub-directory under gdata cache directory for the given sub
  // directory type. Example:  <user_profile_dir>/GCache/v1/tmp
  //
  // Can be called on any thread.
  FilePath GetCacheDirectoryPath(CacheSubDirectoryType sub_dir_type) const;

  // Returns absolute path of the file if it were cached or to be cached.
  //
  // Can be called on any thread.
  FilePath GetCacheFilePath(const std::string& resource_id,
                            const std::string& md5,
                            CacheSubDirectoryType sub_dir_type,
                            CachedFileOrigin file_orign) const;

  // Returns true if the given path is under gdata cache directory, i.e.
  // <user_profile_dir>/GCache/v1
  //
  // Can be called on any thread.
  bool IsUnderGDataCacheDirectory(const FilePath& path) const;

  // Frees up disk space to store the given number of bytes, while keeping
  // kMinFreSpace bytes on the disk, if needed.  |has_enough_space| is
  // updated to indicate if we have enough space.
  void FreeDiskSpaceIfNeededFor(int64 num_bytes,
                                bool* has_enough_space);

  // Checks if file corresponding to |resource_id| and |md5| exists in cache.
  void GetFile(const std::string& resource_id,
               const std::string& md5,
               base::PlatformFileError* error,
               FilePath* cache_file_path);

  // Modifies cache state, which involves the following:
  // - moves or copies (per |file_operation_type|) |source_path|
  //   to |dest_path| in the cache dir
  // - if necessary, creates symlink
  // - deletes stale cached versions of |resource_id| in
  // |dest_path|'s directory.
  void Store(const std::string& resource_id,
             const std::string& md5,
             const FilePath& source_path,
             FileOperationType file_operation_type,
             base::PlatformFileError* error);

  // Modifies cache state, which involves the following:
  // - moves |source_path| to |dest_path| in persistent dir if
  //   file is not dirty
  // - creates symlink in pinned dir that references downloaded or locally
  //   modified file
  void Pin(const std::string& resource_id,
           const std::string& md5,
           FileOperationType file_operation_type,
           base::PlatformFileError* error);

  // Modifies cache state, which involves the following:
  // - moves |source_path| to |dest_path| in tmp dir if file is not dirty
  // - deletes symlink from pinned dir
  void Unpin(const std::string& resource_id,
             const std::string& md5,
             FileOperationType file_operation_type,
             base::PlatformFileError* error);

  // Modifies cache state, which involves the following:
  // - moves |source_path| to |dest_path|, where
  //   if we're mounting: |source_path| is the unmounted path and has .<md5>
  //       extension, and |dest_path| is the mounted path in persistent dir
  //       and has .<md5>.mounted extension;
  //   if we're unmounting: the opposite is true for the two paths, i.e.
  //       |dest_path| is the mounted path and |source_path| the unmounted path.
  void SetMountedState(const FilePath& file_path,
                       bool to_mount,
                       base::PlatformFileError* error,
                       FilePath* cache_file_path);

  // Modifies cache state, which involves the following:
  // - moves |source_path| to |dest_path| in persistent dir, where
  //   |source_path| has .<md5> extension and |dest_path| has .local extension
  // - if file is pinned, updates symlink in pinned dir to reference dirty file
  void MarkDirty(const std::string& resource_id,
                 const std::string& md5,
                 FileOperationType file_operation_type,
                 base::PlatformFileError* error,
                 FilePath* cache_file_path);

  // Modifies cache state, i.e. creates symlink in outgoing
  // dir to reference dirty file in persistent dir.
  void CommitDirty(const std::string& resource_id,
                   const std::string& md5,
                   FileOperationType file_operation_type,
                   base::PlatformFileError* error);

  // Modifies cache state, which involves the following:
  // - moves |source_path| to |dest_path| in persistent dir if
  //   file is pinned or tmp dir otherwise, where |source_path| has .local
  //   extension and |dest_path| has .<md5> extension
  // - deletes symlink in outgoing dir
  // - if file is pinned, updates symlink in pinned dir to reference
  //   |dest_path|
  void ClearDirty(const std::string& resource_id,
                  const std::string& md5,
                  FileOperationType file_operation_type,
                  base::PlatformFileError* error);

  // Does the following:
  // - remove all delete stale cache versions corresponding to |resource_id| in
  //   persistent, tmp and pinned directories
  // - remove entry corresponding to |resource_id| from cache map.
  void Remove(const std::string& resource_id,
              base::PlatformFileError* error);

  // TODO(hashimoto): Remove this method when crbug.com/131756 is fixed.
  const std::vector<FilePath>& cache_paths() const { return cache_paths_; }

  // Initializes cache.
  virtual void Initialize() = 0;

  // Sets |cache_map_| data member to formal parameter |new_cache_map|.
  virtual void SetCacheMap(const CacheMap& new_cache_map) = 0;

  // Updates cache map with entry corresponding to |resource_id|.
  // Creates new entry if it doesn't exist, otherwise update the entry.
  virtual void UpdateCache(const std::string& resource_id,
                           const std::string& md5,
                           CacheSubDirectoryType subdir,
                           int cache_state) = 0;

  // Removes entry corresponding to |resource_id| from cache map.
  virtual void RemoveFromCache(const std::string& resource_id) = 0;

  // Returns the cache entry for file corresponding to |resource_id| and |md5|
  // if entry exists in cache map.  Otherwise, returns NULL.
  // |md5| can be empty if only matching |resource_id| is desired, which may
  // happen when looking for pinned entries where symlinks' filenames have no
  // extension and hence no md5.
  virtual scoped_ptr<CacheEntry> GetCacheEntry(const std::string& resource_id,
                                               const std::string& md5) = 0;

  // Removes temporary files (files in CACHE_TYPE_TMP) from the cache map.
  virtual void RemoveTemporaryFiles() = 0;

  // Factory methods for GDataCache.
  // |pool| and |sequence_token| are used to assert that the functions are
  // called on the right sequenced worker pool with the right sequence token.
  //
  // For testing, the thread assertion can be disabled by passing NULL and
  // the default value of SequenceToken.
  static scoped_ptr<GDataCache> CreateGDataCache(
      const FilePath& cache_root_path,
      base::SequencedWorkerPool* pool,
      const base::SequencedWorkerPool::SequenceToken& sequence_token);

  // Gets the cache root path (i.e. <user_profile_dir>/GCache/v1) from the
  // profile.
  // TODO(satorux): Write a unit test for this.
  static FilePath GetCacheRootPath(Profile* profile);

 protected:
  GDataCache(
      const FilePath& cache_root_path,
      base::SequencedWorkerPool* pool_,
      const base::SequencedWorkerPool::SequenceToken& sequence_token);

  // Checks whether the current thread is on the right sequenced worker pool
  // with the right sequence ID. If not, DCHECK will fail.
  void AssertOnSequencedWorkerPool();

 private:
  // The root directory of the cache (i.e. <user_profile_dir>/GCache/v1).
  const FilePath cache_root_path_;
  // Paths for all subdirectories of GCache, one for each
  // GDataCache::CacheSubDirectoryType enum.
  const std::vector<FilePath> cache_paths_;
  base::SequencedWorkerPool* pool_;
  const base::SequencedWorkerPool::SequenceToken sequence_token_;

  DISALLOW_COPY_AND_ASSIGN(GDataCache);
};


// The minimum free space to keep. GDataFileSystem::GetFileByPath() returns
// base::PLATFORM_FILE_ERROR_NO_SPACE if the available space is smaller than
// this value.
//
// Copied from cryptohome/homedirs.h.
// TODO(satorux): Share the constant.
const int64 kMinFreeSpace = 512 * 1LL << 20;

// Interface class used for getting the free disk space. Only for testing.
class FreeDiskSpaceGetterInterface {
 public:
  virtual ~FreeDiskSpaceGetterInterface() {}
  virtual int64 AmountOfFreeDiskSpace() const = 0;
};

// Sets the free disk space getter for testing.
// The existing getter is deleted.
void SetFreeDiskSpaceGetterForTesting(
    FreeDiskSpaceGetterInterface* getter);

}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_CACHE_H_
