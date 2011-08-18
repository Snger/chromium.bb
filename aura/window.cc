// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "aura/window.h"

#include <algorithm>

#include "aura/desktop.h"
#include "aura/window_delegate.h"
#include "base/logging.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/compositor/layer.h"

namespace aura {

// TODO: do we need to support child windows?
Window::Window(Desktop* desktop)
    : delegate_(NULL),
      visibility_(VISIBILITY_HIDDEN),
      layer_(new ui::Layer(desktop->compositor())),
      needs_paint_all_(true),
      parent_(NULL),
      id_(-1) {
}

Window::~Window() {
}

void Window::SetVisibility(Visibility visibility) {
  if (visibility_ == visibility)
    return;

  visibility_ = visibility;
}

void Window::SetBounds(const gfx::Rect& bounds, int anim_ms) {
  // TODO: support anim_ms
  // TODO: funnel this through the Desktop.
  bounds_ = bounds;
  layer_->SetBounds(bounds);
}

void Window::SchedulePaint(const gfx::Rect& bounds) {
  if (dirty_rect_.IsEmpty())
    dirty_rect_ = bounds;
  else
    dirty_rect_ = dirty_rect_.Union(bounds);
}

void Window::SetCanvas(const SkCanvas& canvas, const gfx::Point& origin) {
  // TODO: figure out how this is going to work when animating the layer. In
  // particular if we're animating the size then the underyling Texture is going
  // to be unhappy if we try to set a texture on a size bigger than the size of
  // the texture.
  layer_->SetCanvas(canvas, origin);
}

void Window::DrawTree() {
  UpdateLayerCanvas();
  Draw();

  // First pass updates the layer bitmaps.
  for (Windows::iterator i = children_.begin(); i != children_.end(); ++i)
    (*i)->DrawTree();
}

void Window::AddChild(Window* child) {
  DCHECK(std::find(children_.begin(), children_.end(), child) ==
      children_.end());
  child->parent_ = this;
  layer_->Add(child->layer_.get());
  children_.push_back(child);
}

void Window::RemoveChild(Window* child) {
  Windows::iterator i = std::find(children_.begin(), children_.end(), child);
  DCHECK(i != children_.end());
  child->parent_ = NULL;
  layer_->Remove(child->layer_.get());
  children_.erase(i);
}

void Window::UpdateLayerCanvas() {
  if (needs_paint_all_) {
    needs_paint_all_ = false;
    dirty_rect_ = gfx::Rect(0, 0, bounds().width(), bounds().height());
  }
  gfx::Rect dirty_rect = dirty_rect_.Intersect(
      gfx::Rect(0, 0, bounds().width(), bounds().height()));
  dirty_rect_.SetRect(0, 0, 0, 0);
  if (dirty_rect.IsEmpty())
    return;
  if (delegate_)
    delegate_->OnPaint(dirty_rect);
}

void Window::Draw() {
  if (visibility_ != VISIBILITY_HIDDEN)
    layer_->Draw();
}

}  // namespace aura
