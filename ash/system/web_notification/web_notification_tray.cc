// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/web_notification/web_notification_tray.h"

#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "base/bind.h"
#include "base/message_loop.h"
#include "base/stringprintf.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "ui/aura/window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/button/menu_button_listener.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_model_adapter.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget_observer.h"

namespace {

// Tray constants
const int kTrayContainerVeritcalPaddingBottomAlignment = 3;
const int kTrayContainerHorizontalPaddingBottomAlignment = 1;
const int kTrayContainerVerticalPaddingVerticalAlignment = 1;
const int kTrayContainerHorizontalPaddingVerticalAlignment = 0;
const int kPaddingFromLeftEdgeOfSystemTrayBottomAlignment = 8;
const int kPaddingFromTopEdgeOfSystemTrayVerticalAlignment = 10;
const int kTrayWidth = 40;
const int kTrayHeight = 31;
const int kTraySideWidth = 32;
const int kTraySideHeight = 24;

// Web Notification Bubble constants
const int kWebNotificationBubbleMinHeight = 80;
const int kWebNotificationBubbleMaxHeight = 480;
// Delay laying out the Bubble until all notifications have been added and icons
// have had a chance to load.
const int kUpdateDelayMs = 50;
// Limit the number of visible notifications.
const int kMaxVisibleNotifications = 100;
const int kAutocloseDelaySeconds = 5;
const SkColor kMessageCountColor = SkColorSetARGB(0xff, 0xff, 0xff, 0xff);
const SkColor kNotificationColor = SkColorSetRGB(0xfe, 0xfe, 0xfe);
const SkColor kNotificationReadColor = SkColorSetRGB(0xfa, 0xfa, 0xfa);


// Individual notifications constants
const int kWebNotificationWidth = 320;
const int kWebNotificationButtonWidth = 32;
const int kWebNotificationIconSize = 40;

// Menu constants
const int kTogglePermissionCommand = 0;
const int kToggleExtensionCommand = 1;
const int kShowSettingsCommand = 2;

std::string GetNotificationText(int notification_count) {
  if (notification_count >= 100)
    return "99+";
  return base::StringPrintf("%d", notification_count);
}

}  // namespace

namespace ash {

namespace internal {

struct WebNotification {
  WebNotification() : is_read(false) {}
  std::string id;
  string16 title;
  string16 message;
  string16 display_source;
  std::string extension_id;
  gfx::ImageSkia image;
  bool is_read;
};

// Web Notification List -------------------------------------------------------

// A helper class to manage the list of notifications.
class WebNotificationList {
 public:
  typedef std::list<WebNotification> Notifications;

  WebNotificationList()
      : message_center_visible_(false),
        unread_count_(0) {
  }

  void SetMessageCenterVisible(bool visible) {
    if (message_center_visible_ == visible)
      return;
    message_center_visible_ = visible;
    if (visible) {
      // Clear the unread count when the list is shown.
      unread_count_ = 0;
    } else {
      // Mark all notifications as read when the list is hidden.
      for (Notifications::iterator iter = notifications_.begin();
           iter != notifications_.end(); ++iter) {
        iter->is_read = true;
      }
    }
  }

  void AddNotification(const std::string& id,
                       const string16& title,
                       const string16& message,
                       const string16& display_source,
                       const std::string& extension_id) {
    WebNotification notification;
    notification.id = id;
    notification.title = title;
    notification.message = message;
    notification.display_source = display_source;
    notification.extension_id = extension_id;
    notification.is_read = false;
    PushNotification(notification);
  }

  void UpdateNotificationMessage(const std::string& old_id,
                                 const std::string& new_id,
                                 const string16& title,
                                 const string16& message) {
    Notifications::iterator iter = GetNotification(old_id);
    if (iter == notifications_.end())
      return;
    // Copy and update notification, then move it to the front of the list.
    WebNotification notification(*iter);
    notification.id = new_id;
    notification.title = title;
    notification.message = message;
    notification.is_read = false;
    EraseNotification(iter);
    PushNotification(notification);
  }

  // Returns true if the notification was removed.
  bool RemoveNotification(const std::string& id) {
    Notifications::iterator iter = GetNotification(id);
    if (iter == notifications_.end())
      return false;
    EraseNotification(iter);
    return true;
  }

  void RemoveAllNotifications() {
    notifications_.clear();
  }

  void SendRemoveNotificationsBySource(WebNotificationTray* tray,
                                       const std::string& id) {
    Notifications::iterator source_iter = GetNotification(id);
    if (source_iter == notifications_.end())
      return;
    string16 display_source = source_iter->display_source;
    for (Notifications::iterator loopiter = notifications_.begin();
         loopiter != notifications_.end(); ) {
      Notifications::iterator curiter = loopiter++;
      if (curiter->display_source == display_source)
        tray->SendRemoveNotification(curiter->id);
    }
  }

  void SendRemoveNotificationsByExtension(WebNotificationTray* tray,
                                          const std::string& id) {
    Notifications::iterator source_iter = GetNotification(id);
    if (source_iter == notifications_.end())
      return;
    std::string extension_id = source_iter->extension_id;
    for (Notifications::iterator loopiter = notifications_.begin();
         loopiter != notifications_.end(); ) {
      Notifications::iterator curiter = loopiter++;
      if (curiter->extension_id == extension_id)
        tray->SendRemoveNotification(curiter->id);
    }
  }

