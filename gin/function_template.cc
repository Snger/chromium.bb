// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gin/function_template.h"

#include "gin/per_isolate_data.h"

namespace gin {

WrapperInfo internal::CallbackHolderBase::kWrapperInfo = { kEmbedderNativeGin };

WrapperInfo* internal::CallbackHolderBase::GetWrapperInfo() {
  return &kWrapperInfo;
}

void InitFunctionTemplates(PerIsolateData* isolate_data) {
  if (!isolate_data->GetObjectTemplate(
          &internal::CallbackHolderBase::kWrapperInfo).IsEmpty()) {
    return;
  }

  v8::Handle<v8::ObjectTemplate> templ(v8::ObjectTemplate::New());
  templ->SetInternalFieldCount(kNumberOfInternalFields);
  isolate_data->SetObjectTemplate(&internal::CallbackHolderBase::kWrapperInfo,
                                  templ);
}

}  // namespace gin
