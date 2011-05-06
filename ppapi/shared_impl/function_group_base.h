// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_FUNCTION_GROUP_BASE_H_
#define PPAPI_SHARED_IMPL_FUNCTION_GROUP_BASE_H_

namespace ppapi {

namespace thunk {
class ResourceCreationAPI;
}

namespace shared_impl {

class FunctionGroupBase {
 public:
  // Dynamic casting for this object. Returns the pointer to the given type if
  // it's supported.
  virtual thunk::ResourceCreationAPI* AsResourceCreation() { return NULL; }

  template <typename T> T* GetAs() { return NULL; }
};

template<>
inline thunk::ResourceCreationAPI* FunctionGroupBase::GetAs() {
  return AsResourceCreation();
}

}  // namespace shared_impl
}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_FUNCTION_GROUP_BASE_H_
