// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_FILE_UTIL_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_FILE_UTIL_H_

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/memory/ref_counted.h"
#include "extensions/common/manifest.h"

namespace base {
class DictionaryValue;
class FilePath;
}

namespace extensions {
class Extension;
struct InstallWarning;
}

// Utilities for manipulating the on-disk storage of extensions.
namespace extension_file_util {

extern const base::FilePath::CharType kTempDirectoryName[];

// Copies |unpacked_source_dir| into the right location under |extensions_dir|.
// The destination directory is returned on success, or empty path is returned
// on failure.
base::FilePath InstallExtension(const base::FilePath& unpacked_source_dir,
                                const std::string& id,
                                const std::string& version,
                                const base::FilePath& extensions_dir);

// Removes all versions of the extension with |id| from |extensions_dir|.
void UninstallExtension(const base::FilePath& extensions_dir,
                        const std::string& id);

// Loads and validates an extension from the specified directory. Returns NULL
// on failure, with a description of the error in |error|.
scoped_refptr<extensions::Extension> LoadExtension(
    const base::FilePath& extension_root,
    extensions::Manifest::Location location,
    int flags,
    std::string* error);

// The same as LoadExtension except use the provided |extension_id|.
scoped_refptr<extensions::Extension> LoadExtension(
    const base::FilePath& extension_root,
    const std::string& extension_id,
    extensions::Manifest::Location location,
    int flags,
    std::string* error);

// Loads an extension manifest from the specified directory. Returns NULL
// on failure, with a description of the error in |error|.
base::DictionaryValue* LoadManifest(const base::FilePath& extension_root,
                                    std::string* error);

// Returns true if the given extension object is valid and consistent.
// May also append a series of warning messages to |warnings|, but they
// should not prevent the extension from running.
//
// Otherwise, returns false, and a description of the error is
// returned in |error|.
bool ValidateExtension(const extensions::Extension* extension,
                       std::string* error,
                       std::vector<extensions::InstallWarning>* warnings);

// Returns a list of paths (relative to the extension dir) for images that
// the browser might load (like themes and page action icons) for the given
// extension.
std::set<base::FilePath> GetBrowserImagePaths(
    const extensions::Extension* extension);

// Returns a list of files that contain private keys inside |extension_dir|.
std::vector<base::FilePath> FindPrivateKeyFiles(
    const base::FilePath& extension_dir);

// We need to reserve the namespace of entries that start with "_" for future
// use by Chrome.
// If any files or directories are found using "_" prefix and are not on
// reserved list we return false, and set error message.
bool CheckForIllegalFilenames(const base::FilePath& extension_path,
                              std::string* error);

// Returns a path to a temporary directory for unpacking an extension that will
// be installed into |extensions_dir|. Creates the directory if necessary.
// The directory will be on the same file system as |extensions_dir| so
// that the extension directory can be efficiently renamed into place. Returns
// an empty file path on failure.
base::FilePath GetInstallTempDir(const base::FilePath& extensions_dir);

// Helper function to delete files. This is used to avoid ugly casts which
// would be necessary with PostMessage since base::Delete is overloaded.
// TODO(skerner): Make a version of Delete that is not overloaded in file_util.
void DeleteFile(const base::FilePath& path, bool recursive);

}  // namespace extension_file_util

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_FILE_UTIL_H_
