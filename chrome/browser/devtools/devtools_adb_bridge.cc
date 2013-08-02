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
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/devtools/tethering_adb_filter.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
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

static const char kDevToolsAdbBridgeThreadName[] = "Chrome_DevToolsADBThread";
static const char kHostDevicesCommand[] = "host:devices";
static const char kHostTransportCommand[] = "host:transport:%s|%s";
static const char kLocalAbstractCommand[] = "localabstract:%s";
static const char kDeviceModelCommand[] = "shell:getprop ro.product.model";
static const char kUnknownModel[] = "Unknown";
static const char kOpenedUnixSocketsCommand[] = "shell:cat /proc/net/unix";

static const char kPageListRequest[] = "GET /json HTTP/1.1\r\n\r\n";
static const char kVersionRequest[] = "GET /json/version HTTP/1.1\r\n\r\n";
const int kAdbPort = 5037;
const int kBufferSize = 16 * 1024;
const int kAdbPollingIntervalMs = 1000;

typedef DevToolsAdbBridge::Callback Callback;
typedef DevToolsAdbBridge::PagesCallback PagesCallback;
typedef std::vector<scoped_refptr<DevToolsAdbBridge::AndroidDevice> >
    AndroidDevices;
typedef base::Callback<void(const AndroidDevices&)> AndroidDevicesCallback;

class AdbDeviceImpl : public DevToolsAdbBridge::AndroidDevice {
 public:
  explicit AdbDeviceImpl(const std::string& serial)
      : AndroidDevice(serial) {
  }

  virtual void RunCommand(const std::string& command,
                          const CommandCallback& callback) OVERRIDE {
    std::string query = base::StringPrintf(kHostTransportCommand,
                                           serial().c_str(), command.c_str());
    AdbClientSocket::AdbQuery(kAdbPort, query, callback);
  }

  virtual void OpenSocket(const std::string& name,
                          const SocketCallback& callback) OVERRIDE {
    std::string socket_name =
        base::StringPrintf(kLocalAbstractCommand, name.c_str());
    AdbClientSocket::TransportQuery(kAdbPort, serial(), socket_name, callback);
  }
 private:
  virtual ~AdbDeviceImpl() {}
};

class UsbDeviceImpl : public DevToolsAdbBridge::AndroidDevice {
 public:
  explicit UsbDeviceImpl(AndroidUsbDevice* device)
      : AndroidDevice(device->serial()),
        device_(device) {
  }

  virtual void RunCommand(const std::string& command,
                          const CommandCallback& callback) OVERRIDE {
    net::StreamSocket* socket = device_->CreateSocket(command);
    int result = socket->Connect(base::Bind(&UsbDeviceImpl::OpenedForCommand,
                                            this, callback, socket));
    if (result != net::ERR_IO_PENDING)
      callback.Run(result, std::string());
  }

  virtual void OpenSocket(const std::string& name,
                          const SocketCallback& callback) OVERRIDE {
    std::string socket_name =
        base::StringPrintf(kLocalAbstractCommand, name.c_str());
    net::StreamSocket* socket = device_->CreateSocket(socket_name);
    int result = socket->Connect(base::Bind(&UsbDeviceImpl::OnOpenSocket, this,
                                            callback, socket));
    if (result != net::ERR_IO_PENDING)
      callback.Run(result, NULL);
  }

 private:
  void OnOpenSocket(const SocketCallback& callback,
                    net::StreamSocket* socket,
                    int result) {
    callback.Run(result, result == net::OK ? socket : NULL);
  }

  void OpenedForCommand(const CommandCallback& callback,
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

  void OnRead(net::StreamSocket* socket,
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

  virtual ~UsbDeviceImpl() {}
  scoped_refptr<AndroidUsbDevice> device_;
};

class AdbQueryCommand : public base::RefCounted<AdbQueryCommand> {
 public:
  AdbQueryCommand(const std::string& query,
                  const Callback& callback)
      : query_(query),
        callback_(callback) {
  }

  void Run() {
    AdbClientSocket::AdbQuery(kAdbPort, query_,
                              base::Bind(&AdbQueryCommand::Handle, this));
  }

 private:
  friend class base::RefCounted<AdbQueryCommand>;
  virtual ~AdbQueryCommand() {}

  void Handle(int result, const std::string& response) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&AdbQueryCommand::Respond, this, result, response));
  }

  void Respond(int result, const std::string& response) {
    callback_.Run(result, response);
  }

  std::string query_;
  Callback callback_;
};

