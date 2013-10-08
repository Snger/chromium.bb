// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/devtools_adb_bridge.h"

#include <map>
#include <vector>

#include "base/base64.h"
#include "base/bind.h"
#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/json/json_reader.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread.h"
#include "base/values.h"
#include "chrome/browser/devtools/adb/android_rsa.h"
#include "chrome/browser/devtools/adb_client_socket.h"
#include "chrome/browser/devtools/adb_web_socket.h"
#include "chrome/browser/devtools/devtools_protocol.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile.h"
#include "components/browser_context_keyed_service/browser_context_dependency_manager.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_client_host.h"
#include "content/public/browser/devtools_external_agent_proxy.h"
#include "content/public/browser/devtools_external_agent_proxy_delegate.h"
#include "content/public/browser/devtools_manager.h"
#include "crypto/rsa_private_key.h"
#include "net/base/net_errors.h"

using content::BrowserThread;

namespace {

const char kDevToolsAdbBridgeThreadName[] = "Chrome_DevToolsADBThread";
const char kHostDevicesCommand[] = "host:devices";
const char kHostTransportCommand[] = "host:transport:%s|%s";
const char kLocalAbstractCommand[] = "localabstract:%s";
const char kDeviceModelCommand[] = "shell:getprop ro.product.model";
const char kOpenedUnixSocketsCommand[] = "shell:cat /proc/net/unix";
const char kListProcessesCommand[] = "shell:ps";
const char kDumpsysCommand[] = "shell:dumpsys window policy";
const char kDumpsysScreenSizePrefix[] = "mStable=";

const char kPageListRequest[] = "GET /json HTTP/1.1\r\n\r\n";
const char kVersionRequest[] = "GET /json/version HTTP/1.1\r\n\r\n";
const char kClosePageRequest[] = "GET /json/close/%s HTTP/1.1\r\n\r\n";
const char kNewPageRequest[] = "GET /json/new HTTP/1.1\r\n\r\n";
const char kActivatePageRequest[] =
    "GET /json/activate/%s HTTP/1.1\r\n\r\n";
const int kAdbPort = 5037;
const int kBufferSize = 16 * 1024;
const int kAdbPollingIntervalMs = 1000;

const char kUrlParam[] = "url";
const char kPageReloadCommand[] = "Page.reload";
const char kPageNavigateCommand[] = "Page.navigate";

#if defined(DEBUG_DEVTOOLS)
const char kChrome[] = "Chrome";
const char kLocalChrome[] = "Local Chrome";
#endif  // defined(DEBUG_DEVTOOLS)

typedef DevToolsAdbBridge::Callback Callback;
typedef std::vector<scoped_refptr<DevToolsAdbBridge::AndroidDevice> >
    AndroidDevices;
typedef base::Callback<void(const AndroidDevices&)> AndroidDevicesCallback;


// AdbDeviceImpl --------------------------------------------------------------

class AdbDeviceImpl : public DevToolsAdbBridge::AndroidDevice {
 public:
  explicit AdbDeviceImpl(const std::string& serial);
  virtual void RunCommand(const std::string& command,
                          const CommandCallback& callback) OVERRIDE;
  virtual void OpenSocket(const std::string& name,
                          const SocketCallback& callback) OVERRIDE;
 private:
  virtual ~AdbDeviceImpl() {}
};

AdbDeviceImpl::AdbDeviceImpl(const std::string& serial)
    : AndroidDevice(serial) {
}

void AdbDeviceImpl::RunCommand(const std::string& command,
                               const CommandCallback& callback) {
  std::string query = base::StringPrintf(kHostTransportCommand,
                                         serial().c_str(), command.c_str());
  AdbClientSocket::AdbQuery(kAdbPort, query, callback);
}

void AdbDeviceImpl::OpenSocket(const std::string& name,
                               const SocketCallback& callback) {
  std::string socket_name =
      base::StringPrintf(kLocalAbstractCommand, name.c_str());
  AdbClientSocket::TransportQuery(kAdbPort, serial(), socket_name, callback);
}


// UsbDeviceImpl --------------------------------------------------------------

class UsbDeviceImpl : public DevToolsAdbBridge::AndroidDevice {
 public:
  explicit UsbDeviceImpl(AndroidUsbDevice* device);
  virtual void RunCommand(const std::string& command,
                          const CommandCallback& callback) OVERRIDE;
  virtual void OpenSocket(const std::string& name,
                          const SocketCallback& callback) OVERRIDE;

 private:
  void OnOpenSocket(const SocketCallback& callback,
                    net::StreamSocket* socket,
                    int result);
  void OpenedForCommand(const CommandCallback& callback,
                        net::StreamSocket* socket,
                        int result);
  void OnRead(net::StreamSocket* socket,
              scoped_refptr<net::IOBuffer> buffer,
              const std::string& data,
              const CommandCallback& callback,
              int result);

