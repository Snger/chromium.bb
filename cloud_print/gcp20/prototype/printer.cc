// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/gcp20/prototype/printer.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/file_util.h"
#include "base/guid.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/strings/string_number_conversions.h"
#include "cloud_print/gcp20/prototype/command_line_reader.h"
#include "cloud_print/gcp20/prototype/service_parameters.h"
#include "net/base/net_util.h"
#include "net/base/url_util.h"

const base::FilePath::CharType kPrinterStatePath[] =
    FILE_PATH_LITERAL("printer_state.json");

namespace {

const char* kServiceType = "_privet._tcp.local";
const char* kServiceNamePrefix = "first_gcp20_device";
const char* kServiceDomainName = "my-privet-device.local";

const char* kPrinterName = "Google GCP2.0 Prototype";
const char* kPrinterDescription = "Printer emulator";

const char* kCdd =
"{\n"
" 'version': '1.0',\n"
"  'printer': {\n"
"    'vendor_capability': [\n"
"      {\n"
"        'id': 'psk:MediaType',\n"
"        'display_name': 'Media Type',\n"
"        'type': 'SELECT',\n"
"        'select_cap': {\n"
"          'option': [\n"
"            {\n"
"              'value': 'psk:Plain',\n"
"              'display_name': 'Plain Paper',\n"
"              'is_default': true\n"
"            },\n"
"            {\n"
"              'value': 'ns0000:Glossy',\n"
"              'display_name': 'Glossy Photo',\n"
"              'is_default': false\n"
"            }\n"
"          ]\n"
"        }\n"
"      }\n"
"    ],\n"
"    'reverse_order': { 'default': false }\n"
"  }\n"
"}\n";

// Returns local IP address number of first interface found (except loopback).
// Return value is empty if no interface found. Possible interfaces names are
// "eth0", "wlan0" etc. If interface name is empty, function will return IP
// address of first interface found.
net::IPAddressNumber GetLocalIp(const std::string& interface_name,
                                bool return_ipv6_number) {
  net::NetworkInterfaceList interfaces;
  bool success = net::GetNetworkList(&interfaces);
  DCHECK(success);

  size_t expected_address_size = return_ipv6_number ? net::kIPv6AddressSize
                                                    : net::kIPv4AddressSize;

  for (net::NetworkInterfaceList::iterator iter = interfaces.begin();
       iter != interfaces.end(); ++iter) {
    if (iter->address.size() == expected_address_size &&
        (interface_name.empty() || interface_name == iter->name)) {
      LOG(INFO) << net::IPAddressToString(iter->address);
      return iter->address;
    }
  }

  return net::IPAddressNumber();
}

}  // namespace

Printer::RegistrationInfo::RegistrationInfo() : state(DEV_REG_UNREGISTERED) {
}

Printer::RegistrationInfo::~RegistrationInfo() {
}

Printer::Printer() : http_server_(this) {
}

Printer::~Printer() {
  Stop();
}

bool Printer::Start() {
  if (IsOnline())
    return true;

  // TODO(maksymb): Add switch for command line to control interface name.
  net::IPAddressNumber ip = GetLocalIp("", false);
  if (ip.empty()) {
    LOG(ERROR) << "No local IP found. Cannot start printer.";
    return false;
  }
  VLOG(1) << "Local address: " << net::IPAddressToString(ip);

  uint16 port = command_line_reader::ReadHttpPort();

  // Starting HTTP server.
  if (!http_server_.Start(port))
    return false;

  if (!LoadFromFile(base::FilePath(kPrinterStatePath)))
    reg_info_ = RegistrationInfo();

  // Starting DNS-SD server.
  if (!dns_server_.Start(
      ServiceParameters(kServiceType, kServiceNamePrefix, kServiceDomainName,
                        ip, port),
      command_line_reader::ReadTtl(),
      CreateTxt())) {
    http_server_.Shutdown();
    return false;
  }

  // Creating Cloud Requester.
  requester_.reset(
      new CloudPrintRequester(
          base::MessageLoop::current()->message_loop_proxy(),
          this));

  xtoken_ = XPrivetToken();
  starttime_ = base::Time::Now();

  return true;
}

bool Printer::IsOnline() const {
  return requester_;
}

void Printer::Stop() {
  dns_server_.Shutdown();
  http_server_.Shutdown();
  requester_.reset();
}

