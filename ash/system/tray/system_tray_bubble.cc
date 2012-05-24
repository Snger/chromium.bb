// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray_bubble.h"

#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/system_tray.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/wm/shelf_layout_manager.h"
#include "ash/wm/window_animations.h"
#include "base/message_loop.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/effects/SkBlurImageFilter.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/screen.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

const int kShadowThickness = 4;

const int kBottomLineHeight = 1;

const int kSystemTrayBubbleHorizontalInset = 1;
const int kSystemTrayBubbleVerticalInset = 1;

const int kArrowHeight = 10;
const int kArrowWidth = 20;
const int kArrowPaddingFromRight = 20;
const int kArrowPaddingFromBottom = 17;
const int kMinArrowOffset = 12;

const int kAnimationDurationForPopupMS = 200;

// Normally a detailed view is the same size as the default view. However,
// when showing a detailed view directly (e.g. clicking on a notification),
// we may not know the height of the default view, or the default view may
// be too short, so we use this as a default and minimum height for any
// detailed view.
const int kDetailedBubbleMaxHeight = kTrayPopupItemHeight * 5;

const SkColor kShadowColor = SkColorSetARGB(0xff, 0, 0, 0);

void DrawBlurredShadowAroundView(gfx::Canvas* canvas,
                                 int top,
                                 int bottom,
                                 int width,
                                 const gfx::Insets& inset) {
  SkPath path;
  path.incReserve(4);
  path.moveTo(SkIntToScalar(inset.left() + kShadowThickness),
              SkIntToScalar(top + kShadowThickness + 1));
  path.lineTo(SkIntToScalar(inset.left() + kShadowThickness),
              SkIntToScalar(bottom));
  path.lineTo(SkIntToScalar(width),
              SkIntToScalar(bottom));
  path.lineTo(SkIntToScalar(width),
              SkIntToScalar(top + kShadowThickness + 1));

  SkPaint paint;
  paint.setColor(kShadowColor);
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  paint.setStrokeWidth(SkIntToScalar(3));
  paint.setImageFilter(new SkBlurImageFilter(
      SkIntToScalar(3), SkIntToScalar(3)))->unref();
  canvas->sk_canvas()->drawPath(path, paint);
}

// A view with some special behaviour for tray items in the popup:
// - changes background color on hover.
class TrayPopupItemContainer : public views::View {
 public:
  explicit TrayPopupItemContainer(views::View* view) : hover_(false) {
    set_notify_enter_exit_on_child(true);
    set_border(view->border() ? views::Border::CreateEmptyBorder(0, 0, 0, 0) :
        views::Border::CreateSolidSidedBorder(1, 1, 0, 1, kBorderDarkColor));
    views::BoxLayout* layout = new views::BoxLayout(
        views::BoxLayout::kVertical, 0, 0, 0);
    layout->set_spread_blank_space(true);
    SetLayoutManager(layout);
    SetPaintToLayer(view->layer() != NULL);
    if (view->layer())
      SetFillsBoundsOpaquely(view->layer()->fills_bounds_opaquely());
    AddChildView(view);
    SetVisible(view->visible());
  }

  virtual ~TrayPopupItemContainer() {}

 private:
  // Overridden from views::View.
  virtual void ChildVisibilityChanged(View* child) OVERRIDE {
    if (visible() == child->visible())
      return;
    SetVisible(child->visible());
    PreferredSizeChanged();
  }

  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE {
    PreferredSizeChanged();
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
    if (child_count() == 0)
      return;

    views::View* view = child_at(0);
    if (!view->background()) {
      canvas->FillRect(gfx::Rect(size()),
          hover_ ? kHoverBackgroundColor : kBackgroundColor);
    }
  }

  bool hover_;

  DISALLOW_COPY_AND_ASSIGN(TrayPopupItemContainer);
};

class SystemTrayBubbleBorder : public views::BubbleBorder {
 public:
  SystemTrayBubbleBorder(views::View* owner,
                         views::BubbleBorder::ArrowLocation arrow_location,
                         int arrow_offset)
      : views::BubbleBorder(arrow_location,
                            views::BubbleBorder::NO_SHADOW),
        owner_(owner),
        arrow_offset_(std::max(arrow_offset, kMinArrowOffset)) {
    set_alignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  }

  virtual ~SystemTrayBubbleBorder() {}