  // Returns true if the notification exists and was updated.
  bool SetNotificationImage(const std::string& id,
                            const gfx::ImageSkia& image) {
    Notifications::iterator iter = GetNotification(id);
    if (iter == notifications_.end())
      return false;
    iter->image = image;
    return true;
  }

  std::string GetFirstId() {
    if (notifications_.empty())
      return std::string();
    return notifications_.front().id;
  }

  bool HasNotification(const std::string& id) {
    return GetNotification(id) != notifications_.end();
  }

  const Notifications& notifications() const { return notifications_; }
  int unread_count() const { return unread_count_; }

 private:
  // Iterates through the list and returns the first notification matching |id|
  // (should always be unique).
  Notifications::iterator GetNotification(const std::string& id) {
    for (Notifications::iterator iter = notifications_.begin();
         iter != notifications_.end(); ++iter) {
      if (iter->id == id)
        return iter;
    }
    return notifications_.end();
  }

  void EraseNotification(Notifications::iterator iter) {
    if (!message_center_visible_ && !iter->is_read)
      --unread_count_;
    notifications_.erase(iter);
  }

  void PushNotification(const WebNotification& notification) {
    // Ensure that notification.id is unique by erasing any existing
    // notification with the same id (shouldn't normally happen).
    Notifications::iterator iter = GetNotification(notification.id);
    if (iter != notifications_.end())
      EraseNotification(iter);
    // Add the notification to the front (top) of the list.
    if (!message_center_visible_)
      ++unread_count_;
    notifications_.push_front(notification);
  }

  Notifications notifications_;
  bool message_center_visible_;
  int unread_count_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationList);
};

// Web notification view -------------------------------------------------------

// A dropdown menu for notifications.
class WebNotificationMenuModel : public ui::SimpleMenuModel,
                                 public ui::SimpleMenuModel::Delegate {
 public:
  explicit WebNotificationMenuModel(WebNotificationTray* tray,
                                    const WebNotification& notification)
      : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
        tray_(tray),
        notification_(notification) {
    // Add 'disable notifications' menu item.
    if (!notification.extension_id.empty()) {
      AddItem(kToggleExtensionCommand,
              GetLabelForCommandId(kToggleExtensionCommand));
    } else if (!notification.display_source.empty()) {
      AddItem(kTogglePermissionCommand,
              GetLabelForCommandId(kTogglePermissionCommand));
    }
    // Add settings menu item.
    if (!notification.display_source.empty()) {
      AddItem(kShowSettingsCommand,
              GetLabelForCommandId(kShowSettingsCommand));
    }
  }

  virtual ~WebNotificationMenuModel() {
  }

  // Overridden from ui::SimpleMenuModel:
  virtual string16 GetLabelForCommandId(int command_id) const OVERRIDE {
    switch (command_id) {
      case kToggleExtensionCommand:
        return l10n_util::GetStringUTF16(
            IDS_ASH_WEB_NOTFICATION_TRAY_EXTENSIONS_DISABLE);
      case kTogglePermissionCommand:
        return l10n_util::GetStringFUTF16(
            IDS_ASH_WEB_NOTFICATION_TRAY_SITE_DISABLE,
            notification_.display_source);
      case kShowSettingsCommand:
        return l10n_util::GetStringUTF16(
            IDS_ASH_WEB_NOTFICATION_TRAY_SETTINGS);
      default:
        NOTREACHED();
    }
    return string16();
  }

  // Overridden from ui::SimpleMenuModel::Delegate:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE {
    return false;
  }

  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE {
    return true;
  }

  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE {
    return false;
  }

  virtual void ExecuteCommand(int command_id) OVERRIDE {
    switch (command_id) {
      case kToggleExtensionCommand:
        tray_->DisableByExtension(notification_.id);
        break;
      case kTogglePermissionCommand:
        tray_->DisableByUrl(notification_.id);
        break;
      case kShowSettingsCommand:
        tray_->ShowSettings(notification_.id);
        break;
      default:
        NOTREACHED();
    }
  }

 private:
  WebNotificationTray* tray_;
  WebNotification notification_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationMenuModel);
};

