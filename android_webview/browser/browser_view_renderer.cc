// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/browser_view_renderer.h"

#include "android_webview/browser/hardware_renderer.h"
#include "android_webview/public/browser/draw_gl.h"
#include "base/android/jni_android.h"
#include "base/auto_reset.h"
#include "base/debug/trace_event.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "content/public/browser/android/synchronous_compositor.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "ui/gfx/vector2d_conversions.h"

using base::android::AttachCurrentThread;
using base::android::JavaRef;
using base::android::ScopedJavaLocalRef;
using content::BrowserThread;

namespace android_webview {

namespace {

const int64 kFallbackTickTimeoutInMilliseconds = 20;

}  // namespace

BrowserViewRenderer::BrowserViewRenderer(BrowserViewRendererClient* client,
                                         content::WebContents* web_contents)
    : client_(client),
      web_contents_(web_contents),
      compositor_(NULL),
      is_paused_(false),
      view_visible_(false),
      window_visible_(false),
      attached_to_window_(false),
      dip_scale_(0.0),
      page_scale_factor_(1.0),
      on_new_picture_enable_(false),
      clear_view_(false),
      compositor_needs_continuous_invalidate_(false),
      block_invalidates_(false),
      width_(0),
      height_(0) {
  CHECK(web_contents_);
  content::SynchronousCompositor::SetClientForWebContents(web_contents_, this);

  // Currently the logic in this class relies on |compositor_| remaining NULL
  // until the DidInitializeCompositor() call, hence it is not set here.
}

BrowserViewRenderer::~BrowserViewRenderer() {
  content::SynchronousCompositor::SetClientForWebContents(web_contents_, NULL);
}

void BrowserViewRenderer::TrimMemory(int level) {
  if (hardware_renderer_) {
    client_->UpdateGlobalVisibleRect();
    bool visible = view_visible_ && window_visible_ &&
                   !cached_global_visible_rect_.IsEmpty();
    if (hardware_renderer_->TrimMemory(level, visible)) {
      // Force a draw for compositor to drop tiles synchronously.
      ForceFakeCompositeSW();
    }
  }
}

bool BrowserViewRenderer::OnDraw(jobject java_canvas,
                                 bool is_hardware_canvas,
                                 const gfx::Vector2d& scroll,
                                 const gfx::Rect& clip) {
  scroll_at_start_of_frame_ = scroll;
  if (clear_view_)
    return false;
  if (is_hardware_canvas && attached_to_window_) {
    // We should be performing a hardware draw here. If we don't have the
    // compositor yet or if RequestDrawGL fails, it means we failed this draw
    // and thus return false here to clear to background color for this draw.
    return compositor_ && client_->RequestDrawGL(java_canvas);
  }
  // Perform a software draw
  return DrawSWInternal(java_canvas, clip);
}

void BrowserViewRenderer::DrawGL(AwDrawGLInfo* draw_info) {
  if (!attached_to_window_ || !compositor_)
    return;

  client_->UpdateGlobalVisibleRect();
  if (cached_global_visible_rect_.IsEmpty())
    return;

  if (!hardware_renderer_)
    hardware_renderer_.reset(new HardwareRenderer(compositor_, client_));

  DrawGLInput input;
  input.global_visible_rect = cached_global_visible_rect_;
  input.scroll = scroll_at_start_of_frame_;
  DrawGLResult result;
  {
    base::AutoReset<bool> auto_reset(&block_invalidates_, true);
    result = hardware_renderer_->DrawGL(draw_info, input);
  }

  if (result.did_draw) {
    fallback_tick_.Cancel();
    block_invalidates_ = false;
    EnsureContinuousInvalidation(draw_info, !result.clip_contains_visible_rect);
  }
}

void BrowserViewRenderer::SetGlobalVisibleRect(const gfx::Rect& visible_rect) {
  cached_global_visible_rect_ = visible_rect;
}

bool BrowserViewRenderer::DrawSWInternal(jobject java_canvas,
                                         const gfx::Rect& clip) {
  if (clip.IsEmpty()) {
    TRACE_EVENT_INSTANT0(
        "android_webview", "EarlyOut_EmptyClip", TRACE_EVENT_SCOPE_THREAD);
    return true;
  }

  if (!compositor_) {
    TRACE_EVENT_INSTANT0(
        "android_webview", "EarlyOut_NoCompositor", TRACE_EVENT_SCOPE_THREAD);
    return false;
  }

  return BrowserViewRendererJavaHelper::GetInstance()
      ->RenderViaAuxilaryBitmapIfNeeded(
            java_canvas,
            scroll_at_start_of_frame_,
            clip,
            base::Bind(&BrowserViewRenderer::CompositeSW,
                       base::Unretained(this)));
}

skia::RefPtr<SkPicture> BrowserViewRenderer::CapturePicture(int width,
                                                            int height) {
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::CapturePicture");

  // Return empty Picture objects for empty SkPictures.
  skia::RefPtr<SkPicture> picture = skia::AdoptRef(new SkPicture);
  if (width <= 0 || height <= 0) {
    return picture;
  }

  // Reset scroll back to the origin, will go back to the old
  // value when scroll_reset is out of scope.
  base::AutoReset<gfx::Vector2dF> scroll_reset(&scroll_offset_dip_,
                                               gfx::Vector2d());

  SkCanvas* rec_canvas = picture->beginRecording(width, height, 0);
  if (compositor_)
    CompositeSW(rec_canvas);
  picture->endRecording();
  return picture;
}

void BrowserViewRenderer::EnableOnNewPicture(bool enabled) {
  on_new_picture_enable_ = enabled;
  EnsureContinuousInvalidation(NULL, false);
}

void BrowserViewRenderer::ClearView() {
  TRACE_EVENT_INSTANT0("android_webview",
                       "BrowserViewRenderer::ClearView",
                       TRACE_EVENT_SCOPE_THREAD);
  if (clear_view_)
    return;

  clear_view_ = true;
  // Always invalidate ignoring the compositor to actually clear the webview.
  EnsureContinuousInvalidation(NULL, true);
}

void BrowserViewRenderer::SetIsPaused(bool paused) {
  TRACE_EVENT_INSTANT1("android_webview",
                       "BrowserViewRenderer::SetIsPaused",
                       TRACE_EVENT_SCOPE_THREAD,
                       "paused",
                       paused);
  is_paused_ = paused;
  EnsureContinuousInvalidation(NULL, false);
}

void BrowserViewRenderer::SetViewVisibility(bool view_visible) {
  TRACE_EVENT_INSTANT1("android_webview",
                       "BrowserViewRenderer::SetViewVisibility",
                       TRACE_EVENT_SCOPE_THREAD,
                       "view_visible",
                       view_visible);
  view_visible_ = view_visible;
}

void BrowserViewRenderer::SetWindowVisibility(bool window_visible) {
  TRACE_EVENT_INSTANT1("android_webview",
                       "BrowserViewRenderer::SetWindowVisibility",
                       TRACE_EVENT_SCOPE_THREAD,
                       "window_visible",
                       window_visible);
  window_visible_ = window_visible;
  EnsureContinuousInvalidation(NULL, false);
}

void BrowserViewRenderer::OnSizeChanged(int width, int height) {
  TRACE_EVENT_INSTANT2("android_webview",
                       "BrowserViewRenderer::OnSizeChanged",
                       TRACE_EVENT_SCOPE_THREAD,
                       "width",
                       width,
                       "height",
                       height);
  width_ = width;
  height_ = height;
}

void BrowserViewRenderer::OnAttachedToWindow(int width, int height) {
  TRACE_EVENT2("android_webview",
               "BrowserViewRenderer::OnAttachedToWindow",
               "width",
               width,
               "height",
               height);
  attached_to_window_ = true;
  width_ = width;
  height_ = height;
}

void BrowserViewRenderer::OnDetachedFromWindow() {
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::OnDetachedFromWindow");
  attached_to_window_ = false;
  hardware_renderer_.reset();
}

bool BrowserViewRenderer::IsAttachedToWindow() const {
  return attached_to_window_;
}

bool BrowserViewRenderer::IsVisible() const {
  // Ignore |window_visible_| if |attached_to_window_| is false.
  return view_visible_ && (!attached_to_window_ || window_visible_);
}

gfx::Rect BrowserViewRenderer::GetScreenRect() const {
  return gfx::Rect(client_->GetLocationOnScreen(), gfx::Size(width_, height_));
}

void BrowserViewRenderer::DidInitializeCompositor(
    content::SynchronousCompositor* compositor) {
  TRACE_EVENT0("android_webview",
               "BrowserViewRenderer::DidInitializeCompositor");
  DCHECK(compositor && compositor_ == NULL);
  compositor_ = compositor;
}

void BrowserViewRenderer::DidDestroyCompositor(
    content::SynchronousCompositor* compositor) {
  TRACE_EVENT0("android_webview", "BrowserViewRenderer::DidDestroyCompositor");
  DCHECK(compositor_ == compositor);
  DCHECK(!hardware_renderer_.get());
  compositor_ = NULL;
}

void BrowserViewRenderer::SetContinuousInvalidate(bool invalidate) {
  if (compositor_needs_continuous_invalidate_ == invalidate)
    return;

  TRACE_EVENT_INSTANT1("android_webview",
                       "BrowserViewRenderer::SetContinuousInvalidate",
                       TRACE_EVENT_SCOPE_THREAD,
                       "invalidate",
                       invalidate);
  compositor_needs_continuous_invalidate_ = invalidate;
  EnsureContinuousInvalidation(NULL, false);
}

void BrowserViewRenderer::SetDipScale(float dip_scale) {
  dip_scale_ = dip_scale;
  CHECK(dip_scale_ > 0);
}

gfx::Vector2d BrowserViewRenderer::max_scroll_offset() const {
  DCHECK_GT(dip_scale_, 0);
  return gfx::ToCeiledVector2d(gfx::ScaleVector2d(
      max_scroll_offset_dip_, dip_scale_ * page_scale_factor_));
}

void BrowserViewRenderer::ScrollTo(gfx::Vector2d scroll_offset) {
  gfx::Vector2d max_offset = max_scroll_offset();
  gfx::Vector2dF scroll_offset_dip;
  // To preserve the invariant that scrolling to the maximum physical pixel
  // value also scrolls to the maximum dip pixel value we transform the physical
  // offset into the dip offset by using a proportion (instead of dividing by
  // dip_scale * page_scale_factor).
  if (max_offset.x()) {
    scroll_offset_dip.set_x((scroll_offset.x() * max_scroll_offset_dip_.x()) /
                            max_offset.x());
  }
  if (max_offset.y()) {
    scroll_offset_dip.set_y((scroll_offset.y() * max_scroll_offset_dip_.y()) /
                            max_offset.y());
  }

  DCHECK_LE(0, scroll_offset_dip.x());
  DCHECK_LE(0, scroll_offset_dip.y());
  DCHECK_LE(scroll_offset_dip.x(), max_scroll_offset_dip_.x());
  DCHECK_LE(scroll_offset_dip.y(), max_scroll_offset_dip_.y());

  if (scroll_offset_dip_ == scroll_offset_dip)
    return;

  scroll_offset_dip_ = scroll_offset_dip;

  if (compositor_)
    compositor_->DidChangeRootLayerScrollOffset();
}

void BrowserViewRenderer::DidUpdateContent() {
  TRACE_EVENT_INSTANT0("android_webview",
                       "BrowserViewRenderer::DidUpdateContent",
                       TRACE_EVENT_SCOPE_THREAD);
  clear_view_ = false;
  EnsureContinuousInvalidation(NULL, false);
  if (on_new_picture_enable_)
    client_->OnNewPicture();
}

void BrowserViewRenderer::SetMaxRootLayerScrollOffset(
    gfx::Vector2dF new_value_dip) {
  DCHECK_GT(dip_scale_, 0);

  max_scroll_offset_dip_ = new_value_dip;
  DCHECK_LE(0, max_scroll_offset_dip_.x());
  DCHECK_LE(0, max_scroll_offset_dip_.y());

  client_->SetMaxContainerViewScrollOffset(max_scroll_offset());
}

void BrowserViewRenderer::SetTotalRootLayerScrollOffset(
    gfx::Vector2dF scroll_offset_dip) {
  // TOOD(mkosiba): Add a DCHECK to say that this does _not_ get called during
  // DrawGl when http://crbug.com/249972 is fixed.
  if (scroll_offset_dip_ == scroll_offset_dip)
    return;

  scroll_offset_dip_ = scroll_offset_dip;

  gfx::Vector2d max_offset = max_scroll_offset();
  gfx::Vector2d scroll_offset;
  // For an explanation as to why this is done this way see the comment in
  // BrowserViewRenderer::ScrollTo.
  if (max_scroll_offset_dip_.x()) {
    scroll_offset.set_x((scroll_offset_dip.x() * max_offset.x()) /
                        max_scroll_offset_dip_.x());
  }

  if (max_scroll_offset_dip_.y()) {
    scroll_offset.set_y((scroll_offset_dip.y() * max_offset.y()) /
                        max_scroll_offset_dip_.y());
  }

  DCHECK(0 <= scroll_offset.x());
  DCHECK(0 <= scroll_offset.y());
  // Disabled because the conditions are being violated while running
  // AwZoomTest.testMagnification, see http://crbug.com/340648
  // DCHECK(scroll_offset.x() <= max_offset.x());
  // DCHECK(scroll_offset.y() <= max_offset.y());

  client_->ScrollContainerViewTo(scroll_offset);
}

gfx::Vector2dF BrowserViewRenderer::GetTotalRootLayerScrollOffset() {
  return scroll_offset_dip_;
}

bool BrowserViewRenderer::IsExternalFlingActive() const {
  return client_->IsFlingActive();
}

void BrowserViewRenderer::SetRootLayerPageScaleFactorAndLimits(
    float page_scale_factor,
    float min_page_scale_factor,
    float max_page_scale_factor) {
  page_scale_factor_ = page_scale_factor;
  DCHECK_GT(page_scale_factor_, 0);
  client_->SetPageScaleFactorAndLimits(
      page_scale_factor, min_page_scale_factor, max_page_scale_factor);
}

void BrowserViewRenderer::SetRootLayerScrollableSize(
    gfx::SizeF scrollable_size) {
  client_->SetContentsSize(scrollable_size);
}

void BrowserViewRenderer::DidOverscroll(gfx::Vector2dF accumulated_overscroll,
                                        gfx::Vector2dF latest_overscroll_delta,
                                        gfx::Vector2dF current_fling_velocity) {
  const float physical_pixel_scale = dip_scale_ * page_scale_factor_;
  if (accumulated_overscroll == latest_overscroll_delta)
    overscroll_rounding_error_ = gfx::Vector2dF();
  gfx::Vector2dF scaled_overscroll_delta =
      gfx::ScaleVector2d(latest_overscroll_delta, physical_pixel_scale);
  gfx::Vector2d rounded_overscroll_delta = gfx::ToRoundedVector2d(
      scaled_overscroll_delta + overscroll_rounding_error_);
  overscroll_rounding_error_ =
      scaled_overscroll_delta - rounded_overscroll_delta;
  client_->DidOverscroll(rounded_overscroll_delta);
}

void BrowserViewRenderer::EnsureContinuousInvalidation(
    AwDrawGLInfo* draw_info,
    bool invalidate_ignore_compositor) {
  // This method should be called again when any of these conditions change.
  bool need_invalidate =
      compositor_needs_continuous_invalidate_ || invalidate_ignore_compositor;
  if (!need_invalidate || block_invalidates_)
    return;

  // Always call view invalidate. We rely the Android framework to ignore the
  // invalidate when it's not needed such as when view is not visible.
  if (draw_info) {
    draw_info->dirty_left = cached_global_visible_rect_.x();
    draw_info->dirty_top = cached_global_visible_rect_.y();
    draw_info->dirty_right = cached_global_visible_rect_.right();
    draw_info->dirty_bottom = cached_global_visible_rect_.bottom();
    draw_info->status_mask |= AwDrawGLInfo::kStatusMaskDraw;
  } else {
    client_->PostInvalidate();
  }

  // Stop fallback ticks when one of these is true.
  // 1) Webview is paused. Also need to check we are not in clear view since
  // paused, offscreen still expect clear view to recover.
  // 2) If we are attached to window and the window is not visible (eg when
  // app is in the background). We are sure in this case the webview is used
  // "on-screen" but that updates are not needed when in the background.
  bool throttle_fallback_tick =
      (is_paused_ && !clear_view_) || (attached_to_window_ && !window_visible_);
  if (throttle_fallback_tick)
    return;

  block_invalidates_ = compositor_needs_continuous_invalidate_;

  // Unretained here is safe because the callback is cancelled when
  // |fallback_tick_| is destroyed.
  fallback_tick_.Reset(base::Bind(&BrowserViewRenderer::FallbackTickFired,
                                  base::Unretained(this)));

  // No need to reschedule fallback tick if compositor does not need to be
  // ticked. This can happen if this is reached because
  // invalidate_ignore_compositor is true.
  if (compositor_needs_continuous_invalidate_) {
    BrowserThread::PostDelayedTask(
        BrowserThread::UI,
        FROM_HERE,
        fallback_tick_.callback(),
        base::TimeDelta::FromMilliseconds(kFallbackTickTimeoutInMilliseconds));
  }
}

void BrowserViewRenderer::FallbackTickFired() {
  TRACE_EVENT1("android_webview",
               "BrowserViewRenderer::FallbackTickFired",
               "compositor_needs_continuous_invalidate_",
               compositor_needs_continuous_invalidate_);

  // This should only be called if OnDraw or DrawGL did not come in time, which
  // means block_invalidates_ must still be true.
  DCHECK(block_invalidates_);
  if (compositor_needs_continuous_invalidate_ && compositor_)
    ForceFakeCompositeSW();
}

void BrowserViewRenderer::ForceFakeCompositeSW() {
  DCHECK(compositor_);
  SkBitmapDevice device(SkBitmap::kARGB_8888_Config, 1, 1);
  SkCanvas canvas(&device);
  CompositeSW(&canvas);
}

bool BrowserViewRenderer::CompositeSW(SkCanvas* canvas) {
  DCHECK(compositor_);

  fallback_tick_.Cancel();
  block_invalidates_ = true;
  bool result = compositor_->DemandDrawSw(canvas);
  block_invalidates_ = false;
  EnsureContinuousInvalidation(NULL, false);
  return result;
}

std::string BrowserViewRenderer::ToString(AwDrawGLInfo* draw_info) const {
  std::string str;
  base::StringAppendF(&str, "is_paused: %d ", is_paused_);
  base::StringAppendF(&str, "view_visible: %d ", view_visible_);
  base::StringAppendF(&str, "window_visible: %d ", window_visible_);
  base::StringAppendF(&str, "dip_scale: %f ", dip_scale_);
  base::StringAppendF(&str, "page_scale_factor: %f ", page_scale_factor_);
  base::StringAppendF(&str,
                      "compositor_needs_continuous_invalidate: %d ",
                      compositor_needs_continuous_invalidate_);
  base::StringAppendF(&str, "block_invalidates: %d ", block_invalidates_);
  base::StringAppendF(&str, "view width height: [%d %d] ", width_, height_);
  base::StringAppendF(&str, "attached_to_window: %d ", attached_to_window_);
  base::StringAppendF(&str,
                      "global visible rect: %s ",
                      cached_global_visible_rect_.ToString().c_str());
  base::StringAppendF(&str,
                      "scroll_at_start_of_frame: %s ",
                      scroll_at_start_of_frame_.ToString().c_str());
  base::StringAppendF(
      &str, "scroll_offset_dip: %s ", scroll_offset_dip_.ToString().c_str());
  base::StringAppendF(&str,
                      "overscroll_rounding_error_: %s ",
                      overscroll_rounding_error_.ToString().c_str());
  base::StringAppendF(
      &str, "on_new_picture_enable: %d ", on_new_picture_enable_);
  base::StringAppendF(&str, "clear_view: %d ", clear_view_);
  if (draw_info) {
    base::StringAppendF(&str,
                        "clip left top right bottom: [%d %d %d %d] ",
                        draw_info->clip_left,
                        draw_info->clip_top,
                        draw_info->clip_right,
                        draw_info->clip_bottom);
    base::StringAppendF(&str,
                        "surface width height: [%d %d] ",
                        draw_info->width,
                        draw_info->height);
    base::StringAppendF(&str, "is_layer: %d ", draw_info->is_layer);
  }
  return str;
}

}  // namespace android_webview
