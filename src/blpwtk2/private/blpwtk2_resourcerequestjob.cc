/*
 * Copyright (C) 2018 Bloomberg Finance L.P.
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

#include <blpwtk2_resourcerequestjob.h>

namespace blpwtk2 {

ResourceRequestJob::ResourceRequestJob(
    net::URLRequest* request,
    net::NetworkDelegate* network_delegate)
  : net::URLRequestJob(request, network_delegate)
  , url_(request->url().spec()) {
}

void ResourceRequestJob::Start() {
  NotifyHeadersComplete();
}

static bool EndsWith(const std::string& hay, const std::string& needle) {
  if (hay.length() >= needle.length())
    return (0 == hay.compare(hay.length() - needle.length(), needle.length(), needle));

  return false;
}

bool ResourceRequestJob::GetMimeType(std::string* mime_type) const {
  std::string url = url_;
  std::transform(url.begin(), url.end(), url.begin(), ::tolower);

  // Mapping from
  // https://developer.mozilla.org/en-US/docs/Web/HTTP/Basics_of_HTTP/MIME_types/Complete_list_of_MIME_types

  if (EndsWith(url, ".html") || EndsWith(url, ".htm"))
    mime_type->assign("text/html");

  else if (EndsWith(url, ".css"))
    mime_type->assign("text/css");

  else if (EndsWith(url, ".js"))
    mime_type->assign("application/javascript");

  else if (EndsWith(url, ".png"))
    mime_type->assign("image/png");

  else if (EndsWith(url, ".jpeg") || EndsWith(url, ".jpg"))
    mime_type->assign("image/jpeg");

  else if (EndsWith(url, ".svg"))
    mime_type->assign("image/svg+xml");

  else if (EndsWith(url, ".gif"))
    mime_type->assign("image/gif");

  else if (EndsWith(url, ".otf"))
    mime_type->assign("font/otf");

  else if (EndsWith(url, ".ttf"))
    mime_type->assign("font/ttf");

  else
    return net::URLRequestJob::GetMimeType(mime_type);

  return true;
}

}  // close namespace blpwtk2

