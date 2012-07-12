// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/gdata/gdata_test_util.h"

#include "base/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "content/public/browser/browser_thread.h"

namespace gdata {
namespace test_util {

// This class is used to monitor if any task is posted to a message loop.
class TaskObserver : public MessageLoop::TaskObserver {
 public:
  TaskObserver() : posted_(false) {}
  virtual ~TaskObserver() {}

  // MessageLoop::TaskObserver overrides.
  virtual void WillProcessTask(base::TimeTicks time_posted) {}
  virtual void DidProcessTask(base::TimeTicks time_posted) {
    posted_ = true;
  }

  // Returns true if any task was posted.
  bool posted() const { return posted_; }

 private:
  bool posted_;
  DISALLOW_COPY_AND_ASSIGN(TaskObserver);
};

void RunBlockingPoolTask() {
  while (true) {
    content::BrowserThread::GetBlockingPool()->FlushForTesting();

    TaskObserver task_observer;
    MessageLoop::current()->AddTaskObserver(&task_observer);
    MessageLoop::current()->RunAllPending();
    MessageLoop::current()->RemoveTaskObserver(&task_observer);
    if (!task_observer.posted())
      break;
  }
}

GDataCacheEntry ToCacheEntry(int cache_state) {
  GDataCacheEntry cache_entry;
  cache_entry.SetPresent(cache_state & CACHE_STATE_PRESENT);
  cache_entry.SetPinned(cache_state & CACHE_STATE_PINNED);
  cache_entry.SetDirty(cache_state & CACHE_STATE_DIRTY);
  cache_entry.SetMounted(cache_state & CACHE_STATE_MOUNTED);
  cache_entry.SetPersistent(cache_state & CACHE_STATE_PERSISTENT);
  return cache_entry;
}

bool CacheStatesEqual(const GDataCacheEntry& a, const GDataCacheEntry& b) {
  return (a.IsPresent() == b.IsPresent() &&
          a.IsPinned() == b.IsPinned() &&
          a.IsDirty() == b.IsDirty() &&
          a.IsMounted() == b.IsMounted() &&
          a.IsPersistent() == b.IsPersistent());
}

}  // namespace test_util
}  // namespace gdata
