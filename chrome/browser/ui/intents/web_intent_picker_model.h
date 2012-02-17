// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_MODEL_H_
#define CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_MODEL_H_
#pragma once

#include <vector>

#include "base/string16.h"
#include "googleurl/src/gurl.h"
#include "ui/gfx/image/image.h"

class WebIntentPickerModelObserver;

namespace gfx {
class Image;
}

// Model for the WebIntentPicker.
class WebIntentPickerModel {
 public:
  // The intent service disposition.
  enum Disposition {
    DISPOSITION_WINDOW,  // Display the intent service in a new window.
    DISPOSITION_INLINE,  // Display the intent service in the picker.
  };

  // An intent service to display in the picker.
  struct Item {
    Item(const string16& title, const GURL& url, Disposition disposition);
    ~Item();

    // The title of this service.
    string16 title;

    // The URL of this service.
    GURL url;

    // A favicon of this service.
    gfx::Image favicon;

    // The disposition to use when displaying this service.
    Disposition disposition;
  };

  // A suggested extension to display in the picker.
  struct SuggestedExtension {
    SuggestedExtension(const string16& title,
                       const string16& id,
                       double average_rating);
    ~SuggestedExtension();

    // The title of the intent service.
    string16 title;

    // The id of the extension that provides the intent service.
    string16 id;

    // The average rating of the extension.
    double average_rating;

    // The extension's icon.
    gfx::Image icon;
  };

  WebIntentPickerModel();
  ~WebIntentPickerModel();

  void set_observer(WebIntentPickerModelObserver* observer) {
    observer_ = observer;
  }

  // Add a new item with |title|, |url| and |disposition| to the picker.
  void AddItem(const string16& title, const GURL& url, Disposition disposition);

  // Remove an item from the picker at |index|.
  void RemoveItemAt(size_t index);

  // Remove all items from the picker, and resets to not displaying inline
  // disposition.  Note that this does not clear the observer.
  void Clear();

  // Return the intent service item at |index|.
  const Item& GetItemAt(size_t index) const;

  // Return the number of intent services in the picker.
  size_t GetItemCount() const;

  // Update the favicon for the intent service at |index| to |image|.
  void UpdateFaviconAt(size_t index, const gfx::Image& image);

  // Add a new suggested extension with |id|, |title| and |average_rating| to
  // the picker.
  void AddSuggestedExtension(const string16& id,
                             const string16& title,
                             double average_rating);

  // Remove a suggested extension from the picker at |index|.
  void RemoveSuggestedExtensionAt(size_t index);

  // Return the suggested extension at |index|.
  const SuggestedExtension& GetSuggestedExtensionAt(size_t index) const;

  // Return the number of suggested extensions.
  size_t GetSuggestedExtensionCount() const;

  // Set the picker to display the intent service at |index| inline.
  void SetInlineDisposition(size_t index);

  // Returns true if the picker is currently displaying an inline service.
  bool IsInlineDisposition() const;

  // Returns the index of the intent service that is being displayed inline, or
  // std::string::npos if none.
  size_t inline_disposition_index() const { return inline_disposition_index_; }

 private:
  // Delete all elements in |items_| and |suggested_extensions_|.
  // Note that this method does not reset the observer.
  void DestroyAll();

  // A vector of all items in the picker. Each item is owned by this model.
  std::vector<Item*> items_;

  // A vector of all suggested extensions in the picker. Each element is owned
  // by this model.
  std::vector<SuggestedExtension*> suggested_extensions_;

  // The observer to send notifications to, or NULL if none.
  WebIntentPickerModelObserver* observer_;

  // The index of the intent service that is being displayed inline, or
  // std::string::npos if none.
  size_t inline_disposition_index_;

  DISALLOW_COPY_AND_ASSIGN(WebIntentPickerModel);
};

#endif  // CHROME_BROWSER_UI_INTENTS_WEB_INTENT_PICKER_MODEL_H_
