// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_STORE_H_
#define COMPONENTS_DOM_DISTILLER_CORE_DOM_DISTILLER_STORE_H_

#include <vector>

#include "base/containers/hash_tables.h"
#include "base/memory/weak_ptr.h"
#include "components/dom_distiller/core/article_entry.h"
#include "components/dom_distiller/core/dom_distiller_database.h"
#include "sync/api/sync_change.h"
#include "sync/api/sync_data.h"
#include "sync/api/sync_error.h"
#include "sync/api/sync_error_factory.h"
#include "sync/api/sync_merge_result.h"
#include "sync/api/syncable_service.h"

namespace base {
class FilePath;
}

namespace dom_distiller {

// Interface for accessing the stored/synced DomDistiller entries.
class DomDistillerStoreInterface {
 public:
  virtual ~DomDistillerStoreInterface() {}

  // Gets the syncable service for this store or null if it is not synced.
  virtual syncer::SyncableService* GetSyncableService() = 0;

  virtual bool AddEntry(const ArticleEntry& entry) = 0;

  // Gets a copy of all the current entries.
  virtual std::vector<ArticleEntry> GetEntries() const = 0;

  // TODO(cjhopman): This should have a way to observe changes to the underlying
  // model.
};

// Implements syncing/storing of DomDistiller entries. This keeps three
// models of the DOM distiller data in sync: the local database, sync, and the
// user (i.e. of DomDistillerStore). No changes are accepted while the local
// database is loading. Once the local database has loaded, changes from any of
// the three sources (technically just two, since changes don't come from the
// database) are handled similarly:
// 1. convert the change to a SyncChangeList.
// 2. apply that change to the in-memory model, calculating what changed
// (changes_applied) and what is missing--i.e. entries missing for a full merge,
// conflict resolution for normal changes-- (changes_missing).
// 3. send a message (possibly handled asynchronously) containing
// changes_missing to the source of the change.
// 4. send messages (possibly handled asynchronously) containing changes_applied
// to the other (i.e. non-source) two models.
// TODO(cjhopman): Support deleting entries.
class DomDistillerStore : public syncer::SyncableService,
                          DomDistillerStoreInterface {
 public:
  typedef base::hash_map<std::string, ArticleEntry> EntryMap;

  // Creates storage using the given database for local storage. Initializes the
  // database with |database_dir|.
  DomDistillerStore(scoped_ptr<DomDistillerDatabaseInterface> database,
                    const base::FilePath& database_dir);

  // Creates storage using the given database for local storage. Initializes the
  // database with |database_dir|.  Also initializes the internal model to
  // |initial_model|.
  DomDistillerStore(scoped_ptr<DomDistillerDatabaseInterface> database,
                    const EntryMap& initial_model,
                    const base::FilePath& database_dir);

  virtual ~DomDistillerStore();

  // DomDistillerStoreInterface implementation.
  virtual syncer::SyncableService* GetSyncableService() OVERRIDE;
  virtual bool AddEntry(const ArticleEntry& entry) OVERRIDE;
  virtual std::vector<ArticleEntry> GetEntries() const OVERRIDE;

  // syncer::SyncableService implementation.
  virtual syncer::SyncMergeResult MergeDataAndStartSyncing(
      syncer::ModelType type,
      const syncer::SyncDataList& initial_sync_data,
      scoped_ptr<syncer::SyncChangeProcessor> sync_processor,
      scoped_ptr<syncer::SyncErrorFactory> error_handler) OVERRIDE;
  virtual void StopSyncing(syncer::ModelType type) OVERRIDE;
  virtual syncer::SyncDataList GetAllSyncData(syncer::ModelType type) const
      OVERRIDE;
  virtual syncer::SyncError ProcessSyncChanges(
      const tracked_objects::Location& from_here,
      const syncer::SyncChangeList& change_list) OVERRIDE;

  static ArticleEntry GetEntryFromChange(const syncer::SyncChange& change);

 private:
  void OnDatabaseInit(bool success);
  void OnDatabaseLoad(bool success, scoped_ptr<EntryVector> entries);
  void OnDatabaseSave(bool success);

  syncer::SyncMergeResult MergeDataWithModel(
      const syncer::SyncDataList& data,
      syncer::SyncChangeList* changes_applied,
      syncer::SyncChangeList* changes_missing);

  // Convert a SyncDataList to a SyncChangeList of add or update changes based
  // on the state of the in-memory model. Also calculate the entries missing
  // from the SyncDataList.
  void CalculateChangesForMerge(const syncer::SyncDataList& data,
                                syncer::SyncChangeList* changes_to_apply,
                                syncer::SyncChangeList* changes_missing);

  bool ApplyChangesToSync(const tracked_objects::Location& from_here,
                          const syncer::SyncChangeList& change_list);
  bool ApplyChangesToDatabase(const syncer::SyncChangeList& change_list);

  // Applies the change list to the in-memory model, appending the actual
  // changes made to the model to changes_applied. If conflict resolution does
  // not apply the requested change, then adds the "diff" to changes_missing.
  syncer::SyncError ApplyChangesToModel(
      const syncer::SyncChangeList& change_list,
      syncer::SyncChangeList* changes_applied,
      syncer::SyncChangeList* changes_missing);

  void ApplyChangeToModel(const syncer::SyncChange& change,
                          syncer::SyncChangeList* changes_applied,
                          syncer::SyncChangeList* changes_missing);

  scoped_ptr<syncer::SyncChangeProcessor> sync_processor_;
  scoped_ptr<syncer::SyncErrorFactory> error_factory_;
  scoped_ptr<DomDistillerDatabaseInterface> database_;
  bool database_loaded_;

  EntryMap model_;

  base::WeakPtrFactory<DomDistillerStore> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DomDistillerStore);
};

}  // namespace dom_distiller

#endif
