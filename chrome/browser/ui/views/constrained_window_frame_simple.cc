// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/constrained_window_frame_simple.h"

#include "chrome/browser/ui/constrained_window_constants.h"
#include "chrome/browser/ui/constrained_window.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "grit/shared_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget.h"

ConstrainedWindowFrameSimple::ConstrainedWindowFrameSimple(
    ConstrainedWindowViews* container,
    ConstrainedWindowViews::ChromeStyleClientInsets client_insets)
    : container_(container),
      title_label_(
          new views::Label(container->widget_delegate()->GetWindowTitle())),
      ALLOW_THIS_IN_INITIALIZER_LIST(close_button_(
          new views::ImageButton(this))) {
  container_->set_frame_type(views::Widget::FRAME_TYPE_FORCE_CUSTOM);

  views::GridLayout* layout = new views::GridLayout(this);
  const int kHeaderTopPadding = std::min(
      ConstrainedWindowConstants::kCloseButtonPadding,
      ConstrainedWindowConstants::kTitleTopPadding);
  layout->SetInsets(kHeaderTopPadding,
                    ConstrainedWindowConstants::kHorizontalPadding,
                    0,
                    ConstrainedWindowConstants::kCloseButtonPadding);
  SetLayoutManager(layout);
  views::ColumnSet* cs = layout->AddColumnSet(0);
  cs->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING, 1,
                views::GridLayout::USE_PREF, 0, 0);  // Title.
  cs->AddPaddingColumn(0, ConstrainedWindowConstants::kCloseButtonPadding);
  cs->AddColumn(views::GridLayout::TRAILING, views::GridLayout::LEADING, 0,
                views::GridLayout::USE_PREF, 0, 0);  // Close Button.

  layout->StartRow(0, 0);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  title_label_->SetFont(rb.GetFont(
      ConstrainedWindowConstants::kTitleFontStyle));
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label_->SetEnabledColor(ConstrainedWindow::GetTextColor());
  title_label_->set_border(views::Border::CreateEmptyBorder(
      ConstrainedWindowConstants::kTitleTopPadding - kHeaderTopPadding,
      0, 0, 0));
  layout->AddView(title_label_);

  close_button_->SetImage(views::CustomButton::BS_NORMAL,
                          rb.GetImageSkiaNamed(IDR_SHARED_IMAGES_X));
  close_button_->SetImage(views::CustomButton::BS_HOT,
                          rb.GetImageSkiaNamed(IDR_SHARED_IMAGES_X_HOVER));
  close_button_->SetImage(views::CustomButton::BS_PUSHED,
                          rb.GetImageSkiaNamed(IDR_SHARED_IMAGES_X_PRESSED));
  close_button_->set_border(views::Border::CreateEmptyBorder(
      ConstrainedWindowConstants::kCloseButtonPadding - kHeaderTopPadding,
      0, 0, 0));
  layout->AddView(close_button_);

  set_background(views::Background::CreateSolidBackground(
      ConstrainedWindow::GetBackgroundColor()));

  // Client insets have no relation to header insets:
  // - The client insets are the distance from the window border to the client
  //   view.
  // - The header insets are the distance from the window border to the header
  //   elements.
  //
  // The NO_ISNETS consumers draw atop the views above.
  if (client_insets == ConstrainedWindowViews::DEFAULT_INSETS) {
    const int kTitleBuiltinBottomPadding = 4;
    set_border(views::Border::CreateEmptyBorder(
        ConstrainedWindowConstants::kClientTopPadding + kHeaderTopPadding +
            std::max(close_button_->GetPreferredSize().height(),
                     title_label_->GetPreferredSize().height()) -
            kTitleBuiltinBottomPadding,
        ConstrainedWindowConstants::kHorizontalPadding,
        ConstrainedWindowConstants::kClientBottomPadding,
        ConstrainedWindowConstants::kHorizontalPadding));
  }
}

ConstrainedWindowFrameSimple::~ConstrainedWindowFrameSimple() {
}

gfx::Rect ConstrainedWindowFrameSimple::GetBoundsForClientView() const {
  return GetContentsBounds();
}

gfx::Rect ConstrainedWindowFrameSimple::GetWindowBoundsForClientBounds(
    const gfx::Rect& client_bounds) const {
  gfx::Rect bounds(client_bounds);
  bounds.Inset(-GetInsets());
  bounds.set_width(std::max(
       bounds.width(),
       ConstrainedWindowConstants::kHorizontalPadding +
           2 * ConstrainedWindowConstants::kCloseButtonPadding +
           title_label_->GetPreferredSize().width() +
           close_button_->GetPreferredSize().width()));
  return bounds;
}

int ConstrainedWindowFrameSimple::NonClientHitTest(const gfx::Point& point) {
  if (!bounds().Contains(point))
    return HTNOWHERE;
  return HTCLIENT;
}

void ConstrainedWindowFrameSimple::GetWindowMask(const gfx::Size& size,
                                                 gfx::Path* window_mask) {
#if defined(USE_AURA)
  SkRect rect = {0, 0, static_cast<SkScalar>(size.width() - 1),
                 static_cast<SkScalar>(size.height() - 1)};
#else
  // There appears to be a bug in the window mask calculation on Windows
  // which causes the width, but not the height, to be off by one.
  SkRect rect = {0, 0, size.width(), size.height() - 1};
#endif
  SkScalar radius = SkIntToScalar(ConstrainedWindowConstants::kBorderRadius);
  SkScalar radii[8] = {radius, radius, radius, radius,
                       radius, radius, radius, radius};

  // NB: We're not using the addRoundRect uniform radius overload as it
  // mishandles the bottom corners on Windows
  window_mask->addRoundRect(rect, radii);
}

void ConstrainedWindowFrameSimple::ResetWindowControls() {
}

void ConstrainedWindowFrameSimple::UpdateWindowIcon() {
}

void ConstrainedWindowFrameSimple::UpdateWindowTitle() {
  title_label_->SetText(container_->widget_delegate()->GetWindowTitle());
}

gfx::Size ConstrainedWindowFrameSimple::GetPreferredSize() {
  return GetWindowBoundsForClientBounds(
      gfx::Rect(container_->client_view()->GetPreferredSize())).size();
}

void ConstrainedWindowFrameSimple::ButtonPressed(views::Button* sender,
                                                 const ui::Event& event) {
  if (sender == close_button_)
    sender->GetWidget()->Close();
}
