// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js_arg_list.h"

#include "base/json/json_writer.h"

namespace browser_sync {

JsArgList::JsArgList() : args_(new SharedListValue()) {}

JsArgList::JsArgList(ListValue* args)
    : args_(new SharedListValue(args)) {}

JsArgList::~JsArgList() {}

const ListValue& JsArgList::Get() const {
  return args_->Get();
}

std::string JsArgList::ToString() const {
  std::string str;
  base::JSONWriter::Write(&Get(), false, &str);
  return str;
}

JsArgList::SharedListValue::SharedListValue() {}

JsArgList::SharedListValue::SharedListValue(ListValue* list_value) {
  list_value_.Swap(list_value);
}

const ListValue& JsArgList::SharedListValue::Get() const {
  return list_value_;
}

JsArgList::SharedListValue::~SharedListValue() {}

}  // namespace browser_sync
