// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOM_STORAGE_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_DOM_STORAGE_CONTEXT_H_
#pragma once

#include <vector>

#include "base/string16.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"

class FilePath;

namespace content {

class BrowserContext;

// Represents the per-BrowserContext Local Storage data.
// Call these methods only on the WebKit thread.
class DOMStorageContext : public base::RefCountedThreadSafe<DOMStorageContext> {
 public:
  virtual ~DOMStorageContext() {}

  CONTENT_EXPORT static DOMStorageContext* GetForBrowserContext(
      BrowserContext* browser_context);

  // Returns all the file paths of local storage files.
  virtual std::vector<FilePath> GetAllStorageFiles() = 0;

  // Get the file name of the local storage file for the given origin.
  virtual FilePath GetFilePath(const string16& origin_id) const = 0;

  // Deletes the local storage file for the given origin.
  virtual void DeleteForOrigin(const string16& origin_id) = 0;

  // Deletes a single local storage file.
  virtual void DeleteLocalStorageFile(const FilePath& file_path) = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOM_STORAGE_CONTEXT_H_
