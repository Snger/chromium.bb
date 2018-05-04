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

#include <blpwtk2_urlrequestcontextgetterimpl.h>

#include <blpwtk2_networkdelegateimpl.h>

#include <base/bind.h>
#include <base/command_line.h>
#include <base/logging.h>  // for DCHECK
#include <base/strings/string_util.h>
#include <base/threading/sequenced_worker_pool.h>
#include <base/threading/worker_pool.h>
#include <base/memory/ptr_util.h>
#include <content/public/browser/browser_thread.h>
#include <content/public/common/content_switches.h>
#include <content/public/common/url_constants.h>
#include <net/cert/cert_verifier.h>
#include <net/cookies/cookie_monster.h>
#include <net/dns/mapped_host_resolver.h>
#include <net/extras/sqlite/cookie_crypto_delegate.h>
#include <net/extras/sqlite/sqlite_persistent_cookie_store.h>
#include <net/http/http_auth_handler_factory.h>
#include <net/http/http_cache.h>
#include <net/http/http_network_layer.h>
#include <net/http/http_network_session.h>
#include <net/http/http_server_properties_impl.h>
#include <net/proxy/proxy_service.h>
#include <net/proxy/proxy_config_service.h>
#include <net/proxy/proxy_config_service_fixed.h>
#include <net/ssl/channel_id_service.h>
#include <net/ssl/default_channel_id_store.h>
#include <net/ssl/ssl_config_service_defaults.h>
#include <net/url_request/data_protocol_handler.h>
#include <net/url_request/file_protocol_handler.h>
#include <net/url_request/static_http_user_agent_settings.h>
#include <net/url_request/url_request_context.h>
#include <net/url_request/url_request_context_storage.h>
#include <net/url_request/url_request_context_builder.h>
#include <net/url_request/url_request_job_factory_impl.h>

namespace blpwtk2 {

URLRequestContextGetterImpl::URLRequestContextGetterImpl(
    const base::FilePath& path,
    bool diskCacheEnabled,
    bool cookiePersistenceEnabled)
: d_gotProtocolHandlers(false)
, d_path(path)
, d_diskCacheEnabled(diskCacheEnabled)
, d_cookiePersistenceEnabled(cookiePersistenceEnabled)
, d_wasProxyInitialized(false)
{
}

URLRequestContextGetterImpl::~URLRequestContextGetterImpl()
{
}

void URLRequestContextGetterImpl::setProxyConfig(const net::ProxyConfig& config)
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    d_wasProxyInitialized = true;

    net::ProxyConfigService* proxyConfigService =
        new net::ProxyConfigServiceFixed(config);

    GetNetworkTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&URLRequestContextGetterImpl::updateProxyConfig,
                   this,
                   proxyConfigService));
}

void URLRequestContextGetterImpl::useSystemProxyConfig()
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    d_wasProxyInitialized = true;

    auto ioLoop =
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::IO);

    auto fileLoop =
        content::BrowserThread::GetTaskRunnerForThread(
            content::BrowserThread::FILE);

    // We must create the proxy config service on the UI loop on Linux
    // because it must synchronously run on the glib message loop.  This
    // will be passed to the ProxyServer on the IO thread.
    net::ProxyConfigService* proxyConfigService =
        net::ProxyService::CreateSystemProxyConfigService(
            ioLoop, fileLoop).release();

    GetNetworkTaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&URLRequestContextGetterImpl::updateProxyConfig,
                   this,
                   proxyConfigService));
}

void URLRequestContextGetterImpl::setProtocolHandlers(
    content::ProtocolHandlerMap* protocolHandlers,
    content::URLRequestInterceptorScopedVector requestInterceptors)
{
    // Note: It is guaranteed that this is only called once, and it happens
    //       before GetURLRequestContext() is called on the IO thread.
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

    // If we haven't got a proxy configuration at this point, just initialize
    // to use the system proxy settings.  Note that proxy configuration must be
    // setup before the IO thread starts using this URLRequestContextGetter.
    if (!d_wasProxyInitialized) {
        useSystemProxyConfig();
        DCHECK(d_wasProxyInitialized);
    }

    base::AutoLock guard(d_protocolHandlersLock);
    DCHECK(!d_gotProtocolHandlers);
    std::swap(d_protocolHandlers, *protocolHandlers);
    d_requestInterceptors = std::move(requestInterceptors);
    d_gotProtocolHandlers = true;
}


// net::URLRequestContextGetter implementation.

