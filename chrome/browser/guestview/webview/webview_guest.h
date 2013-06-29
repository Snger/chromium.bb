// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GUESTVIEW_WEBVIEW_WEBVIEW_GUEST_H_
#define CHROME_BROWSER_GUESTVIEW_WEBVIEW_WEBVIEW_GUEST_H_

#include "base/observer_list.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/guestview/guestview.h"
#include "content/public/browser/web_contents_observer.h"

namespace extensions {
class ScriptExecutor;
}  // namespace extensions

// A WebViewGuest is a WebContentsObserver on the guest WebContents of a
// <webview> tag. It provides the browser-side implementation of the <webview>
// API and manages the lifetime of <webview> extension events. WebViewGuest is
// created on attachment. That is, when a guest WebContents is associated with
// a particular embedder WebContents. This happens on either initial navigation
// or through the use of the New Window API, when a new window is attached to
// a particular <webview>.
class WebViewGuest : public GuestView,
                     public content::WebContentsObserver {
 public:
  explicit WebViewGuest(content::WebContents* guest_web_contents);

  static WebViewGuest* From(int embedder_process_id, int instance_id);

  // GuestView implementation.
  virtual void Attach(content::WebContents* embedder_web_contents,
                      const std::string& extension_id,
                      int view_instance_id,
                      const base::DictionaryValue& args) OVERRIDE;
  virtual GuestView::Type GetViewType() const OVERRIDE;
  virtual WebViewGuest* AsWebView() OVERRIDE;
  virtual AdViewGuest* AsAdView() OVERRIDE;

  // If possible, navigate the guest to |relative_index| entries away from the
  // current navigation entry.
  void Go(int relative_index);

  extensions::ScriptExecutor* script_executor() {
    return script_executor_.get();
  }

 private:
  virtual ~WebViewGuest();

  virtual void DidCommitProvisionalLoadForFrame(
      int64 frame_id,
      bool is_main_frame,
      const GURL& url,
      content::PageTransition transition_type,
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void DidStopLoading(
      content::RenderViewHost* render_view_host) OVERRIDE;
  virtual void WebContentsDestroyed(
      content::WebContents* web_contents) OVERRIDE;

  void AddWebViewToExtensionRendererState();
  static void RemoveWebViewFromExtensionRendererState(
      content::WebContents* web_contents);

  ObserverList<extensions::TabHelper::ScriptExecutionObserver>
      script_observers_;
  scoped_ptr<extensions::ScriptExecutor> script_executor_;

  DISALLOW_COPY_AND_ASSIGN(WebViewGuest);
};

#endif  // CHROME_BROWSER_GUESTVIEW_WEBVIEW_WEBVIEW_GUEST_H_
