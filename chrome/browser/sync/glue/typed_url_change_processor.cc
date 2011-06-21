// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/typed_url_change_processor.h"

#include "base/string_util.h"
#include "base/tracked.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/history/history_backend.h"
#include "chrome/browser/history/history_notifications.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/typed_url_model_associator.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/protocol/typed_url_specifics.pb.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"

namespace browser_sync {

TypedUrlChangeProcessor::TypedUrlChangeProcessor(
    TypedUrlModelAssociator* model_associator,
    history::HistoryBackend* history_backend,
    UnrecoverableErrorHandler* error_handler)
    : ChangeProcessor(error_handler),
      model_associator_(model_associator),
      history_backend_(history_backend),
      observing_(false),
      expected_loop_(MessageLoop::current()) {
  DCHECK(model_associator);
  DCHECK(history_backend);
  DCHECK(error_handler);
  DCHECK(!BrowserThread::CurrentlyOn(BrowserThread::UI));
  // When running in unit tests, there is already a NotificationService object.
  // Since only one can exist at a time per thread, check first.
  if (!NotificationService::current())
    notification_service_.reset(new NotificationService);
  StartObserving();
}

TypedUrlChangeProcessor::~TypedUrlChangeProcessor() {
  DCHECK(expected_loop_ == MessageLoop::current());
}

void TypedUrlChangeProcessor::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  DCHECK(expected_loop_ == MessageLoop::current());
  if (!observing_)
    return;

  VLOG(1) << "Observed typed_url change.";
  DCHECK(running());
  DCHECK(NotificationType::HISTORY_TYPED_URLS_MODIFIED == type ||
         NotificationType::HISTORY_URLS_DELETED == type ||
         NotificationType::HISTORY_URL_VISITED == type);
  if (type == NotificationType::HISTORY_TYPED_URLS_MODIFIED) {
    HandleURLsModified(Details<history::URLsModifiedDetails>(details).ptr());
  } else if (type == NotificationType::HISTORY_URLS_DELETED) {
    HandleURLsDeleted(Details<history::URLsDeletedDetails>(details).ptr());
  } else if (type == NotificationType::HISTORY_URL_VISITED) {
    HandleURLsVisited(Details<history::URLVisitedDetails>(details).ptr());
  }
}

void TypedUrlChangeProcessor::HandleURLsModified(
    history::URLsModifiedDetails* details) {

  sync_api::WriteTransaction trans(FROM_HERE, share_handle());
  for (std::vector<history::URLRow>::iterator url =
       details->changed_urls.begin(); url != details->changed_urls.end();
       ++url) {
    // Exit if we were unable to update the sync node.
    if (!CreateOrUpdateSyncNode(*url, &trans))
      return;
  }
}

bool TypedUrlChangeProcessor::CreateOrUpdateSyncNode(
    const history::URLRow& url, sync_api::WriteTransaction* trans) {
  // Get the visits for this node.
  history::VisitVector visit_vector;
  if (!history_backend_->GetVisitsForURL(url.id(), &visit_vector)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
                                          "Could not get the url's visits.");
    return false;
  }

  // Make sure our visit vector is not empty by ensuring at least the most
  // recent visit is found. Workaround for http://crbug.com/84258.
  if (visit_vector.empty()) {
    history::VisitRow visit(
        url.id(), url.last_visit(), 0, PageTransition::TYPED, 0);
    visit_vector.push_back(visit);
  }

  sync_api::ReadNode typed_url_root(trans);
  if (!typed_url_root.InitByTagLookup(kTypedUrlTag)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "Server did not create the top-level typed_url node. We "
         "might be running against an out-of-date server.");
    return false;
  }

  std::string tag = url.url().spec();
  DCHECK(!visit_vector.empty());

  sync_api::WriteNode update_node(trans);
  if (update_node.InitByClientTagLookup(syncable::TYPED_URLS, tag)) {
    // TODO(atwilson): Don't bother updating if the only change is
    // a visit deletion or addition of a RELOAD visit (http://crbug.com/82451).
    model_associator_->WriteToSyncNode(url, visit_vector, &update_node);
  } else {
    sync_api::WriteNode create_node(trans);
    if (!create_node.InitUniqueByCreation(syncable::TYPED_URLS,
                                          typed_url_root, tag)) {
      error_handler()->OnUnrecoverableError(
          FROM_HERE, "Failed to create typed_url sync node.");
      return false;
    }

    create_node.SetTitle(UTF8ToWide(tag));
    model_associator_->WriteToSyncNode(url, visit_vector, &create_node);
    model_associator_->Associate(&tag, create_node.GetId());
  }
  return true;
}

