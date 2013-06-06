// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_SUGGESTION_CONTAINER_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_SUGGESTION_CONTAINER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_layout.h"

namespace autofill {
  class AutofillDialogController;
}

@class AutofillTextField;

// Delegate to notify when the user activates the edit link.
@protocol AutofillSuggestionEditDelegate

// Called when user clicks |edit_link_|.
- (void)editLinkClicked;

// Returns the text to display for edit_link_.
- (NSString*)editLinkTitle;

@end

// Container for the data suggested for a particular input section.
@interface AutofillSuggestionContainer : NSViewController<AutofillLayout> {
 @private
  // The label that holds the suggestion description text.
  scoped_nsobject<NSTextField> label_;

  // The second (and longer) line of text that describes the suggestion.
  scoped_nsobject<NSTextField> label2_;

  // The icon that comes just before |label_|.
  scoped_nsobject<NSImageView> iconImageView_;

  // The input set by ShowTextfield.
  scoped_nsobject<AutofillTextField> inputField_;

  // An "Edit" link that flips to editable inputs rather than suggestion text.
  scoped_nsobject<NSButton> editLink_;

  id<AutofillSuggestionEditDelegate> delegate_;  // weak.
  autofill::AutofillDialogController* controller_;  // Not owned.
}

// Designated intializer.
- (id)initWithDelegate:(id<AutofillSuggestionEditDelegate>)delegate;

// Marks the suggestion as editable or not, controls |edit_link_|.
- (void)setEditable:(BOOL)editable;

// Set the icon for the suggestion.
- (void)setIcon:(NSImage*)iconImage;

// Set the main suggestion text and the font used to render that text.
- (void)setSuggestionText:(NSString*)line1
                    line2:(NSString*)line2
                 withFont:(NSFont*)font;

// Turns editable textfield on, setting the field's placeholder text and icon.
- (void)showTextfield:(NSString*)text withIcon:(NSImage*)icon;

@end

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_SUGGESTION_CONTAINER_H_