// The view for a notification entry (icon + message + buttons).
class WebNotificationView : public views::View,
                            public views::ButtonListener,
                            public ui::ImplicitAnimationObserver {
 public:
  WebNotificationView(WebNotificationTray* tray,
                      const WebNotification& notification)
      : tray_(tray),
        notification_(notification),
        icon_(NULL),
        close_button_(NULL),
        scroller_(NULL),
        gesture_scroll_amount_(0.f) {
    InitView(tray, notification);
  }

  virtual ~WebNotificationView() {
  }

  void set_scroller(views::ScrollView* scroller) { scroller_ = scroller; }

  void InitView(WebNotificationTray* tray,
                const WebNotification& notification) {
    set_border(views::Border::CreateSolidSidedBorder(
        1, 0, 0, 0, kBorderLightColor));
    SkColor bg_color = notification.is_read ?
        kNotificationReadColor : kNotificationColor;
    set_background(views::Background::CreateSolidBackground(bg_color));
    SetPaintToLayer(true);
    SetFillsBoundsOpaquely(false);

    icon_ = new views::ImageView;
    icon_->SetImageSize(
        gfx::Size(kWebNotificationIconSize, kWebNotificationIconSize));
    icon_->SetImage(notification.image);

    views::Label* title = new views::Label(notification.title);
    title->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    title->SetFont(title->font().DeriveFont(0, gfx::Font::BOLD));
    views::Label* message = new views::Label(notification.message);
    message->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    message->SetMultiLine(true);

    close_button_ = new views::ImageButton(this);
    close_button_->SetImage(
        views::CustomButton::BS_NORMAL,
        ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
            IDR_AURA_UBER_TRAY_NOTIFY_CLOSE));
    close_button_->SetImageAlignment(views::ImageButton::ALIGN_CENTER,
                                     views::ImageButton::ALIGN_MIDDLE);

    views::GridLayout* layout = new views::GridLayout(this);
    SetLayoutManager(layout);

    views::ColumnSet* columns = layout->AddColumnSet(0);

    const int padding_width = kTrayPopupPaddingHorizontal/2;
    columns->AddPaddingColumn(0, padding_width);

    // Notification Icon.
    columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                       0, /* resize percent */
                       views::GridLayout::FIXED,
                       kWebNotificationIconSize, kWebNotificationIconSize);

    columns->AddPaddingColumn(0, padding_width);

    // Notification message text.
    const int message_width = kWebNotificationWidth - kWebNotificationIconSize -
        kWebNotificationButtonWidth - (padding_width * 3);
    columns->AddColumn(views::GridLayout::FILL, views::GridLayout::LEADING,
                       100, /* resize percent */
                       views::GridLayout::FIXED, message_width, message_width);

    columns->AddPaddingColumn(0, padding_width);

    // Close button.
    columns->AddColumn(views::GridLayout::CENTER, views::GridLayout::LEADING,
                       0, /* resize percent */
                       views::GridLayout::FIXED,
                       kWebNotificationButtonWidth,
                       kWebNotificationButtonWidth);

    // Layout rows
    layout->AddPaddingRow(0, kTrayPopupPaddingBetweenItems);

    layout->StartRow(0, 0);
    layout->AddView(icon_, 1, 2);
    layout->AddView(title, 1, 1);
    layout->AddView(close_button_, 1, 1);

    layout->StartRow(0, 0);
    layout->SkipColumns(2);
    layout->AddView(message, 1, 1);
    layout->AddPaddingRow(0, kTrayPopupPaddingBetweenItems);
  }

  // views::View overrides.
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE {
    if (event.flags() & ui::EF_RIGHT_MOUSE_BUTTON) {
      ShowMenu(event.location());
      return true;
    }
    tray_->OnClicked(notification_.id);
    return true;
  }

  virtual ui::GestureStatus OnGestureEvent(
      const ui::GestureEvent& event) OVERRIDE {
    if (event.type() == ui::ET_GESTURE_TAP) {
      tray_->OnClicked(notification_.id);
      return ui::GESTURE_STATUS_CONSUMED;
    }

    if (event.type() == ui::ET_GESTURE_LONG_PRESS) {
      ShowMenu(event.location());
      return ui::GESTURE_STATUS_CONSUMED;
    }

    if (event.type() == ui::ET_SCROLL_FLING_START) {
      // The threshold for the fling velocity is computed empirically.
      // The unit is in pixels/second.
      const float kFlingThresholdForClose = 800.f;
      if (fabsf(event.details().velocity_x()) > kFlingThresholdForClose) {
        SlideOutAndClose(event.details().velocity_x() < 0 ? SLIDE_LEFT :
                                                            SLIDE_RIGHT);
      } else if (scroller_) {
        RestoreVisualState();
        scroller_->OnGestureEvent(event);
      }
      return ui::GESTURE_STATUS_CONSUMED;
    }

    if (!event.IsScrollGestureEvent())
      return ui::GESTURE_STATUS_UNKNOWN;

    if (event.type() == ui::ET_GESTURE_SCROLL_BEGIN) {
      gesture_scroll_amount_ = 0.f;
    } else if (event.type() == ui::ET_GESTURE_SCROLL_UPDATE) {
      // The scroll-update events include the incremental scroll amount.
      gesture_scroll_amount_ += event.details().scroll_x();

      ui::Transform transform;
      transform.SetTranslateX(gesture_scroll_amount_);
      layer()->SetTransform(transform);
      layer()->SetOpacity(
          1.f - std::min(fabsf(gesture_scroll_amount_) / width(), 1.f));

    } else if (event.type() == ui::ET_GESTURE_SCROLL_END) {
      const float kScrollRatioForClosingNotification = 0.5f;
      float scrolled_ratio = fabsf(gesture_scroll_amount_) / width();
      if (scrolled_ratio >= kScrollRatioForClosingNotification)
        SlideOutAndClose(gesture_scroll_amount_ < 0 ? SLIDE_LEFT : SLIDE_RIGHT);
      else
        RestoreVisualState();
    }

    if (scroller_)
      scroller_->OnGestureEvent(event);
    return ui::GESTURE_STATUS_CONSUMED;
  }

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    if (sender == close_button_)
      tray_->SendRemoveNotification(notification_.id);
  }

  // Overridden from ImplicitAnimationObserver.
  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    tray_->SendRemoveNotification(notification_.id);
  }

 private:
  enum SlideDirection {
    SLIDE_LEFT,
    SLIDE_RIGHT
  };

  // Shows the menu for the notification.
  void ShowMenu(gfx::Point screen_location) {
    WebNotificationMenuModel menu_model(tray_, notification_);
    if (menu_model.GetItemCount() == 0)
      return;

    views::MenuModelAdapter menu_model_adapter(&menu_model);
    views::MenuRunner menu_runner(menu_model_adapter.CreateMenu());

    views::View::ConvertPointToScreen(this, &screen_location);
    ignore_result(menu_runner.RunMenuAt(
        GetWidget()->GetTopLevelWidget(),
        NULL,
        gfx::Rect(screen_location, gfx::Size()),
        views::MenuItemView::TOPRIGHT,
        views::MenuRunner::HAS_MNEMONICS));
  }

  // Restores the transform and opacity of the view.
  void RestoreVisualState() {
    // Restore the layer state.
    const int kSwipeRestoreDurationMS = 150;
    ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(kSwipeRestoreDurationMS));
    layer()->SetTransform(ui::Transform());
    layer()->SetOpacity(1.f);
  }

  // Slides the view out and closes it after the animation.
  void SlideOutAndClose(SlideDirection direction) {
    const int kSwipeOutTotalDurationMS = 150;
    int swipe_out_duration = kSwipeOutTotalDurationMS * layer()->opacity();
    ui::ScopedLayerAnimationSettings settings(layer()->GetAnimator());
    settings.SetTransitionDuration(
        base::TimeDelta::FromMilliseconds(swipe_out_duration));
    settings.AddObserver(this);

    ui::Transform transform;
    transform.SetTranslateX(direction == SLIDE_LEFT ? -width() : width());
    layer()->SetTransform(transform);
    layer()->SetOpacity(0.f);
  }

  WebNotificationTray* tray_;
  WebNotification notification_;
  views::ImageView* icon_;
  views::ImageButton* close_button_;

  views::ScrollView* scroller_;
  float gesture_scroll_amount_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationView);
};