void TypedUrlChangeProcessor::HandleURLsDeleted(
    history::URLsDeletedDetails* details) {
  sync_api::WriteTransaction trans(FROM_HERE, share_handle());

  if (details->all_history) {
    if (!model_associator_->DeleteAllNodes(&trans)) {
      error_handler()->OnUnrecoverableError(FROM_HERE, std::string());
      return;
    }
  } else {
    for (std::set<GURL>::iterator url = details->urls.begin();
         url != details->urls.end(); ++url) {
      sync_api::WriteNode sync_node(&trans);
      int64 sync_id = model_associator_->GetSyncIdFromChromeId(url->spec());
      if (sync_api::kInvalidId != sync_id) {
        if (!sync_node.InitByIdLookup(sync_id)) {
          error_handler()->OnUnrecoverableError(FROM_HERE,
              "Typed url node lookup failed.");
          return;
        }
        model_associator_->Disassociate(sync_node.GetId());
        sync_node.Remove();
      }
    }
  }
}

void TypedUrlChangeProcessor::HandleURLsVisited(
    history::URLVisitedDetails* details) {
  if (!details->row.typed_count()) {
    // We only care about typed urls.
    return;
  }
  sync_api::WriteTransaction trans(FROM_HERE, share_handle());
  CreateOrUpdateSyncNode(details->row, &trans);
}

