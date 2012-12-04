// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_loader.h"

#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/instant/instant_controller.h"
#include "chrome/browser/thumbnails/thumbnail_tab_helper.h"
#include "chrome/browser/ui/blocked_content/blocked_content_tab_helper.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/constrained_window_tab_helper_delegate.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_delegate.h"
#include "ipc/ipc_message.h"

namespace {

int kUserDataKey;

class InstantLoaderUserData : public base::SupportsUserData::Data {
 public:
  explicit InstantLoaderUserData(InstantLoader* loader) : loader_(loader) {}

  InstantLoader* loader() const { return loader_; }

 private:
  ~InstantLoaderUserData() {}

  InstantLoader* const loader_;

  DISALLOW_COPY_AND_ASSIGN(InstantLoaderUserData);
};

}

// WebContentsDelegateImpl -----------------------------------------------------

class InstantLoader::WebContentsDelegateImpl
    : public ConstrainedWindowTabHelperDelegate,
      public CoreTabHelperDelegate,
      public content::WebContentsDelegate {
 public:
  explicit WebContentsDelegateImpl(InstantLoader* loader);

 private:
  // Overridden from ConstrainedWindowTabHelperDelegate:
  virtual bool ShouldFocusConstrainedWindow() OVERRIDE;

  // Overridden from CoreTabHelperDelegate:
  virtual void SwapTabContents(content::WebContents* old_contents,
                               content::WebContents* new_contents) OVERRIDE;

  // Overridden from content::WebContentsDelegate:
  virtual bool ShouldSuppressDialogs() OVERRIDE;
  virtual bool ShouldFocusPageAfterCrash() OVERRIDE;
  virtual void LostCapture() OVERRIDE;
  virtual void WebContentsFocused(content::WebContents* contents) OVERRIDE;
  virtual bool CanDownload(content::RenderViewHost* render_view_host,
                           int request_id,
                           const std::string& request_method) OVERRIDE;
  virtual void HandleMouseDown() OVERRIDE;
  virtual void HandleMouseUp() OVERRIDE;
  virtual void HandlePointerActivate() OVERRIDE;
  virtual void HandleGestureEnd() OVERRIDE;
  virtual void DragEnded() OVERRIDE;
  virtual bool OnGoToEntryOffset(int offset) OVERRIDE;

  void MaybeCommitFromPointerRelease();

  InstantLoader* const loader_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDelegateImpl);
};

InstantLoader::WebContentsDelegateImpl::WebContentsDelegateImpl(
    InstantLoader* loader)
    : loader_(loader) {
}

bool InstantLoader::WebContentsDelegateImpl::ShouldFocusConstrainedWindow() {
  // Return false so that constrained windows are not initially focused. If we
  // did otherwise the preview would prematurely get committed when focus goes
  // to the constrained window.
  return false;
}

void InstantLoader::WebContentsDelegateImpl::SwapTabContents(
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  // If this is being called, something is swapping in to loader's |contents_|
  // before we've added it to the tab strip.
  loader_->ReplacePreviewContents(old_contents, new_contents);
}

bool InstantLoader::WebContentsDelegateImpl::ShouldSuppressDialogs() {
  // Any message shown during Instant cancels Instant, so we suppress them.
  return true;
}

bool InstantLoader::WebContentsDelegateImpl::ShouldFocusPageAfterCrash() {
  return false;
}

void InstantLoader::WebContentsDelegateImpl::LostCapture() {
  MaybeCommitFromPointerRelease();
}

void InstantLoader::WebContentsDelegateImpl::WebContentsFocused(
    content::WebContents* /* contents */) {
  loader_->controller_->InstantLoaderContentsFocused();
}

bool InstantLoader::WebContentsDelegateImpl::CanDownload(
    content::RenderViewHost* /* render_view_host */,
    int /* request_id */,
    const std::string& /* request_method */) {
  // Downloads are disabled.
  return false;
}

void InstantLoader::WebContentsDelegateImpl::HandleMouseDown() {
  loader_->is_pointer_down_from_activate_ = true;
}

void InstantLoader::WebContentsDelegateImpl::HandleMouseUp() {
  MaybeCommitFromPointerRelease();
}

void InstantLoader::WebContentsDelegateImpl::HandlePointerActivate() {
  loader_->is_pointer_down_from_activate_ = true;
}

void InstantLoader::WebContentsDelegateImpl::HandleGestureEnd() {
  MaybeCommitFromPointerRelease();
}