  virtual ~UsbDeviceImpl() {}
  scoped_refptr<AndroidUsbDevice> device_;
};


UsbDeviceImpl::UsbDeviceImpl(AndroidUsbDevice* device)
    : AndroidDevice(device->serial()),
      device_(device) {
  device_->InitOnCallerThread();
}

void UsbDeviceImpl::RunCommand(const std::string& command,
                               const CommandCallback& callback) {
  net::StreamSocket* socket = device_->CreateSocket(command);
  int result = socket->Connect(base::Bind(&UsbDeviceImpl::OpenedForCommand,
                                          this, callback, socket));
  if (result != net::ERR_IO_PENDING)
    callback.Run(result, std::string());
}

void UsbDeviceImpl::OpenSocket(const std::string& name,
                               const SocketCallback& callback) {
  std::string socket_name =
      base::StringPrintf(kLocalAbstractCommand, name.c_str());
  net::StreamSocket* socket = device_->CreateSocket(socket_name);
  int result = socket->Connect(base::Bind(&UsbDeviceImpl::OnOpenSocket, this,
                                          callback, socket));
  if (result != net::ERR_IO_PENDING)
    callback.Run(result, NULL);
}

void UsbDeviceImpl::OnOpenSocket(const SocketCallback& callback,
                  net::StreamSocket* socket,
                  int result) {
  callback.Run(result, result == net::OK ? socket : NULL);
}

void UsbDeviceImpl::OpenedForCommand(const CommandCallback& callback,
                                     net::StreamSocket* socket,
                                     int result) {
  if (result != net::OK) {
    callback.Run(result, std::string());
    return;
  }
  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(kBufferSize);
  result = socket->Read(buffer, kBufferSize,
                        base::Bind(&UsbDeviceImpl::OnRead, this,
                                   socket, buffer, std::string(), callback));
  if (result != net::ERR_IO_PENDING)
    OnRead(socket, buffer, std::string(), callback, result);
}

void UsbDeviceImpl::OnRead(net::StreamSocket* socket,
                           scoped_refptr<net::IOBuffer> buffer,
                           const std::string& data,
                           const CommandCallback& callback,
                           int result) {
  if (result <= 0) {
    callback.Run(result, result == 0 ? data : std::string());
    delete socket;
    return;
  }

  std::string new_data = data + std::string(buffer->data(), result);
  result = socket->Read(buffer, kBufferSize,
                        base::Bind(&UsbDeviceImpl::OnRead, this,
                                   socket, buffer, new_data, callback));
  if (result != net::ERR_IO_PENDING)
    OnRead(socket, buffer, new_data, callback, result);
}


// AdbCountDevicesCommand -----------------------------------------------------

class AdbCountDevicesCommand : public base::RefCountedThreadSafe<
    AdbCountDevicesCommand,
    content::BrowserThread::DeleteOnUIThread> {
 public:
  typedef base::Callback<void(int)> Callback;

  AdbCountDevicesCommand(
      scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread> adb_thread,
      const Callback& callback);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<AdbCountDevicesCommand>;

  virtual ~AdbCountDevicesCommand();
  void RequestAdbDeviceCount();
  void ReceivedAdbDeviceCount(int result, const std::string& response);
  void Respond(int count);

  scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread> adb_thread_;
  Callback callback_;
};

AdbCountDevicesCommand::AdbCountDevicesCommand(
    scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread> adb_thread,
    const Callback& callback)
    : adb_thread_(adb_thread),
      callback_(callback) {
  adb_thread_->message_loop()->PostTask(
      FROM_HERE, base::Bind(&AdbCountDevicesCommand::RequestAdbDeviceCount,
                            this));
}

AdbCountDevicesCommand::~AdbCountDevicesCommand() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void AdbCountDevicesCommand::RequestAdbDeviceCount() {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  AdbClientSocket::AdbQuery(
      kAdbPort, kHostDevicesCommand,
      base::Bind(&AdbCountDevicesCommand::ReceivedAdbDeviceCount, this));
}

void AdbCountDevicesCommand::ReceivedAdbDeviceCount(
    int result,
    const std::string& response) {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  std::vector<std::string> serials;
  Tokenize(response, "\n", &serials);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&AdbCountDevicesCommand::Respond, this, serials.size()));
}

void AdbCountDevicesCommand::Respond(int count) {
  callback_.Run(count);
}


// AdbPagesCommand ------------------------------------------------------------