 private:
  // Overridden from views::BubbleBorder.
  // Override views::BubbleBorder to set the bubble on top of the anchor when
  // it has no arrow.
  virtual gfx::Rect GetBounds(const gfx::Rect& position_relative_to,
                              const gfx::Size& contents_size) const OVERRIDE {
    if (arrow_location() != NONE) {
      return views::BubbleBorder::GetBounds(position_relative_to,
                                            contents_size);
    }

    gfx::Size border_size(contents_size);
    gfx::Insets insets;
    GetInsets(&insets);
    border_size.Enlarge(insets.width(), insets.height());

    const int kArrowOverlap = 3;
    int x = position_relative_to.x() +
            position_relative_to.width() / 2 - border_size.width() / 2;
    // Position the bubble on top of the anchor.
    int y = position_relative_to.y() +
            kArrowOverlap - border_size.height();
    return gfx::Rect(x, y, border_size.width(), border_size.height());
  }

  // Overridden from views::Border.
  virtual void Paint(const views::View& view,
                     gfx::Canvas* canvas) const OVERRIDE {
    gfx::Insets inset;
    GetInsets(&inset);
    DrawBlurredShadowAroundView(canvas, 0, owner_->height(), owner_->width(),
        inset);

    // Draw the bottom line.
    int y = owner_->height() + 1;
    canvas->FillRect(gfx::Rect(inset.left(), y, owner_->width(),
                               kBottomLineHeight), kBorderDarkColor);

    if (!Shell::GetInstance()->shelf()->IsVisible() ||
        arrow_location() == views::BubbleBorder::NONE)
      return;

    // Draw the arrow after drawing child borders, so that the arrow can cover
    // the its overlap section with child border.
    SkPath path;
    path.incReserve(4);
    if (arrow_location() == views::BubbleBorder::BOTTOM_RIGHT) {
      int tip_x = base::i18n::IsRTL() ? arrow_offset_ :
          owner_->width() - arrow_offset_;
      if (tip_x < kArrowPaddingFromRight + kArrowWidth / 2)
        tip_x = kArrowPaddingFromRight + kArrowWidth / 2;
      if (tip_x > owner_->width() - kArrowPaddingFromRight - kArrowWidth / 2)
        tip_x = owner_->width() - kArrowPaddingFromRight - kArrowWidth / 2;
      int left_base_x = tip_x - kArrowWidth / 2;
      int left_base_y = y;
      int tip_y = left_base_y + kArrowHeight;
      path.moveTo(SkIntToScalar(left_base_x), SkIntToScalar(left_base_y));
      path.lineTo(SkIntToScalar(tip_x), SkIntToScalar(tip_y));
      path.lineTo(SkIntToScalar(left_base_x + kArrowWidth),
                  SkIntToScalar(left_base_y));
    } else if (arrow_location() == views::BubbleBorder::LEFT_BOTTOM) {
      int tip_y = y - arrow_offset_;
      int top_base_y = tip_y - kArrowWidth / 2;
      int top_base_x = inset.left() + kSystemTrayBubbleHorizontalInset;
      int tip_x = top_base_x - kArrowHeight;
      path.moveTo(SkIntToScalar(top_base_x), SkIntToScalar(top_base_y));
      path.lineTo(SkIntToScalar(tip_x), SkIntToScalar(tip_y));
      path.lineTo(SkIntToScalar(top_base_x),
                  SkIntToScalar(top_base_y + kArrowWidth));
    } else if (arrow_location() == views::BubbleBorder::RIGHT_BOTTOM){
      int tip_y = y - arrow_offset_;
      int top_base_y = tip_y - kArrowWidth / 2;
      int top_base_x = inset.left() + owner_->width() -
                       kSystemTrayBubbleHorizontalInset;
      int tip_x = top_base_x + kArrowHeight;
      path.moveTo(SkIntToScalar(top_base_x), SkIntToScalar(top_base_y));
      path.lineTo(SkIntToScalar(tip_x), SkIntToScalar(tip_y));
      path.lineTo(SkIntToScalar(top_base_x),
                  SkIntToScalar(top_base_y + kArrowWidth));
    }

    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(kHeaderBackgroundColorDark);
    canvas->DrawPath(path, paint);

    // Now draw the arrow border.
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(kBorderDarkColor);
    canvas->DrawPath(path, paint);

  }

