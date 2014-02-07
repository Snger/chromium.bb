// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/password_syncable_service.h"

#include "base/location.h"
#include "base/memory/scoped_vector.h"
#include "base/metrics/histogram.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_store.h"
#include "net/base/escape.h"
#include "sync/api/sync_error_factory.h"

namespace {

// Merges the local and sync passwords and outputs the entry into
// |new_password_form|. Returns true if the local and the sync passwords differ.
// Returns false if they are identical.
bool MergeLocalAndSyncPasswords(
    const sync_pb::PasswordSpecificsData& password_specifics,
    const autofill::PasswordForm& password_form,
    autofill::PasswordForm* new_password_form) {
  DCHECK(new_password_form);
  if (password_form.scheme == password_specifics.scheme() &&
      password_form.signon_realm == password_specifics.signon_realm() &&
      password_form.origin.spec() == password_specifics.origin() &&
      password_form.action.spec() == password_specifics.action() &&
      base::UTF16ToUTF8(password_form.username_element) ==
          password_specifics.username_element() &&
      base::UTF16ToUTF8(password_form.password_element) ==
          password_specifics.password_element() &&
      base::UTF16ToUTF8(password_form.username_value) ==
          password_specifics.username_value() &&
      base::UTF16ToUTF8(password_form.password_value) ==
          password_specifics.password_value() &&
      password_form.ssl_valid == password_specifics.ssl_valid() &&
      password_form.preferred == password_specifics.preferred() &&
      password_form.date_created.ToInternalValue() ==
          password_specifics.date_created() &&
      password_form.blacklisted_by_user == password_specifics.blacklisted()) {
    return false;
  }

  // If the passwords differ, take the one that was created more recently.
  if (base::Time::FromInternalValue(password_specifics.date_created()) <=
          password_form.date_created) {
    *new_password_form = password_form;
  } else {
    PasswordFromSpecifics(password_specifics, new_password_form);
  }

  return true;
}

std::string MakePasswordSyncTag(const std::string& origin_url,
                                const std::string& username_element,
                                const std::string& username_value,
                                const std::string& password_element,
                                const std::string& signon_realm) {
  return net::EscapePath(origin_url) + "|" +
         net::EscapePath(username_element) + "|" +
         net::EscapePath(username_value) + "|" +
         net::EscapePath(password_element) + "|" +
         net::EscapePath(signon_realm);
}

std::string MakePasswordSyncTag(const autofill::PasswordForm& password) {
  return MakePasswordSyncTag(password.origin.spec(),
                             base::UTF16ToUTF8(password.username_element),
                             base::UTF16ToUTF8(password.username_value),
                             base::UTF16ToUTF8(password.password_element),
                             password.signon_realm);
}

syncer::SyncChange::SyncChangeType GetSyncChangeType(
    PasswordStoreChange::Type type) {
  switch (type) {
    case PasswordStoreChange::ADD:
      return syncer::SyncChange::ACTION_ADD;
    case PasswordStoreChange::UPDATE:
      return syncer::SyncChange::ACTION_UPDATE;
    case PasswordStoreChange::REMOVE:
      return syncer::SyncChange::ACTION_DELETE;
  }
  NOTREACHED();
  return syncer::SyncChange::ACTION_INVALID;
}

void AppendChanges(const PasswordStoreChangeList& new_changes,
                   PasswordStoreChangeList* all_changes) {
  all_changes->insert(all_changes->end(),
                      new_changes.begin(),
                      new_changes.end());
}

}  // namespace

PasswordSyncableService::PasswordSyncableService(
    scoped_refptr<PasswordStore> password_store)
    : password_store_(password_store) {
}

PasswordSyncableService::~PasswordSyncableService() {}

