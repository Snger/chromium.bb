// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray_user.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/size.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace {

// A custom textbutton with some extra vertical padding, and custom border,
// alignment and hover-effects.
class TrayButton : public views::TextButton {
 public:
  TrayButton(views::ButtonListener* listener, std::string text)
      : views::TextButton(listener, ASCIIToUTF16(text)),
        hover_(false),
        hover_bg_(views::Background::CreateSolidBackground(SkColorSetARGB(
               10, 0, 0, 0))) {
    set_alignment(ALIGN_CENTER);
  }

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size size = views::TextButton::GetPreferredSize();
    size.Enlarge(0, 16);
    return size;
  }

  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE {
    hover_ = true;
    SchedulePaint();
  }

  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE {
    hover_ = false;
    SchedulePaint();
  }

  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE {
    if (hover_)
      hover_bg_->Paint(canvas, this);
    else
      views::TextButton::OnPaintBackground(canvas);
  }

  bool hover_;
  views::Background* hover_bg_;

  DISALLOW_COPY_AND_ASSIGN(TrayButton);
};

class UserView : public views::View,
                 public views::ButtonListener {
 public:
  UserView() {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
          0, 0, 3));

    views::View* user = new views::View;
    user->SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
          14, 5, 0));
    ash::SystemTrayDelegate* tray = ash::Shell::GetInstance()->tray_delegate();
    username_ = new views::Label(ASCIIToUTF16(tray->GetUserDisplayName()));
    username_->SetFont(username_->font().DeriveFont(2));
    username_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    user->AddChildView(username_);

    email_ = new views::Label(ASCIIToUTF16(tray->GetUserEmail()));
    email_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    email_->SetEnabled(false);
    user->AddChildView(email_);

    AddChildView(user);

    views::View* button_container = new views::View;
    views::BoxLayout *layout = new
        views::BoxLayout(views::BoxLayout::kHorizontal, 0, 5, 0);
    layout->set_spread_blank_space(true);
    button_container->SetLayoutManager(layout);

    shutdown_ = new TrayButton(this, std::string("Shut down"));
    signout_ = new TrayButton(this, std::string("Sign out"));
    lock_ = new TrayButton(this, std::string("Lock"));
    button_container->AddChildView(shutdown_);
    button_container->AddChildView(signout_);
    button_container->AddChildView(lock_);

    shutdown_->set_border(NULL);
    signout_->set_border(views::Border::CreateSolidSidedBorder(
          0, 1, 0, 1, SkColorSetARGB(25, 0, 0, 0)));
    lock_->set_border(NULL);

    AddChildView(button_container);
  }

 private:
  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    ash::SystemTrayDelegate* tray = ash::Shell::GetInstance()->tray_delegate();
    if (sender == shutdown_)
      tray->ShutDown();
    else if (sender == signout_)
      tray->SignOut();
    else if (sender == lock_)
      tray->LockScreen();
  }

  views::Label* username_;
  views::Label* email_;

  TrayButton* shutdown_;
  TrayButton* signout_;
  TrayButton* lock_;

  DISALLOW_COPY_AND_ASSIGN(UserView);
};

}  // namespace

namespace ash {
namespace internal {

TrayUser::TrayUser() {
}

TrayUser::~TrayUser() {
}

views::View* TrayUser::CreateTrayView() {
  views::ImageView* avatar = new views::ImageView;
  avatar->SetImage(ash::Shell::GetInstance()->tray_delegate()->GetUserImage());
  avatar->SetImageSize(gfx::Size(32, 32));
  return avatar;
}

views::View* TrayUser::CreateDefaultView() {
  return new UserView;
}

views::View* TrayUser::CreateDetailedView() {
  return NULL;
}

void TrayUser::DestroyTrayView() {
}

void TrayUser::DestroyDefaultView() {
}

void TrayUser::DestroyDetailedView() {
}

}  // namespace internal
}  // namespace ash