net::URLRequestContext* URLRequestContextGetterImpl::GetURLRequestContext()
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));

    if (!d_urlRequestContext.get())
        initialize();
    return d_urlRequestContext.get();
}

scoped_refptr<base::SingleThreadTaskRunner>
URLRequestContextGetterImpl::GetNetworkTaskRunner() const
{
    return content::BrowserThread::GetTaskRunnerForThread(
           content::BrowserThread::IO);
}

void URLRequestContextGetterImpl::initialize()
{
    DCHECK(d_proxyService.get());

    if (d_cookiePersistenceEnabled) {
        d_cookieStore =
            new net::SQLitePersistentCookieStore(
                d_path.Append(FILE_PATH_LITERAL("Cookies")),
                GetNetworkTaskRunner(),
                content::BrowserThread::GetTaskRunnerForThread(
                    content::BrowserThread::FILE),
                true,
                (net::CookieCryptoDelegate*)0);
    }

    const base::CommandLine& cmdline = *base::CommandLine::ForCurrentProcess();

    net::URLRequestContextBuilder builder;

    builder.set_proxy_service(std::move(d_proxyService));
    builder.set_network_delegate(std::unique_ptr<NetworkDelegateImpl>(new NetworkDelegateImpl()));
    builder.SetCookieAndChannelIdStores(
        std::unique_ptr<net::CookieMonster>(
            new net::CookieMonster(d_cookieStore.get(), 0)),
        std::unique_ptr<net::ChannelIDService>(
            new net::ChannelIDService(new net::DefaultChannelIDStore(NULL),
                                      base::WorkerPool::GetTaskRunner(true))));

    builder.set_accept_language("en-us,en");
    builder.set_user_agent(base::EmptyString());

    std::unique_ptr<net::HostResolver> hostResolver
        = net::HostResolver::CreateDefaultResolver(0);

    if (cmdline.HasSwitch(switches::kHostResolverRules)) {
        std::unique_ptr<net::MappedHostResolver> mappedHostResolver(
            new net::MappedHostResolver(std::move(hostResolver)));
        mappedHostResolver->SetRulesFromString(
            cmdline.GetSwitchValueASCII(switches::kHostResolverRules));
        hostResolver = std::move(mappedHostResolver);
    }

    // Give d_storage ownership at the end in case it's mappedHostResolver.
    builder.set_host_resolver(std::move(hostResolver));

    net::URLRequestContextBuilder::HttpCacheParams cache_params;

    if (d_diskCacheEnabled) {
        cache_params.type =
            net::URLRequestContextBuilder::HttpCacheParams::DISK;
    }
    else {
        cache_params.type =
            net::URLRequestContextBuilder::HttpCacheParams::IN_MEMORY;

    }

    builder.EnableHttpCache(cache_params);
    {
        base::AutoLock guard(d_protocolHandlersLock);
        DCHECK(d_gotProtocolHandlers);

        for (auto& scheme_handler : d_protocolHandlers) {
          builder.SetProtocolHandler(
              scheme_handler.first,
              std::unique_ptr<net::URLRequestJobFactory::ProtocolHandler>(scheme_handler.second.release()));
        }
        d_protocolHandlers.clear();
    }

    builder.SetProtocolHandler(
        url::kDataScheme,
        std::unique_ptr<net::DataProtocolHandler>(new net::DataProtocolHandler));

    builder.SetProtocolHandler(
        url::kFileScheme,
        std::unique_ptr<net::FileProtocolHandler>(new net::FileProtocolHandler(
                            content::BrowserThread::GetBlockingPool()->
                                GetTaskRunnerWithShutdownBehavior(
                                    base::SequencedWorkerPool::SKIP_ON_SHUTDOWN))));

    d_urlRequestContext = std::move(builder.Build());
}

void URLRequestContextGetterImpl::updateProxyConfig(
    net::ProxyConfigService* proxyConfigService)
{
    DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
    std::unique_ptr<net::ProxyConfigService> proxyConfigService_(proxyConfigService);

    if (d_urlRequestContext) {
        net::ProxyService* proxyService = d_urlRequestContext->proxy_service();
        DCHECK(proxyService);
        proxyService->ResetConfigService(std::move(proxyConfigService_));
        return;
    }

    // TODO(jam): use v8 if possible, look at chrome code.
    d_proxyService = net::ProxyService::CreateUsingSystemProxyResolver(
            std::move(proxyConfigService_), 0, 0);
}

}  // close namespace blpwtk2

// vim: ts=4 et