// The view for the buttons at the bottom of the web notification tray.
class WebNotificationButtonView : public views::View,
                                  public views::ButtonListener {
 public:
  explicit WebNotificationButtonView(WebNotificationTray* tray)
      : tray_(tray),
        close_all_button_(NULL) {
    set_background(views::Background::CreateBackgroundPainter(
        true,
        views::Painter::CreateVerticalGradient(
            kHeaderBackgroundColorLight,
            kHeaderBackgroundColorDark)));
    set_border(views::Border::CreateSolidSidedBorder(
        2, 0, 0, 0, ash::kBorderDarkColor));

    views::GridLayout* layout = new views::GridLayout(this);
    SetLayoutManager(layout);
    views::ColumnSet* columns = layout->AddColumnSet(0);
    columns->AddPaddingColumn(100, 0);
    columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER,
                       0, /* resize percent */
                       views::GridLayout::USE_PREF, 0, 0);

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    close_all_button_ = new TrayPopupTextButton(
        this, rb.GetLocalizedString(IDS_ASH_WEB_NOTFICATION_TRAY_CLEAR_ALL));

    layout->StartRow(0, 0);
    layout->AddView(close_all_button_);
  }

  virtual ~WebNotificationButtonView() {
  }

  void SetCloseAllVisible(bool visible) {
    close_all_button_->SetVisible(visible);
  }

  // Overridden from ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    if (sender == close_all_button_)
      tray_->SendRemoveAllNotifications();
  }

 private:
  WebNotificationTray* tray_;
  TrayPopupTextButton* close_all_button_;

  DISALLOW_COPY_AND_ASSIGN(WebNotificationButtonView);
};

// Web notification bubble contents --------------------------------------------

// Base class for the contents of a web notification bubble.
class WebContentsView : public views::View {
 public:
  explicit WebContentsView(WebNotificationTray* tray)
      : tray_(tray) {
    // TODO(stevenjb): Remove this border when TrayBubbleBorder is integrated
    // with BubbleBorder.
    int left = (tray->shelf_alignment() == SHELF_ALIGNMENT_LEFT) ? 0 : 1;
    int right = (tray->shelf_alignment() == SHELF_ALIGNMENT_RIGHT) ? 0 : 1;
    int bottom = (tray->shelf_alignment() == SHELF_ALIGNMENT_BOTTOM) ? 0 : 1;
    set_border(views::Border::CreateSolidSidedBorder(
        1, left, bottom, right, ash::kBorderDarkColor));
    set_notify_enter_exit_on_child(true);
  }
  virtual ~WebContentsView() {}

  virtual void Update(
      const WebNotificationList::Notifications& notifications) = 0;

 protected:
  WebNotificationTray* tray_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WebContentsView);
};

// Message Center contents.
class MessageCenterContentsView : public WebContentsView {
 public:
  class ScrollContentView : public views::View {
   public:
    ScrollContentView() {
      views::BoxLayout* layout =
          new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1);
      layout->set_spread_blank_space(true);
      SetLayoutManager(layout);
    }

    virtual ~ScrollContentView() {};

