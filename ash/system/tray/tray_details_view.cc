// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_details_view.h"

#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "ui/views/background.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"

namespace ash {
namespace internal {

TrayDetailsView::TrayDetailsView()
    : footer_(NULL),
      scroller_(NULL),
      scroll_content_(NULL) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
      0, 0, 0));
  set_background(views::Background::CreateSolidBackground(kBackgroundColor));
}

TrayDetailsView::~TrayDetailsView() {
}

void TrayDetailsView::CreateSpecialRow(int string_id,
                                       ViewClickListener* listener) {
  DCHECK(!footer_);
  footer_ = new SpecialPopupRow();
  footer_->SetTextLabel(string_id, listener);
  AddChildViewAt(footer_, child_count());
}

void TrayDetailsView::CreateScrollableList() {
  DCHECK(!scroller_);
  scroll_content_ = new views::View;
  scroll_content_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kVertical, 0, 0, 1));
  scroller_ = new FixedSizedScrollView;
  scroller_->SetContentsView(scroll_content_);
  AddChildView(scroller_);
}

void TrayDetailsView::Reset() {
  RemoveAllChildViews(true);
  footer_ = NULL;
  scroller_ = NULL;
  scroll_content_ = NULL;
}

void TrayDetailsView::Layout() {
  if (!scroller_ || !footer_ || bounds().IsEmpty()) {
    views::View::Layout();
    return;
  }

  scroller_->SetFixedSize(gfx::Size());
  gfx::Size size = GetPreferredSize();
  if (size.height() > height()) {
    // The available size is smaller than the requested size. Squeeze the
    // scroller so that everything fits in the available size.
    gfx::Size scroller_size = scroll_content_->GetPreferredSize();
    scroller_->SetFixedSize(gfx::Size(
        width() + scroller_->GetScrollBarWidth(),
        scroller_size.height() - (size.height() - height())));
  }
  views::View::Layout();
  // Always make sure the footer element is bottom aligned.
  gfx::Rect fbounds = footer_->bounds();
  fbounds.set_y(height() - footer_->height());
  footer_->SetBoundsRect(fbounds);
}

}  // namespace internal
}  // namespace ash