void InstantLoader::WebContentsDelegateImpl::DragEnded() {
  // If the user drags, we won't get a mouse up (at least on Linux). Commit the
  // Instant result when the drag ends, so that during the drag the page won't
  // move around.
  MaybeCommitFromPointerRelease();
}

bool InstantLoader::WebContentsDelegateImpl::OnGoToEntryOffset(int offset) {
  return false;
}

void InstantLoader::WebContentsDelegateImpl::MaybeCommitFromPointerRelease() {
  if (loader_->is_pointer_down_from_activate_) {
    loader_->is_pointer_down_from_activate_ = false;
    loader_->controller_->CommitIfCurrent(INSTANT_COMMIT_FOCUS_LOST);
  }
}

// InstantLoader ---------------------------------------------------------------

// static
InstantLoader* InstantLoader::FromWebContents(
    const content::WebContents* web_contents) {
  InstantLoaderUserData* data = static_cast<InstantLoaderUserData*>(
      web_contents->GetUserData(&kUserDataKey));
  return data ? data->loader() : NULL;
}

InstantLoader::InstantLoader(InstantController* controller,
                             const std::string& instant_url)
    : client_(ALLOW_THIS_IN_INITIALIZER_LIST(this)),
      controller_(controller),
      delegate_(new WebContentsDelegateImpl(
                        ALLOW_THIS_IN_INITIALIZER_LIST(this))),
      instant_url_(instant_url),
      supports_instant_(false),
      is_pointer_down_from_activate_(false) {
}

InstantLoader::~InstantLoader() {
}

void InstantLoader::InitContents(const content::WebContents* active_tab) {
  contents_.reset(content::WebContents::CreateWithSessionStorage(
      active_tab->GetBrowserContext(), NULL, MSG_ROUTING_NONE, active_tab,
      active_tab->GetController().GetSessionStorageNamespaceMap()));
  // Not a leak. TabContents will delete itself when the WebContents is gone.
  TabContents::Factory::CreateTabContents(contents());
  SetupPreviewContents();

  // This HTTP header and value are set on loads that originate from Instant.
  const char kInstantHeader[] = "X-Purpose: Instant";
  DVLOG(1) << "LoadURL: " << instant_url_;
  contents_->GetController().LoadURL(GURL(instant_url_), content::Referrer(),
      content::PAGE_TRANSITION_GENERATED, kInstantHeader);
  contents_->WasHidden();
}

content::WebContents* InstantLoader::ReleaseContents() {
  CleanupPreviewContents();
  return contents_.release();
}

void InstantLoader::DidNavigate(
    const history::HistoryAddPageArgs& add_page_args) {
  last_navigation_ = add_page_args;
}

void InstantLoader::Update(const string16& text,
                           size_t selection_start,
                           size_t selection_end,
                           bool verbatim) {
  last_navigation_ = history::HistoryAddPageArgs();
  client_.Update(text, selection_start, selection_end, verbatim);
}

void InstantLoader::Submit(const string16& text) {
  client_.Submit(text);
}

void InstantLoader::Cancel(const string16& text) {
  client_.Cancel(text);
}

void InstantLoader::SetOmniboxBounds(const gfx::Rect& bounds) {
  client_.SetOmniboxBounds(bounds);
}

void InstantLoader::SendAutocompleteResults(
    const std::vector<InstantAutocompleteResult>& results) {
  client_.SendAutocompleteResults(results);
}

void InstantLoader::UpOrDownKeyPressed(int count) {
  client_.UpOrDownKeyPressed(count);
}

void InstantLoader::SearchModeChanged(const chrome::search::Mode& mode) {
  client_.SearchModeChanged(mode);
}

void InstantLoader::SendThemeBackgroundInfo(
    const ThemeBackgroundInfo& theme_info) {
  client_.SendThemeBackgroundInfo(theme_info);
}

void InstantLoader::SendThemeAreaHeight(int height) {
  client_.SendThemeAreaHeight(height);
}

void InstantLoader::SetDisplayInstantResults(bool display_instant_results) {
  client_.SetDisplayInstantResults(display_instant_results);
}

void InstantLoader::SetSuggestions(
    const std::vector<InstantSuggestion>& suggestions) {
  InstantSupportDetermined(true);
  controller_->SetSuggestions(contents(), suggestions);
}

void InstantLoader::InstantSupportDetermined(bool supports_instant) {
  // If we had already determined that the page supports Instant, nothing to do.
  if (supports_instant_)
    return;

  supports_instant_ = supports_instant;
  controller_->InstantSupportDetermined(contents(), supports_instant);
}

