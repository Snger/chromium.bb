// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_SHARED_IMPL_PRIVATE_FLASH_NET_ADDRESS_IMPL_H_
#define PPAPI_SHARED_IMPL_PRIVATE_FLASH_NET_ADDRESS_IMPL_H_

#include "base/basictypes.h"
#include "ppapi/shared_impl/ppapi_shared_export.h"

struct PP_Flash_NetAddress;

namespace ppapi {

class PPAPI_SHARED_EXPORT FlashNetAddressImpl {
 public:
  static bool ValidateNetAddress(const PP_Flash_NetAddress& addr);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(FlashNetAddressImpl);
};

}  // namespace ppapi

#endif  // PPAPI_SHARED_IMPL_PRIVATE_FLASH_NET_ADDRESS_IMPL_H_