class AdbPagesCommand : public base::RefCountedThreadSafe<
    AdbPagesCommand,
    content::BrowserThread::DeleteOnUIThread> {
 public:
  typedef base::Callback<void(DevToolsAdbBridge::RemoteDevices*)> Callback;

  AdbPagesCommand(
      scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread> adb_thread,
      crypto::RSAPrivateKey* rsa_key, bool discover_usb,
      const Callback& callback);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<AdbPagesCommand>;

  virtual ~AdbPagesCommand();
  void ReceivedUsbDevices(const AndroidUsbDevices& usb_devices);
  void WrapUsbDevices(const AndroidUsbDevices& usb_devices);

  void ReceivedAdbDevices(int result, const std::string& response);
  void ProcessSerials();
  void ReceivedModel(int result, const std::string& response);
  void ReceivedSockets(int result, const std::string& response);
  void ReceivedDumpsys(int result, const std::string& response);
  void ReceivedProcesses(int result, const std::string& response);
  void ProcessSockets();
  void ReceivedVersion(int result, const std::string& response);
  void ReceivedPages(int result, const std::string& response);
  void Respond();
  void ParseSocketsList(const std::string& response);
  void ParseProcessList(const std::string& response);
  void ParseDumpsysResponse(const std::string& response);
  void ParseScreenSize(const std::string& str);

  scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread> adb_thread_;
  Callback callback_;
  AndroidDevices devices_;
  DevToolsAdbBridge::RemoteBrowsers browsers_;
  scoped_ptr<DevToolsAdbBridge::RemoteDevices> remote_devices_;
};

AdbPagesCommand::AdbPagesCommand(
    scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread> adb_thread,
    crypto::RSAPrivateKey* rsa_key, bool discover_usb,
    const Callback& callback)
    : adb_thread_(adb_thread),
      callback_(callback) {
  remote_devices_.reset(new DevToolsAdbBridge::RemoteDevices());

  if (discover_usb) {
    AndroidUsbDevice::Enumerate(rsa_key,
        base::Bind(&AdbPagesCommand::ReceivedUsbDevices, this));
  } else {
    ReceivedUsbDevices(AndroidUsbDevices());
  }
}

AdbPagesCommand::~AdbPagesCommand() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
}

void AdbPagesCommand::ReceivedUsbDevices(const AndroidUsbDevices& usb_devices) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  adb_thread_->message_loop()->PostTask(
      FROM_HERE, base::Bind(&AdbPagesCommand::WrapUsbDevices, this,
                            usb_devices));
}

void AdbPagesCommand::WrapUsbDevices(const AndroidUsbDevices& usb_devices) {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());

#if defined(DEBUG_DEVTOOLS)
  devices_.push_back(new AdbDeviceImpl(""));  // For desktop remote debugging.
#endif  // defined(DEBUG_DEVTOOLS)

  for (AndroidUsbDevices::const_iterator it = usb_devices.begin();
       it != usb_devices.end(); ++it) {
    devices_.push_back(new UsbDeviceImpl(*it));
  }

  AdbClientSocket::AdbQuery(
      kAdbPort, kHostDevicesCommand,
      base::Bind(&AdbPagesCommand::ReceivedAdbDevices, this));
}

void AdbPagesCommand::ReceivedAdbDevices(
    int result,
    const std::string& response) {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  std::vector<std::string> serials;
  Tokenize(response, "\n", &serials);
  for (size_t i = 0; i < serials.size(); ++i) {
    std::vector<std::string> tokens;
    Tokenize(serials[i], "\t ", &tokens);
    devices_.push_back(new AdbDeviceImpl(tokens[0]));
  }
  ProcessSerials();
}

void AdbPagesCommand::ProcessSerials() {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  if (devices_.size() == 0) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&AdbPagesCommand::Respond, this));
    return;
  }

#if defined(DEBUG_DEVTOOLS)
  // For desktop remote debugging.
  if (devices_.back()->serial().empty()) {
    scoped_refptr<DevToolsAdbBridge::AndroidDevice> device =
        devices_.back();
    device->set_model(kLocalChrome);
    remote_devices_->push_back(
        new DevToolsAdbBridge::RemoteDevice(device));
    scoped_refptr<DevToolsAdbBridge::RemoteBrowser> remote_browser =
        new DevToolsAdbBridge::RemoteBrowser(
            adb_thread_, device, std::string());
    remote_browser->set_product(kChrome);
    remote_devices_->back()->AddBrowser(remote_browser);
    browsers_.push_back(remote_browser);
    device->HttpQuery(
        std::string(), kVersionRequest,
        base::Bind(&AdbPagesCommand::ReceivedVersion, this));
    return;
  }
#endif  // defined(DEBUG_DEVTOOLS)

  scoped_refptr<DevToolsAdbBridge::AndroidDevice> device = devices_.back();
  device->RunCommand(kDeviceModelCommand,
                     base::Bind(&AdbPagesCommand::ReceivedModel, this));
}

void AdbPagesCommand::ReceivedModel(int result, const std::string& response) {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  if (result < 0) {
    devices_.pop_back();
    ProcessSerials();
    return;
  }
  scoped_refptr<DevToolsAdbBridge::AndroidDevice> device = devices_.back();
  device->set_model(response);
  remote_devices_->push_back(
      new DevToolsAdbBridge::RemoteDevice(device));
  device->RunCommand(kOpenedUnixSocketsCommand,
                     base::Bind(&AdbPagesCommand::ReceivedSockets, this));
}

