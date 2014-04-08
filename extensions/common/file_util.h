// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_COMMON_FILE_UTIL_H_
#define EXTENSIONS_COMMON_FILE_UTIL_H_

#include <string>

#include "extensions/common/message_bundle.h"

class GURL;

namespace base {
class FilePath;
}

namespace extensions {
namespace file_util {

// Get a relative file path from a chrome-extension:// URL.
base::FilePath ExtensionURLToRelativeFilePath(const GURL& url);

// Get a full file path from a chrome-extension-resource:// URL, If the URL
// points a file outside of root, this function will return empty FilePath.
base::FilePath ExtensionResourceURLToFilePath(const GURL& url,
                                              const base::FilePath& root);

// Loads extension message catalogs and returns message bundle.
// Returns NULL on error or if the extension is not localized.
MessageBundle* LoadMessageBundle(const base::FilePath& extension_path,
                                 const std::string& default_locale,
                                 std::string* error);

// Loads the extension message bundle substitution map. Contains at least
// the extension_id item.
MessageBundle::SubstitutionMap* LoadMessageBundleSubstitutionMap(
    const base::FilePath& extension_path,
    const std::string& extension_id,
    const std::string& default_locale);

}  // namespace file_util
}  // namespace extensions

#endif  // EXTENSIONS_COMMON_FILE_UTIL_H_