    virtual gfx::Size GetPreferredSize() OVERRIDE {
      if (!preferred_size_.IsEmpty())
        return preferred_size_;
      return views::View::GetPreferredSize();
    }

    void set_preferred_size(const gfx::Size& size) { preferred_size_ = size; }

   private:
    gfx::Size preferred_size_;
    DISALLOW_COPY_AND_ASSIGN(ScrollContentView);
  };

  explicit MessageCenterContentsView(WebNotificationTray* tray)
      : WebContentsView(tray) {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));

    scroll_content_ = new ScrollContentView;
    scroller_ = new internal::FixedSizedScrollView;
    scroller_->SetContentsView(scroll_content_);
    AddChildView(scroller_);

    scroller_->SetPaintToLayer(true);
    scroller_->SetFillsBoundsOpaquely(false);
    scroller_->layer()->SetMasksToBounds(true);

    button_view_ = new internal::WebNotificationButtonView(tray);
    AddChildView(button_view_);

    // Build initial view with no notifications.
    Update(WebNotificationList::Notifications());
  }

  void Update(const WebNotificationList::Notifications& notifications) {
    scroll_content_->RemoveAllChildViews(true);
    scroll_content_->set_preferred_size(gfx::Size());
    int num_children = 0;
    for (WebNotificationList::Notifications::const_iterator iter =
             notifications.begin(); iter != notifications.end(); ++iter) {
      WebNotificationView* view = new WebNotificationView(tray_, *iter);
      view->set_scroller(scroller_);
      scroll_content_->AddChildView(view);
      if (++num_children >= kMaxVisibleNotifications)
        break;
    }
    if (num_children == 0) {
      views::Label* label = new views::Label(l10n_util::GetStringUTF16(
          IDS_ASH_WEB_NOTFICATION_TRAY_NO_MESSAGES));
      label->SetFont(label->font().DeriveFont(1));
      label->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
      label->SetEnabledColor(SK_ColorGRAY);
      scroll_content_->AddChildView(label);
      button_view_->SetCloseAllVisible(false);
    } else {
      button_view_->SetCloseAllVisible(true);
    }
    SizeScrollContent();
    Layout();
    if (GetWidget())
      GetWidget()->GetRootView()->SchedulePaint();
  }

 private:
  void SizeScrollContent() {
    gfx::Size scroll_size = scroll_content_->GetPreferredSize();
    const int button_height = button_view_->GetPreferredSize().height();
    const int min_height = kWebNotificationBubbleMinHeight - button_height;
    const int max_height = kWebNotificationBubbleMaxHeight - button_height;
    int scroll_height = std::min(std::max(
        scroll_size.height(), min_height), max_height);
    scroll_size.set_height(scroll_height);
    if (scroll_height == min_height)
      scroll_content_->set_preferred_size(scroll_size);
    else
      scroll_content_->set_preferred_size(gfx::Size());
    scroller_->SetFixedSize(scroll_size);
    scroller_->SizeToPreferredSize();
  }

  internal::FixedSizedScrollView* scroller_;
  ScrollContentView* scroll_content_;
  internal::WebNotificationButtonView* button_view_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterContentsView);
};

// Popup notifications contents.
class PopupBubbleContentsView : public WebContentsView {
 public:
  explicit PopupBubbleContentsView(WebNotificationTray* tray)
      : WebContentsView(tray) {
    SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
    set_background(views::Background::CreateSolidBackground(kBackgroundColor));

    content_ = new views::View;
    content_->SetLayoutManager(
        new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 1));
    AddChildView(content_);

    content_->SetPaintToLayer(true);
    content_->SetFillsBoundsOpaquely(false);
    content_->layer()->SetMasksToBounds(true);

    // Build initial view with no notification.
    Update(WebNotificationList::Notifications());
  }

  void Update(
      const WebNotificationList::Notifications& notifications) OVERRIDE {
    content_->RemoveAllChildViews(true);
    const WebNotification& notification = (notifications.size() > 0) ?
        notifications.front() : WebNotification();
    WebNotificationView* view = new WebNotificationView(tray_, notification);
    content_->AddChildView(view);
    content_->SizeToPreferredSize();
    Layout();
    if (GetWidget())
      GetWidget()->GetRootView()->SchedulePaint();
  }

 private:
  views::View* content_;

  DISALLOW_COPY_AND_ASSIGN(PopupBubbleContentsView);
};

}  // namespace internal

using internal::TrayBubbleView;
using internal::WebNotificationList;
using internal::WebContentsView;

// Web notification bubbles ----------------------------------------------------

