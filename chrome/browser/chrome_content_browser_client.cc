// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_content_browser_client.h"

#include "chrome/browser/character_encoding.h"
#include "chrome/browser/debugger/devtools_handler.h"
#include "chrome/browser/desktop_notification_handler.h"
#include "chrome/browser/extensions/extension_message_handler.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/printing/printing_message_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_host/chrome_render_message_filter.h"
#include "chrome/browser/renderer_host/chrome_render_view_host_observer.h"
#include "chrome/browser/search_engines/search_provider_install_state_message_filter.h"
#include "chrome/browser/spellcheck_message_filter.h"
#include "chrome/browser/ui/webui/chrome_web_ui_factory.h"
#include "chrome/common/pref_names.h"
#include "content/browser/renderer_host/browser_render_process_host.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"

namespace chrome {

void ChromeContentBrowserClient::RenderViewHostCreated(
    RenderViewHost* render_view_host) {
  new ChromeRenderViewHostObserver(render_view_host);
  new DesktopNotificationHandler(render_view_host);
  new DevToolsHandler(render_view_host);
  new ExtensionMessageHandler(render_view_host);
}

void ChromeContentBrowserClient::PreCreateRenderView(
    RenderViewHost* render_view_host,
    Profile* profile,
    const GURL& url) {
  // Tell the RenderViewHost whether it will be used for an extension process.
  ExtensionService* service = profile->GetExtensionService();
  if (service) {
    bool is_extension_process = service->ExtensionBindingsAllowed(url);
    render_view_host->set_is_extension_process(is_extension_process);

    const Extension* installed_app = service->GetInstalledApp(url);
    if (installed_app) {
      service->SetInstalledAppForRenderer(
          render_view_host->process()->id(), installed_app);
    }
  }
}

void ChromeContentBrowserClient::BrowserRenderProcessHostCreated(
    BrowserRenderProcessHost* host) {
  host->channel()->AddFilter(new ChromeRenderMessageFilter(
      host->id(),
      host->profile(),
      host->profile()->GetRequestContextForRenderProcess(host->id())));
  host->channel()->AddFilter(new PrintingMessageFilter());
  host->channel()->AddFilter(
      new SearchProviderInstallStateMessageFilter(host->id(), host->profile()));
  host->channel()->AddFilter(new SpellCheckMessageFilter());
}

content::WebUIFactory* ChromeContentBrowserClient::GetWebUIFactory() {
  return ChromeWebUIFactory::GetInstance();
}

GURL ChromeContentBrowserClient::GetEffectiveURL(Profile* profile,
                                                 const GURL& url) {
  // Get the effective URL for the given actual URL. If the URL is part of an
  // installed app, the effective URL is an extension URL with the ID of that
  // extension as the host. This has the effect of grouping apps together in
  // a common SiteInstance.
  if (!profile || !profile->GetExtensionService())
    return url;

  const Extension* extension =
      profile->GetExtensionService()->GetExtensionByWebExtent(url);
  if (!extension)
    return url;

  // If the URL is part of an extension's web extent, convert it to an
  // extension URL.
  return extension->GetResourceURL(url.path());
}

GURL ChromeContentBrowserClient::GetAlternateErrorPageURL(
    const TabContents* tab) {
  GURL url;
  // Disable alternate error pages when in OffTheRecord/Incognito mode.
  if (tab->profile()->IsOffTheRecord())
    return url;

  PrefService* prefs = tab->profile()->GetPrefs();
  DCHECK(prefs);
  if (prefs->GetBoolean(prefs::kAlternateErrorPagesEnabled)) {
    url = google_util::AppendGoogleLocaleParam(
        GURL(google_util::kLinkDoctorBaseURL));
    url = google_util::AppendGoogleTLDParam(url);
  }
  return url;
}

std::string ChromeContentBrowserClient::GetCanonicalEncodingNameByAliasName(
    const std::string& alias_name) {
  return CharacterEncoding::GetCanonicalEncodingNameByAliasName(alias_name);
}

}  // namespace chrome
