// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/dbus/mock_dbus_thread_manager_without_gmock.h"

#include "chromeos/dbus/dbus_thread_manager_observer.h"
#include "chromeos/dbus/fake_bluetooth_adapter_client.h"
#include "chromeos/dbus/fake_bluetooth_agent_manager_client.h"
#include "chromeos/dbus/fake_bluetooth_device_client.h"
#include "chromeos/dbus/fake_bluetooth_input_client.h"
#include "chromeos/dbus/fake_bluetooth_profile_manager_client.h"
#include "chromeos/dbus/fake_cros_disks_client.h"
#include "chromeos/dbus/ibus/mock_ibus_client.h"
#include "chromeos/dbus/ibus/mock_ibus_config_client.h"
#include "chromeos/dbus/ibus/mock_ibus_engine_factory_service.h"
#include "chromeos/dbus/ibus/mock_ibus_engine_service.h"
#include "chromeos/dbus/ibus/mock_ibus_input_context_client.h"
#include "chromeos/dbus/ibus/mock_ibus_panel_service.h"
#include "dbus/fake_bus.h"

namespace chromeos {

MockDBusThreadManagerWithoutGMock::MockDBusThreadManagerWithoutGMock()
  : fake_bluetooth_adapter_client_(new FakeBluetoothAdapterClient()),
    fake_bluetooth_agent_manager_client_(new FakeBluetoothAgentManagerClient()),
    fake_bluetooth_device_client_(new FakeBluetoothDeviceClient()),
    fake_bluetooth_input_client_(new FakeBluetoothInputClient()),
    fake_bluetooth_profile_manager_client_(
        new FakeBluetoothProfileManagerClient()),
    fake_cros_disks_client_(new FakeCrosDisksClient),
    mock_ibus_client_(new MockIBusClient),
    mock_ibus_input_context_client_(new MockIBusInputContextClient),
    ibus_bus_(NULL) {
}

MockDBusThreadManagerWithoutGMock::~MockDBusThreadManagerWithoutGMock() {
  FOR_EACH_OBSERVER(DBusThreadManagerObserver, observers_,
                    OnDBusThreadManagerDestroying(this));
}

void MockDBusThreadManagerWithoutGMock::AddObserver(
    DBusThreadManagerObserver* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void MockDBusThreadManagerWithoutGMock::RemoveObserver(
    DBusThreadManagerObserver* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

void MockDBusThreadManagerWithoutGMock::InitIBusBus(
    const std::string& ibus_address,
    const base::Closure& closure) {
  dbus::Bus::Options options;
  ibus_bus_ = new dbus::FakeBus(options);
}

dbus::Bus* MockDBusThreadManagerWithoutGMock::GetSystemBus() {
  return NULL;
}

dbus::Bus* MockDBusThreadManagerWithoutGMock::GetIBusBus() {
  return ibus_bus_;
}

BluetoothAdapterClient*
    MockDBusThreadManagerWithoutGMock::GetBluetoothAdapterClient() {
  NOTIMPLEMENTED();
  return NULL;
}

BluetoothDeviceClient*
    MockDBusThreadManagerWithoutGMock::GetBluetoothDeviceClient() {
  NOTIMPLEMENTED();
  return NULL;
}

BluetoothInputClient*
    MockDBusThreadManagerWithoutGMock::GetBluetoothInputClient() {
  NOTIMPLEMENTED();
  return NULL;
}

BluetoothManagerClient*
    MockDBusThreadManagerWithoutGMock::GetBluetoothManagerClient() {
  NOTIMPLEMENTED();
  return NULL;
}

BluetoothNodeClient*
    MockDBusThreadManagerWithoutGMock::GetBluetoothNodeClient() {
  NOTIMPLEMENTED();
  return NULL;
}

CrasAudioClient* MockDBusThreadManagerWithoutGMock::GetCrasAudioClient() {
  NOTIMPLEMENTED();
  return NULL;
}

CrosDisksClient* MockDBusThreadManagerWithoutGMock::GetCrosDisksClient() {
  return fake_cros_disks_client_.get();
}

CryptohomeClient* MockDBusThreadManagerWithoutGMock::GetCryptohomeClient() {
  NOTIMPLEMENTED();
  return NULL;
}

DebugDaemonClient* MockDBusThreadManagerWithoutGMock::GetDebugDaemonClient() {
  NOTIMPLEMENTED();
  return NULL;
}

ExperimentalBluetoothAdapterClient*
    MockDBusThreadManagerWithoutGMock::GetExperimentalBluetoothAdapterClient() {
  return fake_bluetooth_adapter_client_.get();
}

ExperimentalBluetoothAgentManagerClient*
    MockDBusThreadManagerWithoutGMock::
        GetExperimentalBluetoothAgentManagerClient() {
  return fake_bluetooth_agent_manager_client_.get();
}

ExperimentalBluetoothDeviceClient*
    MockDBusThreadManagerWithoutGMock::GetExperimentalBluetoothDeviceClient() {
  return fake_bluetooth_device_client_.get();
}

ExperimentalBluetoothInputClient*
    MockDBusThreadManagerWithoutGMock::GetExperimentalBluetoothInputClient() {
  return fake_bluetooth_input_client_.get();
}

ExperimentalBluetoothProfileManagerClient*
    MockDBusThreadManagerWithoutGMock::
        GetExperimentalBluetoothProfileManagerClient() {
  return fake_bluetooth_profile_manager_client_.get();
}

ShillDeviceClient*
    MockDBusThreadManagerWithoutGMock::GetShillDeviceClient() {
  NOTIMPLEMENTED();
  return NULL;
}

ShillIPConfigClient*
    MockDBusThreadManagerWithoutGMock::GetShillIPConfigClient() {
  NOTIMPLEMENTED();
  return NULL;
}

ShillManagerClient*
    MockDBusThreadManagerWithoutGMock::GetShillManagerClient() {
  NOTIMPLEMENTED();
  return NULL;
}

ShillProfileClient*
    MockDBusThreadManagerWithoutGMock::GetShillProfileClient() {
  NOTIMPLEMENTED();
  return NULL;
}

ShillServiceClient*
    MockDBusThreadManagerWithoutGMock::GetShillServiceClient() {
  NOTIMPLEMENTED();
  return NULL;
}

GsmSMSClient* MockDBusThreadManagerWithoutGMock::GetGsmSMSClient() {
  NOTIMPLEMENTED();
  return NULL;
}

ImageBurnerClient* MockDBusThreadManagerWithoutGMock::GetImageBurnerClient() {
  NOTIMPLEMENTED();
  return NULL;
}

IntrospectableClient*
    MockDBusThreadManagerWithoutGMock::GetIntrospectableClient() {
  NOTIMPLEMENTED();
  return NULL;
}

ModemMessagingClient*
    MockDBusThreadManagerWithoutGMock::GetModemMessagingClient() {
  NOTIMPLEMENTED();
  return NULL;
}

PermissionBrokerClient*
    MockDBusThreadManagerWithoutGMock::GetPermissionBrokerClient() {
  NOTIMPLEMENTED();
  return NULL;
}

PowerManagerClient* MockDBusThreadManagerWithoutGMock::GetPowerManagerClient() {
  NOTIMPLEMENTED();
  return NULL;
}

PowerPolicyController*
MockDBusThreadManagerWithoutGMock::GetPowerPolicyController() {
  NOTIMPLEMENTED();
  return NULL;
}

SessionManagerClient*
    MockDBusThreadManagerWithoutGMock::GetSessionManagerClient() {
  NOTIMPLEMENTED();
  return NULL;
}

SMSClient* MockDBusThreadManagerWithoutGMock::GetSMSClient() {
  NOTIMPLEMENTED();
  return NULL;
}

SystemClockClient* MockDBusThreadManagerWithoutGMock::GetSystemClockClient() {
  NOTIMPLEMENTED();
  return NULL;
}

UpdateEngineClient* MockDBusThreadManagerWithoutGMock::GetUpdateEngineClient() {
  NOTIMPLEMENTED();
  return NULL;
}

BluetoothOutOfBandClient*
    MockDBusThreadManagerWithoutGMock::GetBluetoothOutOfBandClient() {
  NOTIMPLEMENTED();
  return NULL;
}

IBusClient* MockDBusThreadManagerWithoutGMock::GetIBusClient() {
  return mock_ibus_client_.get();
}

IBusConfigClient* MockDBusThreadManagerWithoutGMock::GetIBusConfigClient() {
  return mock_ibus_config_client_.get();
}

IBusInputContextClient*
    MockDBusThreadManagerWithoutGMock::GetIBusInputContextClient() {
  return mock_ibus_input_context_client_.get();
}

IBusEngineFactoryService*
    MockDBusThreadManagerWithoutGMock::GetIBusEngineFactoryService() {
  return mock_ibus_engine_factory_service_.get();
}

IBusEngineService* MockDBusThreadManagerWithoutGMock::GetIBusEngineService(
    const dbus::ObjectPath& object_path) {
  return mock_ibus_engine_service_.get();
}

void MockDBusThreadManagerWithoutGMock::RemoveIBusEngineService(
    const dbus::ObjectPath& object_path) {
}

IBusPanelService* MockDBusThreadManagerWithoutGMock::GetIBusPanelService() {
  return mock_ibus_panel_service_.get();
}

}  // namespace chromeos