class WebNotificationTray::Bubble : public TrayBubbleView::Host,
                                    public views::WidgetObserver {
 public:
  explicit Bubble(WebNotificationTray* tray)
      : tray_(tray),
        bubble_view_(NULL),
        bubble_widget_(NULL),
        ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)) {
  }

  void Initialize(WebContentsView* contents_view) {
    DCHECK(bubble_view_);

    bubble_view_->AddChildView(contents_view);

    bubble_widget_ = views::BubbleDelegateView::CreateBubble(bubble_view_);
    bubble_widget_->AddObserver(this);

    InitializeAndShowBubble(bubble_widget_, bubble_view_, tray_);
    UpdateBubbleView();
  }

  virtual ~Bubble() {
    if (bubble_view_)
      bubble_view_->reset_host();
    if (bubble_widget_) {
      bubble_widget_->RemoveObserver(this);
      bubble_widget_->Close();
    }
  }

  void ScheduleUpdate() {
    weak_ptr_factory_.InvalidateWeakPtrs();  // Cancel any pending update.
    MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(&WebNotificationTray::Bubble::UpdateBubbleView,
                   weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(kUpdateDelayMs));
  }

  // Updates the bubble; implementation dependent.
  virtual void UpdateBubbleView() = 0;

  bool IsVisible() const {
    return bubble_widget_ && bubble_widget_->IsVisible();
  }

  views::Widget* bubble_widget() const { return bubble_widget_; }
  TrayBubbleView* bubble_view() const { return bubble_view_; }

  // Overridden from TrayBubbleView::Host.
  virtual void BubbleViewDestroyed() OVERRIDE {
    bubble_view_ = NULL;
  }

  virtual void OnMouseEnteredView() OVERRIDE {
    tray_->UpdateShouldShowLauncher();
  }

  virtual void OnMouseExitedView() OVERRIDE {
    tray_->UpdateShouldShowLauncher();
  }

  virtual void OnClickedOutsideView() OVERRIDE {
    // May delete |this|.
    tray_->HideMessageCenterBubble();
  }

  virtual string16 GetAccessibleName() OVERRIDE {
    return tray_->GetAccessibleName();
  }

  // Overridden from views::WidgetObserver:
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE {
    CHECK_EQ(bubble_widget_, widget);
    bubble_widget_ = NULL;
    tray_->HideBubble(this);  // Will destroy |this|.
  }

 protected:
  TrayBubbleView::InitParams GetInitParams() {
    TrayBubbleView::InitParams init_params(TrayBubbleView::ANCHOR_TYPE_TRAY,
                                           tray_->shelf_alignment());
    init_params.bubble_width = kWebNotificationWidth;
    if (tray_->shelf_alignment() == SHELF_ALIGNMENT_BOTTOM) {
      views::View* anchor = tray_->tray_container();
      gfx::Point bounds(anchor->width() / 2, 0);
      ConvertPointToWidget(anchor, &bounds);
      init_params.arrow_offset = bounds.x();
    }
    return init_params;
  }

 protected:
  WebNotificationTray* tray_;
  TrayBubbleView* bubble_view_;
  views::Widget* bubble_widget_;
  base::WeakPtrFactory<Bubble> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Bubble);
};

// Bubble for message center.
class WebNotificationTray::MessageCenterBubble :
      public WebNotificationTray::Bubble {
 public:
  explicit MessageCenterBubble(WebNotificationTray* tray) :
      WebNotificationTray::Bubble(tray),
      contents_view_(NULL) {
    TrayBubbleView::InitParams init_params = GetInitParams();
    init_params.max_height = kWebNotificationBubbleMaxHeight;
    init_params.can_activate = true;
    views::View* anchor = tray_->tray_container();
    bubble_view_ = TrayBubbleView::Create(anchor, this, init_params);
    contents_view_ = new internal::MessageCenterContentsView(tray);

    Initialize(contents_view_);
  }

  virtual ~MessageCenterBubble() {}

  // Overridden from TrayBubbleView::Host.
  virtual void BubbleViewDestroyed() OVERRIDE {
    contents_view_ = NULL;
    WebNotificationTray::Bubble::BubbleViewDestroyed();
  }

 private:
  // Overridden from Bubble.
  virtual void UpdateBubbleView() OVERRIDE {
    contents_view_->Update(tray_->notification_list()->notifications());
    bubble_view_->Show();
    bubble_view_->UpdateBubble();
  }

  internal::MessageCenterContentsView* contents_view_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterBubble);
};

// Bubble for popup notifications.
class WebNotificationTray::PopupBubble : public WebNotificationTray::Bubble {
 public:
  explicit PopupBubble(WebNotificationTray* tray) :
      WebNotificationTray::Bubble(tray),
      contents_view_(NULL) {
    TrayBubbleView::InitParams init_params = GetInitParams();
    init_params.arrow_color = kBackgroundColor;
    init_params.close_on_deactivate = false;
    views::View* anchor = tray_->tray_container();
    bubble_view_ = TrayBubbleView::Create(anchor, this, init_params);
    contents_view_ = new internal::PopupBubbleContentsView(tray);

    Initialize(contents_view_);
  }

  virtual ~PopupBubble() {}

  // Overridden from TrayBubbleView::Host.
  virtual void BubbleViewDestroyed() OVERRIDE {
    contents_view_ = NULL;
    WebNotificationTray::Bubble::BubbleViewDestroyed();
  }

  virtual void OnMouseEnteredView() OVERRIDE {
    StopAutoCloseTimer();
    WebNotificationTray::Bubble::OnMouseEnteredView();
  }

  virtual void OnMouseExitedView() OVERRIDE {
    StartAutoCloseTimer();
    WebNotificationTray::Bubble::OnMouseExitedView();
  }

