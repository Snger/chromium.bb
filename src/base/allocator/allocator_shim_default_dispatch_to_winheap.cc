// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_shim.h"

#include "base/allocator/winheap_stubs_win.h"
#include "base/logging.h"
#include <windows.h>

__int64 allocator_shim_counter = 0;

namespace {

using base::allocator::AllocatorDispatch;

void* DefaultWinHeapMallocImpl(const AllocatorDispatch*, size_t size) {
  void* ptr = base::allocator::WinHeapMalloc(size);

  if (ptr) {
    ::InterlockedAdd64(
	    &allocator_shim_counter,
	    base::allocator::WinHeapGetSizeEstimateFromUserSize(size));
  }
  return ptr;
}

void* DefaultWinHeapCallocImpl(const AllocatorDispatch* self,
                               size_t n,
                               size_t elem_size) {
  // Overflow check.
  const size_t size = n * elem_size;
  if (elem_size != 0 && size / elem_size != n)
    return nullptr;

  void* result = DefaultWinHeapMallocImpl(self, size);
  if (result) {
    memset(result, 0, size);
  }
  return result;
}

void* DefaultWinHeapMemalignImpl(const AllocatorDispatch* self,
                                 size_t alignment,
                                 size_t size) {
  CHECK(false) << "The windows heap does not support memalign.";
  return nullptr;
}

void* DefaultWinHeapReallocImpl(const AllocatorDispatch* self,
                                void* address,
                                size_t size) {
  size_t old_size = 0;
  if (address) {
    old_size = base::allocator::WinHeapGetSizeEstimate(address);
  }

  void* new_address = base::allocator::WinHeapRealloc(address, size);
  if (new_address) {
    if (size >= old_size) {
      ::InterlockedAdd64(&allocator_shim_counter, size - old_size);
    }
    else {
      ::InterlockedAdd64(
        &allocator_shim_counter,
        -static_cast<LONG64>(old_size - size));
    }
  }
  return new_address;
}

void DefaultWinHeapFreeImpl(const AllocatorDispatch*, void* address) {
  if (address) {
    size_t size = base::allocator::WinHeapGetSizeEstimate(address);
    ::InterlockedAdd64(&allocator_shim_counter, -static_cast<LONG64>(size));
  }
  base::allocator::WinHeapFree(address);
}

size_t DefaultWinHeapGetSizeEstimateImpl(const AllocatorDispatch*,
                                         void* address) {
  return base::allocator::WinHeapGetSizeEstimate(address);
}

}  // namespace

const AllocatorDispatch AllocatorDispatch::default_dispatch = {
    &DefaultWinHeapMallocImpl,
    &DefaultWinHeapCallocImpl,
    &DefaultWinHeapMemalignImpl,
    &DefaultWinHeapReallocImpl,
    &DefaultWinHeapFreeImpl,
    &DefaultWinHeapGetSizeEstimateImpl,
    nullptr, /* next */
};