syncer::SyncMergeResult PasswordSyncableService::MergeDataAndStartSyncing(
    syncer::ModelType type,
    const syncer::SyncDataList& initial_sync_data,
    scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
    scoped_ptr<syncer::SyncErrorFactory> sync_error_factory) {
  DCHECK_EQ(syncer::PASSWORDS, type);
  syncer::SyncMergeResult merge_result(type);
  sync_error_factory_ = sync_error_factory.Pass();
  sync_processor_ = sync_processor.Pass();

  ScopedVector<autofill::PasswordForm> password_entries;
  if (!password_store_->FillAutofillableLogins(&password_entries.get())) {
    // Password store often fails to load passwords. Track failures with UMA.
    // (http://crbug.com/249000)
    UMA_HISTOGRAM_ENUMERATION("Sync.LocalDataFailedToLoad",
                              syncer::PASSWORDS,
                              syncer::MODEL_TYPE_COUNT);
    merge_result.set_error(sync_error_factory_->CreateAndUploadError(
        FROM_HERE,
        "Failed to get passwords from store."));
    return merge_result;
  }

  PasswordEntryMap new_local_entries;
  for (PasswordForms::iterator it = password_entries.begin();
       it != password_entries.end(); ++it) {
     autofill::PasswordForm* password_form = *it;
     // We add all the db entries as |new_local_entries| initially. During
     // model association entries that match a sync entry will be
     // removed and this list will only contain entries that are not in sync.
     new_local_entries.insert(
         std::make_pair(MakePasswordSyncTag(*password_form), password_form));
  }

  merge_result.set_num_items_before_association(new_local_entries.size());

  // List that contains the entries that are known only to sync.
  ScopedVector<autofill::PasswordForm> new_sync_entries;

  // List that contains the entries that are known to both sync and db but
  // have updates in sync. They need to be updated in the passwords db.
  ScopedVector<autofill::PasswordForm> updated_sync_entries;

  // Changes from password db that need to be propagated to sync.
  syncer::SyncChangeList updated_db_entries;
  for (syncer::SyncDataList::const_iterator sync_iter =
           initial_sync_data.begin();
       sync_iter != initial_sync_data.end(); ++sync_iter) {
    CreateOrUpdateEntry(*sync_iter,
                        &new_local_entries,
                        &new_sync_entries,
                        &updated_sync_entries,
                        &updated_db_entries);
  }

  WriteToPasswordStore(new_sync_entries.get(),
                       updated_sync_entries.get());

  merge_result.set_num_items_after_association(
      merge_result.num_items_before_association() + new_sync_entries.size());

  merge_result.set_num_items_added(new_sync_entries.size());

  merge_result.set_num_items_modified(updated_sync_entries.size());

  for (PasswordEntryMap::iterator it = new_local_entries.begin();
       it != new_local_entries.end();
       ++it) {
    updated_db_entries.push_back(
        syncer::SyncChange(FROM_HERE,
                           syncer::SyncChange::ACTION_ADD,
                           SyncDataFromPassword(*it->second)));
  }

  merge_result.set_error(
      sync_processor_->ProcessSyncChanges(FROM_HERE, updated_db_entries));
  return merge_result;
}

void PasswordSyncableService::StopSyncing(syncer::ModelType type) {
  sync_processor_.reset();
  sync_error_factory_.reset();
}

syncer::SyncDataList PasswordSyncableService::GetAllSyncData(
    syncer::ModelType type) const {
  syncer::SyncDataList sync_data;
  return sync_data;
}

syncer::SyncError PasswordSyncableService::ProcessSyncChanges(
    const tracked_objects::Location& from_here,
    const syncer::SyncChangeList& change_list) {
  syncer::SyncError error(FROM_HERE,
                          syncer::SyncError::UNRECOVERABLE_ERROR,
                          "Password Syncable Service Not Implemented.",
                          syncer::PASSWORDS);
  return error;
}

void PasswordSyncableService::ActOnPasswordStoreChanges(
    const PasswordStoreChangeList& local_changes) {
  if (!sync_processor_)
    return;
  syncer::SyncChangeList sync_changes;
  for (PasswordStoreChangeList::const_iterator it = local_changes.begin();
       it != local_changes.end();
       ++it) {
    sync_changes.push_back(
        syncer::SyncChange(FROM_HERE,
                           GetSyncChangeType(it->type()),
                           SyncDataFromPassword(it->form())));
  }
  sync_processor_->ProcessSyncChanges(FROM_HERE, sync_changes);
}

void PasswordSyncableService::WriteToPasswordStore(
    const PasswordForms& new_entries,
    const PasswordForms& updated_entries) {
  PasswordStoreChangeList changes;
  for (std::vector<autofill::PasswordForm*>::const_iterator it =
           new_entries.begin();
       it != new_entries.end();
       ++it) {
    AppendChanges(password_store_->AddLoginImpl(**it), &changes);
  }

  for (std::vector<autofill::PasswordForm*>::const_iterator it =
           updated_entries.begin();
       it != updated_entries.end();
       ++it) {
    AppendChanges(password_store_->UpdateLoginImpl(**it), &changes);
  }

  // We have to notify password store observers of the change by hand since
  // we use internal password store interfaces to make changes synchronously.
  NotifyPasswordStoreOfLoginChanges(changes);
}

void PasswordSyncableService::NotifyPasswordStoreOfLoginChanges(
    const PasswordStoreChangeList& changes) {
  password_store_->NotifyLoginsChanged(changes);
}