void InstantLoader::ShowInstantPreview(InstantShownReason reason,
                                       int height,
                                       InstantSizeUnits units) {
  InstantSupportDetermined(true);
  controller_->ShowInstantPreview(reason, height, units);
}

void InstantLoader::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
#if defined(OS_MACOSX)
  if (type == content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED) {
    if (content::RenderWidgetHostView* rwhv =
            contents_->GetRenderWidgetHostView())
      rwhv->SetTakesFocusOnlyOnMouseDown(true);
    return;
  }
  NOTREACHED();
#endif
}

void InstantLoader::SetupPreviewContents() {
  client_.SetContents(contents());
  contents_->SetUserData(&kUserDataKey, new InstantLoaderUserData(this));
  contents_->SetDelegate(delegate_.get());

  // Disable popups and such (mainly to avoid losing focus and reverting the
  // preview prematurely).
  if (BlockedContentTabHelper* blocked_content_tab_helper =
          BlockedContentTabHelper::FromWebContents(contents()))
    blocked_content_tab_helper->SetAllContentsBlocked(true);
  if (ConstrainedWindowTabHelper* constrained_window_tab_helper =
          ConstrainedWindowTabHelper::FromWebContents(contents()))
    constrained_window_tab_helper->set_delegate(delegate_.get());
  if (TabSpecificContentSettings* tab_specific_content_settings =
          TabSpecificContentSettings::FromWebContents(contents()))
    tab_specific_content_settings->SetPopupsBlocked(true);
  if (CoreTabHelper* core_tab_helper =
          CoreTabHelper::FromWebContents(contents()))
    core_tab_helper->set_delegate(delegate_.get());
  if (ThumbnailTabHelper* thumbnail_tab_helper =
          ThumbnailTabHelper::FromWebContents(contents()))
    thumbnail_tab_helper->set_enabled(false);

#if defined(OS_MACOSX)
  // If |contents_| doesn't yet have a RWHV, SetTakesFocusOnlyOnMouseDown() will
  // be called later, when NOTIFICATION_RENDER_VIEW_HOST_CHANGED is received.
  if (content::RenderWidgetHostView* rwhv =
          contents_->GetRenderWidgetHostView())
    rwhv->SetTakesFocusOnlyOnMouseDown(true);
  registrar_.Add(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                 content::Source<content::NavigationController>(
                     &contents_->GetController()));
#endif
}

void InstantLoader::CleanupPreviewContents() {
  client_.SetContents(NULL);
  contents_->RemoveUserData(&kUserDataKey);
  contents_->SetDelegate(NULL);

  if (BlockedContentTabHelper* blocked_content_tab_helper =
          BlockedContentTabHelper::FromWebContents(contents()))
    blocked_content_tab_helper->SetAllContentsBlocked(false);
  if (ConstrainedWindowTabHelper* constrained_window_tab_helper =
          ConstrainedWindowTabHelper::FromWebContents(contents()))
    constrained_window_tab_helper->set_delegate(NULL);
  if (TabSpecificContentSettings* tab_specific_content_settings =
          TabSpecificContentSettings::FromWebContents(contents()))
    tab_specific_content_settings->SetPopupsBlocked(false);
  if (CoreTabHelper* core_tab_helper =
          CoreTabHelper::FromWebContents(contents()))
    core_tab_helper->set_delegate(NULL);
  if (ThumbnailTabHelper* thumbnail_tab_helper =
          ThumbnailTabHelper::FromWebContents(contents()))
    thumbnail_tab_helper->set_enabled(true);

#if defined(OS_MACOSX)
  if (content::RenderWidgetHostView* rwhv =
          contents_->GetRenderWidgetHostView())
    rwhv->SetTakesFocusOnlyOnMouseDown(false);
  registrar_.Remove(this, content::NOTIFICATION_RENDER_VIEW_HOST_CHANGED,
                    content::Source<content::NavigationController>(
                        &contents_->GetController()));
#endif
}

void InstantLoader::ReplacePreviewContents(content::WebContents* old_contents,
                                           content::WebContents* new_contents) {
  DCHECK_EQ(old_contents, contents());
  CleanupPreviewContents();
  // We release here without deleting so that the caller still has the
  // responsibility for deleting the WebContents.
  ignore_result(contents_.release());
  contents_.reset(new_contents);
  SetupPreviewContents();
  controller_->SwappedWebContents();
}