class AdbPagesCommand : public base::RefCountedThreadSafe<
    AdbPagesCommand,
    content::BrowserThread::DeleteOnUIThread> {
 public:
  explicit AdbPagesCommand(DevToolsAdbBridge* bridge,
                           const PagesCallback& callback)
     : bridge_(bridge),
       callback_(callback) {
    pages_.reset(new DevToolsAdbBridge::RemotePages());
    bridge_->EnumerateUsbDevices(
        base::Bind(&AdbPagesCommand::ReceivedUsbDevices, this));
  }

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<AdbPagesCommand>;

  virtual ~AdbPagesCommand() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  void ReceivedUsbDevices(const AndroidDevices& devices) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    devices_ = devices;
    bridge_->GetAdbMessageLoop()->PostTask(FROM_HERE,
        base::Bind(&DevToolsAdbBridge::EnumerateAdbDevices, bridge_,
                   base::Bind(&AdbPagesCommand::ReceivedAdbDevices, this)));
  }

  void ReceivedAdbDevices(const AndroidDevices& devices) {
    devices_.insert(devices_.end(), devices.begin(), devices.end());
    ProcessSerials();
  }

  void ProcessSerials() {
    DCHECK_EQ(bridge_->GetAdbMessageLoop(), base::MessageLoop::current());
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
      sockets_.push_back(std::string());
      device->set_model(kUnknownModel);
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

  void ReceivedModel(int result, const std::string& response) {
    DCHECK_EQ(bridge_->GetAdbMessageLoop(), base::MessageLoop::current());
    if (result < 0) {
      devices_.pop_back();
      ProcessSerials();
      return;
    }
    scoped_refptr<DevToolsAdbBridge::AndroidDevice> device = devices_.back();
    device->set_model(response);
    device->RunCommand(kOpenedUnixSocketsCommand,
                       base::Bind(&AdbPagesCommand::ReceivedSockets, this));
  }

  void ReceivedSockets(int result,
                       const std::string& response) {
    DCHECK_EQ(bridge_->GetAdbMessageLoop(), base::MessageLoop::current());
    if (result < 0) {
      devices_.pop_back();
      ProcessSerials();
      return;
    }

    ParseSocketsList(response);
    if (sockets_.size() == 0) {
      devices_.pop_back();
      ProcessSerials();
    } else {
      ProcessSockets();
    }
  }

  void ProcessSockets() {
    DCHECK_EQ(bridge_->GetAdbMessageLoop(), base::MessageLoop::current());
    if (sockets_.size() == 0) {
      devices_.pop_back();
      ProcessSerials();
    } else {
      scoped_refptr<DevToolsAdbBridge::AndroidDevice> device = devices_.back();
      device->HttpQuery(sockets_.back(), kVersionRequest,
                        base::Bind(&AdbPagesCommand::ReceivedVersion, this));
    }
  }

  void ReceivedVersion(int result,
                       const std::string& response) {
    DCHECK_EQ(bridge_->GetAdbMessageLoop(), base::MessageLoop::current());
    if (result < 0) {
      sockets_.pop_back();
      ProcessSockets();
      return;
    }

    // Parse version, append to package name if available,
    std::string body = response.substr(result);
    scoped_ptr<base::Value> value(base::JSONReader::Read(body));
    base::DictionaryValue* dict;
    if (value && value->GetAsDictionary(&dict)) {
      std::string browser;
      if (dict->GetString("Browser", &browser)) {
        socket_to_package_[sockets_.back()] = base::StringPrintf(
            "%s (%s)", socket_to_package_[sockets_.back()].c_str(),
            browser.c_str());
      }
    }

    scoped_refptr<DevToolsAdbBridge::AndroidDevice> device = devices_.back();
    device->HttpQuery(sockets_.back(), kPageListRequest,
                      base::Bind(&AdbPagesCommand::ReceivedPages, this));
  }

  void ReceivedPages(int result,
                     const std::string& response) {
    DCHECK_EQ(bridge_->GetAdbMessageLoop(), base::MessageLoop::current());
    std::string socket = sockets_.back();
    sockets_.pop_back();
    if (result < 0) {
      ProcessSockets();
      return;
    }

    std::string body = response.substr(result);
    scoped_ptr<base::Value> value(base::JSONReader::Read(body));
    base::ListValue* list_value;
    if (!value || !value->GetAsList(&list_value)) {
      ProcessSockets();
      return;
    }

    scoped_refptr<DevToolsAdbBridge::AndroidDevice> device = devices_.back();
    base::Value* item;
    for (size_t i = 0; i < list_value->GetSize(); ++i) {
      list_value->Get(i, &item);
      base::DictionaryValue* dict;
      if (!item || !item->GetAsDictionary(&dict))
        continue;
      pages_->push_back(
          new DevToolsAdbBridge::RemotePage(
              device, socket_to_package_[socket], socket, *dict));
    }
    ProcessSockets();
  }

  void Respond() {
    callback_.Run(net::OK, pages_.release());
  }

  void ParseSocketsList(const std::string& response) {
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

    socket_to_package_.clear();
    std::vector<std::string> entries;
    Tokenize(response, "\n", &entries);
    const std::string channel_pattern =
        base::StringPrintf(kDevToolsChannelNameFormat, "");
    for (size_t i = 1; i < entries.size(); ++i) {
      std::vector<std::string> fields;
      Tokenize(entries[i], " ", &fields);
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
      std::string socket = path_field.substr(1, path_field.size() - 2);
      sockets_.push_back(socket);
      std::string package = path_field.substr(1, socket_name_pos - 1);
      if (socket_name_pos + channel_pattern.size() < path_field.size() - 1) {
        package += path_field.substr(
            socket_name_pos + channel_pattern.size(), path_field.size() - 1);
      }
      package[0] = base::ToUpperASCII(package[0]);
      socket_to_package_[socket] = package;
    }
  }

  scoped_refptr<DevToolsAdbBridge> bridge_;
  PagesCallback callback_;
  AndroidDevices devices_;
  std::vector<std::string> sockets_;
  std::map<std::string, std::string> socket_to_package_;
  scoped_ptr<DevToolsAdbBridge::RemotePages> pages_;
};

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

