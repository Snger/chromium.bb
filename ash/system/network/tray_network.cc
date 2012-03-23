// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/tray_network.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_more.h"
#include "ash/system/tray/tray_views.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace {

// Height of the list of networks in the popup.
const int kNetworkListHeight = 160;

}

namespace ash {
namespace internal {

namespace tray {

enum ResourceSize {
  SMALL,
  LARGE,
};

class NetworkTrayView : public views::View {
 public:
  explicit NetworkTrayView(ResourceSize size) : resource_size_(size) {
    SetLayoutManager(new views::FillLayout());

    image_view_ = new views::ImageView;
    AddChildView(image_view_);

    NetworkIconInfo info;
    Shell::GetInstance()->tray_delegate()->
        GetMostRelevantNetworkIcon(&info, resource_size_ == LARGE);
    Update(info);
  }

  virtual ~NetworkTrayView() {}

  void Update(const NetworkIconInfo& info) {
    image_view_->SetImage(info.image);
    SchedulePaint();
  }

 private:
  views::ImageView* image_view_;
  ResourceSize resource_size_;

  DISALLOW_COPY_AND_ASSIGN(NetworkTrayView);
};

class NetworkDefaultView : public TrayItemMore {
 public:
  explicit NetworkDefaultView(SystemTrayItem* owner)
      : TrayItemMore(owner) {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
        kTrayPopupPaddingHorizontal, 0, kTrayPopupPaddingBetweenItems));

    icon_ = new NetworkTrayView(LARGE);
    AddChildView(icon_);

    label_ = new views::Label();
    AddChildView(label_);

    AddMore();

    NetworkIconInfo info;
    Shell::GetInstance()->tray_delegate()->
        GetMostRelevantNetworkIcon(&info, true);
    Update(info);
  }

  virtual ~NetworkDefaultView() {}

  void Update(const NetworkIconInfo& info) {
    icon_->Update(info);
    label_->SetText(info.description);
  }

 private:
  NetworkTrayView* icon_;
  views::Label* label_;

