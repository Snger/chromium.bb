// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_ANDROID_DEVICE_H_
#define CHROME_BROWSER_DEVTOOLS_ANDROID_DEVICE_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "chrome/browser/devtools/adb/android_usb_device.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "crypto/rsa_private_key.h"
#include "net/socket/stream_socket.h"

class AndroidDeviceManager;

class AndroidDevice : public base::RefCounted<AndroidDevice>,
                      public base::NonThreadSafe {
 protected:
  friend class AndroidDeviceManager;

  typedef base::Callback<void(int, const std::string&)> CommandCallback;
  typedef base::Callback<void(int result, net::StreamSocket*)> SocketCallback;

  AndroidDevice(const std::string& serial, bool is_connected);

  virtual void RunCommand(const std::string& command,
                          const CommandCallback& callback) = 0;
  virtual void OpenSocket(const std::string& socket_name,
                          const SocketCallback& callback) = 0;
  virtual void HttpQuery(const std::string& la_name,
                         const std::string& request,
                         const CommandCallback& callback);
  void HttpUpgrade(const std::string& la_name,
                   const std::string& request,
                   const SocketCallback& callback);

  std::string serial() { return serial_; }
  bool is_connected() { return is_connected_; }

  friend class base::RefCounted<AndroidDevice>;
  virtual ~AndroidDevice();

 private:
  void OnHttpSocketOpened(const std::string& request,
                          const CommandCallback& callback,
                          int result,
                          net::StreamSocket* socket);
  void OnHttpSocketOpened2(const std::string& request,
                           const SocketCallback& callback,
                           int result,
                           net::StreamSocket* socket);

  const std::string serial_;
  const bool is_connected_;

  DISALLOW_COPY_AND_ASSIGN(AndroidDevice);
};


class AndroidDeviceProvider
    : public base::RefCountedThreadSafe<
          AndroidDeviceProvider,
          content::BrowserThread::DeleteOnUIThread> {
 protected:
  friend class AndroidDeviceManager;

  typedef std::vector<scoped_refptr<AndroidDevice> > AndroidDevices;
  typedef base::Callback<void(const AndroidDevices&)> QueryDevicesCallback;

  virtual void QueryDevices(const QueryDevicesCallback& callback) = 0;

 public:
  static scoped_refptr<AndroidDeviceProvider> GetAdbDeviceProvider();
  static scoped_refptr<AndroidDeviceProvider>
      GetUsbDeviceProvider(Profile* profile);
#if defined(DEBUG_DEVTOOLS)
  static scoped_refptr<AndroidDeviceProvider> GetSelfAsDeviceProvider();
#endif
  // Implemented in browser_tests.
  static scoped_refptr<AndroidDeviceProvider> GetMockDeviceProviderForTest();

 protected:
  friend struct
      content::BrowserThread::DeleteOnThread<content::BrowserThread::UI>;
  friend class base::DeleteHelper<AndroidDeviceProvider>;

  AndroidDeviceProvider();
  virtual ~AndroidDeviceProvider();
};

class AndroidDeviceManager : public base::RefCounted<AndroidDeviceManager>,
                             public base::NonThreadSafe {
 public:
  static scoped_refptr<AndroidDeviceManager> Create();

  typedef base::Callback<void(int, const std::string&)> CommandCallback;
  typedef base::Callback<void(int result, net::StreamSocket*)> SocketCallback;

  typedef std::vector<scoped_refptr<AndroidDeviceProvider> > DeviceProviders;

  typedef base::Callback<void (const std::vector<std::string>&)>
      QueryDevicesCallback;

  void QueryDevices(const DeviceProviders& providers,
                    const QueryDevicesCallback& callback);

  void Stop();

  bool IsConnected(const std::string& serial);

  void RunCommand(const std::string& serial,
                  const std::string& command,
                  const CommandCallback& callback);

  void OpenSocket(const std::string& serial,
                  const std::string& socket_name,
                  const SocketCallback& callback);

  void HttpQuery(const std::string& serial,
                 const std::string& la_name,
                 const std::string& request,
                 const CommandCallback& callback);

  void HttpUpgrade(const std::string& serial,
                   const std::string& la_name,
                   const std::string& request,
                   const SocketCallback& callback);

 private:
  AndroidDeviceManager();

  friend class base::RefCounted<AndroidDeviceManager>;

  virtual ~AndroidDeviceManager();

  void QueryNextProvider(
      const QueryDevicesCallback& callback,
      const DeviceProviders& providers,
      const AndroidDeviceProvider::AndroidDevices& total_devices,
      const AndroidDeviceProvider::AndroidDevices& new_devices);

  AndroidDevice* FindDevice(const std::string& serial);

  typedef std::map<std::string, scoped_refptr<AndroidDevice> > DeviceMap;
  DeviceMap devices_;

  bool stopped_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_ANDROID_DEVICE_H_
