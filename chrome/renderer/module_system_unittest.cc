// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_piece.h"
#include "chrome/renderer/module_system.h"
#include "testing/gtest/include/gtest/gtest.h"

#include <map>
#include <string>

// Native JS functions for doing asserts.
class AssertNatives : public NativeHandler {
 public:
  AssertNatives()
      : native_function_called_(false),
        failed_(false) {
    RouteFunction("AssertTrue", base::Bind(&AssertNatives::AssertTrue,
        base::Unretained(this)));
  }

  bool native_function_called() { return native_function_called_; }
  bool failed() { return failed_; }

  v8::Handle<v8::Value> AssertTrue(const v8::Arguments& args) {
    native_function_called_ = true;
    failed_ = failed_ || !args[0]->ToBoolean()->Value();
    return v8::Undefined();
  }

 private:
  bool native_function_called_;
  bool failed_;
};

class StringSourceMap : public ModuleSystem::SourceMap {
 public:
  StringSourceMap() {}
  virtual ~StringSourceMap() {}

  v8::Handle<v8::Value> GetSource(const std::string& name) OVERRIDE {
    if (source_map_.count(name) == 0)
      return v8::Undefined();
    return v8::String::New(source_map_[name].c_str());
  }

  bool Contains(const std::string& name) OVERRIDE {
    return source_map_.count(name);
  }

  void RegisterModule(const std::string& name, const std::string& source) {
    source_map_[name] = source;
  }

 private:
  std::map<std::string, std::string> source_map_;
};

// Native JS functions for disabling injection in ModuleSystem.
class DisableNativesHandler : public NativeHandler {
 public:
  explicit DisableNativesHandler(ModuleSystem* module_system)
      : module_system_(module_system) {
    RouteFunction("DisableNatives",
                  base::Bind(&DisableNativesHandler::DisableNatives,
                             base::Unretained(this)));
  }

  v8::Handle<v8::Value> DisableNatives(const v8::Arguments& args) {
    module_system_->set_natives_enabled(false);
    return v8::Undefined();
  }

 private:
  ModuleSystem* module_system_;
};

class ModuleSystemTest : public testing::Test {
 public:
  ModuleSystemTest()
      : context_(v8::Context::New()),
        source_map_(new StringSourceMap()) {
    context_->Enter();
    assert_natives_ = new AssertNatives();
    module_system_.reset(new ModuleSystem(source_map_.get()));
    module_system_->RegisterNativeHandler("assert", scoped_ptr<NativeHandler>(
        assert_natives_));
    RegisterModule("add", "exports.Add = function(x, y) { return x + y; };");
  }

  ~ModuleSystemTest() {
    context_->Exit();
    context_.Dispose();
  }

  void RegisterModule(const std::string& name, const std::string& code) {
    source_map_->RegisterModule(name, code);
  }

  virtual void TearDown() {
    // All tests must call a native function at least once.
    ASSERT_TRUE(assert_natives_->native_function_called());
    ASSERT_FALSE(assert_natives_->failed());
    ASSERT_FALSE(try_catch_.HasCaught());
  }

  v8::Persistent<v8::Context> context_;
  v8::HandleScope handle_scope_;
  v8::TryCatch try_catch_;
  AssertNatives* assert_natives_;
  scoped_ptr<StringSourceMap> source_map_;
  scoped_ptr<ModuleSystem> module_system_;
};

TEST_F(ModuleSystemTest, TestRequire) {
  RegisterModule("test",
      "var Add = require('add').Add;"
      "requireNative('assert').AssertTrue(Add(3, 5) == 8);");
  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestNestedRequire) {
  RegisterModule("double",
      "var Add = require('add').Add;"
      "exports.Double = function(x) { return Add(x, x); };");
  RegisterModule("test",
      "var Double = require('double').Double;"
      "requireNative('assert').AssertTrue(Double(3) == 6);");
  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestModuleInsulation) {
  RegisterModule("x",
      "var x = 10;"
      "exports.X = function() { return x; };");
  RegisterModule("y",
      "var x = 15;"
      "require('x');"
      "exports.Y = function() { return x; };");
  RegisterModule("test",
      "var Y = require('y').Y;"
      "var X = require('x').X;"
      "var assert = requireNative('assert');"
      "assert.AssertTrue(!this.hasOwnProperty('x'));"
      "assert.AssertTrue(Y() == 15);"
      "assert.AssertTrue(X() == 10);");
  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestDisableNativesPreventsNativeModulesBeingLoaded) {
  module_system_->RegisterNativeHandler("disable",
      scoped_ptr<NativeHandler>(
          new DisableNativesHandler(module_system_.get())));
  RegisterModule("test",
      "var assert = requireNative('assert');"
      "var disable = requireNative('disable');"
      "disable.DisableNatives();"
      "var caught = false;"
      "try {"
      "  requireNative('assert');"
      "} catch (e) {"
      "  caught = true;"
      "}"
      "assert.AssertTrue(caught);");
  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestLazyObject) {
  v8::Handle<v8::String> source = v8::String::New("({x: 5})");
  v8::Handle<v8::Object> lazy_object =
      ModuleSystem::CreateLazyObject("lazy.js", source);
  v8::Context::GetCurrent()->Global()->Set(v8::String::New("lazy"),
                                           lazy_object);
  RegisterModule("test",
      "var assert = requireNative('assert');"
      "assert.AssertTrue(lazy.x == 5);"
      "assert.AssertTrue(lazy.x == 5);");
  module_system_->Require("test");
}

TEST_F(ModuleSystemTest, TestLazyInstanceOnlyGetsEvaledOnce) {
  v8::Context::GetCurrent()->Global()->Set(v8::String::New("evalCount"),
                                           v8::Integer::New(0));
  v8::Handle<v8::String> source = v8::String::New("evalCount++; ({x: 5})");
  v8::Handle<v8::Object> lazy_object =
      ModuleSystem::CreateLazyObject("lazy.js", source);
  v8::Context::GetCurrent()->Global()->Set(v8::String::New("lazy"),
                                           lazy_object);
  RegisterModule("test",
      "var assert = requireNative('assert');"
      "assert.AssertTrue(evalCount == 0);"
      "lazy.x;"
      "assert.AssertTrue(evalCount == 1);"
      "lazy.x;"
      "assert.AssertTrue(evalCount == 1);");
  module_system_->Require("test");
}
