// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_MOCK_FLIMFLAM_DEVICE_CLIENT_H_
#define CHROMEOS_DBUS_MOCK_FLIMFLAM_DEVICE_CLIENT_H_

#include "base/values.h"
#include "chromeos/dbus/flimflam_device_client.h"
#include "dbus/object_path.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromeos {

class MockFlimflamDeviceClient : public FlimflamDeviceClient {
 public:
  MockFlimflamDeviceClient();
  virtual ~MockFlimflamDeviceClient();

  MOCK_METHOD2(SetPropertyChangedHandler,
               void(const dbus::ObjectPath& device_path,
                    const PropertyChangedHandler& handler));
  MOCK_METHOD1(ResetPropertyChangedHandler,
               void(const dbus::ObjectPath& device_path));
  MOCK_METHOD2(GetProperties, void(const dbus::ObjectPath& device_path,
                                   const DictionaryValueCallback& callback));
  MOCK_METHOD2(ProposeScan, void(const dbus::ObjectPath& device_path,
                                 const VoidCallback& callback));
  MOCK_METHOD4(SetProperty, void(const dbus::ObjectPath& device_path,
                                 const std::string& name,
                                 const base::Value& value,
                                 const VoidCallback& callback));
  MOCK_METHOD3(ClearProperty, void(const dbus::ObjectPath& device_path,
                                   const std::string& name,
                                   const VoidCallback& callback));
  MOCK_METHOD3(AddIPConfig, void(const dbus::ObjectPath& device_path,
                                 const std::string& method,
                                 const ObjectPathCallback& callback));
  MOCK_METHOD4(RequirePin, void(const dbus::ObjectPath& device_path,
                                const std::string& pin,
                                bool require,
                                const VoidCallback& callback));
  MOCK_METHOD3(EnterPin, void(const dbus::ObjectPath& device_path,
                              const std::string& pin,
                              const VoidCallback& callback));
  MOCK_METHOD4(UnblockPin, void(const dbus::ObjectPath& device_path,
                                const std::string& puk,
                                const std::string& pin,
                                const VoidCallback& callback));
  MOCK_METHOD4(ChangePin, void(const dbus::ObjectPath& device_path,
                               const std::string& old_pin,
                               const std::string& new_pin,
                               const VoidCallback& callback));
  MOCK_METHOD3(Register, void(const dbus::ObjectPath& device_path,
                              const std::string& network_id,
                              const VoidCallback& callback));
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_MOCK_FLIMFLAM_DEVICE_CLIENT_H_
