// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_webrequest_api_helpers.h"

#include "base/values.h"
#include "chrome/browser/extensions/extension_webrequest_api.h"
#include "net/http/http_util.h"

namespace extension_webrequest_api_helpers {


EventResponseDelta::EventResponseDelta(
    const std::string& extension_id, const base::Time& extension_install_time)
    : extension_id(extension_id),
      extension_install_time(extension_install_time),
      cancel(false) {
}

EventResponseDelta::~EventResponseDelta() {
}


EventLogEntry::EventLogEntry(
    net::NetLog::EventType event_type,
    const scoped_refptr<net::NetLog::EventParameters>& params)
    : event_type(event_type),
      params(params) {
}

EventLogEntry::~EventLogEntry() {
}


// NetLog parameter to indicate the ID of the extension that caused an event.
class NetLogExtensionIdParameter : public net::NetLog::EventParameters {
 public:
  explicit NetLogExtensionIdParameter(const std::string& extension_id)
      : extension_id_(extension_id) {}
  virtual ~NetLogExtensionIdParameter() {}

  virtual base::Value* ToValue() const OVERRIDE {
    DictionaryValue* dict = new DictionaryValue();
    dict->SetString("extension_id", extension_id_);
    return dict;
  }

 private:
  const std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(NetLogExtensionIdParameter);
};


// NetLog parameter to indicate that an extension modified a request.
class NetLogModificationParameter : public NetLogExtensionIdParameter {
 public:
  explicit NetLogModificationParameter(const std::string& extension_id)
      : NetLogExtensionIdParameter(extension_id) {}
  virtual ~NetLogModificationParameter() {}

  virtual base::Value* ToValue() const OVERRIDE {
    Value* parent = NetLogExtensionIdParameter::ToValue();
    DCHECK(parent->IsType(Value::TYPE_DICTIONARY));
    DictionaryValue* dict = static_cast<DictionaryValue*>(parent);
    dict->Set("modified_headers", modified_headers_.DeepCopy());
    dict->Set("deleted_headers", deleted_headers_.DeepCopy());
    return dict;
  }

  void DeletedHeader(const std::string& key) {
    deleted_headers_.Append(Value::CreateStringValue(key));
  }

  void ModifiedHeader(const std::string& key, const std::string& value) {
    modified_headers_.Append(Value::CreateStringValue(key + ": " + value));
  }

 private:
  ListValue modified_headers_;
  ListValue deleted_headers_;

  DISALLOW_COPY_AND_ASSIGN(NetLogModificationParameter);
};


bool InDecreasingExtensionInstallationTimeOrder(
    const linked_ptr<EventResponseDelta>& a,
    const linked_ptr<EventResponseDelta>& b) {
  return a->extension_install_time > b->extension_install_time;
}

ListValue* StringToCharList(const std::string& s) {
  ListValue* result = new ListValue;
  for (size_t i = 0, n = s.size(); i < n; ++i) {
    result->Append(
        Value::CreateIntegerValue(
            *reinterpret_cast<const unsigned char*>(&s[i])));
  }
  return result;
}

bool CharListToString(ListValue* list, std::string* out) {
  if (!list)
    return false;
  const size_t list_length = list->GetSize();
  out->resize(list_length);
  int value = 0;
  for (size_t i = 0; i < list_length; ++i) {
    if (!list->GetInteger(i, &value) || value < 0 || value > 255)
      return false;
    unsigned char tmp = static_cast<unsigned char>(value);
    (*out)[i] = *reinterpret_cast<char*>(&tmp);
  }
  return true;
}

EventResponseDelta* CalculateOnBeforeRequestDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    const GURL& new_url) {
  EventResponseDelta* result =
      new EventResponseDelta(extension_id, extension_install_time);
  result->cancel = cancel;
  result->new_url = new_url;
  return result;
}

EventResponseDelta* CalculateOnBeforeSendHeadersDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    net::HttpRequestHeaders* old_headers,
    net::HttpRequestHeaders* new_headers) {
  EventResponseDelta* result =
      new EventResponseDelta(extension_id, extension_install_time);
  result->cancel = cancel;

  // Find deleted headers.
  {
    net::HttpRequestHeaders::Iterator i(*old_headers);
    while (i.GetNext()) {
      if (!new_headers->HasHeader(i.name())) {
        result->deleted_request_headers.push_back(i.name());
      }
    }
  }

  // Find modified headers.
  {
    net::HttpRequestHeaders::Iterator i(*new_headers);
    while (i.GetNext()) {
      std::string value;
      if (!old_headers->GetHeader(i.name(), &value) || i.value() != value) {
        result->modified_request_headers.SetHeader(i.name(), i.value());
      }
    }
  }
  return result;
}