void DevToolsAdbBridge::AndroidDevice::HttpQuery(
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
        frontend_url_(frontend_url),
        profile_(profile),
        tethering_adb_filter_(kAdbPort, device->serial()) {
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
    return tethering_adb_filter_.ProcessIncomingMessage(message);
  }

  virtual void ProcessOutgoingMessage(const std::string& message) OVERRIDE {
    tethering_adb_filter_.ProcessOutgoingMessage(message);
  }

  const std::string id_;
  const std::string frontend_url_;
  Profile* profile_;

  scoped_ptr<content::DevToolsExternalAgentProxy> proxy_;
  TetheringAdbFilter tethering_adb_filter_;
  scoped_refptr<AdbWebSocket> web_socket_;
  DISALLOW_COPY_AND_ASSIGN(AgentHostDelegate);
};

DevToolsAdbBridge::RemotePage::RemotePage(scoped_refptr<AndroidDevice> device,
                                          const std::string& package,
                                          const std::string& socket,
                                          const base::DictionaryValue& value)
    : device_(device),
      package_(package),
      socket_(socket) {
  value.GetString("id", &id_);
  value.GetString("url", &url_);
  value.GetString("title", &title_);
  value.GetString("descirption", &description_);
  value.GetString("faviconUrl", &favicon_url_);
  value.GetString("webSocketDebuggerUrl", &debug_url_);
  value.GetString("devtoolsFrontendUrl", &frontend_url_);

  if (debug_url_.find("ws://") == 0)
    debug_url_ = debug_url_.substr(5);
  else
    debug_url_ = "";

  size_t ws_param = frontend_url_.find("?ws");
  if (ws_param != std::string::npos)
    frontend_url_ = frontend_url_.substr(0, ws_param);
  if (frontend_url_.find("http:") == 0)
    frontend_url_ = "https:" + frontend_url_.substr(5);
}

DevToolsAdbBridge::RemotePage::~RemotePage() {
}

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

DevToolsAdbBridge::DevToolsAdbBridge(Profile* profile)
    : profile_(profile),
      adb_thread_(RefCountedAdbThread::GetInstance()),
      has_message_loop_(adb_thread_->message_loop() != NULL) {
  rsa_key_.reset(AndroidRSAPrivateKey(profile));
}