void AdbPagesCommand::ReceivedSockets(int result,
                                      const std::string& response) {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  if (result < 0) {
    devices_.pop_back();
    ProcessSerials();
    return;
  }

  ParseSocketsList(response);
  scoped_refptr<DevToolsAdbBridge::AndroidDevice> device = devices_.back();
  device->RunCommand(kDumpsysCommand,
                     base::Bind(&AdbPagesCommand::ReceivedDumpsys, this));
}

void AdbPagesCommand::ReceivedDumpsys(int result,
                                      const std::string& response) {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  if (result >= 0)
    ParseDumpsysResponse(response);

  scoped_refptr<DevToolsAdbBridge::AndroidDevice> device = devices_.back();
  device->RunCommand(kListProcessesCommand,
                     base::Bind(&AdbPagesCommand::ReceivedProcesses, this));
}

void AdbPagesCommand::ReceivedProcesses(int result,
                                        const std::string& response) {
  if (result >= 0)
    ParseProcessList(response);

  if (browsers_.size() == 0) {
    devices_.pop_back();
    ProcessSerials();
  } else {
    ProcessSockets();
  }
}

void AdbPagesCommand::ProcessSockets() {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  if (browsers_.size() == 0) {
    devices_.pop_back();
    ProcessSerials();
  } else {
    scoped_refptr<DevToolsAdbBridge::AndroidDevice> device = devices_.back();
    device->HttpQuery(browsers_.back()->socket(), kVersionRequest,
                      base::Bind(&AdbPagesCommand::ReceivedVersion, this));
  }
}

void AdbPagesCommand::ReceivedVersion(int result,
                     const std::string& response) {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  if (result < 0) {
    browsers_.pop_back();
    ProcessSockets();
    return;
  }

  // Parse version, append to package name if available,
  scoped_ptr<base::Value> value(base::JSONReader::Read(response));
  base::DictionaryValue* dict;
  if (value && value->GetAsDictionary(&dict)) {
    std::string browser;
    if (dict->GetString("Browser", &browser)) {
      std::vector<std::string> parts;
      Tokenize(browser, "/", &parts);
      if (parts.size() == 2) {
        if (parts[0] != "Version")  // WebView has this for legacy reasons.
          browsers_.back()->set_product(parts[0]);
        browsers_.back()->set_version(parts[1]);
      } else {
        browsers_.back()->set_version(browser);
      }
    }
  }

  scoped_refptr<DevToolsAdbBridge::AndroidDevice> device = devices_.back();
  device->HttpQuery(browsers_.back()->socket(), kPageListRequest,
                    base::Bind(&AdbPagesCommand::ReceivedPages, this));
}

void AdbPagesCommand::ReceivedPages(int result,
                   const std::string& response) {
  DCHECK_EQ(adb_thread_->message_loop(), base::MessageLoop::current());
  scoped_refptr<DevToolsAdbBridge::RemoteBrowser> browser = browsers_.back();
  browsers_.pop_back();
  if (result < 0) {
    ProcessSockets();
    return;
  }

  scoped_ptr<base::Value> value(base::JSONReader::Read(response));
  base::ListValue* list_value;
  if (!value || !value->GetAsList(&list_value)) {
    ProcessSockets();
    return;
  }

  base::Value* item;

  for (size_t i = 0; i < list_value->GetSize(); ++i) {
    list_value->Get(i, &item);
    base::DictionaryValue* dict;
    if (!item || !item->GetAsDictionary(&dict))
      continue;
    browser->AddPage(new DevToolsAdbBridge::RemotePage(
        adb_thread_, browser->device(), browser->socket(), *dict));
  }
  ProcessSockets();
}

void AdbPagesCommand::Respond() {
  callback_.Run(remote_devices_.release());
}

void AdbPagesCommand::ParseSocketsList(const std::string& response) {
  // On Android, '/proc/net/unix' looks like this:
  //
  // Num       RefCount Protocol Flags    Type St Inode Path
  // 00000000: 00000002 00000000 00010000 0001 01 331813 /dev/socket/zygote
  // 00000000: 00000002 00000000 00010000 0001 01 358606 @xxx_devtools_remote
  // 00000000: 00000002 00000000 00010000 0001 01 347300 @yyy_devtools_remote
  //
  // We need to find records with paths starting from '@' (abstract socket)
  // and containing "devtools_remote". We have to extract the inode number
  // in order to find the owning process name.

  scoped_refptr<DevToolsAdbBridge::RemoteDevice> remote_device =
      remote_devices_->back();

  std::vector<std::string> entries;
  Tokenize(response, "\n", &entries);
  const std::string channel_pattern =
      base::StringPrintf(kDevToolsChannelNameFormat, "");
  for (size_t i = 1; i < entries.size(); ++i) {
    std::vector<std::string> fields;
    Tokenize(entries[i], " \r", &fields);
    if (fields.size() < 8)
      continue;
    if (fields[3] != "00010000" || fields[5] != "01")
      continue;
    std::string path_field = fields[7];
    if (path_field.size() < 1 || path_field[0] != '@')
      continue;
    size_t socket_name_pos = path_field.find(channel_pattern);
    if (socket_name_pos == std::string::npos)
      continue;

    std::string socket = path_field.substr(1);
    scoped_refptr<DevToolsAdbBridge::RemoteBrowser> remote_browser =
        new DevToolsAdbBridge::RemoteBrowser(
            adb_thread_, remote_device->device(), socket);

    std::string product = path_field.substr(1, socket_name_pos - 1);
    product[0] = base::ToUpperASCII(product[0]);
    remote_browser->set_product(product);

    size_t socket_name_end = socket_name_pos + channel_pattern.size();
    if (socket_name_end < path_field.size() &&
        path_field[socket_name_end] == '_') {
      remote_browser->set_pid(path_field.substr(socket_name_end + 1));
    }
    remote_device->AddBrowser(remote_browser);
  }
  browsers_ = remote_device->browsers();
}

