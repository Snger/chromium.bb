// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/frame_painter.h"

#include "base/logging.h"  // DCHECK
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkShader.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/theme_provider.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace {
// Size of border along top edge, used for resize handle computations.
const int kTopThickness = 1;
// TODO(jamescook): Border is specified to be a single pixel overlapping
// the web content and may need to be built into the shadow layers instead.
const int kBorderThickness = 0;
// Ash windows do not have a traditional visible window frame.  Window content
// extends to the edge of the window.  We consider a small region outside the
// window bounds and an even smaller region overlapping the window to be the
// "non-client" area and use it for resizing.
const int kResizeOutsideBoundsSize = 6;
const int kResizeInsideBoundsSize = 1;
// In the window corners, the resize areas don't actually expand bigger, but the
// 16 px at the end of each edge triggers diagonal resizing.
const int kResizeAreaCornerSize = 16;
// Space between left edge of window and popup window icon.
const int kIconOffsetX = 4;
// Space between top of window and popup window icon.
const int kIconOffsetY = 6;
// Height and width of window icon.
const int kIconSize = 16;
// Space between the title text and the caption buttons.
const int kTitleLogoSpacing = 5;
// Space between window icon and title text.
const int kTitleIconOffsetX = 4;
// Space between window edge and title text, when there is no icon.
const int kTitleNoIconOffsetX = 8;
// Space between title text and top of window.
const int kTitleOffsetY = 7;
// Color for the title text.
const SkColor kTitleColor = SkColorSetRGB(40, 40, 40);
// Size of header/content separator line below the header image.
const int kHeaderContentSeparatorSize = 1;
// Color of header bottom edge line.
const SkColor kHeaderContentSeparatorColor = SkColorSetRGB(128, 128, 128);
// Space between close button and right edge of window.
const int kCloseButtonOffsetX = 0;
// Space between close button and top edge of window.
const int kCloseButtonOffsetY = 0;

// Tiles an image into an area, rounding the top corners.
void TileRoundRect(gfx::Canvas* canvas,
                   int x, int y, int w, int h,
                   const SkBitmap& bitmap,
                   int corner_radius) {
  SkRect rect;
  rect.iset(x, y, x + w, y + h);
  const SkScalar kRadius = SkIntToScalar(corner_radius);
  SkScalar radii[8] = {
      kRadius, kRadius,  // top-left
      kRadius, kRadius,  // top-right
      0, 0,   // bottom-right
      0, 0};  // bottom-left
  SkPath path;
  path.addRoundRect(rect, radii, SkPath::kCW_Direction);

  SkPaint paint;
  SkShader* shader = SkShader::CreateBitmapShader(bitmap,
                                                  SkShader::kRepeat_TileMode,
                                                  SkShader::kRepeat_TileMode);
  paint.setShader(shader);
  paint.setXfermodeMode(SkXfermode::kSrcOver_Mode);
  // CreateBitmapShader returns a Shader with a reference count of one, we
  // need to unref after paint takes ownership of the shader.
  shader->unref();
  canvas->sk_canvas()->drawPath(path, paint);
}
}  // namespace

namespace ash {

///////////////////////////////////////////////////////////////////////////////
// FramePainter, public:

FramePainter::FramePainter()
    : frame_(NULL),
      window_icon_(NULL),
      maximize_button_(NULL),
      close_button_(NULL),
      button_separator_(NULL),
      top_left_corner_(NULL),
      top_edge_(NULL),
      top_right_corner_(NULL),
      header_left_edge_(NULL),
      header_right_edge_(NULL) {
}

FramePainter::~FramePainter() {
}

void FramePainter::Init(views::Widget* frame,
                        views::View* window_icon,
                        views::ImageButton* maximize_button,
                        views::ImageButton* close_button) {
  DCHECK(frame);
  // window_icon may be NULL.
  DCHECK(maximize_button);
  DCHECK(close_button);
  frame_ = frame;
  window_icon_ = window_icon;
  maximize_button_ = maximize_button;
  close_button_ = close_button;

  // Window frame image parts.
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  button_separator_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_BUTTON_SEPARATOR).ToSkBitmap();
  top_left_corner_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_TOP_LEFT).ToSkBitmap();
  top_edge_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_TOP).ToSkBitmap();
  top_right_corner_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_TOP_RIGHT).ToSkBitmap();
  header_left_edge_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_LEFT).ToSkBitmap();
  header_right_edge_ =
      rb.GetImageNamed(IDR_AURA_WINDOW_HEADER_SHADE_RIGHT).ToSkBitmap();

  // Ensure we get resize cursors for a few pixels outside our bounds.
  frame_->GetNativeWindow()->SetHitTestBoundsOverride(kResizeOutsideBoundsSize,
                                                      kResizeInsideBoundsSize);
}

