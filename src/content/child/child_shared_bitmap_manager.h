// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_CHILD_SHARED_BITMAP_MANAGER_H_
#define CONTENT_CHILD_CHILD_SHARED_BITMAP_MANAGER_H_

#include <stdint.h>

#include <memory>

#include "base/containers/hash_tables.h"
#include "base/hash.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/shared_memory.h"
#include "cc/resources/shared_bitmap_manager.h"
#include "content/common/content_export.h"
#include "content/child/thread_safe_sender.h"

namespace BASE_HASH_NAMESPACE {
template <>
struct hash<cc::SharedBitmapId> {
  size_t operator()(const cc::SharedBitmapId& id) const {
    return base::Hash(reinterpret_cast<const char*>(id.name), sizeof(id.name));
  }
};
}  // namespace BASE_HASH_NAMESPACE

namespace content {

class SharedMemoryBitmap : public cc::SharedBitmap {
 public:
  base::SharedMemory* shared_memory() { return shared_memory_; }

 protected:
  SharedMemoryBitmap(uint8_t* pixels,
                     const cc::SharedBitmapId& id,
                     base::SharedMemory* shared_memory);

  base::SharedMemory* shared_memory_;
};

class CONTENT_EXPORT ChildSharedBitmapManager : public cc::SharedBitmapManager {
 public:
  ChildSharedBitmapManager(scoped_refptr<ThreadSafeSender> sender);
  ~ChildSharedBitmapManager() override;

  // cc::SharedBitmapManager implementation.
  std::unique_ptr<cc::SharedBitmap> AllocateSharedBitmap(
      const gfx::Size& size) override;
  std::unique_ptr<cc::SharedBitmap> GetSharedBitmapFromId(
      const gfx::Size&,
      const cc::SharedBitmapId&) override;

  std::unique_ptr<cc::SharedBitmap> GetBitmapForSharedMemory(
      base::SharedMemory* mem);
  std::unique_ptr<SharedMemoryBitmap> AllocateSharedMemoryBitmap(
      const gfx::Size& size);

  void FreeSharedMemoryFromMap(const cc::SharedBitmapId& id);

 private:
  scoped_refptr<ThreadSafeSender> sender_;

  typedef base::hash_map<cc::SharedBitmapId, base::SharedMemory* >
      SharedMemoryMap;
  SharedMemoryMap shared_memory_map_;

  DISALLOW_COPY_AND_ASSIGN(ChildSharedBitmapManager);
};

}  // namespace content

#endif  // CONTENT_CHILD_CHILD_SHARED_BITMAP_MANAGER_H_