void AdbPagesCommand::ParseProcessList(const std::string& response) {
  // On Android, 'ps' output looks like this:
  // USER PID PPID VSIZE RSS WCHAN PC ? NAME
  typedef std::map<std::string, std::string> StringMap;
  StringMap pid_to_package;
  std::vector<std::string> entries;
  Tokenize(response, "\n", &entries);
  for (size_t i = 1; i < entries.size(); ++i) {
    std::vector<std::string> fields;
    Tokenize(entries[i], " \r", &fields);
    if (fields.size() < 9)
      continue;
    pid_to_package[fields[1]] = fields[8];
  }
  DevToolsAdbBridge::RemoteBrowsers browsers =
      remote_devices_->back()->browsers();
  for (DevToolsAdbBridge::RemoteBrowsers::iterator it = browsers.begin();
      it != browsers.end(); ++it) {
    StringMap::iterator pit = pid_to_package.find((*it)->pid());
    if (pit != pid_to_package.end())
      (*it)->set_package(pit->second);
  }
}

void AdbPagesCommand::ParseDumpsysResponse(const std::string& response) {
  std::vector<std::string> lines;
  Tokenize(response, "\r", &lines);
  for (size_t i = 0; i < lines.size(); ++i) {
    std::string line = lines[i];
    size_t pos = line.find(kDumpsysScreenSizePrefix);
    if (pos != std::string::npos) {
      ParseScreenSize(
          line.substr(pos + std::string(kDumpsysScreenSizePrefix).size()));
      break;
    }
  }
}

void AdbPagesCommand::ParseScreenSize(const std::string& str) {
  std::vector<std::string> pairs;
  Tokenize(str, "-", &pairs);
  if (pairs.size() != 2)
    return;

  int width;
  int height;
  std::vector<std::string> numbers;
  Tokenize(pairs[1].substr(1, pairs[1].size() - 2), ",", &numbers);
  if (numbers.size() != 2 ||
      !base::StringToInt(numbers[0], &width) ||
      !base::StringToInt(numbers[1], &height))
    return;

  remote_devices_->back()->SetScreenSize(gfx::Size(width, height));
}


// AdbProtocolCommand ---------------------------------------------------------

class AdbProtocolCommand : public AdbWebSocket::Delegate {
 public:
  AdbProtocolCommand(
      scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread> adb_thread,
      scoped_refptr<DevToolsAdbBridge::AndroidDevice> device,
      const std::string& socket_name,
      const std::string& debug_url,
      const std::string& command);

 private:
  virtual void OnSocketOpened() OVERRIDE;
  virtual void OnFrameRead(const std::string& message) OVERRIDE;
  virtual void OnSocketClosed(bool closed_by_device) OVERRIDE;
  virtual bool ProcessIncomingMessage(const std::string& message) OVERRIDE;

  scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread> adb_thread_;
  const std::string command_;
  scoped_refptr<AdbWebSocket> web_socket_;

  DISALLOW_COPY_AND_ASSIGN(AdbProtocolCommand);
};

AdbProtocolCommand::AdbProtocolCommand(
    scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread> adb_thread,
    scoped_refptr<DevToolsAdbBridge::AndroidDevice> device,
    const std::string& socket_name,
    const std::string& debug_url,
    const std::string& command)
    : adb_thread_(adb_thread),
      command_(command) {
  web_socket_ = new AdbWebSocket(
      device, socket_name, debug_url, adb_thread_->message_loop(), this);
}

void AdbProtocolCommand::OnSocketOpened() {
  web_socket_->SendFrame(command_);
  web_socket_->Disconnect();
}

void AdbProtocolCommand::OnFrameRead(const std::string& message) {}

void AdbProtocolCommand::OnSocketClosed(bool closed_by_device) {
  delete this;
}

bool AdbProtocolCommand::ProcessIncomingMessage(const std::string& message) {
  return false;
}

}  // namespace

const char kDevToolsChannelNameFormat[] = "%s_devtools_remote";

