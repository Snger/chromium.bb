// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UTILS_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UTILS_H_

#include "webkit/common/resource_type.h"

namespace content {

class ServiceWorkerUtils {
 public:
  static bool IsMainResourceType(ResourceType::Type type) {
    return ResourceType::IsFrame(type) ||
           ResourceType::IsSharedWorker(type);
  }

  static bool IsServiceWorkerResourceType(ResourceType::Type type) {
    // TODO(kinuko): Add IsServiceWorker() to resource_type.h.
    return type == ResourceType::SERVICE_WORKER;
  }
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_UTILS_H_
