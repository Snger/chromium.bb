// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "gin/arguments.h"
#include "gin/gin.h"
#include "gin/per_isolate_data.h"
#include "gin/test/v8_test.h"
#include "gin/wrappable.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gin {
namespace {

class MyObject : public Wrappable {
 public:
  static scoped_refptr<MyObject> Create();

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

scoped_refptr<MyObject> MyObject::Create() {
  return make_scoped_refptr(new MyObject());
}

WrapperInfo* MyObject::GetWrapperInfo() {
  return &kWrapperInfo;
}

}  // namespace

template<>
struct Converter<MyObject*> : public WrappableConverter<MyObject> {};

namespace {

// TODO(abarth): This is too much typing.

void MyObjectGetValue(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Arguments args(info);

  MyObject* obj = 0;
  CHECK(args.Holder(&obj));

  args.Return(obj->value());
}

void MyObjectSetValue(const v8::FunctionCallbackInfo<v8::Value>& info) {
  Arguments args(info);

  MyObject* obj = 0;
  CHECK(args.Holder(&obj));

  int val = 0;
  if (!args.GetNext(&val))
    return args.ThrowError();

  obj->set_value(val);
}

void RegisterTemplate(v8::Isolate* isolate) {
  PerIsolateData* data = PerIsolateData::From(isolate);
  DCHECK(data->GetObjectTemplate(&MyObject::kWrapperInfo).IsEmpty());

  v8::Handle<v8::ObjectTemplate> templ = v8::ObjectTemplate::New();
  templ->SetInternalFieldCount(kNumberOfInternalFields);
  templ->SetAccessorProperty(
      StringToSymbol(isolate, "value"),
      v8::FunctionTemplate::New(MyObjectGetValue),
      v8::FunctionTemplate::New(MyObjectSetValue));

  data->SetObjectTemplate(&MyObject::kWrapperInfo, templ);
}

typedef V8Test WrappableTest;

TEST_F(WrappableTest, WrapAndUnwrap) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  RegisterTemplate(isolate);
  scoped_refptr<MyObject> obj = MyObject::Create();

  v8::Handle<v8::Value> wrapper = ConvertToV8(isolate, obj.get());
  EXPECT_FALSE(wrapper.IsEmpty());

  MyObject* unwrapped = 0;
  EXPECT_TRUE(ConvertFromV8(wrapper, &unwrapped));
  EXPECT_EQ(obj, unwrapped);
}

TEST_F(WrappableTest, GetAndSetProperty) {
  v8::Isolate* isolate = instance_->isolate();
  v8::HandleScope handle_scope(isolate);

  RegisterTemplate(isolate);
  scoped_refptr<MyObject> obj = MyObject::Create();

  obj->set_value(42);
  EXPECT_EQ(42, obj->value());

  v8::Handle<v8::String> source = StringToV8(isolate,
      "(function (obj) {"
      "   if (obj.value !== 42) throw 'FAIL';"
      "   else obj.value = 191; })");
  EXPECT_FALSE(source.IsEmpty());

  v8::TryCatch try_catch;
  v8::Handle<v8::Script> script = v8::Script::New(source);
  EXPECT_FALSE(script.IsEmpty());
  v8::Handle<v8::Value> val = script->Run();
  EXPECT_FALSE(val.IsEmpty());
  v8::Handle<v8::Function> func;
  EXPECT_TRUE(ConvertFromV8(val, &func));
  v8::Handle<v8::Value> argv[] = {
    ConvertToV8(isolate, obj.get()),
  };
  func->Call(v8::Undefined(isolate), 1, argv);

  EXPECT_EQ(191, obj->value());
}

}  // namespace
}  // namespace gin