void PasswordSyncableService::CreateOrUpdateEntry(
    const syncer::SyncData& data,
    PasswordEntryMap* umatched_data_from_password_db,
    ScopedVector<autofill::PasswordForm>* new_sync_entries,
    ScopedVector<autofill::PasswordForm>* updated_sync_entries,
    syncer::SyncChangeList* updated_db_entries) {
  const sync_pb::EntitySpecifics& specifics = data.GetSpecifics();
  const sync_pb::PasswordSpecificsData& password_specifics(
      specifics.password().client_only_encrypted_data());
  std::string tag = MakePasswordSyncTag(password_specifics);

  // Check whether the data from sync is already in the password store.
  PasswordEntryMap::iterator existing_local_entry_iter =
      umatched_data_from_password_db->find(tag);
  if (existing_local_entry_iter == umatched_data_from_password_db->end()) {
    // The sync data is not in the password store, so we need to create it in
    // the password store. Add the entry to the new_entries list.
    scoped_ptr<autofill::PasswordForm> new_password(new autofill::PasswordForm);
    PasswordFromSpecifics(password_specifics, new_password.get());
    new_sync_entries->push_back(new_password.release());
  } else {
    // The entry is in password store. If the entries are not identical, then
    // the entries need to be merged.
    scoped_ptr<autofill::PasswordForm> new_password(new autofill::PasswordForm);
    if (MergeLocalAndSyncPasswords(password_specifics,
                                   *existing_local_entry_iter->second,
                                   new_password.get())) {
      // Rather than checking which database -- sync or local -- needs updating,
      // simply push an update to both. This will end up being a noop for the
      // database that didn't need an update.
      updated_db_entries->push_back(
          syncer::SyncChange(FROM_HERE,
                             syncer::SyncChange::ACTION_UPDATE,
                             SyncDataFromPassword(*new_password)));

      updated_sync_entries->push_back(new_password.release());
    }
    // Remove the entry from the entry map to indicate a match has been found.
    // Entries that remain in the map at the end of associating all sync entries
    // will be treated as additions that need to be propagated to sync.
    umatched_data_from_password_db->erase(existing_local_entry_iter);
  }
}

syncer::SyncData SyncDataFromPassword(
    const autofill::PasswordForm& password_form) {
  sync_pb::EntitySpecifics password_data;
  sync_pb::PasswordSpecificsData* password_specifics =
      password_data.mutable_password()->mutable_client_only_encrypted_data();
  password_specifics->set_scheme(password_form.scheme);
  password_specifics->set_signon_realm(password_form.signon_realm);
  password_specifics->set_origin(password_form.origin.spec());
  password_specifics->set_action(password_form.action.spec());
  password_specifics->set_username_element(
      base::UTF16ToUTF8(password_form.username_element));
  password_specifics->set_password_element(
      base::UTF16ToUTF8(password_form.password_element));
  password_specifics->set_username_value(
      base::UTF16ToUTF8(password_form.username_value));
  password_specifics->set_password_value(
      base::UTF16ToUTF8(password_form.password_value));
  password_specifics->set_ssl_valid(password_form.ssl_valid);
  password_specifics->set_preferred(password_form.preferred);
  password_specifics->set_date_created(
      password_form.date_created.ToInternalValue());
  password_specifics->set_blacklisted(password_form.blacklisted_by_user);

  std::string tag = MakePasswordSyncTag(*password_specifics);
  return syncer::SyncData::CreateLocalData(tag, tag, password_data);
}

void PasswordFromSpecifics(const sync_pb::PasswordSpecificsData& password,
                           autofill::PasswordForm* new_password) {
  new_password->scheme =
      static_cast<autofill::PasswordForm::Scheme>(password.scheme());
  new_password->signon_realm = password.signon_realm();
  new_password->origin = GURL(password.origin());
  new_password->action = GURL(password.action());
  new_password->username_element =
      base::UTF8ToUTF16(password.username_element());
  new_password->password_element =
      base::UTF8ToUTF16(password.password_element());
  new_password->username_value = base::UTF8ToUTF16(password.username_value());
  new_password->password_value = base::UTF8ToUTF16(password.password_value());
  new_password->ssl_valid = password.ssl_valid();
  new_password->preferred = password.preferred();
  new_password->date_created =
      base::Time::FromInternalValue(password.date_created());
  new_password->blacklisted_by_user = password.blacklisted();
}

std::string MakePasswordSyncTag(
    const sync_pb::PasswordSpecificsData& password) {
  return MakePasswordSyncTag(password.origin(),
                             password.username_element(),
                             password.username_value(),
                             password.password_element(),
                             password.signon_realm());
}
