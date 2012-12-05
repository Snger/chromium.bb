// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_
#define CHROME_BROWSER_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#include "chrome/browser/policy/cloud_policy_store.h"
#include "chrome/browser/policy/device_local_account_policy_store.h"

namespace chromeos {
class SessionManagerClient;
}

namespace policy {

class CloudPolicyClient;
class CloudPolicyRefreshScheduler;
class CloudPolicyService;
class DeviceManagementService;

// This class manages the policy settings for a single device-local account,
// hosting the corresponding DeviceLocalAccountPolicyStore as well as the
// CloudPolicyClient (for updating the policy from the cloud) if applicable.
class DeviceLocalAccountPolicyBroker {
 public:
  DeviceLocalAccountPolicyBroker(
      const std::string& account_id,
      chromeos::SessionManagerClient* session_manager_client,
      chromeos::DeviceSettingsService* device_settings_service);
  ~DeviceLocalAccountPolicyBroker();

  CloudPolicyStore* store() { return &store_; }
  const CloudPolicyStore* store() const { return &store_; }

  CloudPolicyClient* client() { return client_.get(); }
  const CloudPolicyClient* client() const { return client_.get(); }

  const std::string& account_id() const { return account_id_; }

  // Refreshes policy (if applicable) and invokes |callback| when done.
  void RefreshPolicy(const base::Closure& callback);

  // Establish a cloud connection for the service.
  void Connect(scoped_ptr<CloudPolicyClient> client);

  // Destroy the cloud connection, stopping policy refreshes.
  void Disconnect();

  // Updates the refresh scheduler's delay from the key::kPolicyRefreshRate
  // policy in |store_|.
  void UpdateRefreshDelay();

 private:
  const std::string account_id_;

  DeviceLocalAccountPolicyStore store_;
  scoped_ptr<CloudPolicyClient> client_;
  scoped_ptr<CloudPolicyService> service_;
  scoped_ptr<CloudPolicyRefreshScheduler> refresh_scheduler_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyBroker);
};

// Manages user policy blobs for device-local accounts present on the device.
// The actual policy blobs are brokered by session_manager (to prevent file
// manipulation), and we're making signature checks on the policy blobs to
// ensure they're issued by the device owner.
class DeviceLocalAccountPolicyService
    : public chromeos::DeviceSettingsService::Observer,
      public CloudPolicyStore::Observer {
 public:
  // Interface for interested parties to observe policy changes.
  class Observer {
   public:
    virtual ~Observer() {}

    // Policy for the given account has changed.
    virtual void OnPolicyUpdated(const std::string& account_id) = 0;

    // The list of accounts has been updated.
    virtual void OnDeviceLocalAccountsChanged() = 0;
  };

  DeviceLocalAccountPolicyService(
      chromeos::SessionManagerClient* session_manager_client,
      chromeos::DeviceSettingsService* device_settings_service);
  virtual ~DeviceLocalAccountPolicyService();

  // Initializes the cloud policy service connection.
  void Connect(DeviceManagementService* device_management_service);

  // Prevents further policy fetches from the cloud.
  void Disconnect();

  // Get the policy broker for a given account. Returns NULL if that account is
  // not valid.
  DeviceLocalAccountPolicyBroker* GetBrokerForAccount(
      const std::string& account_id);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // DeviceSettingsService::Observer:
  virtual void OwnershipStatusChanged() OVERRIDE;
  virtual void DeviceSettingsUpdated() OVERRIDE;

  // CloudPolicyStore::Observer:
  virtual void OnStoreLoaded(CloudPolicyStore* store) OVERRIDE;
  virtual void OnStoreError(CloudPolicyStore* store) OVERRIDE;

 private:
  typedef std::map<std::string, DeviceLocalAccountPolicyBroker*>
      PolicyBrokerMap;

  // Re-queries the list of defined device-local accounts from device settings
  // and updates |policy_brokers_| to match that list.
  void UpdateAccountList(
      const enterprise_management::ChromeDeviceSettingsProto& device_settings);

  // Creates a broker for the given account ID.
  scoped_ptr<DeviceLocalAccountPolicyBroker> CreateBroker(
      const std::string& account_id);

  // Deletes brokers in |map| and clears it.
  void DeleteBrokers(PolicyBrokerMap* map);

  // Find the broker for a given |store|. Returns NULL if |store| is unknown.
  DeviceLocalAccountPolicyBroker* GetBrokerForStore(CloudPolicyStore* store);

  // Creates and initializes a cloud policy client for |account_id|. Returns
  // NULL if the device doesn't have credentials in device settings (i.e. is not
  // enterprise-enrolled).
  scoped_ptr<CloudPolicyClient> CreateClientForAccount(
      const std::string& account_id);

  chromeos::SessionManagerClient* session_manager_client_;
  chromeos::DeviceSettingsService* device_settings_service_;

  DeviceManagementService* device_management_service_;

  // The device-local account policy brokers, keyed by account ID.
  PolicyBrokerMap policy_brokers_;

  ObserverList<Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(DeviceLocalAccountPolicyService);
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVICE_LOCAL_ACCOUNT_POLICY_SERVICE_H_
