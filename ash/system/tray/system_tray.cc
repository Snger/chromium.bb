// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray.h"

#include "ash/shell.h"
#include "ash/shell/panel_window.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/user/login_status.h"
#include "ash/wm/shadow_types.h"
#include "base/logging.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace internal {

const int kArrowHeight = 10;
const int kArrowWidth = 20;
const int kArrowPaddingFromRight = 20;

const int kShadowOffset = 3;
const int kShadowHeight = 3;

const SkColor kDarkColor = SkColorSetRGB(120, 120, 120);
const SkColor kLightColor = SkColorSetRGB(240, 240, 240);
const SkColor kBackgroundColor = SK_ColorWHITE;
const SkColor kShadowColor = SkColorSetARGB(25, 0, 0, 0);

class SystemTrayBubbleBackground : public views::Background {
 public:
  explicit SystemTrayBubbleBackground(views::View* owner)
      : owner_(owner) {
  }

  virtual ~SystemTrayBubbleBackground() {}

 private:
  // Overridden from views::Background.
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    views::View* last_view = NULL;
    for (int i = 0; i < owner_->child_count(); i++) {
      views::View* v = owner_->child_at(i);

      if (!v->background()) {
        canvas->FillRect(v->bounds(), kBackgroundColor);
      } else if (last_view) {
        canvas->FillRect(gfx::Rect(v->x() + kShadowOffset, v->y(),
                                   v->width() - kShadowOffset, kShadowHeight),
                         kShadowColor);
      }

      if (!v->border()) {
        canvas->DrawLine(gfx::Point(v->x() - 1, v->y() - 1),
            gfx::Point(v->x() + v->width() + 1, v->y() - 1),
            !last_view || last_view->border() ? kDarkColor : kLightColor);
        canvas->DrawLine(gfx::Point(v->x() - 1, v->y() - 1),
            gfx::Point(v->x() - 1, v->y() + v->height() + 1),
            kDarkColor);
        canvas->DrawLine(gfx::Point(v->x() + v->width(), v->y() - 1),
            gfx::Point(v->x() + v->width(), v->y() + v->height() + 1),
            kDarkColor);
      } else if (last_view && !last_view->border()) {
        canvas->DrawLine(gfx::Point(v->x() - 1, v->y() - 1),
            gfx::Point(v->x() + v->width() + 1, v->y() - 1),
            kDarkColor);
      }

      last_view = v;
    }
  }

  views::View* owner_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubbleBackground);
};

class SystemTrayBubbleBorder : public views::Border {
 public:
  explicit SystemTrayBubbleBorder(views::View* owner)
      : owner_(owner) {
  }

  virtual ~SystemTrayBubbleBorder() {}

 private:
  // Overridden from views::Border.
  virtual void Paint(const views::View& view,
                     gfx::Canvas* canvas) const OVERRIDE {
    // Draw a line first.
    int x = 4;
    int y = owner_->height() + 1;
    canvas->DrawLine(gfx::Point(x, y),
                     gfx::Point(owner_->width() + x, y),
                     kDarkColor);

    // Now, draw a shadow.
    canvas->FillRect(gfx::Rect(x + kShadowOffset, y,
                               owner_->width() - kShadowOffset, kShadowHeight),
                     kShadowColor);

    // Draw the arrow.
    int left_base_x = owner_->width() - kArrowPaddingFromRight - kArrowWidth;
    int left_base_y = y;
    int tip_x = left_base_x + kArrowWidth / 2;
    int tip_y = left_base_y + kArrowHeight;
    SkPath path;
    path.incReserve(4);
    path.moveTo(SkIntToScalar(left_base_x), SkIntToScalar(left_base_y));
    path.lineTo(SkIntToScalar(tip_x), SkIntToScalar(tip_y));
    path.lineTo(SkIntToScalar(left_base_x + kArrowWidth),
                SkIntToScalar(left_base_y));

    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(kBackgroundColor);
    canvas->GetSkCanvas()->drawPath(path, paint);

    // Now the draw the outline.
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(kDarkColor);
    canvas->GetSkCanvas()->drawPath(path, paint);
  }

  virtual void GetInsets(gfx::Insets* insets) const OVERRIDE {
    insets->Set(0, 0, kArrowHeight, 0);
  }

  views::View* owner_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubbleBorder);
};

class SystemTrayBubble : public views::BubbleDelegateView {
 public:
  SystemTrayBubble(ash::SystemTray* tray,
                   std::vector<ash::SystemTrayItem*>& items,
                   bool detailed)
      : views::BubbleDelegateView(tray, views::BubbleBorder::BOTTOM_RIGHT),
        tray_(tray),
        items_(items),
        detailed_(detailed),
        autoclose_delay_(0) {
    set_margin(0);
    set_parent_window(ash::Shell::GetInstance()->GetContainer(
        ash::internal::kShellWindowId_SettingBubbleContainer));
    set_notify_enter_exit_on_child(true);
  }

