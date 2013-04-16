// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_

#include <string>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/string16.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/gfx/native_widget_types.h"

namespace base {
class DictionaryValue;
class ListValue;
class Value;
}

namespace chromeos {

// Class that collects Localized Values for translation.
class LocalizedValuesBuilder {
 public:
  explicit LocalizedValuesBuilder(base::DictionaryValue* dict);
  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message.
  void Add(const std::string& key, int message_id);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // one format parameter subsituted by |a|.
  void AddF(const std::string& key,
            int message_id,
            const string16& a);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // two format parameters subsituted by |a| and |b| respectively.
  void AddF(const std::string& key,
            int message_id,
            const string16& a,
            const string16& b);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // one format parameter subsituted by resource identified by |message_id_a|.
  void AddF(const std::string& key,
            int message_id,
            int message_id_a);

  // Method to declare localized value. |key| is the i18n key used in html.
  // |message_id| is a resource id of message. Message is expected to have
  // two format parameters subsituted by resource identified by |message_id_a|
  // and |message_id_b| respectively.
  void AddF(const std::string& key,
            int message_id,
            int message_id_a,
            int message_id_b);
 private:
  // Not owned.
  base::DictionaryValue* dict_;
};

// Base class for the OOBE/Login WebUI handlers.
class BaseScreenHandler : public content::WebUIMessageHandler {
 public:
  BaseScreenHandler();
  virtual ~BaseScreenHandler();

  // Gets localized strings to be used on the page.
  void GetLocalizedStrings(
      base::DictionaryValue* localized_strings);

  // This method is called when page is ready. It propagates to inherited class
  // via virtual Initialize() method (see below).
  void InitializeBase();

 protected:
  // All subclasses should implement this method to provide localized values.
  virtual void DeclareLocalizedValues(LocalizedValuesBuilder* builder) = 0;

  // Subclasses can override these methods to pass additional parameters
  // to loadTimeData. Generally, it is a bad approach, and it should be replaced
  // with Context at some point.
  virtual void GetAdditionalParameters(base::DictionaryValue* parameters);

  // Shortcut for calling JS methods on WebUI side.
  void CallJS(const std::string& method);
  void CallJS(const std::string& method,
              const base::Value& arg1);
  void CallJS(const std::string& method,
              const base::Value& arg1,
              const base::Value& arg2);
  void CallJS(const std::string& method,
              const base::Value& arg1,
              const base::Value& arg2,
              const base::Value& arg3);
  void CallJS(const std::string& method,
              const base::Value& arg1,
              const base::Value& arg2,
              const base::Value& arg3,
              const base::Value& arg4);

  // Shortcut method for adding WebUI callbacks.
  template<typename T>
  void AddCallback(const std::string& name,
                   void (T::*callback)(const base::ListValue* args)) {
    web_ui()->RegisterMessageCallback(name,
      base::Bind(callback, base::Unretained(static_cast<T*>(this))));
  }

  // Called when the page is ready and handler can do initialization.
  virtual void Initialize() = 0;

  // Show selected WebUI |screen|. Optionally it can pass screen initialization
  // data via |data| parameter.
  void ShowScreen(const char* screen, const base::DictionaryValue* data);

  // Whether page is ready.
  bool page_is_ready() const { return page_is_ready_; }

  // Returns the window which shows us.
  virtual gfx::NativeWindow GetNativeWindow();

 private:
  // Keeps whether page is ready.
  bool page_is_ready_;

  base::DictionaryValue* localized_values_;

  DISALLOW_COPY_AND_ASSIGN(BaseScreenHandler);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_LOGIN_BASE_SCREEN_HANDLER_H_