void TypedUrlChangeProcessor::ApplyChangesFromSyncModel(
    const sync_api::BaseTransaction* trans,
    const sync_api::SyncManager::ChangeRecord* changes,
    int change_count) {
  DCHECK(expected_loop_ == MessageLoop::current());
  if (!running())
    return;
  StopObserving();

  sync_api::ReadNode typed_url_root(trans);
  if (!typed_url_root.InitByTagLookup(kTypedUrlTag)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "TypedUrl root node lookup failed.");
    return;
  }

  TypedUrlModelAssociator::TypedUrlTitleVector titles;
  TypedUrlModelAssociator::TypedUrlVector new_urls;
  TypedUrlModelAssociator::TypedUrlVisitVector new_visits;
  history::VisitVector deleted_visits;
  TypedUrlModelAssociator::TypedUrlUpdateVector updated_urls;

  for (int i = 0; i < change_count; ++i) {
    if (sync_api::SyncManager::ChangeRecord::ACTION_DELETE ==
        changes[i].action) {
      DCHECK(changes[i].specifics.HasExtension(sync_pb::typed_url)) <<
          "Typed URL delete change does not have necessary specifics.";
      GURL url(changes[i].specifics.GetExtension(sync_pb::typed_url).url());
      history_backend_->DeleteURL(url);
      model_associator_->Disassociate(changes[i].id);
      continue;
    }

    sync_api::ReadNode sync_node(trans);
    if (!sync_node.InitByIdLookup(changes[i].id)) {
      error_handler()->OnUnrecoverableError(FROM_HERE,
          "TypedUrl node lookup failed.");
      return;
    }

    // Check that the changed node is a child of the typed_urls folder.
    DCHECK(typed_url_root.GetId() == sync_node.GetParentId());
    DCHECK(syncable::TYPED_URLS == sync_node.GetModelType());

    const sync_pb::TypedUrlSpecifics& typed_url(
        sync_node.GetTypedUrlSpecifics());
    GURL url(typed_url.url());

    if (sync_api::SyncManager::ChangeRecord::ACTION_ADD == changes[i].action) {
      DCHECK(typed_url.visits_size());
      if (!typed_url.visits_size()) {
        continue;
      }

      history::URLRow new_url(GURL(typed_url.url()));
      TypedUrlModelAssociator::UpdateURLRowFromTypedUrlSpecifics(
          typed_url, &new_url);

      model_associator_->Associate(&new_url.url().spec(), changes[i].id);
      new_urls.push_back(new_url);

      std::vector<history::VisitInfo> added_visits;
      for (int c = 0; c < typed_url.visits_size(); ++c) {
        DCHECK(c == 0 || typed_url.visits(c) > typed_url.visits(c - 1));
        added_visits.push_back(history::VisitInfo(
            base::Time::FromInternalValue(typed_url.visits(c)),
            typed_url.visit_transitions(c)));
      }

      new_visits.push_back(std::pair<GURL, std::vector<history::VisitInfo> >(
          url, added_visits));
    } else {
      DCHECK_EQ(sync_api::SyncManager::ChangeRecord::ACTION_UPDATE,
                changes[i].action);
      history::URLRow old_url;
      if (!history_backend_->GetURL(url, &old_url)) {
        error_handler()->OnUnrecoverableError(FROM_HERE,
            "TypedUrl db lookup failed.");
        return;
      }

      history::VisitVector visits;
      if (!history_backend_->GetVisitsForURL(old_url.id(), &visits)) {
        error_handler()->OnUnrecoverableError(FROM_HERE,
            "Could not get the url's visits.");
        return;
      }

      history::URLRow new_url(old_url);
      TypedUrlModelAssociator::UpdateURLRowFromTypedUrlSpecifics(
          typed_url, &new_url);

      updated_urls.push_back(
        std::pair<history::URLID, history::URLRow>(old_url.id(), new_url));

      if (old_url.title().compare(new_url.title()) != 0) {
        titles.push_back(std::pair<GURL, string16>(new_url.url(),
                                                   new_url.title()));
      }

      std::vector<history::VisitInfo> added_visits;
      history::VisitVector removed_visits;
      TypedUrlModelAssociator::DiffVisits(visits, typed_url,
                                          &added_visits, &removed_visits);
      if (added_visits.size()) {
        new_visits.push_back(std::pair<GURL, std::vector<history::VisitInfo> >(
                url, added_visits));
      }
      if (removed_visits.size()) {
        deleted_visits.insert(deleted_visits.end(), removed_visits.begin(),
                              removed_visits.end());
      }
    }
  }
  if (!model_associator_->WriteToHistoryBackend(&titles, &new_urls,
                                                &updated_urls,
                                                &new_visits, &deleted_visits)) {
    error_handler()->OnUnrecoverableError(FROM_HERE,
        "Could not write to the history backend.");
    return;
  }

  StartObserving();
}

void TypedUrlChangeProcessor::StartImpl(Profile* profile) {
  DCHECK(expected_loop_ == MessageLoop::current());
  observing_ = true;
}

void TypedUrlChangeProcessor::StopImpl() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observing_ = false;
}


void TypedUrlChangeProcessor::StartObserving() {
  DCHECK(expected_loop_ == MessageLoop::current());
  notification_registrar_.Add(this,
                              NotificationType::HISTORY_TYPED_URLS_MODIFIED,
                              NotificationService::AllSources());
  notification_registrar_.Add(this, NotificationType::HISTORY_URLS_DELETED,
                              NotificationService::AllSources());
  notification_registrar_.Add(this, NotificationType::HISTORY_URL_VISITED,
                              NotificationService::AllSources());
}

void TypedUrlChangeProcessor::StopObserving() {
  DCHECK(expected_loop_ == MessageLoop::current());
  notification_registrar_.Remove(this,
                                 NotificationType::HISTORY_TYPED_URLS_MODIFIED,
                                 NotificationService::AllSources());
  notification_registrar_.Remove(this,
                                 NotificationType::HISTORY_URLS_DELETED,
                                 NotificationService::AllSources());
  notification_registrar_.Remove(this,
                                 NotificationType::HISTORY_URL_VISITED,
                                 NotificationService::AllSources());
}

}  // namespace browser_sync