  views::View* owner_;
  const int arrow_offset_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubbleBorder);
};

// Implicit animation observer that deletes itself and the layer at the end of
// the animation.
class AnimationObserverDeleteLayer : public ui::ImplicitAnimationObserver {
 public:
  explicit AnimationObserverDeleteLayer(ui::Layer* layer)
      : layer_(layer) {
  }

  virtual ~AnimationObserverDeleteLayer() {
  }

  virtual void OnImplicitAnimationsCompleted() OVERRIDE {
    MessageLoopForUI::current()->DeleteSoon(FROM_HERE, this);
  }

 private:
  scoped_ptr<ui::Layer> layer_;

  DISALLOW_COPY_AND_ASSIGN(AnimationObserverDeleteLayer);
};

}  // namespace

namespace internal {

// SystemTrayBubbleView

SystemTrayBubbleView::SystemTrayBubbleView(
    views::View* anchor,
    views::BubbleBorder::ArrowLocation arrow_location,
    SystemTrayBubble* host,
    bool can_activate)
    : views::BubbleDelegateView(anchor, arrow_location),
      host_(host),
      can_activate_(can_activate),
      max_height_(0) {
  set_margin(0);
  set_parent_window(ash::Shell::GetInstance()->GetContainer(
      ash::internal::kShellWindowId_SettingBubbleContainer));
  set_notify_enter_exit_on_child(true);
  SetPaintToLayer(true);
  SetFillsBoundsOpaquely(true);
}

SystemTrayBubbleView::~SystemTrayBubbleView() {
  // Inform host items (models) that their views are being destroyed.
  if (host_)
    host_->DestroyItemViews();
}

void SystemTrayBubbleView::SetBubbleBorder(views::BubbleBorder* border) {
  GetBubbleFrameView()->SetBubbleBorder(border);
}

void SystemTrayBubbleView::UpdateAnchor() {
  SizeToContents();
  GetWidget()->GetRootView()->SchedulePaint();
}

void SystemTrayBubbleView::Init() {
  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
  layout->set_spread_blank_space(true);
  SetLayoutManager(layout);
  set_background(NULL);
}

gfx::Rect SystemTrayBubbleView::GetAnchorRect() {
  gfx::Rect rect;
  if (host_)
    rect = host_->GetAnchorRect();
  // TODO(jennyz): May need to add left/right alignment in the following code.
  if (rect.IsEmpty()) {
    rect = gfx::Screen::GetPrimaryMonitor().bounds();
    rect = gfx::Rect(
        base::i18n::IsRTL() ? kPaddingFromRightEdgeOfScreenBottomAlignment :
            rect.width() - kPaddingFromRightEdgeOfScreenBottomAlignment,
        rect.height() - kPaddingFromBottomOfScreenBottomAlignment,
        0, 0);
  }
  return rect;
}

bool SystemTrayBubbleView::CanActivate() const {
  return can_activate_;
}

gfx::Size SystemTrayBubbleView::GetPreferredSize() {
  gfx::Size size = views::BubbleDelegateView::GetPreferredSize();
  int height = size.height();
  if (max_height_ != 0 && height > max_height_)
    height = max_height_;
  return gfx::Size(kTrayPopupWidth, height);
}

void SystemTrayBubbleView::OnMouseEntered(const views::MouseEvent& event) {
  if (host_)
    host_->StopAutoCloseTimer();
}

void SystemTrayBubbleView::OnMouseExited(const views::MouseEvent& event) {
  if (host_)
    host_->RestartAutoCloseTimer();
}

void SystemTrayBubbleView::GetAccessibleState(ui::AccessibleViewState* state) {
  if (can_activate_) {
    state->role = ui::AccessibilityTypes::ROLE_WINDOW;
    state->name = l10n_util::GetStringUTF16(
        IDS_ASH_STATUS_TRAY_ACCESSIBLE_NAME);
  }
}

void SystemTrayBubbleView::ChildPreferredSizeChanged(View* child) {
  SizeToContents();
}

void SystemTrayBubbleView::ViewHierarchyChanged(bool is_add,
                                                views::View* parent,
                                                views::View* child) {
  if (is_add && child == this) {
    parent->SetPaintToLayer(true);
    parent->SetFillsBoundsOpaquely(true);
    parent->layer()->SetMasksToBounds(true);
  }
}

// SystemTrayBubble::InitParams
SystemTrayBubble::InitParams::InitParams(
    SystemTrayBubble::AnchorType anchor_type,
    ShelfAlignment shelf_alignment)
    : anchor(NULL),
      anchor_type(anchor_type),
      can_activate(false),
      login_status(ash::user::LOGGED_IN_NONE),
      arrow_offset(
          (shelf_alignment == SHELF_ALIGNMENT_BOTTOM ?
               kArrowPaddingFromRight : kArrowPaddingFromBottom)
          + kArrowWidth / 2),
      max_height(0) {
}

// SystemTrayBubble

SystemTrayBubble::SystemTrayBubble(
    ash::SystemTray* tray,
    const std::vector<ash::SystemTrayItem*>& items,
    BubbleType bubble_type)
    : tray_(tray),
      bubble_view_(NULL),
      bubble_widget_(NULL),
      items_(items),
      bubble_type_(bubble_type),
      anchor_type_(ANCHOR_TYPE_TRAY),
      autoclose_delay_(0) {
}

SystemTrayBubble::~SystemTrayBubble() {
  // The bubble may be closing without having been hidden first. So it may still
  // be in the message-loop's observer list.
  MessageLoopForUI::current()->RemoveObserver(this);

  DestroyItemViews();
  // Reset the host pointer in bubble_view_ in case its destruction is deferred.
  if (bubble_view_)
    bubble_view_->reset_host();
  if (bubble_widget_) {
    bubble_widget_->RemoveObserver(this);
    // This triggers the destruction of bubble_view_.
    bubble_widget_->Close();
  }
}

void SystemTrayBubble::UpdateView(
    const std::vector<ash::SystemTrayItem*>& items,
    BubbleType bubble_type) {
  DCHECK(bubble_type != BUBBLE_TYPE_NOTIFICATION);
  DCHECK(bubble_type != bubble_type_);

  const int kSwipeDelayMS = 300;
  base::TimeDelta swipe_duration =
      base::TimeDelta::FromMilliseconds(kSwipeDelayMS);
  ui::Layer* layer = bubble_view_->RecreateLayer();
  layer->SuppressPaint();

  // When transitioning from detailed view to default view, animate the existing
  // view (slide out towards the right).
  if (bubble_type == BUBBLE_TYPE_DEFAULT) {
    // Make sure the old view is visibile over the new view during the
    // animation.
    layer->parent()->StackAbove(layer, bubble_view_->layer());
    ui::ScopedLayerAnimationSettings settings(layer->GetAnimator());
    settings.AddObserver(new AnimationObserverDeleteLayer(layer));
    settings.SetTransitionDuration(swipe_duration);
    settings.SetTweenType(ui::Tween::EASE_IN);
    ui::Transform transform;
    transform.SetTranslateX(layer->bounds().width());
    layer->SetTransform(transform);
  }

  {
    // Add a shadow layer to make the old layer darker as the animation
    // progresses.
    ui::Layer* shadow = new ui::Layer(ui::LAYER_SOLID_COLOR);
    shadow->SetColor(SK_ColorBLACK);
    shadow->SetOpacity(0.01f);
    shadow->SetBounds(layer->bounds());
    layer->Add(shadow);
    layer->StackAtTop(shadow);
    {
      // Animate the darkening effect a little longer than the swipe-in. This is
      // to make sure the darkening animation does not end up finishing early,
      // because the dark layer goes away at the end of the animation, and there
      // is a brief moment when the old view is still visible, but it does not
      // have the shadow layer on top.
      ui::ScopedLayerAnimationSettings settings(shadow->GetAnimator());
      settings.AddObserver(new AnimationObserverDeleteLayer(shadow));
      settings.SetTransitionDuration(swipe_duration +
                                     base::TimeDelta::FromMilliseconds(150));
      settings.SetTweenType(ui::Tween::LINEAR);
      shadow->SetOpacity(0.15f);
    }
  }

  DestroyItemViews();
  bubble_view_->RemoveAllChildViews(true);

  items_ = items;
  bubble_type_ = bubble_type;
  CreateItemViews(Shell::GetInstance()->tray_delegate()->GetUserLoginStatus());
  bubble_widget_->GetContentsView()->Layout();
  // Make sure that the bubble is large enough for the default view.
  if (bubble_type_ == BUBBLE_TYPE_DEFAULT) {
    bubble_view_->set_max_height(0);  // Clear max height limit.
    bubble_view_->SizeToContents();
  }

  // When transitioning from default view to detailed view, animate the new
  // view (slide in from the right).
  if (bubble_type == BUBBLE_TYPE_DETAILED) {
    ui::Layer* new_layer = bubble_view_->layer();
    gfx::Rect bounds = new_layer->bounds();
    ui::Transform transform;
    transform.SetTranslateX(bounds.width());
    new_layer->SetTransform(transform);
    {
      ui::ScopedLayerAnimationSettings settings(new_layer->GetAnimator());
      settings.AddObserver(new AnimationObserverDeleteLayer(layer));
      settings.SetTransitionDuration(swipe_duration);
      settings.SetTweenType(ui::Tween::EASE_IN);
      new_layer->SetTransform(ui::Transform());
    }
  }
}

void SystemTrayBubble::InitView(const InitParams& init_params) {
  DCHECK(bubble_view_ == NULL);
  anchor_type_ = init_params.anchor_type;
  views::BubbleBorder::ArrowLocation arrow_location;
  if (anchor_type_ == ANCHOR_TYPE_TRAY) {
    if (tray_->shelf_alignment() == SHELF_ALIGNMENT_BOTTOM) {
      arrow_location = views::BubbleBorder::BOTTOM_RIGHT;
    } else if (tray_->shelf_alignment() == SHELF_ALIGNMENT_LEFT) {
      arrow_location = views::BubbleBorder::LEFT_BOTTOM;
    } else {
      arrow_location = views::BubbleBorder::RIGHT_BOTTOM;
    }
  } else {
    arrow_location = views::BubbleBorder::NONE;
  }
  bubble_view_ = new SystemTrayBubbleView(
      init_params.anchor, arrow_location, this, init_params.can_activate);
  if (bubble_type_ == BUBBLE_TYPE_NOTIFICATION)
    bubble_view_->set_close_on_deactivate(false);
  int max_height = init_params.max_height;
  if (bubble_type_ == BUBBLE_TYPE_DETAILED &&
      max_height < kDetailedBubbleMaxHeight)
    max_height = kDetailedBubbleMaxHeight;
  bubble_view_->set_max_height(max_height);

  CreateItemViews(init_params.login_status);

  DCHECK(bubble_widget_ == NULL);
  bubble_widget_ = views::BubbleDelegateView::CreateBubble(bubble_view_);

  // Must occur after call to CreateBubble()
  bubble_view_->SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  bubble_widget_->non_client_view()->frame_view()->set_background(NULL);
  SystemTrayBubbleBorder* bubble_border = new SystemTrayBubbleBorder(
      bubble_view_, arrow_location, init_params.arrow_offset);
  bubble_view_->SetBubbleBorder(bubble_border);
  // Recalculate with new border.
  bubble_view_->SizeToContents();

  bubble_widget_->AddObserver(this);

  // Setup animation.
  ash::SetWindowVisibilityAnimationType(
      bubble_widget_->GetNativeWindow(),
      ash::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  ash::SetWindowVisibilityAnimationTransition(
      bubble_widget_->GetNativeWindow(),
      ash::ANIMATE_BOTH);
  ash::SetWindowVisibilityAnimationDuration(
      bubble_widget_->GetNativeWindow(),
      base::TimeDelta::FromMilliseconds(kAnimationDurationForPopupMS));

  bubble_view_->Show();
}

gfx::Rect SystemTrayBubble::GetAnchorRect() const {
  gfx::Rect rect;
  views::Widget* widget = bubble_view()->anchor_widget();
  if (widget->IsVisible()) {
    rect = widget->GetWindowScreenBounds();
    if (anchor_type_ == ANCHOR_TYPE_TRAY) {
      if (tray_->shelf_alignment() == SHELF_ALIGNMENT_BOTTOM) {
        rect.Inset(
            base::i18n::IsRTL() ?
                kPaddingFromRightEdgeOfScreenBottomAlignment : 0,
            0,
            base::i18n::IsRTL() ?
                0 : kPaddingFromRightEdgeOfScreenBottomAlignment,
            kPaddingFromBottomOfScreenBottomAlignment);
      } else if (tray_->shelf_alignment() == SHELF_ALIGNMENT_LEFT) {
        rect.Inset(0, 0, kPaddingFromLeftEdgeOfScreenLeftAlignment,
                   kPaddingFromBottomOfScreenVerticalAlignment);
      } else {
        rect.Inset(-kPaddingFromRightEdgeOfScreenRightAlignment,
                   0, 0, kPaddingFromBottomOfScreenVerticalAlignment);
      }
    } else if (anchor_type_ == ANCHOR_TYPE_BUBBLE) {
      // For notification bubble to be achored with uber tray bubble,
      // the anchor can include arrow on left or right, which should
      // be deducted out from the anchor rect.
      views::View* anchor_view = bubble_view()->anchor_view();
      rect = anchor_view->GetScreenBounds();
      gfx::Insets insets = anchor_view->GetInsets();
      rect.Inset(insets);
    }
  }
  return rect;
}

void SystemTrayBubble::DestroyItemViews() {
  for (std::vector<ash::SystemTrayItem*>::iterator it = items_.begin();
       it != items_.end();
       ++it) {
    switch (bubble_type_) {
      case BUBBLE_TYPE_DEFAULT:
        (*it)->DestroyDefaultView();
        break;
      case BUBBLE_TYPE_DETAILED:
        (*it)->DestroyDetailedView();
        break;
      case BUBBLE_TYPE_NOTIFICATION:
        (*it)->DestroyNotificationView();
        break;
    }
  }
}

void SystemTrayBubble::StartAutoCloseTimer(int seconds) {
  autoclose_.Stop();
  autoclose_delay_ = seconds;
  if (autoclose_delay_) {
    autoclose_.Start(FROM_HERE,
                     base::TimeDelta::FromSeconds(autoclose_delay_),
                     this, &SystemTrayBubble::Close);
  }
}

void SystemTrayBubble::StopAutoCloseTimer() {
  autoclose_.Stop();
}

void SystemTrayBubble::RestartAutoCloseTimer() {
  if (autoclose_delay_)
    StartAutoCloseTimer(autoclose_delay_);
}

void SystemTrayBubble::Close() {
  if (bubble_widget_)
    bubble_widget_->Close();
}

void SystemTrayBubble::CreateItemViews(user::LoginStatus login_status) {
  for (std::vector<ash::SystemTrayItem*>::iterator it = items_.begin();
       it != items_.end();
       ++it) {
    views::View* view = NULL;
    switch (bubble_type_) {
      case BUBBLE_TYPE_DEFAULT:
        view = (*it)->CreateDefaultView(login_status);
        break;
      case BUBBLE_TYPE_DETAILED:
        view = (*it)->CreateDetailedView(login_status);
        break;
      case BUBBLE_TYPE_NOTIFICATION:
        view = (*it)->CreateNotificationView(login_status);
        break;
    }
    if (view)
      bubble_view_->AddChildView(new TrayPopupItemContainer(view));
  }
}

base::EventStatus SystemTrayBubble::WillProcessEvent(
    const base::NativeEvent& event) {
  // Check if the user clicked outside of the bubble and close it if they did.
  if (bubble_type_ != BUBBLE_TYPE_NOTIFICATION &&
      ui::EventTypeFromNative(event) == ui::ET_MOUSE_PRESSED) {
    gfx::Point cursor_in_view = ui::EventLocationFromNative(event);
    views::View::ConvertPointFromScreen(bubble_view_, &cursor_in_view);
    if (!bubble_view_->HitTest(cursor_in_view)) {
      bubble_widget_->Close();
    }
  }
  return base::EVENT_CONTINUE;
}

void SystemTrayBubble::DidProcessEvent(const base::NativeEvent& event) {
}

void SystemTrayBubble::OnWidgetClosing(views::Widget* widget) {
  CHECK_EQ(bubble_widget_, widget);
  MessageLoopForUI::current()->RemoveObserver(this);
  bubble_widget_ = NULL;
  tray_->RemoveBubble(this);
}

void SystemTrayBubble::OnWidgetVisibilityChanged(views::Widget* widget,
                                                 bool visible) {
  if (!visible)
    MessageLoopForUI::current()->RemoveObserver(this);
  else
    MessageLoopForUI::current()->AddObserver(this);
}

}  // namespace internal
}  // namespace ash
