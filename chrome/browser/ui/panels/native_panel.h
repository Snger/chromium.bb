// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_H_
#define CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_H_
#pragma once

#include "ui/gfx/native_widget_types.h"

class Panel;

namespace gfx {
class Rect;
}  // namespace gfx

// An interface for a class that implements platform-specific behavior for panel
// windows. We use this interface for two reasons:
// 1. We don't want to use BrowserWindow as the interface between shared panel
//    code and platform-specific code. BrowserWindow has a lot of methods, most
//    of which we don't use and simply stub out with NOTIMPLEMENTED().
// 2. We need some additional methods that BrowserWindow doesn't provide, such
//    as MinimizePanel() and RestorePanel().
// Note that even though we don't use BrowserWindow directly, Windows and GTK+
// still use the BrowserWindow interface as part of their implementation so we
// use Panel in all the method names to avoid collisions.
class NativePanel {
  friend class Panel;

 protected:
  virtual ~NativePanel() {}

  virtual void ShowPanel() = 0;
  virtual void SetPanelBounds(const gfx::Rect& bounds) = 0;
  virtual void MinimizePanel() = 0;
  virtual void RestorePanel() = 0;
  virtual void ClosePanel() = 0;
  virtual void ActivatePanel() = 0;
  virtual void DeactivatePanel() = 0;
  virtual bool IsPanelActive() const = 0;
  virtual gfx::NativeWindow GetNativePanelHandle() = 0;
  virtual void UpdatePanelTitleBar() = 0;
  virtual void ShowTaskManagerForPanel() = 0;
  virtual void NotifyPanelOnUserChangedTheme() = 0;
  virtual void FlashPanelFrame() = 0;
  virtual void DestroyPanelBrowser() = 0;
};

#endif  // CHROME_BROWSER_UI_PANELS_NATIVE_PANEL_H_