EventResponseDelta* CalculateOnHeadersReceivedDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    const std::string& status_line,
    const std::string& response_headers_string) {
  EventResponseDelta* result =
      new EventResponseDelta(extension_id, extension_install_time);
  result->cancel = cancel;

  if (!response_headers_string.empty()) {
    std::string new_headers_string =
        status_line + "\n" + response_headers_string;

    result->new_response_headers =
        new net::HttpResponseHeaders(
            net::HttpUtil::AssembleRawHeaders(new_headers_string.c_str(),
                                              new_headers_string.length()));
  }
  return result;
}

EventResponseDelta* CalculateOnAuthRequiredDelta(
    const std::string& extension_id,
    const base::Time& extension_install_time,
    bool cancel,
    scoped_ptr<net::AuthCredentials>* auth_credentials) {
  EventResponseDelta* result =
      new EventResponseDelta(extension_id, extension_install_time);
  result->cancel = cancel;
  result->auth_credentials.swap(*auth_credentials);
  return result;
}

void MergeCancelOfResponses(
    const EventResponseDeltas& deltas,
    bool* canceled,
    EventLogEntries* event_log_entries) {
  for (EventResponseDeltas::const_iterator i = deltas.begin();
       i != deltas.end(); ++i) {
    if ((*i)->cancel) {
      *canceled = true;
      EventLogEntry log_entry(
          net::NetLog::TYPE_CHROME_EXTENSION_ABORTED_REQUEST,
          make_scoped_refptr(
              new NetLogExtensionIdParameter((*i)->extension_id)));
      event_log_entries->push_back(log_entry);
      break;
    }
  }
}

void MergeOnBeforeRequestResponses(
    const EventResponseDeltas& deltas,
    GURL* new_url,
    std::set<std::string>* conflicting_extensions,
    EventLogEntries* event_log_entries) {
  EventResponseDeltas::const_iterator delta;

  bool redirected = false;
  for (delta = deltas.begin(); delta != deltas.end(); ++delta) {
    if (!(*delta)->new_url.is_empty()) {
      if (!redirected) {
        *new_url = (*delta)->new_url;
        redirected = true;
        EventLogEntry log_entry(
            net::NetLog::TYPE_CHROME_EXTENSION_REDIRECTED_REQUEST,
            make_scoped_refptr(
                new NetLogExtensionIdParameter((*delta)->extension_id)));
        event_log_entries->push_back(log_entry);
      } else {
        conflicting_extensions->insert((*delta)->extension_id);
        EventLogEntry log_entry(
            net::NetLog::TYPE_CHROME_EXTENSION_REDIRECTED_REQUEST,
            make_scoped_refptr(
                new NetLogExtensionIdParameter((*delta)->extension_id)));
        event_log_entries->push_back(log_entry);
      }
    }
  }
}

