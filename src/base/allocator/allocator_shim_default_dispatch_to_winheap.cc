// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_shim.h"

#include "base/allocator/winheap_stubs_win.h"
#include "base/logging.h"
#include "base/synchronization/lock.h"
#include <windows.h>
#include <set>

__int64 allocator_shim_counter = 0;

namespace {

using base::allocator::AllocatorDispatch;

#if defined(STRICT_ALLOC_COUNTER)
#define MAX_POINTER_SET_SIZE 65536

static base::Lock g_lock;
static void *g_pointer_set[MAX_POINTER_SET_SIZE];
static size_t g_pointer_set_size;

static inline void RegisterPointer(void *ptr) {
  g_pointer_set[g_pointer_set_size] = ptr;
  ++g_pointer_set_size;
  if (g_pointer_set_size >= MAX_POINTER_SET_SIZE) {
    *((int*)0x0) = 0x1;
  }
}

static inline void UnregisterPointer(void *ptr) {
  for (size_t i=0; i<g_pointer_set_size; ++i) {
    if (g_pointer_set[i] == ptr) {
      // found
      --g_pointer_set_size;
      for (; i<g_pointer_set_size; ++i) {
        g_pointer_set[i] = g_pointer_set[i+1];
      }
      return;
    }
  }

  // Crash the process. We cannot use any assertion constructs here because
  // the assert macros internally use the logger and the logger.  The logger
  // may attempt to allocate memory, which would trigger a deadlock.
  *((int*)0x0) = 0x1;
}
#endif

void* DefaultWinHeapMallocImpl(const AllocatorDispatch*,
                               size_t size,
                               void* context) {
  void* ptr = base::allocator::WinHeapMalloc(size);

  if (ptr) {
#if defined(STRICT_ALLOC_COUNTER)
    {
      base::AutoLock autoLock(g_lock);
      RegisterPointer(ptr);
    }
#endif
    ::InterlockedAdd64(
	    &allocator_shim_counter,
	    base::allocator::WinHeapGetSizeEstimateFromUserSize(size));
  }
  return ptr;  
}

void* DefaultWinHeapCallocImpl(const AllocatorDispatch* self,
                               size_t n,
                               size_t elem_size,
                               void* context) {
  // Overflow check.
  const size_t size = n * elem_size;
  if (elem_size != 0 && size / elem_size != n)
    return nullptr;

  void* result = DefaultWinHeapMallocImpl(self, size, context);
  if (result) {
    memset(result, 0, size);
  }
  return result;
}

void* DefaultWinHeapMemalignImpl(const AllocatorDispatch* self,
                                 size_t alignment,
                                 size_t size,
                                 void* context) {
  CHECK(false) << "The windows heap does not support memalign.";
  return nullptr;
}

void* DefaultWinHeapReallocImpl(const AllocatorDispatch* self,
                                void* address,
                                size_t size,
                                void* context) {
  size_t old_size = 0;
  if (address) {
#if defined(STRICT_ALLOC_COUNTER)
    {
      base::AutoLock autoLock(g_lock);
      UnregisterPointer(address);
    }
#endif

    old_size = base::allocator::WinHeapGetSizeEstimate(address);
  }

  void* new_address = base::allocator::WinHeapRealloc(address, size);
  if (new_address) {
#if defined(STRICT_ALLOC_COUNTER)
    {
      base::AutoLock autoLock(g_lock);
      RegisterPointer(new_address);
    }
#endif
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

void DefaultWinHeapFreeImpl(const AllocatorDispatch*,
                            void* address,
                            void* context) {
  if (address) {
#if defined(STRICT_ALLOC_COUNTER)
    {
      base::AutoLock autoLock(g_lock);
      UnregisterPointer(address);
    }
#endif
    size_t size = base::allocator::WinHeapGetSizeEstimate(address);
    ::InterlockedAdd64(&allocator_shim_counter, -static_cast<LONG64>(size));
  }
  base::allocator::WinHeapFree(address);  
}

size_t DefaultWinHeapGetSizeEstimateImpl(const AllocatorDispatch*,
                                         void* address,
                                         void* context) {
  return base::allocator::WinHeapGetSizeEstimate(address);
}

}  // namespace

// Guarantee that default_dispatch is compile-time initialized to avoid using
// it before initialization (allocations before main in release builds with
// optimizations disabled).
constexpr AllocatorDispatch AllocatorDispatch::default_dispatch = {
    &DefaultWinHeapMallocImpl,
    &DefaultWinHeapCallocImpl,
    &DefaultWinHeapMemalignImpl,
    &DefaultWinHeapReallocImpl,
    &DefaultWinHeapFreeImpl,
    &DefaultWinHeapGetSizeEstimateImpl,
    nullptr, /* batch_malloc_function */
    nullptr, /* batch_free_function */
    nullptr, /* free_definite_size_function */
    nullptr, /* next */
};
