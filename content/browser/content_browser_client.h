// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_CONTENT_BROWSER_CLIENT_H_
#define CONTENT_BROWSER_CONTENT_BROWSER_CLIENT_H_
#pragma once

#include <string>

#include "content/common/content_client.h"

class BrowserRenderProcessHost;
class CommandLine;
class GURL;
class Profile;
class RenderViewHost;
class TabContents;
class WorkerProcessHost;

namespace content {

class ResourceContext;
class WebUIFactory;

// Embedder API for participating in browser logic.
class ContentBrowserClient {
 public:
  // Notifies that a new RenderHostView has been created.
  virtual void RenderViewHostCreated(RenderViewHost* render_view_host);

  // Initialize a RenderViewHost before its CreateRenderView method is called.
  virtual void PreCreateRenderView(RenderViewHost* render_view_host,
                                   Profile* profile,
                                   const GURL& url);

  // Notifies that a BrowserRenderProcessHost has been created.
  virtual void BrowserRenderProcessHostCreated(BrowserRenderProcessHost* host);

  // Notifies that a WorkerProcessHost has been created.
  virtual void WorkerProcessHostCreated(WorkerProcessHost* host);

  // Gets the WebUIFactory which will be responsible for generating WebUIs.
  virtual WebUIFactory* GetWebUIFactory();

  // Get the effective URL for the given actual URL, to allow an embedder to
  // group different url schemes in the same SiteInstance.
  virtual GURL GetEffectiveURL(Profile* profile, const GURL& url);

  // See RenderViewHostDelegate's comment.
  virtual GURL GetAlternateErrorPageURL(const TabContents* tab);

  // See CharacterEncoding's comment.
  virtual std::string GetCanonicalEncodingNameByAliasName(
      const std::string& alias_name);

  // Allows the embedder to pass extra command line flags.
  // switches::kProcessType will already be set at this point.
  virtual void AppendExtraCommandLineSwitches(CommandLine* command_line,
                                              int child_process_id);

  // Returns the locale used by the application.
  virtual std::string GetApplicationLocale();

  // Allow the embedder to control if an AppCache can be used for the given url.
  // This is called on the IO thread.
  virtual bool AllowAppCache(const GURL& manifest_url,
                             const content::ResourceContext* context);

#if defined(OS_LINUX)
  // Can return an optional fd for crash handling, otherwise returns -1.
  virtual int GetCrashSignalFD(const std::string& process_type);
#endif
};

}  // namespace content

#endif  // CONTENT_BROWSER_CONTENT_BROWSER_CLIENT_H_