PrivetHttpServer::RegistrationErrorStatus Printer::RegistrationStart(
    const std::string& user) {
  PrivetHttpServer::RegistrationErrorStatus status = CheckCommonRegErrors(user);
  if (status != PrivetHttpServer::REG_ERROR_OK)
    return status;

  if (reg_info_.state != RegistrationInfo::DEV_REG_UNREGISTERED)
    return PrivetHttpServer::REG_ERROR_INVALID_ACTION;

  reg_info_ = RegistrationInfo();
  reg_info_.user = user;
  reg_info_.state = RegistrationInfo::DEV_REG_REGISTRATION_STARTED;

  requester_->StartRegistration(GenerateProxyId(), kPrinterName, user, kCdd);

  return PrivetHttpServer::REG_ERROR_OK;
}

bool Printer::CheckXPrivetTokenHeader(const std::string& token) const {
  return xtoken_.CheckValidXToken(token);
}

bool Printer::IsRegistered() const {
  return reg_info_.state == RegistrationInfo::DEV_REG_REGISTERED;
}

PrivetHttpServer::RegistrationErrorStatus Printer::RegistrationGetClaimToken(
    const std::string& user,
    std::string* token,
    std::string* claim_url) {
  PrivetHttpServer::RegistrationErrorStatus status = CheckCommonRegErrors(user);
  if (status != PrivetHttpServer::REG_ERROR_OK)
    return status;

  // TODO(maksymb): Add user confirmation.

  if (reg_info_.state == RegistrationInfo::DEV_REG_REGISTRATION_STARTED)
    return PrivetHttpServer::REG_ERROR_DEVICE_BUSY;

  if (reg_info_.state ==
      RegistrationInfo::DEV_REG_REGISTRATION_CLAIM_TOKEN_READY) {
    *token = reg_info_.registration_token;
    *claim_url = reg_info_.complete_invite_url;
    return PrivetHttpServer::REG_ERROR_OK;
  }

  return PrivetHttpServer::REG_ERROR_INVALID_ACTION;
}

PrivetHttpServer::RegistrationErrorStatus Printer::RegistrationComplete(
    const std::string& user,
    std::string* device_id) {
  PrivetHttpServer::RegistrationErrorStatus status = CheckCommonRegErrors(user);
  if (status != PrivetHttpServer::REG_ERROR_OK)
    return status;

  if (reg_info_.state !=
      RegistrationInfo::DEV_REG_REGISTRATION_CLAIM_TOKEN_READY) {
    return PrivetHttpServer::REG_ERROR_INVALID_ACTION;
  }

  reg_info_.state = RegistrationInfo::DEV_REG_REGISTRATION_COMPLETING;
  requester_->CompleteRegistration();

  *device_id = reg_info_.device_id;

  return PrivetHttpServer::REG_ERROR_OK;
}

PrivetHttpServer::RegistrationErrorStatus Printer::RegistrationCancel(
    const std::string& user) {
  PrivetHttpServer::RegistrationErrorStatus status = CheckCommonRegErrors(user);
  if (status != PrivetHttpServer::REG_ERROR_OK &&
      status != PrivetHttpServer::REG_ERROR_SERVER_ERROR) {
    return status;
  }

  if (reg_info_.state == RegistrationInfo::DEV_REG_UNREGISTERED)
    return PrivetHttpServer::REG_ERROR_INVALID_ACTION;

  reg_info_ = RegistrationInfo();
  return PrivetHttpServer::REG_ERROR_OK;
}

void Printer::GetRegistrationServerError(std::string* description) {
  DCHECK_EQ(reg_info_.state, RegistrationInfo::DEV_REG_REGISTRATION_ERROR) <<
      "Method shouldn't be called when not needed.";

  *description = reg_info_.error_description;
}

void Printer::CreateInfo(PrivetHttpServer::DeviceInfo* info) {
  // TODO(maksymb): Replace "text" with constants.

  *info = PrivetHttpServer::DeviceInfo();
  info->version = "1.0";
  info->name = kPrinterName;
  info->description = kPrinterDescription;
  info->url = kCloudPrintUrl;
  info->id = reg_info_.device_id;
  info->device_state = "idle";
  info->connection_state = "offline";
  info->manufacturer = "Google";
  info->model = "Prototype";
  info->serial_number = "2.3.5.7.13.17.19.31.61.89.107.127.521.607.1279.2203";
  info->firmware = "3.7.31.127.8191.131071.524287.2147483647";
  info->uptime = static_cast<int>((base::Time::Now() - starttime_).InSeconds());

  info->x_privet_token = xtoken_.GenerateXToken();

  if (reg_info_.state == RegistrationInfo::DEV_REG_UNREGISTERED)
    info->api.push_back("/privet/register");

  info->type.push_back("printer");
}