 private:
  // Overridden from Bubble.
  virtual void UpdateBubbleView() OVERRIDE {
    const WebNotificationList::Notifications& notifications =
        tray_->notification_list()->notifications();
    contents_view_->Update(notifications);
    bubble_view_->Show();
    bubble_view_->UpdateBubble();
    StartAutoCloseTimer();
  }

  void StartAutoCloseTimer() {
    autoclose_.Start(FROM_HERE,
                     base::TimeDelta::FromSeconds(kAutocloseDelaySeconds),
                     this, &PopupBubble::OnAutoClose);
  }

  void StopAutoCloseTimer() {
    autoclose_.Stop();
  }

  void OnAutoClose() {
    tray_->HideBubble(this);  // deletes |this|!
  }

  base::OneShotTimer<PopupBubble> autoclose_;
  internal::PopupBubbleContentsView* contents_view_;

  DISALLOW_COPY_AND_ASSIGN(PopupBubble);
};

// WebNotificationTray ---------------------------------------------------------

WebNotificationTray::WebNotificationTray(
    internal::StatusAreaWidget* status_area_widget)
    : internal::TrayBackgroundView(status_area_widget),
      notification_list_(new WebNotificationList()),
      count_label_(NULL),
      delegate_(NULL),
      show_message_center_on_unlock_(false) {
  count_label_ = new views::Label(UTF8ToUTF16("0"));
  internal::SetupLabelForTray(count_label_);
  gfx::Font font = count_label_->font();
  count_label_->SetFont(font.DeriveFont(0, font.GetStyle() & ~gfx::Font::BOLD));
  count_label_->SetHorizontalAlignment(views::Label::ALIGN_CENTER);
  count_label_->SetEnabledColor(kMessageCountColor);

  tray_container()->set_size(gfx::Size(kTrayWidth, kTrayHeight));
  tray_container()->AddChildView(count_label_);

  UpdateTray();
}

WebNotificationTray::~WebNotificationTray() {
  // Release any child views that might have back pointers before ~View().
  notification_list_.reset();
  message_center_bubble_.reset();
  popup_bubble_.reset();
}

void WebNotificationTray::SetDelegate(Delegate* delegate) {
  DCHECK(!delegate_);
  delegate_ = delegate;
}

// Add/Update/RemoveNotification are called by the client code, i.e the
// Delegate implementation or its proxy.

void WebNotificationTray::AddNotification(const std::string& id,
                                          const string16& title,
                                          const string16& message,
                                          const string16& display_source,
                                          const std::string& extension_id) {
  notification_list_->AddNotification(
      id, title, message, display_source, extension_id);
  UpdateTrayAndBubble();
  ShowPopupBubble();
}

void WebNotificationTray::UpdateNotification(const std::string& old_id,
                                             const std::string& new_id,
                                             const string16& title,
                                             const string16& message) {
  notification_list_->UpdateNotificationMessage(old_id, new_id, title, message);
  UpdateTrayAndBubble();
  ShowPopupBubble();
}

void WebNotificationTray::RemoveNotification(const std::string& id) {
  if (id == notification_list_->GetFirstId())
    HidePopupBubble();
  if (!notification_list_->RemoveNotification(id))
    return;
  UpdateTrayAndBubble();
}

void WebNotificationTray::SetNotificationImage(const std::string& id,
                                               const gfx::ImageSkia& image) {
  if (!notification_list_->SetNotificationImage(id, image))
    return;
  UpdateTrayAndBubble();
  if (popup_bubble() && id == notification_list_->GetFirstId())
    ShowPopupBubble();
}

void WebNotificationTray::ShowMessageCenterBubble() {
  if (status_area_widget()->login_status() == user::LOGGED_IN_LOCKED)
    return;
  if (message_center_bubble()) {
    UpdateTray();
    return;
  }
  // Indicate that the message center is visible. Clears the unread count.
  notification_list_->SetMessageCenterVisible(true);
  UpdateTray();
  HidePopupBubble();
  message_center_bubble_.reset(new MessageCenterBubble(this));
  status_area_widget()->SetHideSystemNotifications(true);
  UpdateShouldShowLauncher();
}

void WebNotificationTray::HideMessageCenterBubble() {
  if (!message_center_bubble())
    return;
  message_center_bubble_.reset();
  show_message_center_on_unlock_ = false;
  notification_list_->SetMessageCenterVisible(false);
  status_area_widget()->SetHideSystemNotifications(false);
  UpdateShouldShowLauncher();
}

void WebNotificationTray::HideNotificationBubble() {
  HidePopupBubble();
}

void WebNotificationTray::ShowPopupBubble() {
  if (status_area_widget()->login_status() == user::LOGGED_IN_LOCKED)
    return;
  if (message_center_bubble())
    return;
  if (!status_area_widget()->ShouldShowNonSystemNotifications())
    return;
  UpdateTray();
  if (popup_bubble()) {
    popup_bubble()->ScheduleUpdate();
  } else {
    popup_bubble_.reset(new PopupBubble(this));
  }
}

void WebNotificationTray::HidePopupBubble() {
  popup_bubble_.reset();
}

void WebNotificationTray::UpdateAfterLoginStatusChange(
    user::LoginStatus login_status) {
  if (login_status == user::LOGGED_IN_LOCKED) {
    if (message_center_bubble()) {
      message_center_bubble_.reset();
      show_message_center_on_unlock_ = true;
    }
    HidePopupBubble();
  } else {
    if (show_message_center_on_unlock_)
      ShowMessageCenterBubble();
    show_message_center_on_unlock_ = false;
  }
  UpdateTray();
}

