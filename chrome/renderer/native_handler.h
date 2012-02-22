// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_NATIVE_HANDLER_H_
#define CHROME_RENDERER_NATIVE_HANDLER_H_
#pragma once

#include "base/bind.h"
#include "base/memory/linked_ptr.h"
#include "v8/include/v8.h"

#include <string>
#include <vector>

// A NativeHandler is a factory for JS objects with functions on them that map
// to native C++ functions. Subclasses should call RouteFunction() in their
// constructor to define functions on the created JS objects.
class NativeHandler {
 public:
  explicit NativeHandler();
  virtual ~NativeHandler();

  // Create an object with bindings to the native functions defined through
  // RouteFunction().
  v8::Handle<v8::Object> NewInstance();

 protected:
  typedef base::Callback<v8::Handle<v8::Value>(const v8::Arguments&)>
      HandlerFunction;

  // Installs a new 'route' from |name| to |handler_function|. This means that
  // NewInstance()s of this NativeHandler will have a property |name| which
  // will be handled by |handler_function|.
  void RouteFunction(const std::string& name,
                     const HandlerFunction& handler_function);

 private:
  static v8::Handle<v8::Value> Router(const v8::Arguments& args);

  std::vector<linked_ptr<HandlerFunction> > handler_functions_;
  v8::Handle<v8::ObjectTemplate> object_template_;

  DISALLOW_COPY_AND_ASSIGN(NativeHandler);
};

#endif  // CHROME_RENDERER_NATIVE_HANDLER_H_