class AgentHostDelegate;

typedef std::map<std::string, AgentHostDelegate*> AgentHostDelegates;

base::LazyInstance<AgentHostDelegates>::Leaky g_host_delegates =
    LAZY_INSTANCE_INITIALIZER;

DevToolsAdbBridge::Wrapper::Wrapper(Profile* profile)
    : bridge_(new DevToolsAdbBridge(profile)) {
}

DevToolsAdbBridge::Wrapper::~Wrapper() {
}

DevToolsAdbBridge* DevToolsAdbBridge::Wrapper::Get() {
  return bridge_.get();
}

// static
DevToolsAdbBridge::Factory* DevToolsAdbBridge::Factory::GetInstance() {
  return Singleton<DevToolsAdbBridge::Factory>::get();
}

// static
DevToolsAdbBridge* DevToolsAdbBridge::Factory::GetForProfile(
    Profile* profile) {
  DevToolsAdbBridge::Wrapper* wrapper =
      static_cast<DevToolsAdbBridge::Wrapper*>(GetInstance()->
          GetServiceForBrowserContext(profile, true));
  return wrapper ? wrapper->Get() : NULL;
}

DevToolsAdbBridge::Factory::Factory()
    : BrowserContextKeyedServiceFactory(
          "DevToolsAdbBridge",
          BrowserContextDependencyManager::GetInstance()) {}

DevToolsAdbBridge::Factory::~Factory() {}

BrowserContextKeyedService*
DevToolsAdbBridge::Factory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new DevToolsAdbBridge::Wrapper(Profile::FromBrowserContext(context));
}


// DevToolsAdbBridge::AndroidDevice -------------------------------------------

DevToolsAdbBridge::AndroidDevice::AndroidDevice(const std::string& serial)
    : serial_(serial) {
}

void DevToolsAdbBridge::AndroidDevice::HttpQuery(
    const std::string& la_name,
    const std::string& request,
    const CommandCallback& callback) {
  OpenSocket(la_name, base::Bind(&AndroidDevice::OnHttpSocketOpened, this,
                                 request, callback));
}

void DevToolsAdbBridge::AndroidDevice::HttpUpgrade(
    const std::string& la_name,
    const std::string& request,
    const SocketCallback& callback) {
  OpenSocket(la_name, base::Bind(&AndroidDevice::OnHttpSocketOpened2, this,
                                 request, callback));
}

DevToolsAdbBridge::AndroidDevice::~AndroidDevice() {
}

void DevToolsAdbBridge::AndroidDevice::OnHttpSocketOpened(
    const std::string& request,
    const CommandCallback& callback,
    int result,
    net::StreamSocket* socket) {
  if (result != net::OK) {
    callback.Run(result, std::string());
    return;
  }
  AdbClientSocket::HttpQuery(socket, request, callback);
}

void DevToolsAdbBridge::AndroidDevice::OnHttpSocketOpened2(
    const std::string& request,
    const SocketCallback& callback,
    int result,
    net::StreamSocket* socket) {
  if (result != net::OK) {
    callback.Run(result, NULL);
    return;
  }
  AdbClientSocket::HttpQuery(socket, request, callback);
}


// AgentHostDelegate ----------------------------------------------------------

class AgentHostDelegate : public content::DevToolsExternalAgentProxyDelegate,
                          public AdbWebSocket::Delegate {
 public:
  AgentHostDelegate(
      const std::string& id,
      scoped_refptr<DevToolsAdbBridge::AndroidDevice> device,
      const std::string& socket_name,
      const std::string& debug_url,
      const std::string& frontend_url,
      base::MessageLoop* adb_message_loop,
      Profile* profile)
      : id_(id),
        serial_(device->serial()),
        frontend_url_(frontend_url),
        adb_message_loop_(adb_message_loop),
        profile_(profile) {
    web_socket_ = new AdbWebSocket(
        device, socket_name, debug_url, adb_message_loop, this);
    g_host_delegates.Get()[id] = this;
  }

  void OpenFrontend() {
    if (!proxy_)
      return;
    DevToolsWindow::OpenExternalFrontend(
        profile_, frontend_url_, proxy_->GetAgentHost().get());
  }

 private:
  virtual ~AgentHostDelegate() {
    g_host_delegates.Get().erase(id_);
  }

  virtual void Attach() OVERRIDE {}

  virtual void Detach() OVERRIDE {
    web_socket_->Disconnect();
  }

  virtual void SendMessageToBackend(const std::string& message) OVERRIDE {
    web_socket_->SendFrame(message);
  }

  virtual void OnSocketOpened() OVERRIDE {
    proxy_.reset(content::DevToolsExternalAgentProxy::Create(this));
    OpenFrontend();
  }

  virtual void OnFrameRead(const std::string& message) OVERRIDE {
    proxy_->DispatchOnClientHost(message);
  }

  virtual void OnSocketClosed(bool closed_by_device) OVERRIDE {
    if (proxy_ && closed_by_device)
      proxy_->ConnectionClosed();
    delete this;
  }

  virtual bool ProcessIncomingMessage(const std::string& message) OVERRIDE {
    return false;
  }

  const std::string id_;
  const std::string serial_;
  const std::string frontend_url_;
  base::MessageLoop* adb_message_loop_;
  Profile* profile_;

  scoped_ptr<content::DevToolsExternalAgentProxy> proxy_;
  scoped_refptr<AdbWebSocket> web_socket_;
  DISALLOW_COPY_AND_ASSIGN(AgentHostDelegate);
};


