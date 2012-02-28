// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::_;

using base::Value;
using base::DictionaryValue;
using base::ListValue;

namespace gdata {

class GDataFileSystemTest : public testing::Test {
 protected:
  virtual void SetUp() {
    profile_.reset(new TestingProfile);
    file_system_ = new GDataFileSystem(profile_.get());
  }

  // Loads test json file as root ("/gdata") element.
  void LoadRootFeedDocument(const std::string& filename) {
    LoadSubdirFeedDocument(FilePath("gdata"), filename);
  }

  // Loads test json file as subdirectory content of |directory_path|.
  void LoadSubdirFeedDocument(const FilePath& directory_path,
                              const std::string& filename) {
    std::string error;
    scoped_ptr<Value> document(LoadJSONFile(filename));
    ASSERT_TRUE(document.get());
    ASSERT_TRUE(document->GetType() == Value::TYPE_DICTIONARY);
    GURL unused;
    ASSERT_TRUE(UpdateContent(directory_path, document.get()));
  }

  // Updates the content of directory under |directory_path| with parsed feed
  // |value|.
  bool UpdateContent(const FilePath& directory_path,
                     Value* value) {
    GURL unused;
    return file_system_->UpdateDirectoryWithDocumentFeed(
        directory_path,
        GURL(),   // feed_url
        value,
        true,     // is_initial_feed
        &unused) == base::PLATFORM_FILE_OK;
  }

  void UpdateSubdirContent(const FilePath& search_file_path,
                           const FilePath& directory_path,
                           scoped_refptr<FindFileDelegate> delegate,
                           Value* data) {
    GDataFileSystem::FindFileParams params(
        search_file_path,
        false,
        directory_path,
        GURL(),
        true,
        delegate);
    file_system_->OnGetDocuments(params,
                                 HTTP_SUCCESS,
                                 data);
  }

  static Value* LoadJSONFile(const std::string& filename) {
    FilePath path;
    std::string error;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("chromeos")
        .AppendASCII("gdata")
        .AppendASCII(filename.c_str());
    EXPECT_TRUE(file_util::PathExists(path)) <<
        "Couldn't find " << path.value();

    JSONFileValueSerializer serializer(path);
    Value* value = serializer.Deserialize(NULL, &error);
    EXPECT_TRUE(value) <<
        "Parse error " << path.value() << ": " << error;
    return value;
  }

  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<GDataFileSystem> file_system_;
};


// Delegate used to find a directory element for file system updates.
class MockFindFileDelegate : public gdata::FindFileDelegate {
 public:
  MockFindFileDelegate() {
  }

  virtual ~MockFindFileDelegate() {
  }

  // gdata::FindFileDelegate overrides.
  MOCK_METHOD1(OnFileFound, void(GDataFile*));
  MOCK_METHOD2(OnDirectoryFound, void(const FilePath&, GDataDirectory* dir));
  MOCK_METHOD2(OnEnterDirectory, FindFileTraversalCommand(
      const FilePath&, GDataDirectory* dir));
  MOCK_METHOD1(OnError, void(base::PlatformFileError));
};

TEST_F(GDataFileSystemTest, SearchRootDirectory) {
  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnDirectoryFound(FilePath("gdata"), _))
      .Times(1);

  file_system_->FindFileByPath(FilePath("gdata"),
                               mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, SearchExistingFile) {
  LoadRootFeedDocument("root_feed.json");
  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath("gdata"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate.get(), OnFileFound(_))
      .Times(1);

  file_system_->FindFileByPath(FilePath("gdata/File 1.txt"),
                               mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, SearchExistingDocument) {
  LoadRootFeedDocument("root_feed.json");
  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath("gdata"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate.get(), OnFileFound(_))
      .Times(1);

  file_system_->FindFileByPath(FilePath("gdata/Document 1.gdocument"),
                               mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, SearchDuplicateNames) {
  LoadRootFeedDocument("root_feed.json");

  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();
  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath("gdata"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate.get(), OnFileFound(_))
      .Times(1);
  file_system_->FindFileByPath(FilePath("gdata/Duplicate Name.txt"),
                               mock_find_file_delegate);

  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate2 =
      new MockFindFileDelegate();
  EXPECT_CALL(*mock_find_file_delegate2.get(),
              OnEnterDirectory(FilePath("gdata"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate2.get(), OnFileFound(_))
      .Times(1);
  file_system_->FindFileByPath(FilePath("gdata/Duplicate Name (2).txt"),
                               mock_find_file_delegate2);
}

TEST_F(GDataFileSystemTest, SearchExistingDirectory) {
  LoadRootFeedDocument("root_feed.json");
  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath("gdata"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate.get(), OnDirectoryFound(_, _))
      .Times(1);

  file_system_->FindFileByPath(FilePath("gdata/Directory 1"),
                               mock_find_file_delegate);
}


TEST_F(GDataFileSystemTest, SearchNonExistingFile) {
  LoadRootFeedDocument("root_feed.json");
  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath("gdata"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnError(base::PLATFORM_FILE_ERROR_NOT_FOUND))
      .Times(1);

  file_system_->FindFileByPath(FilePath("gdata/nonexisting.file"),
                               mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, StopFileSearch) {
  LoadRootFeedDocument("root_feed.json");
  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  // Stop on first directory entry.
  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath("gdata"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_TERMINATES));

  file_system_->FindFileByPath(FilePath("gdata/Directory 1"),
                               mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, SearchInSubdir) {
  LoadRootFeedDocument("root_feed.json");
  LoadSubdirFeedDocument(FilePath("gdata/Directory 1"), "subdir_feed.json");

  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath("gdata"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath("gdata/Directory 1"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));

  EXPECT_CALL(*mock_find_file_delegate.get(), OnFileFound(_))
      .Times(1);

  file_system_->FindFileByPath(
      FilePath("gdata/Directory 1/SubDirectory File 1.txt"),
      mock_find_file_delegate);
}

}   // namespace gdata