void DevToolsAdbBridge::EnumerateUsbDevices(
    const AndroidDevicesCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kRemoteDebuggingRawUSB)) {
    AndroidUsbDevice::Enumerate(rsa_key_.get(),
        base::Bind(&DevToolsAdbBridge::ReceivedUsbDevices, this, callback));
  } else {
    ReceivedUsbDevices(callback, AndroidUsbDevices());
  }
}

void DevToolsAdbBridge::EnumerateAdbDevices(
    const AndroidDevicesCallback& callback) {
  DCHECK_EQ(base::MessageLoop::current(), adb_thread_->message_loop());

  AdbClientSocket::AdbQuery(
      kAdbPort, kHostDevicesCommand,
      base::Bind(&DevToolsAdbBridge::ReceivedAdbDevices, this, callback));
}

void DevToolsAdbBridge::Attach(const std::string& page_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!has_message_loop_)
    return;

  for (RemotePages::iterator it = pages_->begin(); it != pages_->end(); ++it) {
    scoped_refptr<RemotePage> page = *it;
    // Assuming page->id() is unique across devices (since it is a GUID).
    if (page->id() == page_id) {
      AgentHostDelegates::iterator it = g_host_delegates.Get().find(page_id);
      if (it != g_host_delegates.Get().end())
        it->second->OpenFrontend();
      else if (!page->debug_url().empty())
        new AgentHostDelegate(
            page_id, page->device(), page->socket(), page->debug_url(),
            page->frontend_url(), GetAdbMessageLoop(), profile_);
      break;
    }
  }
}

void DevToolsAdbBridge::AddListener(Listener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (listeners_.empty())
    RequestPages();
  listeners_.push_back(listener);
}

void DevToolsAdbBridge::RemoveListener(Listener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  Listeners::iterator it =
      std::find(listeners_.begin(), listeners_.end(), listener);
  DCHECK(it != listeners_.end());
  listeners_.erase(it);
}

base::MessageLoop* DevToolsAdbBridge::GetAdbMessageLoop() {
  return adb_thread_->message_loop();
}

DevToolsAdbBridge::~DevToolsAdbBridge() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(listeners_.empty());
}

void DevToolsAdbBridge::ReceivedUsbDevices(
    const AndroidDevicesCallback& callback,
    const AndroidUsbDevices& usb_devices) {
  AndroidDevices devices;

#if defined(DEBUG_DEVTOOLS)
  devices.push_back(new AdbDeviceImpl(""));  // For desktop remote debugging.
#endif  // defined(DEBUG_DEVTOOLS)

  for (AndroidUsbDevices::const_iterator it = usb_devices.begin();
       it != usb_devices.end(); ++it) {
    devices.push_back(new UsbDeviceImpl(*it));
  }

  callback.Run(devices);
}

void DevToolsAdbBridge::ReceivedAdbDevices(
    const AndroidDevicesCallback& callback,
    int result,
    const std::string& response) {
  AndroidDevices devices;
  if (result != net::OK) {
    callback.Run(devices);
    return;
  }

  std::vector<std::string> serials;
  Tokenize(response, "\n", &serials);
  for (size_t i = 0; i < serials.size(); ++i) {
    std::vector<std::string> tokens;
    Tokenize(serials[i], "\t ", &tokens);
    devices.push_back(new AdbDeviceImpl(tokens[0]));
  }
  callback.Run(devices);
}

void DevToolsAdbBridge::RequestPages() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!has_message_loop_)
    return;

  new AdbPagesCommand(this,
                      base::Bind(&DevToolsAdbBridge::ReceivedPages, this));
}

void DevToolsAdbBridge::ReceivedPages(int result, RemotePages* pages_ptr) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  pages_.reset(pages_ptr);

  if (result == net::OK) {
    Listeners copy(listeners_);
    for (Listeners::iterator it = copy.begin(); it != copy.end(); ++it)
      (*it)->RemotePagesChanged(pages_.get());
  }

  if (listeners_.empty())
    return;

  BrowserThread::PostDelayedTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&DevToolsAdbBridge::RequestPages, this),
      base::TimeDelta::FromMilliseconds(kAdbPollingIntervalMs));
}