bool WebNotificationTray::IsMessageCenterBubbleVisible() const {
  return (message_center_bubble() && message_center_bubble_->IsVisible());
}

bool WebNotificationTray::IsMouseInNotificationBubble() const {
  if (!popup_bubble())
    return false;
  return popup_bubble_->bubble_view()->GetBoundsInScreen().Contains(
      gfx::Screen::GetCursorScreenPoint());
}

void WebNotificationTray::SetShelfAlignment(ShelfAlignment alignment) {
  if (alignment == shelf_alignment())
    return;
  internal::TrayBackgroundView::SetShelfAlignment(alignment);
  if (alignment == SHELF_ALIGNMENT_BOTTOM)
    tray_container()->set_size(gfx::Size(kTrayWidth, kTrayHeight));
  else
    tray_container()->set_size(gfx::Size(kTraySideWidth, kTraySideHeight));
  // Destroy any existing bubble so that it will be rebuilt correctly.
  HideMessageCenterBubble();
  HidePopupBubble();
}

void WebNotificationTray::AnchorUpdated() {
  if (popup_bubble_.get()) {
    popup_bubble_->bubble_view()->UpdateBubble();
    // Ensure that the notification buble is above the launcher/status area.
    popup_bubble_->bubble_view()->GetWidget()->StackAtTop();
  }
  if (message_center_bubble_.get())
    message_center_bubble_->bubble_view()->UpdateBubble();
}

string16 WebNotificationTray::GetAccessibleName() {
  return l10n_util::GetStringUTF16(
      IDS_ASH_WEB_NOTIFICATION_TRAY_ACCESSIBLE_NAME);
}

// Private methods invoked by Bubble and its child classes

void WebNotificationTray::SendRemoveNotification(const std::string& id) {
  // If this is the only notification in the list, close the bubble.
  if (notification_list_->notifications().size() == 1 &&
      id == notification_list_->GetFirstId()) {
    HideMessageCenterBubble();
  }
  if (delegate_)
    delegate_->NotificationRemoved(id);
}

void WebNotificationTray::SendRemoveAllNotifications() {
  HideMessageCenterBubble();
  if (delegate_) {
    const WebNotificationList::Notifications& notifications =
        notification_list_->notifications();
    for (WebNotificationList::Notifications::const_iterator loopiter =
             notifications.begin();
         loopiter != notifications.end(); ) {
      WebNotificationList::Notifications::const_iterator curiter = loopiter++;
      std::string notification_id = curiter->id;
      // May call RemoveNotification and erase curiter.
      delegate_->NotificationRemoved(notification_id);
    }
  }
}

// When we disable notifications, we remove any existing matching
// notifications to avoid adding complicated UI to re-enable the source.
void WebNotificationTray::DisableByExtension(const std::string& id) {
  if (delegate_)
    delegate_->DisableExtension(id);
  // Will call SendRemoveNotification for each matching notification.
  notification_list_->SendRemoveNotificationsByExtension(this, id);
}

void WebNotificationTray::DisableByUrl(const std::string& id) {
  if (delegate_)
    delegate_->DisableNotificationsFromSource(id);
  // Will call SendRemoveNotification for each matching notification.
  notification_list_->SendRemoveNotificationsBySource(this, id);
}

bool WebNotificationTray::PerformAction(const ui::Event& event) {
  if (message_center_bubble())
    HideMessageCenterBubble();
  else
    ShowMessageCenterBubble();
  return true;
}

void WebNotificationTray::ShowSettings(const std::string& id) {
  if (delegate_)
    delegate_->ShowSettings(id);
}

void WebNotificationTray::OnClicked(const std::string& id) {
  if (delegate_)
    delegate_->OnClicked(id);
}

// Other private methods

void WebNotificationTray::UpdateTray() {
  count_label_->SetText(UTF8ToUTF16(
      GetNotificationText(notification_list()->unread_count())));
  bool is_visible =
      (status_area_widget()->login_status() != user::LOGGED_IN_NONE) &&
      (status_area_widget()->login_status() != user::LOGGED_IN_LOCKED) &&
      (!notification_list()->notifications().empty());
  SetVisible(is_visible);
  Layout();
  SchedulePaint();
}

void WebNotificationTray::UpdateTrayAndBubble() {
  UpdateTray();

  if (message_center_bubble())
    message_center_bubble()->ScheduleUpdate();

  if (popup_bubble()) {
    if (notification_list_->notifications().size() == 0)
      HidePopupBubble();
    else
      popup_bubble()->ScheduleUpdate();
  }
}

void WebNotificationTray::HideBubble(Bubble* bubble) {
  if (bubble == message_center_bubble()) {
    HideMessageCenterBubble();
  } else if (bubble == popup_bubble()) {
    HidePopupBubble();
  }
}

// Methods for testing

size_t WebNotificationTray::GetNotificationCountForTest() const {
  return notification_list_->notifications().size();
}

bool WebNotificationTray::HasNotificationForTest(const std::string& id) const {
  return notification_list_->HasNotification(id);
}

}  // namespace ash
