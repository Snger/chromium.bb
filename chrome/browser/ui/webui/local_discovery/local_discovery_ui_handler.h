// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_LOCAL_DISCOVERY_LOCAL_DISCOVERY_UI_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_LOCAL_DISCOVERY_LOCAL_DISCOVERY_UI_HANDLER_H_

#include <map>
#include <string>

#include "chrome/browser/local_discovery/privet_confirm_api_flow.h"
#include "chrome/browser/local_discovery/privet_device_lister.h"
#include "chrome/browser/local_discovery/privet_http.h"
#include "chrome/browser/local_discovery/service_discovery_host_client.h"
#include "chrome/common/local_discovery/service_discovery_client.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace local_discovery {

// UI Handler for chrome://devices/
// It listens to local discovery notifications and passes those notifications
// into the Javascript to update the page.
class LocalDiscoveryUIHandler : public content::WebUIMessageHandler,
                                public PrivetRegisterOperation::Delegate,
                                public PrivetDeviceLister::Delegate,
                                public PrivetInfoOperation::Delegate {
 public:
  class Factory {
   public:
    virtual ~Factory() {}
    virtual LocalDiscoveryUIHandler* CreateLocalDiscoveryUIHandler() = 0;
  };

  LocalDiscoveryUIHandler();
  // This constructor should only used by tests.
  explicit LocalDiscoveryUIHandler(
      scoped_ptr<PrivetDeviceLister> privet_lister);
  virtual ~LocalDiscoveryUIHandler();

  static LocalDiscoveryUIHandler* Create();
  static void SetFactory(Factory* factory);

  // WebUIMessageHandler implementation.
  virtual void RegisterMessages() OVERRIDE;

  // PrivetRegisterOperation::Delegate implementation.
  virtual void OnPrivetRegisterClaimToken(const std::string& token,
                                          const GURL& url) OVERRIDE;
  virtual void OnPrivetRegisterError(
      const std::string& action,
      PrivetRegisterOperation::FailureReason reason,
      int printer_http_code,
      const DictionaryValue* json) OVERRIDE;
  virtual void OnPrivetRegisterDone(const std::string& device_id) OVERRIDE;

  // PrivetDeviceLister::Delegate implementation.
  virtual void DeviceChanged(
      bool added,
      const std::string& name,
      const DeviceDescription& description) OVERRIDE;
  virtual void DeviceRemoved(const std::string& name) OVERRIDE;

  // PrivetInfoOperation::Delegate implementation:
  virtual void OnPrivetInfoDone(
      int http_code,
      const base::DictionaryValue* json_value) OVERRIDE;

 private:
  // Message handlers:
  // For registering a device.
  void HandleRegisterDevice(const base::ListValue* args);
  // For when the page is ready to recieve device notifications.

  void HandleStart(const base::ListValue* args);
  // For when info for a device is requested.
  void HandleInfoRequested(const base::ListValue* args);

  // For when the IP address of the printer has been resolved for registration.
  void StartRegisterHTTP(bool success,
                         const net::IPAddressNumber& address);

  // For when the IP address of the printer has been resolved for registration.
  void StartInfoHTTP(bool success,
                     const net::IPAddressNumber& address);

  // For when the confirm operation on the cloudprint server has finished
  // executing.
  void OnConfirmDone(PrivetConfirmApiCallFlow::Status status);

  // Log an error to the web interface.
  void LogRegisterErrorToWeb(const std::string& error);

  // Log a successful registration to the web inteface.
  void LogRegisterDoneToWeb(const std::string& id);


  // Log an error to the web interface.
  void LogInfoErrorToWeb(const std::string& error);

  // The current HTTP client (used for the current operation).
  scoped_ptr<PrivetHTTPClient> current_http_client_;

  // The current device being used in an HTTP operation.
  std::string current_http_device_;

  // The current info operation (operations are currently exclusive).
  scoped_ptr<PrivetInfoOperation> current_info_operation_;

  // The current register operation. Only one allowed at any time.
  scoped_ptr<PrivetRegisterOperation> current_register_operation_;

  // The current confirm call used during the registration flow.
  scoped_ptr<PrivetConfirmApiCallFlow> confirm_api_call_flow_;

  // The device lister used to list devices on the local network.
  scoped_ptr<PrivetDeviceLister> privet_lister_;

  // The service discovery client used listen for devices on the local network.
  scoped_refptr<ServiceDiscoveryHostClient> service_discovery_client_;

  // A map of current device descriptions provided by the PrivetDeviceLister.
  std::map<std::string, DeviceDescription> device_descriptions_;

  // The local domain resolver used to resolve the domains for local devices.
  scoped_ptr<LocalDomainResolver> domain_resolver_;

  DISALLOW_COPY_AND_ASSIGN(LocalDiscoveryUIHandler);
};

}  // namespace local_discovery
#endif  // CHROME_BROWSER_UI_WEBUI_LOCAL_DISCOVERY_LOCAL_DISCOVERY_UI_HANDLER_H_
