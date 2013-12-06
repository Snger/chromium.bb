// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/handle.h"
#include "gin/object_template_builder.h"
#include "gin/per_isolate_data.h"
#include "gin/public/isolate_holder.h"
#include "gin/test/v8_test.h"
#include "gin/try_catch.h"
#include "gin/wrappable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gin {
namespace {

class MyObject : public Wrappable {
 public:
  static gin::Handle<MyObject> Create(v8::Isolate* isolate);

  int value() const { return value_; }
  void set_value(int value) { value_ = value; }

  static WrapperInfo kWrapperInfo;
  virtual WrapperInfo* GetWrapperInfo() OVERRIDE;

 private:
  MyObject() : value_(0) {}
  virtual ~MyObject() {}

  int value_;
};

WrapperInfo MyObject::kWrapperInfo = { kEmbedderNativeGin };

gin::Handle<MyObject> MyObject::Create(v8::Isolate* isolate) {
  return CreateHandle(isolate, new MyObject());
}

WrapperInfo* MyObject::GetWrapperInfo() {
  return &kWrapperInfo;
}

}  // namespace

template<>
struct Converter<MyObject*> : public WrappableConverter<MyObject> {};

namespace {

void RegisterTemplate(v8::Isolate* isolate) {
  PerIsolateData* data = PerIsolateData::From(isolate);
  DCHECK(data->GetObjectTemplate(&MyObject::kWrapperInfo).IsEmpty());

  v8::Handle<v8::ObjectTemplate> templ = ObjectTemplateBuilder(isolate)
      .SetProperty("value", &MyObject::value, &MyObject::set_value)
      .Build();
  templ->SetInternalFieldCount(kNumberOfInternalFields);
  data->SetObjectTemplate(&MyObject::kWrapperInfo, templ);
}

typedef V8Test WrappableTest;

TEST_F(WrappableTest, WrapAndUnwrap) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  RegisterTemplate(isolate);
  Handle<MyObject> obj = MyObject::Create(isolate);

  v8::Handle<v8::Value> wrapper = ConvertToV8(isolate, obj.get());
  EXPECT_FALSE(wrapper.IsEmpty());

  MyObject* unwrapped = 0;
  EXPECT_TRUE(ConvertFromV8(isolate, wrapper, &unwrapped));
  EXPECT_EQ(obj.get(), unwrapped);
}

TEST_F(WrappableTest, GetAndSetProperty) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  RegisterTemplate(isolate);
  gin::Handle<MyObject> obj = MyObject::Create(isolate);

  obj->set_value(42);
  EXPECT_EQ(42, obj->value());

  v8::Handle<v8::String> source = StringToV8(isolate,
      "(function (obj) {"
      "   if (obj.value !== 42) throw 'FAIL';"
      "   else obj.value = 191; })");
  EXPECT_FALSE(source.IsEmpty());

  gin::TryCatch try_catch;
  v8::Handle<v8::Script> script = v8::Script::New(source);
  EXPECT_FALSE(script.IsEmpty());
  v8::Handle<v8::Value> val = script->Run();
  EXPECT_FALSE(val.IsEmpty());
  v8::Handle<v8::Function> func;
  EXPECT_TRUE(ConvertFromV8(isolate, val, &func));
  v8::Handle<v8::Value> argv[] = {
    ConvertToV8(isolate, obj.get()),
  };
  func->Call(v8::Undefined(isolate), 1, argv);
  EXPECT_FALSE(try_catch.HasCaught());
  EXPECT_EQ("", try_catch.GetStackTrace());

  EXPECT_EQ(191, obj->value());
}

}  // namespace
}  // namespace gin
