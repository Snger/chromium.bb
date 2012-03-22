// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/user/tray_user.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/size.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace {

const int kPaddingAroundButtons = 5;

const int kUserInfoHorizontalPadding = 14;
const int kUserInfoVerticalPadding = 10;
const int kUserInfoPaddingBetweenItems = 3;

const int kUserIconSize = 27;
const int kUserIconCornerRadius = 2;

const SkColor kButtonStrokeColor = SkColorSetRGB(0xdd, 0xdd, 0xdd);

// A custom textbutton with some extra vertical padding, and custom border,
// alignment and hover-effects.
class TrayButton : public views::TextButton {
 public:
  TrayButton(views::ButtonListener* listener, const string16& text)
      : views::TextButton(listener, text),
        hover_(false),
        hover_bg_(views::Background::CreateSolidBackground(SkColorSetARGB(
               10, 0, 0, 0))),
        hover_border_(views::Border::CreateSolidBorder(1, kButtonStrokeColor)) {
    set_alignment(ALIGN_CENTER);
    set_border(NULL);
  }

  virtual ~TrayButton() {}

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

  virtual void OnPaintBorder(gfx::Canvas* canvas) OVERRIDE {
    if (hover_)
      hover_border_->Paint(*this, canvas);
    else
      views::TextButton::OnPaintBorder(canvas);
  }

  bool hover_;
  scoped_ptr<views::Background> hover_bg_;
  scoped_ptr<views::Border> hover_border_;

  DISALLOW_COPY_AND_ASSIGN(TrayButton);
};

}  // namespace

namespace ash {
namespace internal {

namespace tray {

class UserView : public views::View,
                 public views::ButtonListener {
 public:
  explicit UserView(ash::user::LoginStatus status)
      : username_(NULL),
        email_(NULL),
        update_(NULL),
        shutdown_(NULL),
        signout_(NULL),
        lock_(NULL) {
    CHECK(status != ash::user::LOGGED_IN_NONE);
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
          0, 0, 0));
    set_background(views::Background::CreateSolidBackground(SK_ColorWHITE));

    bool guest = status == ash::user::LOGGED_IN_GUEST;
    bool kiosk = status == ash::user::LOGGED_IN_KIOSK;
    if (!guest && !kiosk)
      AddUserInfo();

    views::View* button_container = new views::View;
    views::BoxLayout *layout = new
        views::BoxLayout(views::BoxLayout::kHorizontal,
            kPaddingAroundButtons,
            kPaddingAroundButtons,
            -1);
    layout->set_spread_blank_space(true);
    button_container->SetLayoutManager(layout);

    ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();

    if (!kiosk) {
      shutdown_ = new TrayButton(this, bundle.GetLocalizedString(
            IDS_ASH_STATUS_TRAY_SHUT_DOWN));
      button_container->AddChildView(shutdown_);
    } else {
      views::Label* label = new views::Label;
      label->SetText(
          bundle.GetLocalizedString(IDS_ASH_STATUS_TRAY_KIOSK_LABEL));
      label->set_border(views::Border::CreateEmptyBorder(
            0, kTrayPopupPaddingHorizontal, 0, 1));
      label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
      button_container->AddChildView(label);
    }

    signout_ = new TrayButton(this, bundle.GetLocalizedString(
        guest ? IDS_ASH_STATUS_TRAY_EXIT_GUEST :
        kiosk ? IDS_ASH_STATUS_TRAY_EXIT_KIOSK :
                IDS_ASH_STATUS_TRAY_SIGN_OUT));
    signout_->set_border(views::Border::CreateSolidSidedBorder(
          kiosk, 1, kiosk, kiosk || !guest, kButtonStrokeColor));
    button_container->AddChildView(signout_);

    if (!guest && !kiosk) {
      lock_ = new TrayButton(this, bundle.GetLocalizedString(
            IDS_ASH_STATUS_TRAY_LOCK));
      button_container->AddChildView(lock_);
    }

    AddChildView(button_container);
  }

  virtual ~UserView() {}

  // Shows update notification if available.
  void RefreshForUpdate() {
    ash::SystemTrayDelegate* tray = ash::Shell::GetInstance()->tray_delegate();
    if (tray->SystemShouldUpgrade()) {
      if (update_)
        return;
      update_ = new views::View;
      update_->SetLayoutManager(new
          views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 3));

      ui::ResourceBundle& bundle = ui::ResourceBundle::GetSharedInstance();
      views::Label *label = new views::Label(bundle.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_UPDATE));
      label->SetFont(label->font().DeriveFont(-1));
      update_->AddChildView(label);

      views::ImageView* icon = new views::ImageView;
      icon->SetImage(bundle.GetImageNamed(tray->GetSystemUpdateIconResource()).
          ToSkBitmap());
      update_->AddChildView(icon);

      update_->set_border(views::Border::CreateEmptyBorder(
          kUserInfoVerticalPadding,
          kUserInfoHorizontalPadding,
          kUserInfoVerticalPadding,
          kUserInfoHorizontalPadding));

