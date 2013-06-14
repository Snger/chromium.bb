// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_IN_PROCESS_IN_PROCESS_VIEW_RENDERER_H_
#define ANDROID_WEBVIEW_BROWSER_IN_PROCESS_IN_PROCESS_VIEW_RENDERER_H_

#include "android_webview/browser/browser_view_renderer.h"

#include "base/memory/weak_ptr.h"
#include "content/public/browser/android/synchronous_compositor_client.h"

namespace content {
class SynchronousCompositor;
class WebContents;
}

typedef void* EGLContext;
class SkCanvas;

namespace android_webview {

// Provides RenderViewHost wrapper functionality for sending WebView-specific
// IPC messages to the renderer and from there to WebKit.
class InProcessViewRenderer : public BrowserViewRenderer,
                              public content::SynchronousCompositorClient {
 public:
  InProcessViewRenderer(BrowserViewRenderer::Client* client,
                        JavaHelper* java_helper,
                        content::WebContents* web_contents);
  virtual ~InProcessViewRenderer();

  static InProcessViewRenderer* FromWebContents(
      content::WebContents* contents);

  // BrowserViewRenderer overrides
  virtual bool OnDraw(jobject java_canvas,
                      bool is_hardware_canvas,
                      const gfx::Point& scroll,
                      const gfx::Rect& clip) OVERRIDE;
  virtual void DrawGL(AwDrawGLInfo* draw_info) OVERRIDE;
  virtual base::android::ScopedJavaLocalRef<jobject> CapturePicture() OVERRIDE;
  virtual void EnableOnNewPicture(bool enabled) OVERRIDE;
  virtual void OnVisibilityChanged(
      bool view_visible, bool window_visible) OVERRIDE;
  virtual void OnSizeChanged(int width, int height) OVERRIDE;
  virtual void OnAttachedToWindow(int width, int height) OVERRIDE;
  virtual void OnDetachedFromWindow() OVERRIDE;
  virtual bool IsAttachedToWindow() OVERRIDE;
  virtual bool IsViewVisible() OVERRIDE;
  virtual gfx::Rect GetScreenRect() OVERRIDE;

  // SynchronousCompositorClient overrides
  virtual void DidInitializeCompositor(
      content::SynchronousCompositor* compositor) OVERRIDE;
  virtual void DidDestroyCompositor(
      content::SynchronousCompositor* compositor) OVERRIDE;
  virtual void SetContinuousInvalidate(bool invalidate) OVERRIDE;
  virtual void SetTotalRootLayerScrollOffset(gfx::Vector2dF new_value) OVERRIDE;
  virtual gfx::Vector2dF GetTotalRootLayerScrollOffset() OVERRIDE;

  void WebContentsGone();

 private:
  void Invalidate();
  void EnsureContinuousInvalidation();
  bool DrawSWInternal(jobject java_canvas,
                      const gfx::Rect& clip_bounds);
  bool RenderSW(SkCanvas* canvas);
  bool CompositeSW(SkCanvas* canvas);

  BrowserViewRenderer::Client* client_;
  BrowserViewRenderer::JavaHelper* java_helper_;
  content::WebContents* web_contents_;
  content::SynchronousCompositor* compositor_;

  bool view_visible_;

  // When true, we should continuously invalidate and keep drawing, for example
  // to drive animation.
  bool continuous_invalidate_;
  // True while an asynchronous invalidation task is pending.
  bool continuous_invalidate_task_pending_;

  int width_;
  int height_;

  bool attached_to_window_;
  bool hardware_initialized_;
  bool hardware_failed_;

  // Used only for detecting Android View System context changes.
  // Not to be used between draw calls.
  EGLContext egl_context_at_init_;

  // Last View scroll when View.onDraw() was called.
  gfx::Point scroll_at_start_of_frame_;

  gfx::Vector2dF scroll_offset_;

  base::WeakPtrFactory<InProcessViewRenderer> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InProcessViewRenderer);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_IN_PROCESS_IN_PROCESS_VIEW_RENDERER_H_