gfx::Rect FramePainter::GetBoundsForClientView(
    int top_height,
    const gfx::Rect& window_bounds) const {
  return gfx::Rect(
      kBorderThickness,
      top_height,
      std::max(0, window_bounds.width() - (2 * kBorderThickness)),
      std::max(0, window_bounds.height() - top_height - kBorderThickness));
}

gfx::Rect FramePainter::GetWindowBoundsForClientBounds(
    int top_height,
    const gfx::Rect& client_bounds) const {
  return gfx::Rect(std::max(0, client_bounds.x() - kBorderThickness),
                   std::max(0, client_bounds.y() - top_height),
                   client_bounds.width() + (2 * kBorderThickness),
                   client_bounds.height() + top_height + kBorderThickness);
}

int FramePainter::NonClientHitTest(views::NonClientFrameView* view,
                                   const gfx::Point& point) {
  gfx::Rect expanded_bounds = view->bounds();
  expanded_bounds.Inset(-kResizeOutsideBoundsSize, -kResizeOutsideBoundsSize);
  if (!expanded_bounds.Contains(point))
    return HTNOWHERE;

  // No avatar button.

  // Check the frame first, as we allow a small area overlapping the contents
  // to be used for resize handles.
  bool can_resize = frame_->widget_delegate() ?
      frame_->widget_delegate()->CanResize() :
      false;
  int frame_component = view->GetHTComponentForFrame(point,
                                                     kResizeInsideBoundsSize,
                                                     kResizeInsideBoundsSize,
                                                     kResizeAreaCornerSize,
                                                     kResizeAreaCornerSize,
                                                     can_resize);
  if (frame_component != HTNOWHERE)
    return frame_component;

  int client_component = frame_->client_view()->NonClientHitTest(point);
  if (client_component != HTNOWHERE)
    return client_component;

  // Then see if the point is within any of the window controls.
  if (close_button_->visible() &&
      close_button_->GetMirroredBounds().Contains(point))
    return HTCLOSE;
  if (maximize_button_->visible() &&
      maximize_button_->GetMirroredBounds().Contains(point))
    return HTMAXBUTTON;

  // Caption is a safe default.
  return HTCAPTION;
}

gfx::Size FramePainter::GetMinimumSize(views::NonClientFrameView* view) {
  gfx::Size min_size = frame_->client_view()->GetMinimumSize();
  // Ensure we can display the top of the caption area.
  gfx::Rect client_bounds = view->GetBoundsForClientView();
  min_size.Enlarge(0, client_bounds.y());
  // Ensure we have enough space for the window icon and buttons.  We allow
  // the title string to collapse to zero width.
  int title_width = GetTitleOffsetX() +
      maximize_button_->width() +
      button_separator_->width() +
      close_button_->width();
  if (title_width > min_size.width())
    min_size.set_width(title_width);
  return min_size;
}

void FramePainter::PaintHeader(views::NonClientFrameView* view,
                               gfx::Canvas* canvas,
                               const SkBitmap* theme_frame,
                               const SkBitmap* theme_frame_overlay) {

  // Draw the header background, clipping the corners to be rounded.
  const int kCornerRadius = 2;
  TileRoundRect(canvas,
                0, 0, view->width(), theme_frame->height(),
                *theme_frame,
                kCornerRadius);

  // Draw the theme frame overlay, if available.
  if (theme_frame_overlay)
    canvas->DrawBitmapInt(*theme_frame_overlay, 0, 0);

  // Separator between the maximize and close buttons.
  canvas->DrawBitmapInt(*button_separator_,
                        close_button_->x() - button_separator_->width(),
                        close_button_->y());

  // Draw the top corners and edge.
  int top_left_height = top_left_corner_->height();
  canvas->DrawBitmapInt(*top_left_corner_,
                        0, 0, top_left_corner_->width(), top_left_height,
                        0, 0, top_left_corner_->width(), top_left_height,
                        false);
  canvas->TileImageInt(*top_edge_,
      top_left_corner_->width(),
      0,
      view->width() - top_left_corner_->width() - top_right_corner_->width(),
      top_edge_->height());
  int top_right_height = top_right_corner_->height();
  canvas->DrawBitmapInt(*top_right_corner_,
                        0, 0,
                        top_right_corner_->width(), top_right_height,
                        view->width() - top_right_corner_->width(), 0,
                        top_right_corner_->width(), top_right_height,
                        false);

  // Header left edge.
  int header_left_height = theme_frame->height() - top_left_height;
  canvas->TileImageInt(*header_left_edge_,
                       0, top_left_height,
                       header_left_edge_->width(), header_left_height);

  // Header right edge.
  int header_right_height = theme_frame->height() - top_right_height;
  canvas->TileImageInt(*header_right_edge_,
                       view->width() - header_right_edge_->width(),
                       top_right_height,
                       header_right_edge_->width(),
                       header_right_height);

  // We don't draw edges around the content area.  Web content goes flush
  // to the edge of the window.
}

