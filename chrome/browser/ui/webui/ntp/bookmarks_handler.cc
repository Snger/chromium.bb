// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ntp/bookmarks_handler.h"

#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/extensions/extension_bookmark_helpers.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "content/common/notification_service.h"

BookmarksHandler::BookmarksHandler() {
  // TODO(csilv): Register for bookmark model change notifications.
}

BookmarksHandler::~BookmarksHandler() {}

void BookmarksHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("getBookmarksData",
      NewCallback(this, &BookmarksHandler::HandleGetBookmarksData));
}

void BookmarksHandler::Observe(int type,
                               const NotificationSource& source,
                               const NotificationDetails& details) {
  // TODO(csilv): Update UI based on changes to bookmark notifications.
}

void BookmarksHandler::HandleGetBookmarksData(const base::ListValue* args) {
  int64 id;
  std::string id_string;
  PrefService* prefs = Profile::FromWebUI(web_ui_)->GetPrefs();
  if (args->GetString(0, &id_string) && base::StringToInt64(id_string, &id)) {
    // A folder ID was requested, so persist this value.
    prefs->SetInt64(prefs::kNTPShownBookmarksFolder, id);
  } else {
    // No folder ID was requested, so get the default (persisted) value.
    id = prefs->GetInt64(prefs::kNTPShownBookmarksFolder);
  }

  BookmarkModel* model = Profile::FromWebUI(web_ui_)->GetBookmarkModel();
  const BookmarkNode* node = model->GetNodeByID(id);
  if (!node)
    return;

  base::ListValue* items = new base::ListValue();
  int child_count = node->child_count();
  for (int i = 0; i < child_count; ++i) {
    const BookmarkNode* child = node->GetChild(i);
    extension_bookmark_helpers::AddNode(child, items, false);
  }

  base::ListValue* navigation_items = new base::ListValue();
  while (node) {
    extension_bookmark_helpers::AddNode(node, navigation_items, false);
    node = node->parent();
  }

  base::DictionaryValue bookmarksData;
  bookmarksData.Set("items", items);
  bookmarksData.Set("navigationItems", navigation_items);
  web_ui_->CallJavascriptFunction("ntp4.setBookmarksData", bookmarksData);
}

// static
void BookmarksHandler::RegisterUserPrefs(PrefService* prefs) {
  // Default folder is the root node.
  // TODO(csilv): Should we default to the Bookmarks bar?
  // TODO(csilv): Should we sync this preference?
  prefs->RegisterInt64Pref(prefs::kNTPShownBookmarksFolder, 0,
                           PrefService::UNSYNCABLE_PREF);
}