// DevToolsAdbBridge::RemotePage ----------------------------------------------

DevToolsAdbBridge::RemotePage::RemotePage(
    scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread> adb_thread,
    scoped_refptr<AndroidDevice> device,
    const std::string& socket,
    const base::DictionaryValue& value)
    : adb_thread_(adb_thread),
      device_(device),
      socket_(socket) {
  value.GetString("id", &id_);
  value.GetString("url", &url_);
  value.GetString("title", &title_);
  value.GetString("description", &description_);
  value.GetString("faviconUrl", &favicon_url_);
  value.GetString("webSocketDebuggerUrl", &debug_url_);
  value.GetString("devtoolsFrontendUrl", &frontend_url_);

  if (id_.empty() && !debug_url_.empty())  {
    // Target id is not available until Chrome 26. Use page id at the end of
    // debug_url_ instead. For attached targets the id will remain empty.
    std::vector<std::string> parts;
    Tokenize(debug_url_, "/", &parts);
    id_ = parts[parts.size()-1];
  }

  if (debug_url_.find("ws://") == 0)
    debug_url_ = debug_url_.substr(5);
  else
    debug_url_ = "";

  size_t ws_param = frontend_url_.find("?ws");
  if (ws_param != std::string::npos)
    frontend_url_ = frontend_url_.substr(0, ws_param);
  if (frontend_url_.find("http:") == 0)
    frontend_url_ = "https:" + frontend_url_.substr(5);

  agent_id_ = base::StringPrintf("%s:%s:%s",
      device_->serial().c_str(), socket_.c_str(), id_.c_str());
}

bool DevToolsAdbBridge::RemotePage::HasDevToolsWindow() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return g_host_delegates.Get().find(agent_id_) != g_host_delegates.Get().end();
}

void DevToolsAdbBridge::RemotePage::Inspect(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RequestActivate(
      base::Bind(&RemotePage::InspectOnHandlerThread, this, profile));
}

static void Noop(int, const std::string&) {}

void DevToolsAdbBridge::RemotePage::Activate() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  RequestActivate(base::Bind(&Noop));
}

void DevToolsAdbBridge::RemotePage::Close() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (attached())
    return;
  std::string request = base::StringPrintf(kClosePageRequest, id_.c_str());
  adb_thread_->message_loop()->PostTask(FROM_HERE,
      base::Bind(&AndroidDevice::HttpQuery,
          device_, socket_, request, base::Bind(&Noop)));
}

void DevToolsAdbBridge::RemotePage::Reload() {
  SendProtocolCommand(kPageReloadCommand, NULL);
}

void DevToolsAdbBridge::RemotePage::SendProtocolCommand(
    const std::string& method,
    base::DictionaryValue* params) {
  if (attached())
    return;
  DevToolsProtocol::Command command(1, method, params);
  new AdbProtocolCommand(
      adb_thread_, device_, socket_, debug_url_, command.Serialize());
}

DevToolsAdbBridge::RemotePage::~RemotePage() {
}

void DevToolsAdbBridge::RemotePage::RequestActivate(
    const CommandCallback& callback) {
  std::string request = base::StringPrintf(kActivatePageRequest, id_.c_str());
  adb_thread_->message_loop()->PostTask(FROM_HERE,
      base::Bind(&AndroidDevice::HttpQuery,
          device_, socket_, request, callback));
}

void DevToolsAdbBridge::RemotePage::InspectOnHandlerThread(
    Profile* profile, int result, const std::string& response) {
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RemotePage::InspectOnUIThread, this, profile));
}

void DevToolsAdbBridge::RemotePage::InspectOnUIThread(Profile* profile) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  AgentHostDelegates::iterator it =
      g_host_delegates.Get().find(agent_id_);
  if (it != g_host_delegates.Get().end()) {
    it->second->OpenFrontend();
  } else if (!attached()) {
    new AgentHostDelegate(
        agent_id_, device_, socket_, debug_url_,
        frontend_url_, adb_thread_->message_loop(), profile);
  }
}


// DevToolsAdbBridge::RemoteBrowser -------------------------------------------

DevToolsAdbBridge::RemoteBrowser::RemoteBrowser(
    scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread> adb_thread,
    scoped_refptr<AndroidDevice> device,
    const std::string& socket)
    : adb_thread_(adb_thread),
      device_(device),
      socket_(socket) {
}

