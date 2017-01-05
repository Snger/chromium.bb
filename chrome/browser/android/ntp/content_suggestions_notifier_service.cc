// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/ntp/content_suggestions_notifier_service.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "chrome/browser/android/ntp/content_suggestions_notification_helper.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_handler.h"
#include "chrome/browser/profiles/profile.h"
#include "components/ntp_snippets/content_suggestions_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

using ntp_snippets::Category;
using ntp_snippets::CategoryStatus;
using ntp_snippets::ContentSuggestion;
using ntp_snippets::ContentSuggestionsNotificationHelper;
using ntp_snippets::ContentSuggestionsService;
using ntp_snippets::KnownCategories;

namespace {

const char kNotificationIDWithinCategory[] =
    "ContentSuggestionsNotificationIDWithinCategory";

gfx::Image CropSquare(const gfx::Image& image) {
  if (image.IsEmpty()) {
    return image;
  }
  const gfx::ImageSkia* skimage = image.ToImageSkia();
  gfx::Rect bounds{{0, 0}, skimage->size()};
  int size = std::min(bounds.width(), bounds.height());
  bounds.ClampToCenteredSize({size, size});
  return gfx::Image(gfx::ImageSkiaOperations::CreateTiledImage(
      *skimage, bounds.x(), bounds.y(), bounds.width(), bounds.height()));
}

}  // namespace

class ContentSuggestionsNotifierService::NotifyingObserver
    : public ContentSuggestionsService::Observer {
 public:
  NotifyingObserver(ContentSuggestionsService* service,
                    Profile* profile,
                    PrefService* prefs)
      : service_(service), prefs_(prefs), weak_ptr_factory_(this) {}

  void OnNewSuggestions(Category category) override {
    if (!category.IsKnownCategory(KnownCategories::ARTICLES)) {
      return;
    }
    const auto& suggestions = service_->GetSuggestionsForCategory(category);
    if (!suggestions.empty()) {
      const auto& suggestion = suggestions[0];
      service_->FetchSuggestionImage(
          suggestions[0].id(),
          base::Bind(&NotifyingObserver::ImageFetched,
                     weak_ptr_factory_.GetWeakPtr(), suggestion.id(),
                     suggestion.url(), suggestion.title(),
                     suggestion.publisher_name()));
    }
  }

  void OnCategoryStatusChanged(Category category,
                               CategoryStatus new_status) override {
    if (!category.IsKnownCategory(KnownCategories::ARTICLES)) {
      return;
    }
    switch (new_status) {
      case CategoryStatus::AVAILABLE:
      case CategoryStatus::AVAILABLE_LOADING:
        break;  // nothing to do
      case CategoryStatus::INITIALIZING:
      case CategoryStatus::ALL_SUGGESTIONS_EXPLICITLY_DISABLED:
      case CategoryStatus::CATEGORY_EXPLICITLY_DISABLED:
      case CategoryStatus::LOADING_ERROR:
      case CategoryStatus::NOT_PROVIDED:
      case CategoryStatus::SIGNED_OUT:
        ContentSuggestionsNotificationHelper::HideNotification();
        break;
    }
  }

  void OnSuggestionInvalidated(
      const ContentSuggestion::ID& suggestion_id) override {
    if (suggestion_id.category().IsKnownCategory(KnownCategories::ARTICLES) &&
        (suggestion_id.id_within_category() ==
         prefs_->GetString(kNotificationIDWithinCategory))) {
      ContentSuggestionsNotificationHelper::HideNotification();
    }
  }

  void OnFullRefreshRequired() override {
    ContentSuggestionsNotificationHelper::HideNotification();
  }

  void ContentSuggestionsServiceShutdown() override {
    ContentSuggestionsNotificationHelper::HideNotification();
  }

 private:
  void ImageFetched(const ContentSuggestion::ID& id,
                    const GURL& url,
                    const base::string16& title,
                    const base::string16& publisher,
                    const gfx::Image& image) {
    // check if suggestion is still valid.
    DVLOG(1) << "Fetched " << image.Size().width() << "x"
             << image.Size().height() << " image for " << url.spec();
    prefs_->SetString(kNotificationIDWithinCategory, id.id_within_category());
    ContentSuggestionsNotificationHelper::SendNotification(
        url, title, publisher, CropSquare(image));
  }

  ContentSuggestionsService* const service_;
  PrefService* const prefs_;

  base::WeakPtrFactory<NotifyingObserver> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NotifyingObserver);
};

ContentSuggestionsNotifierService::ContentSuggestionsNotifierService(
    Profile* profile,
    ContentSuggestionsService* suggestions,
    PrefService* prefs)
    : observer_(base::MakeUnique<NotifyingObserver>(suggestions,
                                                    profile,
                                                    profile->GetPrefs())) {
  suggestions->AddObserver(observer_.get());
}

ContentSuggestionsNotifierService::~ContentSuggestionsNotifierService() =
    default;

void ContentSuggestionsNotifierService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterStringPref(kNotificationIDWithinCategory, std::string());
}
