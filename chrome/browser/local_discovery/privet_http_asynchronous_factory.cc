// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/local_discovery/privet_http_asynchronous_factory.h"
#include "chrome/browser/local_discovery/privet_http_impl.h"
#include "chrome/common/chrome_switches.h"

namespace local_discovery {

namespace {

std::string IPAddressToHostString(const net::IPAddressNumber& address) {
  std::string address_str = net::IPAddressToString(address);

  // IPv6 addresses need to be surrounded by brackets.
  if (address.size() == net::kIPv6AddressSize) {
    address_str = base::StringPrintf("[%s]", address_str.c_str());
  }

  return address_str;
}

} // namespace

PrivetHTTPAsynchronousFactoryImpl::PrivetHTTPAsynchronousFactoryImpl(
    ServiceDiscoveryClient* service_discovery_client,
    net::URLRequestContextGetter* request_context)
    : service_discovery_client_(service_discovery_client),
      request_context_(request_context) {
}

PrivetHTTPAsynchronousFactoryImpl::~PrivetHTTPAsynchronousFactoryImpl() {
}

scoped_ptr<PrivetHTTPAsynchronousFactory::Resolution>
PrivetHTTPAsynchronousFactoryImpl::CreatePrivetHTTP(
    const net::HostPortPair& address,
    const ResultCallback& callback) {
  return scoped_ptr<Resolution>(new ResolutionImpl(address, callback,
                                                   service_discovery_client_,
                                                   request_context_.get()));
}

PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::ResolutionImpl(
    const net::HostPortPair& address,
    const ResultCallback& callback,
    ServiceDiscoveryClient* service_discovery_client,
    net::URLRequestContextGetter* request_context)
    : hostport_(address), callback_(callback),
      request_context_(request_context) {
  net::AddressFamily address_family = net::ADDRESS_FAMILY_UNSPECIFIED;

  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPrivetIPv6Only)) {
    address_family = net::ADDRESS_FAMILY_IPV6;
  }

  resolver_ = service_discovery_client->CreateLocalDomainResolver(
      address.host(), address_family,
      base::Bind(&ResolutionImpl::ResolveComplete, base::Unretained(this)));
}


PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::~ResolutionImpl() {
}

void PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::Start() {
  resolver_->Start();
}

void PrivetHTTPAsynchronousFactoryImpl::ResolutionImpl::ResolveComplete(
    bool success, const net::IPAddressNumber& address) {
  if (!success) {
    callback_.Run(scoped_ptr<PrivetHTTPClient>());
    return;
  }

  net::HostPortPair new_address = net::HostPortPair(
      IPAddressToHostString(address), hostport_.port());
  callback_.Run(scoped_ptr<PrivetHTTPClient>(
      new PrivetHTTPClientImpl(new_address, request_context_.get())));
}

}  // namespace local_discovery
