// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_EXTENSION_ERROR_H_
#define EXTENSIONS_BROWSER_EXTENSION_ERROR_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "extensions/common/stack_frame.h"
#include "url/gurl.h"

namespace extensions {

class ExtensionError {
 public:
  enum Type {
    MANIFEST_ERROR,
    RUNTIME_ERROR
  };

  virtual ~ExtensionError();

  virtual std::string PrintForTest() const;

  // Return true if this error and |rhs| are considered equal, and should be
  // grouped together.
  bool IsEqual(const ExtensionError* rhs) const;

  Type type() const { return type_; }
  const std::string& extension_id() const { return extension_id_; }
  bool from_incognito() const { return from_incognito_; }
  logging::LogSeverity level() const { return level_; }
  const base::string16& source() const { return source_; }
  const base::string16& message() const { return message_; }
  size_t occurrences() const { return occurrences_; }
  void set_occurrences(size_t occurrences) { occurrences_ = occurrences; }

 protected:
  ExtensionError(Type type,
                 const std::string& extension_id,
                 bool from_incognito,
                 logging::LogSeverity level,
                 const base::string16& source,
                 const base::string16& message);

  virtual bool IsEqualImpl(const ExtensionError* rhs) const = 0;

  // Which type of error this is.
  Type type_;
  // The ID of the extension which caused the error.
  std::string extension_id_;
  // Whether or not the error was caused while incognito.
  bool from_incognito_;
  // The severity level of the error.
  logging::LogSeverity level_;
  // The source for the error; this can be a script, web page, or manifest file.
  // This is stored as a string (rather than a url) since it can be a Chrome
  // script file (e.g., event_bindings.js).
  base::string16 source_;
  // The error message itself.
  base::string16 message_;
  // The number of times this error has occurred.
  size_t occurrences_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionError);
};

class ManifestError : public ExtensionError {
 public:
  ManifestError(const std::string& extension_id,
                const base::string16& message);
  virtual ~ManifestError();

  virtual std::string PrintForTest() const OVERRIDE;
 private:
  virtual bool IsEqualImpl(const ExtensionError* rhs) const OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(ManifestError);
};

class RuntimeError : public ExtensionError {
 public:
  RuntimeError(bool from_incognito,
               const base::string16& source,
               const base::string16& message,
               const StackTrace& stack_trace,
               const GURL& context_url,
               logging::LogSeverity level);
  virtual ~RuntimeError();

  virtual std::string PrintForTest() const OVERRIDE;

  const GURL& context_url() const { return context_url_; }
  const StackTrace& stack_trace() const { return stack_trace_; }
 private:
  virtual bool IsEqualImpl(const ExtensionError* rhs) const OVERRIDE;

  // Since we piggy-back onto other error reporting systems (like V8 and
  // WebKit), the reported information may need to be cleaned up in order to be
  // in a consistent format.
  void CleanUpInit();

  GURL context_url_;
  StackTrace stack_trace_;

  DISALLOW_COPY_AND_ASSIGN(RuntimeError);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_EXTENSION_ERROR_H_
