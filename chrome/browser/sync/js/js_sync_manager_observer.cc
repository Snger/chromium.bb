// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js/js_sync_manager_observer.h"

#include <cstddef>

#include "base/location.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/sync/internal_api/change_record.h"
#include "chrome/browser/sync/js/js_arg_list.h"
#include "chrome/browser/sync/js/js_event_details.h"
#include "chrome/browser/sync/js/js_event_handler.h"
#include "chrome/browser/sync/sessions/session_state.h"
#include "chrome/browser/sync/syncable/model_type.h"

namespace browser_sync {

using browser_sync::SyncProtocolError;

JsSyncManagerObserver::JsSyncManagerObserver() {}

JsSyncManagerObserver::~JsSyncManagerObserver() {}

void JsSyncManagerObserver::SetJsEventHandler(
    const WeakHandle<JsEventHandler>& event_handler) {
  event_handler_ = event_handler;
}

namespace {

// Max number of changes we attempt to convert to values (to avoid
// running out of memory).
const size_t kChangeLimit = 300;

}  // namespace

void JsSyncManagerObserver::OnChangesApplied(
    syncable::ModelType model_type,
    int64 write_transaction_id,
    const sync_api::ImmutableChangeRecordList& changes) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  DictionaryValue details;
  details.SetString("modelType", syncable::ModelTypeToString(model_type));
  details.SetString("writeTransactionId",
                    base::Int64ToString(write_transaction_id));
  Value* changes_value = NULL;
  const size_t changes_size = changes.Get().size();
  if (changes_size <= kChangeLimit) {
    ListValue* changes_list = new ListValue();
    for (sync_api::ChangeRecordList::const_iterator it =
             changes.Get().begin(); it != changes.Get().end(); ++it) {
      changes_list->Append(it->ToValue());
    }
    changes_value = changes_list;
  } else {
    changes_value =
        Value::CreateStringValue(
            base::Uint64ToString(static_cast<uint64>(changes_size)) +
            " changes");
  }
  details.Set("changes", changes_value);
  HandleJsEvent(FROM_HERE, "onChangesApplied", JsEventDetails(&details));
}

void JsSyncManagerObserver::OnChangesComplete(
    syncable::ModelType model_type) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  DictionaryValue details;
  details.SetString("modelType", syncable::ModelTypeToString(model_type));
  HandleJsEvent(FROM_HERE, "onChangesComplete", JsEventDetails(&details));
}

void JsSyncManagerObserver::OnSyncCycleCompleted(
    const sessions::SyncSessionSnapshot* snapshot) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  DictionaryValue details;
  details.Set("snapshot", snapshot->ToValue());
  HandleJsEvent(FROM_HERE, "onSyncCycleCompleted", JsEventDetails(&details));
}

void JsSyncManagerObserver::OnAuthError(
    const GoogleServiceAuthError& auth_error) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  DictionaryValue details;
  details.Set("authError", auth_error.ToValue());
  HandleJsEvent(FROM_HERE, "onAuthError", JsEventDetails(&details));
}

void JsSyncManagerObserver::OnUpdatedToken(const std::string& token) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  DictionaryValue details;
  details.SetString("token", "<redacted>");
  HandleJsEvent(FROM_HERE, "onUpdatedToken", JsEventDetails(&details));
}

void JsSyncManagerObserver::OnPassphraseRequired(
    sync_api::PassphraseRequiredReason reason) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  DictionaryValue details;
  details.SetString("reason",
                     sync_api::PassphraseRequiredReasonToString(reason));
  HandleJsEvent(FROM_HERE, "onPassphraseRequired", JsEventDetails(&details));
}

void JsSyncManagerObserver::OnPassphraseAccepted(
    const std::string& bootstrap_token) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  DictionaryValue details;
  details.SetString("bootstrapToken", "<redacted>");
  HandleJsEvent(FROM_HERE, "onPassphraseAccepted", JsEventDetails(&details));
}

void JsSyncManagerObserver::OnEncryptionComplete(
    const syncable::ModelTypeSet& encrypted_types) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  DictionaryValue details;
  details.Set("encryptedTypes",
               syncable::ModelTypeSetToValue(encrypted_types));
  HandleJsEvent(FROM_HERE, "onEncryptionComplete", JsEventDetails(&details));
}

void JsSyncManagerObserver::OnActionableError(
    const SyncProtocolError& sync_error) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  DictionaryValue details;
  details.Set("syncError",  sync_error.ToValue());
  HandleJsEvent(FROM_HERE, "onActionableError",
                JsEventDetails(&details));
}

void JsSyncManagerObserver::OnInitializationComplete(
    const WeakHandle<JsBackend>& js_backend,
    bool success) {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  // Ignore the |js_backend| argument; it's not really convertible to
  // JSON anyway.
  HandleJsEvent(FROM_HERE, "onInitializationComplete", JsEventDetails());
}

void JsSyncManagerObserver::OnStopSyncingPermanently() {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  HandleJsEvent(FROM_HERE, "onStopSyncingPermanently", JsEventDetails());
}

void JsSyncManagerObserver::OnClearServerDataSucceeded() {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  HandleJsEvent(FROM_HERE, "onClearServerDataSucceeded", JsEventDetails());
}

void JsSyncManagerObserver::OnClearServerDataFailed() {
  if (!event_handler_.IsInitialized()) {
    return;
  }
  HandleJsEvent(FROM_HERE, "onClearServerDataFailed", JsEventDetails());
}

void JsSyncManagerObserver::HandleJsEvent(
    const tracked_objects::Location& from_here,
    const std::string& name, const JsEventDetails& details) {
  if (!event_handler_.IsInitialized()) {
    NOTREACHED();
    return;
  }
  event_handler_.Call(from_here,
                      &JsEventHandler::HandleJsEvent, name, details);
}

}  // namespace browser_sync
