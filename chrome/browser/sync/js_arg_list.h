// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_JS_ARG_LIST_H_
#define CHROME_BROWSER_SYNC_JS_ARG_LIST_H_
#pragma once

// See README.js for design comments.

#include <string>
#include <vector>

#include "base/memory/ref_counted.h"
#include "base/values.h"

namespace browser_sync {

// A thread-safe wrapper around an immutable ListValue.  Used for
// passing around argument lists to different threads.
class JsArgList {
 public:
  // Uses an empty argument list.
  JsArgList();

  // Takes over the data in |args|, leaving |args| empty.
  explicit JsArgList(ListValue* args);

  ~JsArgList();

  const ListValue& Get() const;

  std::string ToString() const;

  // Copy constructor and assignment operator welcome.

 private:
  class SharedListValue : public base::RefCountedThreadSafe<SharedListValue> {
   public:
    SharedListValue();
    explicit SharedListValue(ListValue* list_value);

    const ListValue& Get() const;

   private:
    ~SharedListValue();
    friend class base::RefCountedThreadSafe<SharedListValue>;

    ListValue list_value_;
  };

  scoped_refptr<const SharedListValue> args_;
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_JS_ARG_LIST_H_