  DISALLOW_COPY_AND_ASSIGN(NetworkDefaultView);
};

class NetworkDetailedView : public views::View,
                            public ViewClickListener {
 public:
  explicit NetworkDetailedView(user::LoginStatus login)
      : login_(login),
        header_(NULL),
        airplane_(NULL),
        mobile_account_(NULL),
        other_wifi_(NULL),
        other_mobile_(NULL),
        toggle_wifi_(NULL),
        toggle_mobile_(NULL),
        settings_(NULL),
        proxy_settings_(NULL) {
    SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 1, 1, 1));
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));
    Update();
  }

  virtual ~NetworkDetailedView() {}

  void Update() {
    RemoveAllChildViews(true);

    header_ = NULL;
    airplane_ = NULL;
    mobile_account_ = NULL;
    other_wifi_ = NULL;
    other_mobile_ = NULL;
    toggle_wifi_ = NULL;
    toggle_mobile_ = NULL;
    settings_ = NULL;
    proxy_settings_ = NULL;

    AppendHeaderEntry();
    AppendNetworkEntries();
    AppendNetworkExtra();
    AppendNetworkToggles();
    AppendSettingsEntry();

    Layout();
  }

 private:
  void AppendHeaderEntry() {
    header_ = CreateDetailedHeaderEntry(IDS_ASH_STATUS_TRAY_NETWORK, this);
    AddChildView(header_);
  }

  void AppendNetworkEntries() {
    SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
    std::vector<NetworkIconInfo> list;
    delegate->GetAvailableNetworks(&list);
    FixedSizedScrollView* scroller = new FixedSizedScrollView;
    views::View* networks = new views::View;
    networks->SetLayoutManager(new views::BoxLayout(
        views::BoxLayout::kVertical, 0, 0, 1));
    for (size_t i = 0; i < list.size(); i++) {
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddIconAndLabel(list[i].image, list[i].name,
          list[i].highlight ? gfx::Font::BOLD : gfx::Font::NORMAL);
      networks->AddChildView(container);
      network_map_[container] = list[i].service_path;
    }

    if (login_ != user::LOGGED_IN_NONE) {
      std::string carrier_id, topup_url;
      if (delegate->GetCellularCarrierInfo(&carrier_id, &topup_url)) {
        if (carrier_id != carrier_id_) {
          carrier_id_ = carrier_id;
          if (!topup_url.empty())
            topup_url_ = topup_url;
        }
        if (!topup_url_.empty()) {
          HoverHighlightView* container = new HoverHighlightView(this);
          container->AddLabel(ui::ResourceBundle::GetSharedInstance().
              GetLocalizedString(IDS_ASH_STATUS_TRAY_MOBILE_VIEW_ACCOUNT));
          AddChildView(container);
          mobile_account_ = container;
        }
      }
    }

    scroller->set_border(views::Border::CreateSolidSidedBorder(1, 0, 1, 0,
        SkColorSetARGB(25, 0, 0, 0)));
    scroller->set_fixed_size(
        gfx::Size(networks->GetPreferredSize().width() +
                  scroller->GetScrollBarWidth(),
                  kNetworkListHeight));
    scroller->SetContentsView(networks);
    AddChildView(scroller);
  }

  void AppendNetworkExtra() {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    if (delegate->GetWifiEnabled()) {
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddLabel(rb.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_OTHER_WIFI));
      AddChildView(container);
      other_wifi_ = container;
    }

    if (delegate->GetCellularEnabled()) {
      if (delegate->GetCellularScanSupported()) {
        HoverHighlightView* container = new HoverHighlightView(this);
        container->AddLabel(rb.GetLocalizedString(
            IDS_ASH_STATUS_TRAY_OTHER_MOBILE));
        AddChildView(container);
        other_mobile_ = container;
      }
    }
  }

  void AppendNetworkToggles() {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    if (delegate->GetWifiAvailable()) {
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddLabel(rb.GetLocalizedString(delegate->GetWifiEnabled() ?
          IDS_ASH_STATUS_TRAY_DISABLE_WIFI :
          IDS_ASH_STATUS_TRAY_ENABLE_WIFI), gfx::Font::NORMAL);
      AddChildView(container);
      toggle_wifi_ = container;
    }

    if (delegate->GetCellularAvailable()) {
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddLabel(rb.GetLocalizedString(
          delegate->GetCellularEnabled() ?  IDS_ASH_STATUS_TRAY_DISABLE_MOBILE :
                                            IDS_ASH_STATUS_TRAY_ENABLE_MOBILE),
          gfx::Font::NORMAL);
      AddChildView(container);
      toggle_mobile_ = container;
    }
  }

  void AppendAirplaneModeEntry() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    HoverHighlightView* container = new HoverHighlightView(this);
    container->AddIconAndLabel(
        *rb.GetImageNamed(IDR_AURA_UBER_TRAY_NETWORK_AIRPLANE).ToSkBitmap(),
        rb.GetLocalizedString(IDS_ASH_STATUS_TRAY_AIRPLANE_MODE),
        gfx::Font::NORMAL);
    AddChildView(container);
    airplane_ = container;
  }

  // Adds a settings entry when logged in, and an entry for changing proxy
  // settings otherwise.
  void AppendSettingsEntry() {
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    if (login_ != user::LOGGED_IN_NONE) {
      // Settings, only if logged in.
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddLabel(rb.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_NETWORK_SETTINGS), gfx::Font::NORMAL);
      AddChildView(container);
      settings_ = container;
    } else {
      // Allow changing proxy settings in the login screen.
      HoverHighlightView* container = new HoverHighlightView(this);
      container->AddLabel(rb.GetLocalizedString(
          IDS_ASH_STATUS_TRAY_NETWORK_PROXY_SETTINGS), gfx::Font::NORMAL);
      AddChildView(container);
      proxy_settings_ = container;
    }
  }

  // Overridden from ViewClickListener.
  virtual void ClickedOn(views::View* sender) OVERRIDE {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    if (sender == header_) {
      Shell::GetInstance()->tray()->ShowDefaultView();
    } else if (sender == settings_) {
      delegate->ShowNetworkSettings();
    } else if (sender == proxy_settings_) {
      delegate->ChangeProxySettings();
    } else if (sender == mobile_account_) {
      delegate->ShowCellularTopupURL(topup_url_);
    } else if (sender == other_wifi_) {
      delegate->ShowOtherWifi();
    } else if (sender == other_mobile_) {
      delegate->ShowOtherCellular();
    } else if (sender == toggle_wifi_) {
      delegate->ToggleWifi();
    } else if (sender == toggle_mobile_) {
      delegate->ToggleCellular();
    } else if (sender == airplane_) {
      delegate->ToggleAirplaneMode();
    } else {
      std::map<views::View*, std::string>::iterator find;
      find = network_map_.find(sender);
      if (find != network_map_.end()) {
        std::string network_id = find->second;
        delegate->ConnectToNetwork(network_id);
      }
    }
  }

  std::string carrier_id_;
  std::string topup_url_;

  user::LoginStatus login_;
  std::map<views::View*, std::string> network_map_;
  views::View* header_;
  views::View* airplane_;
  views::View* mobile_account_;
  views::View* other_wifi_;
  views::View* other_mobile_;
  views::View* toggle_wifi_;
  views::View* toggle_mobile_;
  views::View* settings_;
  views::View* proxy_settings_;
  DISALLOW_COPY_AND_ASSIGN(NetworkDetailedView);
};

}  // namespace tray

TrayNetwork::TrayNetwork() {
}

TrayNetwork::~TrayNetwork() {
}

views::View* TrayNetwork::CreateTrayView(user::LoginStatus status) {
  tray_.reset(new tray::NetworkTrayView(tray::SMALL));
  return tray_.get();
}

views::View* TrayNetwork::CreateDefaultView(user::LoginStatus status) {
  default_.reset(new tray::NetworkDefaultView(this));
  return default_.get();
}

views::View* TrayNetwork::CreateDetailedView(user::LoginStatus status) {
  detailed_.reset(new tray::NetworkDetailedView(status));
  return detailed_.get();
}

void TrayNetwork::DestroyTrayView() {
  tray_.reset();
}

void TrayNetwork::DestroyDefaultView() {
  default_.reset();
}

void TrayNetwork::DestroyDetailedView() {
  detailed_.reset();
}

void TrayNetwork::OnNetworkRefresh(const NetworkIconInfo& info) {
  if (tray_.get())
    tray_->Update(info);
  if (default_.get())
    default_->Update(info);
  if (detailed_.get())
    detailed_->Update();
}

}  // namespace internal
}  // namespace ash
