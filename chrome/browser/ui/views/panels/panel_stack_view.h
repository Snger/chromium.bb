// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_PANELS_PANEL_STACK_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_PANELS_PANEL_STACK_VIEW_H_

#include <list>
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/panels/native_panel_stack_window.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

class StackedPanelCollection;
class TaskbarWindowThumbnailerWin;

// A native window that acts as the owner of all panels in the stack, in order
// to make all panels appear as a single window on the taskbar or launcher.
class PanelStackView : public NativePanelStackWindow,
                       public views::WidgetObserver,
                       public views::WidgetDelegateView,
                       public views::WidgetFocusChangeListener {
 public:
  PanelStackView();
  virtual ~PanelStackView();

 protected:
  // Overridden from NativePanelStackWindow:
  virtual void Close() OVERRIDE;
  virtual void AddPanel(Panel* panel) OVERRIDE;
  virtual void RemovePanel(Panel* panel) OVERRIDE;
  virtual bool IsEmpty() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual void DrawSystemAttention(bool draw_attention) OVERRIDE;

 private:
  typedef std::list<Panel*> Panels;

  // Overridden from views::WidgetDelegate:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual gfx::ImageSkia GetWindowAppIcon() OVERRIDE;
  virtual gfx::ImageSkia GetWindowIcon() OVERRIDE;
  virtual views::Widget* GetWidget() OVERRIDE;
  virtual const views::Widget* GetWidget() const OVERRIDE;
  virtual void DeleteDelegate() OVERRIDE;

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetDestroying(views::Widget* widget) OVERRIDE;
  virtual void OnWidgetActivationChanged(views::Widget* widget,
                                         bool active) OVERRIDE;

  // Overridden from views::WidgetFocusChangeListener:
  virtual void OnNativeFocusChange(gfx::NativeView focused_before,
                                   gfx::NativeView focused_now) OVERRIDE;

  // Updates the owner of the underlying window such that multiple panels
  // stacked together could appear as a single window on the taskbar or
  // launcher.
  void UpdateWindowOwnerForTaskbarIconAppearance(Panel* panel);

  void EnsureWindowCreated();

  // Capture the thumbnail of the whole stack and provide it to live preview
  // (available since Windows 7).
  void CaptureThumbnailForLivePreview();

  // Is the taskbar icon of the underlying window being flashed in order to
  // draw the user's attention?
  bool is_drawing_attention_;

  views::Widget* window_;

  // Tracks all panels that are enclosed by this window.
  Panels panels_;

#if defined(OS_WIN)
  // Used to provide custom taskbar thumbnail for Windows 7 and later.
  scoped_ptr<TaskbarWindowThumbnailerWin> thumbnailer_;
#endif

  DISALLOW_COPY_AND_ASSIGN(PanelStackView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_PANELS_PANEL_STACK_VIEW_H_
