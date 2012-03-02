// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/tray_volume.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/slider.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace internal {

namespace {
const int kVolumeImageWidth = 44;
const int kVolumeImageHeight = 44;
const int kVolumeLevel = 5;
}

namespace tray {

class VolumeButton : public views::ToggleImageButton {
 public:
  explicit VolumeButton(views::ButtonListener* listener)
      : views::ToggleImageButton(listener),
        image_index_(-1) {
    image_ = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_AURA_UBER_TRAY_VOLUME_LEVELS);
    Update();
  }

  virtual ~VolumeButton() {}

  void Update() {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    int level = static_cast<int>(delegate->GetVolumeLevel() * 100);
    int image_index = level / (100 / kVolumeLevel);
    if (level > 0 && image_index == 0)
      ++image_index;
    if (level == 100)
      image_index = kVolumeLevel - 1;
    else if (image_index == kVolumeLevel - 1)
      --image_index;
    if (image_index != image_index_) {
      SkIRect region = SkIRect::MakeXYWH(0, image_index * kVolumeImageHeight,
          kVolumeImageWidth, kVolumeImageHeight);
      SkBitmap bitmap;
      image_.ToSkBitmap()->extractSubset(&bitmap, region);
      SetImage(views::CustomButton::BS_NORMAL, &bitmap);
      image_index_ = image_index;
    }
    SchedulePaint();
  }

 private:
  // Overridden from views::View.
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    views::ToggleImageButton::OnPaint(canvas);

    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    if (!delegate->IsAudioMuted())
      return;

    SkPaint paint;
    paint.setColor(SkColorSetARGB(63, 0, 0, 0));
    paint.setStrokeWidth(SkIntToScalar(3));
    canvas->GetSkCanvas()->drawLine(SkIntToScalar(width() - 10),
        SkIntToScalar(10), SkIntToScalar(10), SkIntToScalar(height() - 10),
        paint);
  }

  gfx::Image image_;
  int image_index_;

  DISALLOW_COPY_AND_ASSIGN(VolumeButton);
};

class VolumeView : public views::View,
                   public views::ButtonListener,
                   public views::SliderListener {
 public:
  VolumeView() {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
          0, 0, 5));

    icon_ = new VolumeButton(this);
    AddChildView(icon_);

    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    slider_ = new views::Slider(this, views::Slider::HORIZONTAL);
    slider_->SetValue(delegate->GetVolumeLevel());
    slider_->set_border(views::Border::CreateEmptyBorder(0, 0, 0, 20));
    AddChildView(slider_);
  }

  void SetVolumeLevel(float percent) {
    slider_->SetValue(percent);
  }

 private:
  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    CHECK(sender == icon_);
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    delegate->SetAudioMuted(!delegate->IsAudioMuted());
  }

  // Overridden from views:SliderListener.
  virtual void SliderValueChanged(views::Slider* sender,
                                  float value,
                                  float old_value,
                                  views::SliderChangeReason reason) OVERRIDE {
    if (reason == views::VALUE_CHANGED_BY_USER)
      ash::Shell::GetInstance()->tray_delegate()->SetVolumeLevel(value);
    icon_->Update();
  }

  VolumeButton* icon_;
  views::Slider* slider_;

  DISALLOW_COPY_AND_ASSIGN(VolumeView);
};

}  // namespace tray

TrayVolume::TrayVolume() {
}

TrayVolume::~TrayVolume() {
}

views::View* TrayVolume::CreateTrayView() {
  return NULL;
}

views::View* TrayVolume::CreateDefaultView() {
  volume_view_.reset(new tray::VolumeView);
  return volume_view_.get();
}

views::View* TrayVolume::CreateDetailedView() {
  volume_view_.reset(new tray::VolumeView);
  return volume_view_.get();
}

void TrayVolume::DestroyTrayView() {
}

void TrayVolume::DestroyDefaultView() {
  volume_view_.reset();
}

void TrayVolume::DestroyDetailedView() {
  volume_view_.reset();
}

void TrayVolume::OnVolumeChanged(float percent) {
  if (volume_view_.get()) {
    volume_view_->SetVolumeLevel(percent);
    return;
  }

  PopupDetailedView();
}

}  // namespace internal
}  // namespace ash
