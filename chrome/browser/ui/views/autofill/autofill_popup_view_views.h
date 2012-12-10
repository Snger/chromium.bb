// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_VIEWS_H_

#include "chrome/browser/autofill/autofill_popup_view.h"
#include "content/public/browser/keyboard_listener.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget_observer.h"

class AutofillExternalDelegateViews;

namespace content {
class WebContents;
}

// The View Autofill popup implementation.
class AutofillPopupViewViews : public views::WidgetDelegateView,
                               public AutofillPopupView,
                               public views::WidgetObserver,
                               public KeyboardListener {
 public:
  AutofillPopupViewViews(content::WebContents* web_contents,
                         AutofillExternalDelegateViews* external_delegate,
                         const gfx::Rect& element_bounds);

  // AutofillPopupView implementation.
  virtual void Hide() OVERRIDE;

 private:
  class AutofillPopupWidget;

  virtual ~AutofillPopupViewViews();

  // views:Views implementation.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // views::WidgetObserver implementation.
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseMoved(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnWidgetBoundsChanged(views::Widget* widget,
                                     const gfx::Rect& new_bounds) OVERRIDE;

  // KeyboardListener implementation.
  virtual bool HandleKeyPressEvent(ui::KeyEvent* event) OVERRIDE;

  // AutofillPopupView implementation.
  virtual void ShowInternal() OVERRIDE;
  virtual void InvalidateRow(size_t row) OVERRIDE;
  virtual void UpdateBoundsAndRedrawPopupInternal() OVERRIDE;

  // Draw the given autofill entry in |entry_rect|.
  void DrawAutofillEntry(gfx::Canvas* canvas,
                         int index,
                         const gfx::Rect& entry_rect);

  // Set the initial bounds of the popup to show, including the placement
  // of it.
  void SetInitialBounds();

  // Get the size of the screen that the popup appears of, in pixels.
  gfx::Size GetScreenSize();

  AutofillExternalDelegateViews* external_delegate_;  // Weak reference.
  content::WebContents* web_contents_;  // Weak reference.

  DISALLOW_COPY_AND_ASSIGN(AutofillPopupViewViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_POPUP_VIEW_VIEWS_H_