void MergeOnBeforeSendHeadersResponses(
    const EventResponseDeltas& deltas,
    net::HttpRequestHeaders* request_headers,
    std::set<std::string>* conflicting_extensions,
    EventLogEntries* event_log_entries) {
  EventResponseDeltas::const_iterator delta;

  // Here we collect which headers we have removed or set to new values
  // so far due to extensions of higher precedence.
  std::set<std::string> removed_headers;
  std::set<std::string> set_headers;

  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  for (delta = deltas.begin(); delta != deltas.end(); ++delta) {
    if ((*delta)->modified_request_headers.IsEmpty() &&
        (*delta)->deleted_request_headers.empty()) {
      continue;
    }

    scoped_refptr<NetLogModificationParameter> log(
        new NetLogModificationParameter((*delta)->extension_id));

    // Check whether any modification affects a request header that
    // has been modified differently before. As deltas is sorted by decreasing
    // extension installation order, this takes care of precedence.
    bool extension_conflicts = false;
    {
      net::HttpRequestHeaders::Iterator modification(
          (*delta)->modified_request_headers);
      while (modification.GetNext() && !extension_conflicts) {
        // This modification sets |key| to |value|.
        const std::string& key = modification.name();
        const std::string& value = modification.value();

        // We must not delete anything that has been modified before.
        if (removed_headers.find(key) != removed_headers.end())
          extension_conflicts = true;

        // We must not modify anything that has been set to a *different*
        // value before.
        if (set_headers.find(key) != set_headers.end()) {
          std::string current_value;
          if (!request_headers->GetHeader(key, &current_value) ||
              current_value != value) {
            extension_conflicts = true;
          }
        }
      }
    }

    // Check whether any deletion affects a request header that has been
    // modified before.
    {
      std::vector<std::string>::iterator key;
      for (key = (*delta)->deleted_request_headers.begin();
           key != (*delta)->deleted_request_headers.end() &&
               !extension_conflicts;
           ++key) {
        if (set_headers.find(*key) != set_headers.end())
          extension_conflicts = true;
      }
    }

    // Now execute the modifications if there were no conflicts.
    if (!extension_conflicts) {
      // Copy all modifications into the original headers.
      request_headers->MergeFrom((*delta)->modified_request_headers);
      {
        // Record which keys were changed.
        net::HttpRequestHeaders::Iterator modification(
            (*delta)->modified_request_headers);
        while (modification.GetNext()) {
          set_headers.insert(modification.name());
          log->ModifiedHeader(modification.name(), modification.value());
        }
      }

      // Perform all deletions and record which keys were deleted.
      {
        std::vector<std::string>::iterator key;
        for (key = (*delta)->deleted_request_headers.begin();
            key != (*delta)->deleted_request_headers.end();
             ++key) {
          request_headers->RemoveHeader(*key);
          removed_headers.insert(*key);
          log->DeletedHeader(*key);
        }
      }
      EventLogEntry log_entry(
          net::NetLog::TYPE_CHROME_EXTENSION_MODIFIED_HEADERS, log);
      event_log_entries->push_back(log_entry);
    } else {
      conflicting_extensions->insert((*delta)->extension_id);
      EventLogEntry log_entry(
          net::NetLog::TYPE_CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
          make_scoped_refptr(
              new NetLogExtensionIdParameter((*delta)->extension_id)));
      event_log_entries->push_back(log_entry);
    }
  }
}

void MergeOnHeadersReceivedResponses(
    const EventResponseDeltas& deltas,
    scoped_refptr<net::HttpResponseHeaders>* override_response_headers,
    std::set<std::string>* conflicting_extensions,
    EventLogEntries* event_log_entries) {
  EventResponseDeltas::const_iterator delta;

  // Whether any extension has overridden the response headers, yet.
  bool headers_overridden = false;

  // We assume here that the deltas are sorted in decreasing extension
  // precedence (i.e. decreasing extension installation time).
  for (delta = deltas.begin(); delta != deltas.end(); ++delta) {
    if (!(*delta)->new_response_headers.get())
      continue;

    scoped_refptr<NetLogModificationParameter> log(
        new NetLogModificationParameter((*delta)->extension_id));

    // Conflict if a second extension returned new response headers;
    bool extension_conflicts = headers_overridden;

    if (!extension_conflicts) {
      headers_overridden = true;
      *override_response_headers = (*delta)->new_response_headers;
      EventLogEntry log_entry(
          net::NetLog::TYPE_CHROME_EXTENSION_MODIFIED_HEADERS, log);
      event_log_entries->push_back(log_entry);
    } else {
      conflicting_extensions->insert((*delta)->extension_id);
      EventLogEntry log_entry(
          net::NetLog::TYPE_CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
          make_scoped_refptr(
              new NetLogExtensionIdParameter((*delta)->extension_id)));
      event_log_entries->push_back(log_entry);
    }
  }
}

bool MergeOnAuthRequiredResponses(
    const EventResponseDeltas& deltas,
    net::AuthCredentials* auth_credentials,
    std::set<std::string>* conflicting_extensions,
    EventLogEntries* event_log_entries) {
  CHECK(auth_credentials);
  bool credentials_set = false;

  for (EventResponseDeltas::const_iterator delta = deltas.begin();
       delta != deltas.end();
       ++delta) {
    if (!(*delta)->auth_credentials.get())
      continue;
    if (credentials_set) {
      conflicting_extensions->insert((*delta)->extension_id);
      EventLogEntry log_entry(
          net::NetLog::TYPE_CHROME_EXTENSION_IGNORED_DUE_TO_CONFLICT,
          make_scoped_refptr(
              new NetLogExtensionIdParameter((*delta)->extension_id)));
      event_log_entries->push_back(log_entry);
    } else {
      EventLogEntry log_entry(
          net::NetLog::TYPE_CHROME_EXTENSION_PROVIDE_AUTH_CREDENTIALS,
          make_scoped_refptr(
              new NetLogExtensionIdParameter((*delta)->extension_id)));
      event_log_entries->push_back(log_entry);
      *auth_credentials = *(*delta)->auth_credentials;
      credentials_set = true;
    }
  }
  return credentials_set;
}

}  // namespace extension_webrequest_api_helpers
