// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_EVENT_FILTER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_EVENT_FILTER_H_

#include "ash/wm/toplevel_window_event_filter.h"
#include "ash/wm/workspace/multi_window_resize_controller.h"

namespace aura {
class Window;
}

namespace ash {
namespace internal {

class WorkspaceEventFilterTestHelper;

class WorkspaceEventFilter : public ToplevelWindowEventFilter {
 public:
  explicit WorkspaceEventFilter(aura::Window* owner);
  virtual ~WorkspaceEventFilter();

  // Overridden from ToplevelWindowEventFilter:
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   ui::MouseEvent* event) OVERRIDE;
  virtual ui::EventResult PreHandleGestureEvent(
      aura::Window* target,
      ui::GestureEvent* event) OVERRIDE;

 protected:
  // Overridden from ToplevelWindowEventFilter:
  virtual WindowResizer* CreateWindowResizer(
      aura::Window* window,
      const gfx::Point& point_in_parent,
      int window_component) OVERRIDE;

 private:
  friend class WorkspaceEventFilterTestHelper;

  // Determines if |event| corresponds to a double click on either the top or
  // bottom vertical resize edge, and if so toggles the vertical height of the
  // window between its restored state and the full available height of the
  // workspace.
  void HandleVerticalResizeDoubleClick(aura::Window* target,
                                       ui::MouseEvent* event);

  MultiWindowResizeController multi_window_resize_controller_;

  // If non-NULL, set to true in the destructor.
  bool* destroyed_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceEventFilter);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_EVENT_FILTER_H_
