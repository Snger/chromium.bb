// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FLIMFLAM_CLIENT_HELPER_H_
#define CHROMEOS_DBUS_FLIMFLAM_CLIENT_HELPER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/dbus/dbus_method_call_status.h"

namespace base {

class Value;
class DictionaryValue;

}  // namespace base

namespace dbus {

class MessageWriter;
class MethodCall;
class ObjectPath;
class ObjectProxy;
class Response;
class Signal;

}  // namespace dbus

namespace chromeos {

// A class to help implement Flimflam clients.
class FlimflamClientHelper {
 public:
  // A callback to handle PropertyChanged signals.
  typedef base::Callback<void(const std::string& name,
                              const base::Value& value)> PropertyChangedHandler;

  // A callback to handle responses for methods without results.
  typedef base::Callback<void(DBusMethodCallStatus call_status)> VoidCallback;

  // A callback to handle responses for methods with ObjectPath results.
  typedef base::Callback<void(
      DBusMethodCallStatus call_status,
      const dbus::ObjectPath& result)> ObjectPathCallback;

  // A callback to handle responses for methods with DictionaryValue results.
  typedef base::Callback<void(
      DBusMethodCallStatus call_status,
      const base::DictionaryValue& result)> DictionaryValueCallback;

  explicit FlimflamClientHelper(dbus::ObjectProxy* proxy);

  virtual ~FlimflamClientHelper();

  // Sets PropertyChanged signal handler.
  void SetPropertyChangedHandler(const PropertyChangedHandler& handler);

  // Resets PropertyChanged signal handler.
  void ResetPropertyChangedHandler();

  // Starts monitoring PropertyChanged signal.
  void MonitorPropertyChanged(const std::string& interface_name);

  // Calls a method without results.
  void CallVoidMethod(dbus::MethodCall* method_call,
                      const VoidCallback& callback);

  // Calls a method with an object path result.
  void CallObjectPathMethod(dbus::MethodCall* method_call,
                            const ObjectPathCallback& callback);

  // Calls a method with a dictionary value result.
  void CallDictionaryValueMethod(dbus::MethodCall* method_call,
                                 const DictionaryValueCallback& callback);

  // Appends the value (basic types and string-to-string dictionary) to the
  // writer as a variant.
  static void AppendValueDataAsVariant(dbus::MessageWriter* writer,
                                       const base::Value& value);

 private:
  // Handles the result of signal connection setup.
  void OnSignalConnected(const std::string& interface,
                         const std::string& signal,
                         bool success);

  // Handles PropertyChanged signal.
  void OnPropertyChanged(dbus::Signal* signal);

  // Handles responses for methods without results.
  void OnVoidMethod(const VoidCallback& callback, dbus::Response* response);

  // Handles responses for methods with ObjectPath results.
  void OnObjectPathMethod(const ObjectPathCallback& callback,
                          dbus::Response* response);

  // Handles responses for methods with DictionaryValue results.
  void OnDictionaryValueMethod(const DictionaryValueCallback& callback,
                               dbus::Response* response);

  base::WeakPtrFactory<FlimflamClientHelper> weak_ptr_factory_;
  dbus::ObjectProxy* proxy_;
  PropertyChangedHandler property_changed_handler_;

  DISALLOW_COPY_AND_ASSIGN(FlimflamClientHelper);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FLIMFLAM_CLIENT_HELPER_H_
