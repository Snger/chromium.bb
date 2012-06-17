// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_
#pragma once

#include <string>

#include "chrome/browser/extensions/api/api_function.h"
#include "chrome/browser/extensions/extension_function.h"

#if defined(OS_CHROMEOS)
#include "base/memory/ref_counted.h"
#include "chrome/browser/chromeos/bluetooth/bluetooth_socket.h"

namespace chromeos {

class BluetoothDevice;
class BluetoothSocket;
struct BluetoothOutOfBandPairingData;

}  // namespace chromeos
#endif

namespace extensions {
namespace api {

class BluetoothIsAvailableFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.isAvailable")

 protected:
  virtual ~BluetoothIsAvailableFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothIsPoweredFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.isPowered")

 protected:
  virtual ~BluetoothIsPoweredFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetAddressFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.getAddress")

 protected:
  virtual ~BluetoothGetAddressFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetDevicesWithServiceUUIDFunction
    : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.bluetooth.getDevicesWithServiceUUID")

 protected:
  virtual ~BluetoothGetDevicesWithServiceUUIDFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetDevicesWithServiceNameFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.bluetooth.getDevicesWithServiceName")

#if defined(OS_CHROMEOS)
  BluetoothGetDevicesWithServiceNameFunction();
#endif

 protected:
  virtual ~BluetoothGetDevicesWithServiceNameFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;

 private:
#if defined(OS_CHROMEOS)
  void AddDeviceIfTrue(
      ListValue* list, const chromeos::BluetoothDevice* device, bool result);

  int callbacks_pending_;
#endif
};

class BluetoothConnectFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.connect")

 protected:
  virtual ~BluetoothConnectFunction() {}

  virtual bool RunImpl() OVERRIDE;

 private:
#if defined(OS_CHROMEOS)
  void ConnectToServiceCallback(
      const chromeos::BluetoothDevice* device,
      const std::string& service_uuid,
      scoped_refptr<chromeos::BluetoothSocket> socket);
#endif
};

class BluetoothDisconnectFunction : public SyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.disconnect")

 protected:
  virtual ~BluetoothDisconnectFunction() {}

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothReadFunction : public AsyncAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.read")
  BluetoothReadFunction();

 protected:
  virtual ~BluetoothReadFunction();

  // AsyncAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual bool Respond() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  bool success_;
#if defined(OS_CHROMEOS)
  scoped_refptr<chromeos::BluetoothSocket> socket_;
#endif
};

class BluetoothWriteFunction : public AsyncAPIFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.bluetooth.write")
  BluetoothWriteFunction();

 protected:
  virtual ~BluetoothWriteFunction();

  // AsyncAPIFunction:
  virtual bool Prepare() OVERRIDE;
  virtual bool Respond() OVERRIDE;
  virtual void Work() OVERRIDE;

 private:
  bool success_;
  const base::BinaryValue* data_to_write_;  // memory is owned by args_
#if defined(OS_CHROMEOS)
  scoped_refptr<chromeos::BluetoothSocket> socket_;
#endif
};

class BluetoothSetOutOfBandPairingDataFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.bluetooth.setOutOfBandPairingData")

 protected:
  virtual ~BluetoothSetOutOfBandPairingDataFunction() {}

#if defined(OS_CHROMEOS)
  void OnSuccessCallback();
  void OnErrorCallback();
#endif

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothGetLocalOutOfBandPairingDataFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.bluetooth.getLocalOutOfBandPairingData")

 protected:
  virtual ~BluetoothGetLocalOutOfBandPairingDataFunction() {}

#if defined(OS_CHROMEOS)
  void ReadCallback(const chromeos::BluetoothOutOfBandPairingData& data);
  void ErrorCallback();
#endif

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

class BluetoothClearOutOfBandPairingDataFunction
    : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME(
      "experimental.bluetooth.clearOutOfBandPairingData")

 protected:
  virtual ~BluetoothClearOutOfBandPairingDataFunction() {}

#if defined(OS_CHROMEOS)
  void OnSuccessCallback();
  void OnErrorCallback();
#endif

  // ExtensionFunction:
  virtual bool RunImpl() OVERRIDE;
};

}  // namespace api
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_BLUETOOTH_BLUETOOTH_API_H_