void DevToolsAdbBridge::RemoteBrowser::Open(const std::string& url) {
  adb_thread_->message_loop()->PostTask(FROM_HERE,
      base::Bind(&AndroidDevice::HttpQuery,
          device_, socket_, kNewPageRequest,
          base::Bind(&RemoteBrowser::PageCreatedOnHandlerThread, this, url)));
}

void DevToolsAdbBridge::RemoteBrowser::PageCreatedOnHandlerThread(
    const std::string& url, int result, const std::string& response) {
  if (result < 0)
    return;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&RemoteBrowser::PageCreatedOnUIThread, this, response, url));
}

void DevToolsAdbBridge::RemoteBrowser::PageCreatedOnUIThread(
    const std::string& response, const std::string& url) {
  scoped_ptr<base::Value> value(base::JSONReader::Read(response));
  base::DictionaryValue* dict;
  if (value && value->GetAsDictionary(&dict)) {
    scoped_refptr<RemotePage> new_page =
        new RemotePage(adb_thread_, device_, socket_, *dict);
    base::DictionaryValue params;
    params.SetString(kUrlParam, url);
    new_page->SendProtocolCommand(kPageNavigateCommand, &params);
  }
}

DevToolsAdbBridge::RemoteBrowser::~RemoteBrowser() {
}


// DevToolsAdbBridge::RemoteDevice --------------------------------------------

DevToolsAdbBridge::RemoteDevice::RemoteDevice(
    scoped_refptr<AndroidDevice> device)
    : device_(device) {
}

DevToolsAdbBridge::RemoteDevice::~RemoteDevice() {
}


// DevToolsAdbBridge::RefCountedAdbThread -------------------------------------

DevToolsAdbBridge::RefCountedAdbThread*
DevToolsAdbBridge::RefCountedAdbThread::instance_ = NULL;

// static
scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread>
DevToolsAdbBridge::RefCountedAdbThread::GetInstance() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!instance_)
    new RefCountedAdbThread();
  return instance_;
}

DevToolsAdbBridge::RefCountedAdbThread::RefCountedAdbThread() {
  instance_ = this;
  thread_ = new base::Thread(kDevToolsAdbBridgeThreadName);
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  if (!thread_->StartWithOptions(options)) {
    delete thread_;
    thread_ = NULL;
  }
}

base::MessageLoop* DevToolsAdbBridge::RefCountedAdbThread::message_loop() {
  return thread_ ? thread_->message_loop() : NULL;
}

// static
void DevToolsAdbBridge::RefCountedAdbThread::StopThread(base::Thread* thread) {
  thread->Stop();
}

DevToolsAdbBridge::RefCountedAdbThread::~RefCountedAdbThread() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  instance_ = NULL;
  if (!thread_)
    return;
  // Shut down thread on FILE thread to join into IO.
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE,
      base::Bind(&RefCountedAdbThread::StopThread, thread_));
}


// DevToolsAdbBridge ----------------------------------------------------------

DevToolsAdbBridge::DevToolsAdbBridge(Profile* profile)
    : adb_thread_(RefCountedAdbThread::GetInstance()),
      has_message_loop_(adb_thread_->message_loop() != NULL),
      discover_usb_devices_(false) {
  rsa_key_.reset(AndroidRSAPrivateKey(profile));
}

void DevToolsAdbBridge::AddListener(Listener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (listeners_.empty())
    RequestRemoteDevices();
  listeners_.push_back(listener);
}

void DevToolsAdbBridge::RemoveListener(Listener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Listeners::iterator it =
      std::find(listeners_.begin(), listeners_.end(), listener);
  DCHECK(it != listeners_.end());
  listeners_.erase(it);
}

void DevToolsAdbBridge::CountDevices(
    const base::Callback<void(int)>& callback) {
  if (discover_usb_devices_) {
    // Count raw devices, adb included.
    AndroidUsbDevice::CountDevices(callback);
    return;
  }
  new AdbCountDevicesCommand(adb_thread_, callback);
}

DevToolsAdbBridge::~DevToolsAdbBridge() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(listeners_.empty());
}

void DevToolsAdbBridge::RequestRemoteDevices() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!has_message_loop_)
    return;
  new AdbPagesCommand(
      adb_thread_, rsa_key_.get(), discover_usb_devices_,
      base::Bind(&DevToolsAdbBridge::ReceivedRemoteDevices, this));
}

void DevToolsAdbBridge::ReceivedRemoteDevices(RemoteDevices* devices_ptr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  scoped_ptr<RemoteDevices> devices(devices_ptr);

  Listeners copy(listeners_);
  for (Listeners::iterator it = copy.begin(); it != copy.end(); ++it)
    (*it)->RemoteDevicesChanged(devices.get());

  if (listeners_.empty())
    return;

  BrowserThread::PostDelayedTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DevToolsAdbBridge::RequestRemoteDevices, this),
      base::TimeDelta::FromMilliseconds(kAdbPollingIntervalMs));
}
