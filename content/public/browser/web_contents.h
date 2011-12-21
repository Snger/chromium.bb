// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
#define CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
#pragma once

#include "base/string16.h"
#include "content/common/content_export.h"

class NavigationController;
class RenderViewHost;
class RenderWidgetHostView;
// TODO(jam): of course we will have to rename TabContentsView etc to use
// WebContents.
class TabContentsView;

namespace base {
class PropertyBag;
}

namespace content {

class BrowserContext;
class RenderProcessHost;
class WebContentsDelegate;

// Describes what goes in the main content area of a tab.
class WebContents {
 public:
  // Intrinsic tab state -------------------------------------------------------

  // Returns the property bag for this tab contents, where callers can add
  // extra data they may wish to associate with the tab. Returns a pointer
  // rather than a reference since the PropertyAccessors expect this.
  virtual const base::PropertyBag* GetPropertyBag() const = 0;
  virtual base::PropertyBag* GetPropertyBag() = 0;

  // Gets/Sets the delegate.
  virtual WebContentsDelegate* GetDelegate() = 0;
  virtual void SetDelegate(WebContentsDelegate* delegate) = 0;

  // Gets the controller for this tab contents.
  virtual NavigationController& GetController() = 0;
  virtual const NavigationController& GetController() const = 0;

  // Returns the user browser context associated with this WebContents (via the
  // NavigationController).
  virtual content::BrowserContext* GetBrowserContext() const = 0;

  // Allows overriding the type of this tab.
  virtual void SetViewType(content::ViewType type) = 0;

  // Return the currently active RenderProcessHost and RenderViewHost. Each of
  // these may change over time.
  virtual RenderProcessHost* GetRenderProcessHost() const = 0;

  // Gets the current RenderViewHost for this tab.
  virtual RenderViewHost* GetRenderViewHost() const = 0;

  // Returns the currently active RenderWidgetHostView. This may change over
  // time and can be NULL (during setup and teardown).
  virtual RenderWidgetHostView* GetRenderWidgetHostView() const = 0;

  // The TabContentsView will never change and is guaranteed non-NULL.
  virtual TabContentsView* GetView() const = 0;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_WEB_CONTENTS_H_
