// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/bookmarks/bookmark_tab_helper.h"

#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_node_data.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_tab_helper_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/webui/ntp/new_tab_ui.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"

DEFINE_WEB_CONTENTS_USER_DATA_KEY(BookmarkTabHelper)

namespace {

bool CanShowBookmarkBar(content::WebUI* ui) {
  if (!ui)
    return false;
  NewTabUI* new_tab = NewTabUI::FromWebUIController(ui->GetController());
  return new_tab && new_tab->CanShowBookmarkBar();
}

}  // namespace

BookmarkTabHelper::BookmarkTabHelper(content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents),
      is_starred_(false),
      bookmark_model_(NULL),
      delegate_(NULL),
      bookmark_drag_(NULL) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  bookmark_model_= BookmarkModelFactory::GetForProfile(profile);
  if (bookmark_model_)
    bookmark_model_->AddObserver(this);
}

BookmarkTabHelper::~BookmarkTabHelper() {
  if (bookmark_model_)
    bookmark_model_->RemoveObserver(this);
}

bool BookmarkTabHelper::ShouldShowBookmarkBar() {
  if (web_contents()->ShowingInterstitialPage())
    return false;

  // See WebContents::GetWebUIForCurrentState() comment for more info. This case
  // is very similar, but for non-first loads, we want to use the committed
  // entry. This is so the bookmarks bar disappears at the same time the page
  // does.
  if (web_contents()->GetController().GetLastCommittedEntry()) {
    // Not the first load, always use the committed Web UI.
    return CanShowBookmarkBar(web_contents()->GetCommittedWebUI());
  }

  // When it's the first load, we know either the pending one or the committed
  // one will have the Web UI in it (see GetWebUIForCurrentState), and only one
  // of them will be valid, so we can just check both.
  return CanShowBookmarkBar(web_contents()->GetWebUI());
}

void BookmarkTabHelper::DidNavigateMainFrame(
    const content::LoadCommittedDetails& /*details*/,
    const content::FrameNavigateParams& /*params*/) {
  UpdateStarredStateForCurrentURL();
}

void BookmarkTabHelper::SetBookmarkDragDelegate(
    BookmarkTabHelper::BookmarkDrag* bookmark_drag) {
  bookmark_drag_ = bookmark_drag;
}

BookmarkTabHelper::BookmarkDrag*
    BookmarkTabHelper::GetBookmarkDragDelegate() {
  return bookmark_drag_;
}

void BookmarkTabHelper::UpdateStarredStateForCurrentURL() {
  const bool old_state = is_starred_;
  is_starred_ = (bookmark_model_ &&
                 bookmark_model_->IsBookmarked(web_contents()->GetURL()));

  if (is_starred_ != old_state && delegate())
    delegate()->URLStarredChanged(web_contents(), is_starred_);
}

void BookmarkTabHelper::BookmarkModelChanged() {
}

void BookmarkTabHelper::Loaded(BookmarkModel* model, bool ids_reassigned) {
  UpdateStarredStateForCurrentURL();
}

void BookmarkTabHelper::BookmarkNodeAdded(BookmarkModel* model,
                                          const BookmarkNode* parent,
                                          int index) {
  UpdateStarredStateForCurrentURL();
}

void BookmarkTabHelper::BookmarkNodeRemoved(BookmarkModel* model,
                                            const BookmarkNode* parent,
                                            int old_index,
                                            const BookmarkNode* node) {
  UpdateStarredStateForCurrentURL();
}

void BookmarkTabHelper::BookmarkNodeChanged(BookmarkModel* model,
                                            const BookmarkNode* node) {
  UpdateStarredStateForCurrentURL();
}
