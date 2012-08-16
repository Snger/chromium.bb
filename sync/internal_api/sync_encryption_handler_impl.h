// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SYNC_INTERNAL_API_SYNC_ENCRYPTION_HANDLER_IMPL_H_
#define SYNC_INTERNAL_API_SYNC_ENCRYPTION_HANDLER_IMPL_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "sync/internal_api/public/sync_encryption_handler.h"
#include "sync/syncable/nigori_handler.h"

namespace syncer {

struct UserShare;
class WriteNode;
class WriteTransaction;

// Sync encryption handler implementation.
//
// This class acts as the respository of all sync encryption state, and handles
// encryption related changes/queries coming from both the chrome side and
// the sync side (via NigoriHandler). It is capable of modifying all sync data
// (re-encryption), updating the encrypted types, changing the encryption keys,
// and creating/receiving nigori node updates.
//
// The class should live as long as the directory itself in order to ensure
// any data read/written is properly decrypted/encrypted.
//
// Note: See sync_encryption_handler.h for a description of the chrome visible
// methods and what they do, and nigori_handler.h for a description of the
// sync methods.
//
// TODO(zea): Make this class explicitly non-thread safe and ensure its only
// accessed from the sync thread, with the possible exception of
// GetEncryptedTypes. Need to cache explicit passphrase state on the UI thread.
class SyncEncryptionHandlerImpl
    : public SyncEncryptionHandler,
      public syncable::NigoriHandler {
 public:
  SyncEncryptionHandlerImpl(UserShare* user_share,
                            Cryptographer* cryptographer);
  virtual ~SyncEncryptionHandlerImpl();

  // SyncEncryptionHandler implementation.
  virtual void AddObserver(Observer* observer) OVERRIDE;
  virtual void RemoveObserver(Observer* observer) OVERRIDE;
  virtual void Init() OVERRIDE;
  virtual void SetEncryptionPassphrase(const std::string& passphrase,
                                       bool is_explicit) OVERRIDE;
  virtual void SetDecryptionPassphrase(const std::string& passphrase) OVERRIDE;
  virtual void EnableEncryptEverything() OVERRIDE;
  virtual bool EncryptEverythingEnabled() const OVERRIDE;
  virtual bool IsUsingExplicitPassphrase() const OVERRIDE;

  // NigoriHandler implementation.
  // Note: all methods are invoked while the caller holds a transaction.
  virtual void ApplyNigoriUpdate(
      const sync_pb::NigoriSpecifics& nigori,
      syncable::BaseTransaction* const trans) OVERRIDE;
  virtual void UpdateNigoriFromEncryptedTypes(
      sync_pb::NigoriSpecifics* nigori,
      syncable::BaseTransaction* const trans) const OVERRIDE;
  virtual ModelTypeSet GetEncryptedTypes() const OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest,
                           NigoriEncryptionTypes);
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest,
                           EncryptEverythingExplicit);
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest,
                           EncryptEverythingImplicit);
  FRIEND_TEST_ALL_PREFIXES(SyncEncryptionHandlerImplTest,
                           UnknownSensitiveTypes);

  // Iterate over all encrypted types ensuring each entry is properly encrypted.
  void ReEncryptEverything(WriteTransaction* trans);

  // Apply a nigori update. Updates internal and cryptographer state.
  // Returns true on success, false if |nigori| was incompatible, and the
  // nigori node must be corrected.
  // Note: must be called from within a transaction.
  bool ApplyNigoriUpdateImpl(const sync_pb::NigoriSpecifics& nigori,
                             syncable::BaseTransaction* const trans);

  // Wrapper around WriteEncryptionStateToNigori that creates a new write
  // transaction.
  void RewriteNigori();

  // Write the current encryption state into the nigori node. This includes
  // the encrypted types/encrypt everything state, as well as the keybag/
  // explicit passphrase state (if the cryptographer is ready).
  void WriteEncryptionStateToNigori(WriteTransaction* trans);

  // Updates local encrypted types from |nigori|.
  // Returns true if the local set of encrypted types either matched or was
  // a subset of that in |nigori|. Returns false if the local state already
  // had stricter encryption than |nigori|, and the nigori node needs to be
  // updated with the newer encryption state.
  // Note: must be called from within a transaction.
  bool UpdateEncryptedTypesFromNigori(const sync_pb::NigoriSpecifics& nigori);

  // The final step of SetEncryptionPassphrase and SetDecryptionPassphrase that
  // notifies observers of the result of the set passphrase operation, updates
  // the nigori node, and does re-encryption.
  // |success|: true if the operation was successful and false otherwise. If
  //            success == false, we send an OnPassphraseRequired notification.
  // |bootstrap_token|: used to inform observers if the cryptographer's
  //                    bootstrap token was updated.
  // |is_explicit|: used to differentiate between a custom passphrase (true) and
  //                a GAIA passphrase that is implicitly used for encryption
  //                (false).
  // |trans| and |nigori_node|: used to access data in the cryptographer.
  void FinishSetPassphrase(bool success,
                           const std::string& bootstrap_token,
                           bool is_explicit,
                           WriteTransaction* trans,
                           WriteNode* nigori_node);

  // Merges the given set of encrypted types with the existing set and emits a
  // notification if necessary.
  // Note: must be called from within a transaction.
  void MergeEncryptedTypes(ModelTypeSet encrypted_types);

  base::WeakPtrFactory<SyncEncryptionHandlerImpl> weak_ptr_factory_;

  ObserverList<SyncEncryptionHandler::Observer> observers_;

  // The current user share (for creating transactions).
  UserShare* user_share_;

  // TODO(zea): have the sync encryption handler own the cryptographer, and live
  // in the directory.
  Cryptographer* cryptographer_;

  // The set of types that require encryption. This is accessed on all sync
  // datatype threads when we write to a node, so we must hold a transaction
  // whenever we touch/read it.
  ModelTypeSet encrypted_types_;

  // Sync encryption state. These are only modified and accessed from the sync
  // thread.
  bool encrypt_everything_;
  bool explicit_passphrase_;

  // The number of times we've automatically (i.e. not via SetPassphrase or
  // conflict resolver) updated the nigori's encryption keys in this chrome
  // instantiation.
  int nigori_overwrite_count_;

  DISALLOW_COPY_AND_ASSIGN(SyncEncryptionHandlerImpl);
};

}  // namespace syncer

#endif  // SYNC_INTERNAL_API_PUBLIC_SYNC_ENCRYPTION_HANDLER_IMPL_H_