      user_info_->AddChildView(update_);
    } else if (update_) {
      delete update_;
      update_ = NULL;
    }
    user_info_->InvalidateLayout();
    user_info_->SchedulePaint();
  }

 private:
  void AddUserInfo() {
    user_info_ = new views::View;
    user_info_->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kHorizontal, kUserInfoHorizontalPadding,
        kUserInfoVerticalPadding, kUserInfoPaddingBetweenItems));

    views::View* user = new views::View;
    user->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 0, 5, 0));
    ash::SystemTrayDelegate* tray =
        ash::Shell::GetInstance()->tray_delegate();
    username_ = new views::Label(UTF8ToUTF16(tray->GetUserDisplayName()));
    username_->SetFont(username_->font().DeriveFont(2));
    username_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    user->AddChildView(username_);

    email_ = new views::Label(UTF8ToUTF16(tray->GetUserEmail()));
    email_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    email_->SetEnabled(false);
    user->AddChildView(email_);

    user_info_->AddChildView(user);
    AddChildView(user_info_);

    RefreshForUpdate();
  }

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    ash::SystemTrayDelegate* tray = ash::Shell::GetInstance()->tray_delegate();
    if (sender == shutdown_)
      tray->ShutDown();
    else if (sender == signout_)
      tray->SignOut();
    else if (sender == lock_)
      tray->RequestLockScreen();
  }

  // Overridden from views::View.
  virtual void Layout() OVERRIDE {
    views::View::Layout();
    if (!update_)
      return;

    // Position |update_| appropriately.
    gfx::Rect bounds;
    bounds.set_x(user_info_->width() - update_->width());
    bounds.set_y(0);
    bounds.set_size(update_->GetPreferredSize());
    update_->SetBoundsRect(bounds);
  }

  views::View* user_info_;
  views::Label* username_;
  views::Label* email_;
  views::View* update_;

  TrayButton* shutdown_;
  TrayButton* signout_;
  TrayButton* lock_;

  DISALLOW_COPY_AND_ASSIGN(UserView);
};

// A custom image view with rounded edges.
class RoundedImageView : public views::View {
 public:
  // Constructs a new rounded image view with rounded corners of radius
  // |corner_radius|.
  explicit RoundedImageView(int corner_radius) : corner_radius_(corner_radius) {
  }

  virtual ~RoundedImageView() {
  }

  // Set the bitmap that should be displayed from a pointer. The pointer
  // contents is copied in the receiver's bitmap.
  void SetImage(const SkBitmap& bm) {
    image_ = bm;
    SetImageSize(gfx::Size(bm.width(), bm.height()));
  }

  // Set the desired image size.
  void SetImageSize(const gfx::Size& image_size) {
    image_size_ = image_size;
    PreferredSizeChanged();
  }

  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return image_size_;
  }

  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    View::OnPaint(canvas);
    gfx::Rect image_bounds(image_size_);
    const SkScalar kRadius = SkIntToScalar(corner_radius_);
    SkPath path;
    path.addRoundRect(gfx::RectToSkRect(image_bounds), kRadius, kRadius);

    SkPaint paint;
    SkShader* shader = SkShader::CreateBitmapShader(image_,
                                                    SkShader::kRepeat_TileMode,
                                                    SkShader::kRepeat_TileMode);
    SkMatrix shader_scale;
    shader_scale.setScale(SkFloatToScalar(
        static_cast<float>(image_bounds.width()) / image_.width()),
        SkFloatToScalar(static_cast<float>(
            image_bounds.height()) / image_.height()));
    shader->setLocalMatrix(shader_scale);

    paint.setShader(shader);
    paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
    shader->unref();
    canvas->sk_canvas()->drawPath(path, paint);
  }

 private:
  SkBitmap image_;
  gfx::Size image_size_;
  int corner_radius_;

  DISALLOW_COPY_AND_ASSIGN(RoundedImageView);
};

}  // namespace tray

TrayUser::TrayUser() {
}

TrayUser::~TrayUser() {
}

views::View* TrayUser::CreateTrayView(user::LoginStatus status) {
  avatar_.reset(new tray::RoundedImageView(kUserIconCornerRadius));
  if (status == user::LOGGED_IN_USER || status == user::LOGGED_IN_OWNER) {
    avatar_->SetImage(
        ash::Shell::GetInstance()->tray_delegate()->GetUserImage());
    avatar_->SetImageSize(gfx::Size(kUserIconSize, kUserIconSize));
  } else {
    avatar_->SetVisible(false);
  }
  return avatar_.get();
}

views::View* TrayUser::CreateDefaultView(user::LoginStatus status) {
  if (status == user::LOGGED_IN_NONE)
    return NULL;

  user_.reset(new tray::UserView(status));
  return user_.get();
}

views::View* TrayUser::CreateDetailedView(user::LoginStatus status) {
  return NULL;
}

void TrayUser::DestroyTrayView() {
  avatar_.reset();
}

void TrayUser::DestroyDefaultView() {
  user_.reset();
}

void TrayUser::DestroyDetailedView() {
}

void TrayUser::OnUpdateRecommended() {
  if (user_.get())
    user_->RefreshForUpdate();
}

void TrayUser::OnUserUpdate() {
  avatar_->SetImage(
      ash::Shell::GetInstance()->tray_delegate()->GetUserImage());
}

}  // namespace internal
}  // namespace ash
