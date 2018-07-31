/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include <blpwtk2_contentbrowserclientimpl.h>

#include <blpwtk2_browsercontextimpl.h>
#include <blpwtk2_statics.h>
#include <blpwtk2_urlrequestcontextgetterimpl.h>
#include <blpwtk2_webcontentsviewdelegateimpl.h>
#include <blpwtk2_devtoolsmanagerdelegateimpl.h>
#include <blpwtk2_webviewimpl.h>
#include <blpwtk2_processhostimpl.h>

#include <base/message_loop/message_loop.h>
#include <base/threading/thread.h>
#include <base/threading/platform_thread.h>
#include <content/public/browser/browser_main_parts.h>
#include <content/public/browser/render_view_host.h>
#include <content/public/browser/render_process_host.h>
#include <content/public/browser/resource_dispatcher_host.h>
#include <content/public/browser/resource_dispatcher_host_delegate.h>
#include <content/public/browser/resource_request_info.h>
#include <content/public/browser/web_contents.h>
#include <content/public/common/url_constants.h>
#include <chrome/browser/printing/printing_message_filter.h>

namespace blpwtk2 {
namespace {

                            // ====================================
                            // class ResourceDispatcherHostDelegate
                            // ====================================

class ResourceDispatcherHostDelegate : public content::ResourceDispatcherHostDelegate
{
    ResourceDispatcherHostDelegate() = default;
    DISALLOW_COPY_AND_ASSIGN(ResourceDispatcherHostDelegate);

  public:
    static ResourceDispatcherHostDelegate& Get();
};

                            // ------------------------------------
                            // class ResourceDispatcherHostDelegate
                            // ------------------------------------

ResourceDispatcherHostDelegate& ResourceDispatcherHostDelegate::Get()
{
    static ResourceDispatcherHostDelegate instance;
    return instance;
}

}  // close unnamed namespace


                        // ------------------------------
                        // class ContentBrowserClientImpl
                        // ------------------------------

ContentBrowserClientImpl::ContentBrowserClientImpl()
{
}

ContentBrowserClientImpl::~ContentBrowserClientImpl()
{
}

content::BrowserMainParts* ContentBrowserClientImpl::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters)
{
    return new content::BrowserMainParts{};
}

void ContentBrowserClientImpl::RenderProcessWillLaunch(
    content::RenderProcessHost *host)
{
    DCHECK(Statics::isInBrowserMainThread());

    int id = host->GetID();
    host->AddFilter(new printing::PrintingMessageFilter(id));
}

void ContentBrowserClientImpl::OverrideWebkitPrefs(
    content::RenderViewHost *render_view_host,
    content::WebPreferences *prefs)
{
    content::WebContents* webContents =
        content::WebContents::FromRenderViewHost(render_view_host);
    DCHECK(webContents);

    WebViewImpl* webViewImpl =
        static_cast<WebViewImpl*>(webContents->GetDelegate());
    DCHECK(webViewImpl);

    webViewImpl->overrideWebkitPrefs(prefs);
}

bool ContentBrowserClientImpl::SupportsInProcessRenderer()
{
    return Statics::isInProcessRendererEnabled;
}

void ContentBrowserClientImpl::ResourceDispatcherHostCreated()
{
    content::ResourceDispatcherHost::Get()->SetDelegate(
        &ResourceDispatcherHostDelegate::Get());
}

content::WebContentsViewDelegate*
ContentBrowserClientImpl::GetWebContentsViewDelegate(content::WebContents* webContents)
{
    return new WebContentsViewDelegateImpl(webContents);
}

bool ContentBrowserClientImpl::IsHandledURL(const GURL& url)
{
    if (!url.is_valid())
        return false;
    DCHECK_EQ(url.scheme(), base::ToLowerASCII(url.scheme()));
    // Keep in sync with ProtocolHandlers added by
    // URLRequestContextGetterImpl::GetURLRequestContext().
    static const char* const kProtocolList[] = {
        url::kBlobScheme,
        url::kFileSystemScheme,
        content::kChromeUIScheme,
        content::kChromeDevToolsScheme,
        url::kDataScheme,
        url::kFileScheme,
    };
    for (size_t i = 0; i < arraysize(kProtocolList); ++i) {
        if (url.scheme() == kProtocolList[i])
            return true;
    }
    return false;
}

content::DevToolsManagerDelegate *ContentBrowserClientImpl::GetDevToolsManagerDelegate()
{
    return new DevToolsManagerDelegateImpl();
}

void ContentBrowserClientImpl::ExposeInterfacesToRenderer(
        service_manager::BinderRegistry* registry,
        content::AssociatedInterfaceRegistry* associated_registry,
        content::RenderProcessHost* render_process_host)
{
    ProcessHostImpl::registerMojoInterfaces(registry);
}            

void ContentBrowserClientImpl::StartInProcessRendererThread(
    mojo::edk::OutgoingBrokerClientInvitation* broker_client_invitation,
    const std::string& service_token)
{
    d_broker_client_invitation = broker_client_invitation;
}

mojo::edk::OutgoingBrokerClientInvitation* ContentBrowserClientImpl::GetClientInvitation() const
{
    return d_broker_client_invitation.load();
}

#if 0
std::unique_ptr<base::Value>
ContentBrowserClientImpl::GetServiceManifestOverlay(const std::string& name)
{

}
#endif

}  // close namespace blpwtk2

// vim: ts=4 et

