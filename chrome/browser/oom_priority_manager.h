// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OOM_PRIORITY_MANAGER_H_
#define CHROME_BROWSER_OOM_PRIORITY_MANAGER_H_

#include <vector>

#include "base/hash_tables.h"
#include "base/process.h"
#include "base/string16.h"
#include "base/synchronization/lock.h"
#include "base/task.h"
#include "base/time.h"
#include "base/timer.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

namespace browser {

// The OomPriorityManager periodically checks (see
// ADJUSTMENT_INTERVAL_SECONDS in the source) the status of renderers
// and adjusts the out of memory (OOM) adjustment value (in
// /proc/<pid>/oom_score_adj) of the renderers so that they match the
// algorithm embedded here for priority in being killed upon OOM
// conditions.
//
// The algorithm used favors killing tabs that are not pinned, have
// been idle for longest, and take up the most memory, in that order
// of priority.  We round the idle times to the nearest few minutes
// (see BUCKET_INTERVAL_MINUTES in the source) so that we can bucket
// them, as no two tabs will have exactly the same idle time.
class OomPriorityManager : public NotificationObserver {
 public:
  OomPriorityManager();
  virtual ~OomPriorityManager();

  void Start();
  void Stop();

  // Returns list of tab titles sorted from most interesting (don't kill)
  // to least interesting (OK to kill).
  std::vector<string16> GetTabTitles();

 private:
  struct RendererStats {
    RendererStats();
    ~RendererStats();
    bool is_pinned;
    bool is_selected;
    base::TimeTicks last_selected;
    size_t memory_used;
    base::ProcessHandle renderer_handle;
    string16 title;
  };
  typedef std::vector<RendererStats> StatsList;
  typedef base::hash_map<base::ProcessHandle, int> ProcessScoreMap;

  // Posts DoAdjustOomPriorities task to the file thread.  Called when
  // the timer fires.
  void AdjustOomPriorities();

  // Called by AdjustOomPriorities.  Runs on the file thread.
  void DoAdjustOomPriorities();

  static bool CompareRendererStats(RendererStats first, RendererStats second);

  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  base::RepeatingTimer<OomPriorityManager> timer_;
  // renderer_stats_ is used on both UI and file threads.
  base::Lock renderer_stats_lock_;
  StatsList renderer_stats_;
  // map maintaining the process - oom_score map.
  base::Lock pid_to_oom_score_lock_;
  ProcessScoreMap pid_to_oom_score_;
  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(OomPriorityManager);
};

}  // namespace browser

DISABLE_RUNNABLE_METHOD_REFCOUNT(browser::OomPriorityManager);

#endif  // CHROME_BROWSER_OOM_PRIORITY_MANAGER_H_
