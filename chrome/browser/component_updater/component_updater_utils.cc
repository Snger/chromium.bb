// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_updater_utils.h"
#include "base/guid.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "base/win/windows_version.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/omaha_query_params/omaha_query_params.h"
#include "net/base/load_flags.h"
#include "net/url_request/url_fetcher.h"
#include "net/url_request/url_request_context_getter.h"

namespace component_updater {

std::string BuildProtocolRequest(const std::string& request_body) {
  const char request_format[] =
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<request protocol=\"3.0\" version=\"%s-%s\" prodversion=\"%s\" "
      "requestid=\"{%s}\" updaterchannel=\"%s\" arch=\"%s\" nacl_arch=\"%s\">"
      "<os platform=\"%s\" version=\"%s\" arch=\"%s\"/>"
      "%s"
      "</request>";

  const std::string prod_id(chrome::OmahaQueryParams::GetProdIdString(
      chrome::OmahaQueryParams::CHROME));
  const std::string chrome_version(chrome::VersionInfo().Version().c_str());

  const std::string request(base::StringPrintf(request_format,
      // Chrome version and platform information.
      prod_id.c_str(), chrome_version.c_str(),        // "version"
      chrome_version.c_str(),                         // "prodversion"
      base::GenerateGUID().c_str(),                   // "requestid"
      chrome::OmahaQueryParams::GetChannelString(),   // "updaterchannel"
      chrome::OmahaQueryParams::getArch(),            // "arch"
      chrome::OmahaQueryParams::getNaclArch(),        // "nacl_arch"

      // OS version and platform information.
      chrome::VersionInfo().OSType().c_str(),                  // "platform"
      base::SysInfo().OperatingSystemVersion().c_str(),        // "version"
      base::SysInfo().OperatingSystemArchitecture().c_str(),   // "arch"

      request_body.c_str()));   // The actual payload of the request.

  return request;
}

net::URLFetcher* SendProtocolRequest(
    const GURL& url,
    const std::string& protocol_request,
    net::URLFetcherDelegate* url_fetcher_delegate,
    net::URLRequestContextGetter* url_request_context_getter) {
  net::URLFetcher* url_fetcher(
      net::URLFetcher::Create(0,
                              url,
                              net::URLFetcher::POST,
                              url_fetcher_delegate));

  url_fetcher->SetUploadData("application/xml", protocol_request);
  url_fetcher->SetRequestContext(url_request_context_getter);
  url_fetcher->SetLoadFlags(net::LOAD_DO_NOT_SEND_COOKIES |
                            net::LOAD_DO_NOT_SAVE_COOKIES |
                            net::LOAD_DISABLE_CACHE);
  url_fetcher->SetAutomaticallyRetryOn5xx(false);
  url_fetcher->Start();

  return url_fetcher;
}

}  // namespace component_updater

