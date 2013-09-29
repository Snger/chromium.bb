// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_STATE_OBSERVER_H_
#define ASH_WM_WINDOW_STATE_OBSERVER_H_

#include "ash/wm/wm_types.h"

namespace ash {
namespace wm {
class WindowState;

class ASH_EXPORT WindowStateObserver {
 public:
  // Called when the tracked_by_workspace has changed.
  virtual void OnTrackedByWorkspaceChanged(
      WindowState* window,
      bool old_value) {}

  // Called when the window's show type has changed.
  // This is different from kWindowShowStatekey property change
  // as this will be invoked when the window gets left/right maximized,
  // and auto positioned.
  virtual void OnWindowShowTypeChanged(WindowState* window_state,
                                       WindowShowType type) {}
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_WINDOW_STATE_OBSERVER_H_
