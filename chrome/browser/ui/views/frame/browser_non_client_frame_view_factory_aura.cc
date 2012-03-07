// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view_aura.h"

#include "ash/shell.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/panels/panel_browser_frame_view.h"
#include "chrome/browser/ui/panels/panel_browser_view.h"
#include "chrome/browser/ui/views/frame/app_non_client_frame_view_aura.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/compact_browser_frame_view.h"
#include "chrome/browser/ui/views/frame/opaque_browser_frame_view.h"
#include "chrome/browser/ui/views/frame/popup_non_client_frame_view.h"

namespace browser {

BrowserNonClientFrameView* CreateBrowserNonClientFrameView(
    BrowserFrame* frame, BrowserView* browser_view) {
  if (browser_view->IsPanel()) {
    return new PanelBrowserFrameView(
        frame, static_cast<PanelBrowserView*>(browser_view));
  }

  // Compact mode uses special opaque frames.
  if (ash::Shell::GetInstance()->IsWindowModeCompact())
    return new CompactBrowserFrameView(frame, browser_view);

  // If this is an app window and it's maximized, use the special frame_view.
  if (browser_view->browser()->is_app() && browser_view->IsMaximized())
    return new AppNonClientFrameViewAura(frame, browser_view);

  // Default is potentially translucent fancy frames.
  BrowserNonClientFrameViewAura* frame_view =
      new BrowserNonClientFrameViewAura(frame, browser_view);
  frame_view->Init();
  return frame_view;
}

}  // namespace browser
