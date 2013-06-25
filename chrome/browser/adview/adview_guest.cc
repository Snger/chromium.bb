// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/adview/adview_guest.h"

#include "base/lazy_instance.h"
#include "chrome/browser/adview/adview_constants.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_system.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"

using content::WebContents;

namespace {

typedef std::map<std::pair<int, int>, AdViewGuest*> AdViewGuestMap;
base::LazyInstance<AdViewGuestMap> adview_guest_map =
    LAZY_INSTANCE_INITIALIZER;

}  // namespace

AdViewGuest::AdViewGuest(WebContents* guest_web_contents,
                         WebContents* embedder_web_contents,
                         const std::string& extension_id,
                         int view_instance_id,
                         const base::DictionaryValue& args)
    : WebContentsObserver(guest_web_contents),
      embedder_web_contents_(embedder_web_contents),
      extension_id_(extension_id),
      embedder_render_process_id_(
          embedder_web_contents->GetRenderProcessHost()->GetID()),
      profile_(guest_web_contents->GetBrowserContext()),
      guest_instance_id_(guest_web_contents->GetEmbeddedInstanceID()),
      view_instance_id_(view_instance_id) {
  std::pair<int, int> key(embedder_render_process_id_, guest_instance_id_);
  adview_guest_map.Get().insert(std::make_pair(key, this));
}

// static
AdViewGuest* AdViewGuest::From(int embedder_process_id,
                               int guest_instance_id) {
  AdViewGuestMap* guest_map = adview_guest_map.Pointer();
  AdViewGuestMap::iterator it = guest_map->find(
      std::make_pair(embedder_process_id, guest_instance_id));
  return it == guest_map->end() ? NULL : it->second;
}

AdViewGuest::~AdViewGuest() {
  std::pair<int, int> key(embedder_render_process_id_, guest_instance_id_);
  adview_guest_map.Get().erase(key);
}

void AdViewGuest::DispatchEvent(const std::string& event_name,
                                 scoped_ptr<DictionaryValue> event) {
  Profile* profile = Profile::FromBrowserContext(
      web_contents()->GetBrowserContext());

  extensions::EventFilteringInfo info;
  info.SetURL(GURL());
  info.SetInstanceID(guest_instance_id_);
  scoped_ptr<ListValue> args(new ListValue());
  args->Append(event.release());

  extensions::EventRouter::DispatchEvent(
      embedder_web_contents_, profile, extension_id_,
      event_name, args.Pass(),
      extensions::EventRouter::USER_GESTURE_UNKNOWN, info);
}

void AdViewGuest::DidCommitProvisionalLoadForFrame(
    int64 frame_id,
    bool is_main_frame,
    const GURL& url,
    content::PageTransition transition_type,
    content::RenderViewHost* render_view_host) {
  scoped_ptr<DictionaryValue> event(new DictionaryValue());
  event->SetString(adview::kUrl, url.spec());
  event->SetBoolean(adview::kIsTopLevel, is_main_frame);
  DispatchEvent(adview::kEventLoadCommit, event.Pass());
}

void AdViewGuest::WebContentsDestroyed(WebContents* web_contents) {
  delete this;
}
