// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_EVENT_FILTER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_EVENT_FILTER_H_
#pragma once

#include "ash/wm/toplevel_window_event_filter.h"
#include "ui/aura/window_observer.h"

namespace aura {
class MouseEvent;
class Window;
}

namespace ash {
namespace internal {

class WorkspaceEventFilter : public ToplevelWindowEventFilter,
                             public aura::WindowObserver {
 public:
  explicit WorkspaceEventFilter(aura::Window* owner);
  virtual ~WorkspaceEventFilter();

  // Overridden from ToplevelWindowEventFilter:
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;

  // Overriden from WindowObserver:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

 protected:
  // Overriden from ToplevelWindowEventFilter:
  virtual WindowResizer* CreateWindowResizer(aura::Window* window,
                                             const gfx::Point& point,
                                             int window_component) OVERRIDE;

 private:
  // Updates the top-level window under the mouse so that we can change
  // the look of the caption area based on mouse-hover.
  void UpdateHoveredWindow(aura::Window* toplevel);

  // Top-level window under the mouse cursor.
  aura::Window* hovered_window_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceEventFilter);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_EVENT_FILTER_H_