  virtual ~SystemTrayBubble() {
    for (std::vector<ash::SystemTrayItem*>::iterator it = items_.begin();
        it != items_.end();
        ++it) {
      if (detailed_)
        (*it)->DestroyDetailedView();
      else
        (*it)->DestroyDefaultView();
    }
  }

  void StartAutoCloseTimer(int seconds) {
    autoclose_.Stop();
    autoclose_delay_ = seconds;
    if (autoclose_delay_) {
      autoclose_.Start(FROM_HERE,
          base::TimeDelta::FromSeconds(autoclose_delay_),
          this, &SystemTrayBubble::AutoClose);
    }
  }

 private:
  void AutoClose() {
    StartFade(false);
  }

  // Overridden from views::BubbleDelegateView.
  virtual void Init() OVERRIDE {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
          1, 1, 1));
    set_background(new SystemTrayBubbleBackground(this));

    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    ash::user::LoginStatus login_status = delegate->GetUserLoginStatus();
    for (std::vector<ash::SystemTrayItem*>::iterator it = items_.begin();
        it != items_.end();
        ++it) {
      views::View* view = detailed_ ? (*it)->CreateDetailedView(login_status) :
                                      (*it)->CreateDefaultView(login_status);
      if (view)
        AddChildView(view);
    }
  }

  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE {
    autoclose_.Stop();
  }

  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE {
    if (autoclose_delay_) {
      autoclose_.Stop();
      autoclose_.Start(FROM_HERE,
          base::TimeDelta::FromSeconds(autoclose_delay_),
          this, &SystemTrayBubble::AutoClose);
    }
  }

  ash::SystemTray* tray_;
  std::vector<ash::SystemTrayItem*> items_;
  bool detailed_;

  int autoclose_delay_;
  base::OneShotTimer<SystemTrayBubble> autoclose_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubble);
};

}  // namespace internal

SystemTray::SystemTray()
    : items_(),
      bubble_(NULL),
      popup_(NULL) {
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
      5, 0, 3));
  set_background(views::Background::CreateSolidBackground(
      SkColorSetARGB(127, 0, 0, 0)));
}

SystemTray::~SystemTray() {
  if (popup_)
    popup_->CloseNow();
  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
      it != items_.end();
      ++it) {
    (*it)->DestroyTrayView();
  }
}

void SystemTray::AddTrayItem(SystemTrayItem* item) {
  items_.push_back(item);

  SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
  views::View* tray_item = item->CreateTrayView(delegate->GetUserLoginStatus());
  if (tray_item) {
    AddChildViewAt(tray_item, 0);
    PreferredSizeChanged();
  }
}

void SystemTray::RemoveTrayItem(SystemTrayItem* item) {
  NOTIMPLEMENTED();
}

void SystemTray::ShowDetailedView(SystemTrayItem* item, int close_delay) {
  if (popup_)
    popup_->Close();
  popup_ = NULL;
  bubble_ = NULL;

  std::vector<SystemTrayItem*> items;
  items.push_back(item);
  ShowItems(items, true);
  bubble_->StartAutoCloseTimer(close_delay);
}

void SystemTray::UpdateAfterLoginStatusChange(user::LoginStatus login_status) {
  if (popup_)
    popup_->CloseNow();

  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
      it != items_.end();
      ++it) {
    (*it)->DestroyTrayView();
  }
  RemoveAllChildViews(true);

  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
      it != items_.end();
      ++it) {
    views::View* view = (*it)->CreateTrayView(login_status);
    if (view)
      AddChildViewAt(view, 0);
  }
  PreferredSizeChanged();
}

void SystemTray::ShowItems(std::vector<SystemTrayItem*>& items, bool detailed) {
  CHECK(!popup_);
  CHECK(!bubble_);
  bubble_ = new internal::SystemTrayBubble(this, items, detailed);
  popup_ = views::BubbleDelegateView::CreateBubble(bubble_);
  bubble_->SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  popup_->non_client_view()->frame_view()->set_background(NULL);
  popup_->non_client_view()->frame_view()->set_border(
      new internal::SystemTrayBubbleBorder(bubble_));
  popup_->AddObserver(this);
  bubble_->Show();
}

bool SystemTray::OnMousePressed(const views::MouseEvent& event) {
  if (popup_)
    popup_->Show();
  else
    ShowItems(items_, false);
  return true;
}

void SystemTray::OnWidgetClosing(views::Widget* widget) {
  CHECK_EQ(popup_, widget);
  popup_ = NULL;
  bubble_ = NULL;
}

}  // namespace ash
