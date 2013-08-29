// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/local_discovery/local_discovery_ui_handler.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/browser_resources.h"
#include "grit/generated_resources.h"

namespace {

content::WebUIDataSource* CreateLocalDiscoveryHTMLSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(chrome::kChromeUIDevicesHost);

  source->SetDefaultResource(IDR_LOCAL_DISCOVERY_HTML);
  source->AddResourcePath("local_discovery.css", IDR_LOCAL_DISCOVERY_CSS);
  source->AddResourcePath("local_discovery.js", IDR_LOCAL_DISCOVERY_JS);

  source->SetUseJsonJSFormatV2();
  source->AddLocalizedString("serviceRegister",
                             IDS_LOCAL_DISCOVERY_SERVICE_REGISTER);

  source->AddLocalizedString("registerConfirmMessage",
                             IDS_LOCAL_DISCOVERY_REGISTER_CONFIRMATION);
  source->AddLocalizedString("registerUser",
                             IDS_LOCAL_DISCOVERY_REGISTER_USER);
  source->AddLocalizedString("confirmRegistration",
                             IDS_LOCAL_DISCOVERY_CONFIRM_REGISTRATION);
  source->AddLocalizedString("addingPrinter",
                             IDS_LOCAL_DISCOVERY_ADDING_PRINTER);
  source->AddLocalizedString("addingError",
                             IDS_LOCAL_DISCOVERY_ERROR_OCURRED);
  source->AddLocalizedString("addingErrorMessage",
                             IDS_LOCAL_DISCOVERY_ERROR_OCURRED_MESSAGE);
  source->AddLocalizedString("addingMessage1",
                             IDS_LOCAL_DISCOVERY_ADDING_PRINTER_MESSAGE1);
  source->AddLocalizedString("addingMessage2",
                             IDS_LOCAL_DISCOVERY_ADDING_PRINTER_MESSAGE2);
  source->AddLocalizedString("registeredDevicesTitle",
                             IDS_LOCAL_DISCOVERY_REGISTERED_DEVICES_TITLE);
  source->AddLocalizedString("unregisteredDevicesTitle",
                             IDS_LOCAL_DISCOVERY_UNREGISTERED_DEVICES_TITLE);
  source->AddLocalizedString("devicesTitle",
                             IDS_LOCAL_DISCOVERY_DEVICES_PAGE_TITLE);

  source->SetJsonPath("strings.js");

  source->DisableDenyXFrameOptions();

  return source;
}

}  // namespace

LocalDiscoveryUI::LocalDiscoveryUI(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  // Set up the chrome://devices/ source.
  Profile* profile = Profile::FromWebUI(web_ui);
  content::WebUIDataSource::Add(profile, CreateLocalDiscoveryHTMLSource());

  // TODO(gene): Use LocalDiscoveryUIHandler to send updated to the devices
  // page. For example
  web_ui->AddMessageHandler(local_discovery::LocalDiscoveryUIHandler::Create());
}