void Printer::OnRegistrationStartResponseParsed(
    const std::string& registration_token,
    const std::string& complete_invite_url,
    const std::string& device_id) {
  reg_info_.state = RegistrationInfo::DEV_REG_REGISTRATION_CLAIM_TOKEN_READY;
  reg_info_.device_id = device_id;
  reg_info_.registration_token = registration_token;
  reg_info_.complete_invite_url = complete_invite_url;
}

void Printer::OnGetAuthCodeResponseParsed(const std::string& refresh_token) {
  reg_info_.state = RegistrationInfo::DEV_REG_REGISTERED;
  reg_info_.refresh_token = refresh_token;
  SaveToFile(base::FilePath(kPrinterStatePath));
}

void Printer::OnRegistrationError(const std::string& description) {
  LOG(ERROR) << "server_error: " << description;

  // TODO(maksymb): Implement waiting after error and timeout of registration.
  reg_info_.state = RegistrationInfo::DEV_REG_REGISTRATION_ERROR;
  reg_info_.error_description = description;
}

PrivetHttpServer::RegistrationErrorStatus Printer::CheckCommonRegErrors(
    const std::string& user) const {
  DCHECK(!IsRegistered());

  if (reg_info_.state != RegistrationInfo::DEV_REG_UNREGISTERED &&
      user != reg_info_.user) {
    return PrivetHttpServer::REG_ERROR_DEVICE_BUSY;
  }

  if (reg_info_.state == RegistrationInfo::DEV_REG_REGISTRATION_ERROR)
    return PrivetHttpServer::REG_ERROR_SERVER_ERROR;

  return PrivetHttpServer::REG_ERROR_OK;
}

std::string Printer::GenerateProxyId() const {
  return "{" + base::GenerateGUID() +"}";
}

std::vector<std::string> Printer::CreateTxt() const {
  std::vector<std::string> txt;
  txt.push_back("txtvers=1");
  txt.push_back("ty=" + std::string(kPrinterName));
  txt.push_back("note=" + std::string(kPrinterDescription));
  txt.push_back("url=" + std::string(kCloudPrintUrl));
  txt.push_back("type=printer");
  txt.push_back("id=" + reg_info_.device_id);
  txt.push_back("cs=offline");

  return txt;
}

void Printer::SaveToFile(const base::FilePath& file_path) const {
  base::DictionaryValue json;
  // TODO(maksymb): Get rid of in-place constants.
  if (IsRegistered()) {
    json.SetBoolean("registered", true);
    json.SetString("user", reg_info_.user);
    json.SetString("device_id", reg_info_.device_id);
    json.SetString("refresh_token", reg_info_.refresh_token);
  } else {
    json.SetBoolean("registered", false);
  }

  std::string json_str;
  base::JSONWriter::WriteWithOptions(&json,
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &json_str);
  if (!file_util::WriteFile(file_path, json_str.data(),
                            static_cast<int>(json_str.size()))) {
    LOG(ERROR) << "Cannot write state.";
  }
  LOG(INFO) << "State written to file.";
}

bool Printer::LoadFromFile(const base::FilePath& file_path) {
  if (!base::PathExists(file_path)) {
    LOG(INFO) << "Registration info is not found. Printer is unregistered.";
    return false;
  }

  LOG(INFO) << "Loading registration info from file.";
  std::string json_str;
  if (!file_util::ReadFileToString(file_path, &json_str)) {
    LOG(ERROR) << "Cannot open file.";
    return false;
  }

  scoped_ptr<base::Value> json_val(base::JSONReader::Read(json_str));
  base::DictionaryValue* json = NULL;
  if (!json_val || !json_val->GetAsDictionary(&json)) {
    LOG(ERROR) << "Cannot read JSON dictionary from file.";
    return false;
  }

  bool registered = false;
  if (!json->GetBoolean("registered", &registered)) {
    LOG(ERROR) << "Cannot parse |registered| state.";
    return false;
  }

  if (!registered) {
    reg_info_ = RegistrationInfo();
    return true;
  }

  std::string user;
  if (!json->GetString("user", &user)) {
    LOG(ERROR) << "Cannot parse |user|.";
    return false;
  }

  std::string device_id;
  if (!json->GetString("device_id", &device_id)) {
    LOG(ERROR) << "Cannot parse |device_id|.";
    return false;
  }

  std::string refresh_token;
  if (!json->GetString("refresh_token", &refresh_token)) {
    LOG(ERROR) << "Cannot parse |refresh_token|.";
    return false;
  }

  reg_info_ = RegistrationInfo();
  reg_info_.state = RegistrationInfo::DEV_REG_REGISTERED;
  reg_info_.user = user;
  reg_info_.device_id = device_id;
  reg_info_.refresh_token = refresh_token;

  return true;
}