void FramePainter::PaintHeaderContentSeparator(views::NonClientFrameView* view,
                                               gfx::Canvas* canvas) {
  // Paint the line just above the content area.
  gfx::Rect client_bounds = view->GetBoundsForClientView();
  canvas->FillRect(gfx::Rect(client_bounds.x(),
                             client_bounds.y() - kHeaderContentSeparatorSize,
                             client_bounds.width(),
                             kHeaderContentSeparatorSize),
                   kHeaderContentSeparatorColor);
}

int FramePainter::HeaderContentSeparatorSize() const {
  return kHeaderContentSeparatorSize;
}

void FramePainter::PaintTitleBar(views::NonClientFrameView* view,
                                 gfx::Canvas* canvas,
                                 const gfx::Font& title_font) {
  // The window icon is painted by its own views::View.
  views::WidgetDelegate* delegate = frame_->widget_delegate();
  if (delegate && delegate->ShouldShowWindowTitle()) {
    int title_x = GetTitleOffsetX();
    gfx::Rect title_bounds(
        title_x,
        kTitleOffsetY,
        std::max(0, maximize_button_->x() - kTitleLogoSpacing - title_x),
        title_font.GetHeight());
    canvas->DrawStringInt(delegate->GetWindowTitle(),
                          title_font,
                          kTitleColor,
                          view->GetMirroredXForRect(title_bounds),
                          title_bounds.y(),
                          title_bounds.width(),
                          title_bounds.height());
  }
}

void FramePainter::LayoutHeader(views::NonClientFrameView* view,
                                bool maximized_layout) {
  // The maximized layout uses shorter buttons.
  if (maximized_layout) {
    SetButtonImages(close_button_,
                    IDR_AURA_WINDOW_MAXIMIZED_CLOSE,
                    IDR_AURA_WINDOW_MAXIMIZED_CLOSE_H,
                    IDR_AURA_WINDOW_MAXIMIZED_CLOSE_P);
    SetButtonImages(maximize_button_,
                    IDR_AURA_WINDOW_MAXIMIZED_RESTORE,
                    IDR_AURA_WINDOW_MAXIMIZED_RESTORE_H,
                    IDR_AURA_WINDOW_MAXIMIZED_RESTORE_P);
  } else {
    SetButtonImages(close_button_,
                    IDR_AURA_WINDOW_CLOSE,
                    IDR_AURA_WINDOW_CLOSE_H,
                    IDR_AURA_WINDOW_CLOSE_P);
    SetButtonImages(maximize_button_,
                    IDR_AURA_WINDOW_MAXIMIZE,
                    IDR_AURA_WINDOW_MAXIMIZE_H,
                    IDR_AURA_WINDOW_MAXIMIZE_P);
  }

  gfx::Size close_size = close_button_->GetPreferredSize();
  close_button_->SetBounds(
      view->width() - close_size.width() - kCloseButtonOffsetX,
      kCloseButtonOffsetY,
      close_size.width(),
      close_size.height());

  gfx::Size maximize_size = maximize_button_->GetPreferredSize();
  maximize_button_->SetBounds(
      close_button_->x() - button_separator_->width() - maximize_size.width(),
      close_button_->y(),
      maximize_size.width(),
      maximize_size.height());

  if (window_icon_)
    window_icon_->SetBoundsRect(
        gfx::Rect(kIconOffsetX, kIconOffsetY, kIconSize, kIconSize));
}

///////////////////////////////////////////////////////////////////////////////
// FramePainter, private:

void FramePainter::SetButtonImages(views::ImageButton* button,
                                   int normal_bitmap_id,
                                   int hot_bitmap_id,
                                   int pushed_bitmap_id) {
  ui::ThemeProvider* theme_provider = frame_->GetThemeProvider();
  button->SetImage(views::CustomButton::BS_NORMAL,
                   theme_provider->GetBitmapNamed(normal_bitmap_id));
  button->SetImage(views::CustomButton::BS_HOT,
                   theme_provider->GetBitmapNamed(hot_bitmap_id));
  button->SetImage(views::CustomButton::BS_PUSHED,
                   theme_provider->GetBitmapNamed(pushed_bitmap_id));
}

int FramePainter::GetTitleOffsetX() const {
  return window_icon_ ?
      window_icon_->bounds().right() + kTitleIconOffsetX :
      kTitleNoIconOffsetX;
}

}  // namespace ash
