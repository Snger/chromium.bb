// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_WINDOW_H_
#define VIEWS_WINDOW_WINDOW_H_
#pragma once

#include "ui/gfx/native_widget_types.h"
#include "views/widget/widget.h"
#include "views/window/native_window_delegate.h"

namespace gfx {
class Font;
class Rect;
class Size;
}  // namespace gfx

namespace views {

class NativeWindow;
class Widget;
class WindowDelegate;

////////////////////////////////////////////////////////////////////////////////
// Window class
//
// Encapsulates window-like behavior. See WindowDelegate.
//
class Window : public Widget,
               public internal::NativeWindowDelegate {
 public:
  struct InitParams {
    // |window_delegate| cannot be NULL.
    explicit InitParams(WindowDelegate* window_delegate);

    WindowDelegate* window_delegate;
    gfx::NativeWindow parent_window;
    NativeWindow* native_window;
    Widget::InitParams widget_init_params;
  };

  Window();
  virtual ~Window();

  // Creates an instance of an object implementing this interface.
  // TODO(beng): create a version of this function that takes a NativeView, for
  //             constrained windows.
  static Window* CreateChromeWindow(gfx::NativeWindow parent,
                                    const gfx::Rect& bounds,
                                    WindowDelegate* window_delegate);

  // Returns the preferred size of the contents view of this window based on
  // its localized size data. The width in cols is held in a localized string
  // resource identified by |col_resource_id|, the height in the same fashion.
  // TODO(beng): This should eventually live somewhere else, probably closer to
  //             ClientView.
  static int GetLocalizedContentsWidth(int col_resource_id);
  static int GetLocalizedContentsHeight(int row_resource_id);
  static gfx::Size GetLocalizedContentsSize(int col_resource_id,
                                            int row_resource_id);

  // Initializes the window. Must be called before any post-configuration
  // operations are performed.
  void InitWindow(const InitParams& params);

  // Retrieves the window's bounds, including its frame.
  gfx::Rect GetBounds() const;

  // Retrieves the restored bounds for the window.
  gfx::Rect GetNormalBounds() const;

  // Like Show(), but does not activate the window.
  void ShowInactive();

  // Prevents the window from being rendered as deactivated the next time it is.
  // This state is reset automatically as soon as the window becomes activated
  // again. There is no ability to control the state through this API as this
  // leads to sync problems.
  void DisableInactiveRendering();

  // Toggles the enable state for the Close button (and the Close menu item in
  // the system menu).
  void EnableClose(bool enable);

  // Tell the window to update its title from the delegate.
  void UpdateWindowTitle();

  // Tell the window to update its icon from the delegate.
  void UpdateWindowIcon();

  // Overridden from Widget:
  virtual void Show() OVERRIDE;
  virtual void Close() OVERRIDE;

  WindowDelegate* window_delegate() {
    return const_cast<WindowDelegate*>(
        const_cast<const Window*>(this)->window_delegate());
  }
  const WindowDelegate* window_delegate() const {
    return reinterpret_cast<WindowDelegate*>(widget_delegate());
  }

  NativeWindow* native_window() { return native_window_; }

 protected:
  // Overridden from NativeWindowDelegate:
  virtual bool CanActivate() const OVERRIDE;
  virtual bool IsInactiveRenderingDisabled() const OVERRIDE;
  virtual void EnableInactiveRendering() OVERRIDE;
  virtual bool IsModal() const OVERRIDE;
  virtual bool IsDialogBox() const OVERRIDE;
  virtual gfx::Size GetMinimumSize() OVERRIDE;
  virtual int GetNonClientComponent(const gfx::Point& point) OVERRIDE;
  virtual bool ExecuteCommand(int command_id) OVERRIDE;
  virtual void OnNativeWindowCreated(const gfx::Rect& bounds) OVERRIDE;
  virtual void OnNativeWindowActivationChanged(bool active) OVERRIDE;
  virtual void OnNativeWindowBeginUserBoundsChange() OVERRIDE;
  virtual void OnNativeWindowEndUserBoundsChange() OVERRIDE;
  virtual void OnNativeWindowDestroying() OVERRIDE;
  virtual void OnNativeWindowBoundsChanged() OVERRIDE;
  virtual Window* AsWindow() OVERRIDE;
  virtual internal::NativeWidgetDelegate* AsNativeWidgetDelegate() OVERRIDE;

 private:
  // Sizes and positions the window just after it is created.
  void SetInitialBounds(const gfx::Rect& bounds);

  // Persists the window's restored position and maximized state using the
  // window delegate.
  void SaveWindowPosition();

  NativeWindow* native_window_;

  // The saved maximized state for this window. See note in SetInitialBounds
  // that explains why we save this.
  bool saved_maximized_state_;

  // The smallest size the window can be.
  gfx::Size minimum_size_;

  // True when the window should be rendered as active, regardless of whether
  // or not it actually is.
  bool disable_inactive_rendering_;

  // Set to true if the window is in the process of closing .
  bool window_closed_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace views

#endif  // #ifndef VIEWS_WINDOW_WINDOW_H_
