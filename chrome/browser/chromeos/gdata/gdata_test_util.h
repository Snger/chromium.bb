// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GDATA_GDATA_TEST_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_GDATA_GDATA_TEST_UTIL_H_

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/gdata/drive_resource_metadata.h"
#include "chrome/browser/chromeos/gdata/gdata_errorcode.h"

class FilePath;

namespace gdata {

class DriveCacheEntry;
class DriveEntryProto;

typedef std::vector<DriveEntryProto> DriveEntryProtoVector;

namespace test_util {

// Runs a task posted to the blocking pool, including subquent tasks posted
// to the UI message loop and the blocking pool.
//
// A task is often posted to the blocking pool with PostTaskAndReply(). In
// that case, a task is posted back to the UI message loop, which can again
// post a task to the blocking pool. This function processes these tasks
// repeatedly.
void RunBlockingPoolTask();

// This is a bitmask of cache states in DriveCacheEntry. Used only in tests.
enum TestDriveCacheState {
  TEST_CACHE_STATE_NONE       = 0,
  TEST_CACHE_STATE_PINNED     = 1 << 0,
  TEST_CACHE_STATE_PRESENT    = 1 << 1,
  TEST_CACHE_STATE_DIRTY      = 1 << 2,
  TEST_CACHE_STATE_MOUNTED    = 1 << 3,
  TEST_CACHE_STATE_PERSISTENT = 1 << 4,
};

// Converts |cache_state| which is a bit mask of TestDriveCacheState, to a
// DriveCacheEntry.
DriveCacheEntry ToCacheEntry(int cache_state);

// Returns true if the cache state of the given two cache entries are equal.
bool CacheStatesEqual(const DriveCacheEntry& a, const DriveCacheEntry& b);

// Copies |error| to |output|. Used to run asynchronous functions that take
// FileOperationCallback from tests.
void CopyErrorCodeFromFileOperationCallback(GDataFileError* output,
                                            GDataFileError error);

// Copies |error| and |moved_file_path| to |out_error| and |out_file_path|.
// Used to run asynchronous functions that take FileMoveCallback from tests.
void CopyResultsFromFileMoveCallback(GDataFileError* out_error,
                                     FilePath* out_file_path,
                                     GDataFileError error,
                                     const FilePath& moved_file_path);

// Copies |error| and |entry_proto| to |out_error| and |out_entry_proto|
// respectively. Used to run asynchronous functions that take
// GetEntryInfoCallback from tests.
void CopyResultsFromGetEntryInfoCallback(
    GDataFileError* out_error,
    scoped_ptr<DriveEntryProto>* out_entry_proto,
    GDataFileError error,
    scoped_ptr<DriveEntryProto> entry_proto);

// Copies |error| and |entries| to |out_error| and |out_entries|
// respectively. Used to run asynchronous functions that take
// GetEntryInfoCallback from tests.
void CopyResultsFromReadDirectoryCallback(
    GDataFileError* out_error,
    scoped_ptr<DriveEntryProtoVector>* out_entries,
    GDataFileError error,
    scoped_ptr<DriveEntryProtoVector> entries);

// Copies |error|, |drive_file_path|, and |entry_proto| to |out_error|,
// |out_drive_file_path|, and |out_entry_proto| respectively. Used to run
// asynchronous functions that take GetEntryInfoWithFilePathCallback from
// tests.
void CopyResultsFromGetEntryInfoWithFilePathCallback(
    GDataFileError* out_error,
    FilePath* out_drive_file_path,
    scoped_ptr<DriveEntryProto>* out_entry_proto,
    GDataFileError error,
    const FilePath& drive_file_path,
    scoped_ptr<DriveEntryProto> entry_proto);

// Copies |result| to |out_result|. Used to run asynchronous functions
// that take GetEntryInfoPairCallback from tests.
void CopyResultsFromGetEntryInfoPairCallback(
    scoped_ptr<EntryInfoPairResult>* out_result,
    scoped_ptr<EntryInfoPairResult> result);

}  // namespace test_util
}  // namespace gdata

#endif  // CHROME_BROWSER_CHROMEOS_GDATA_GDATA_TEST_UTIL_H_
