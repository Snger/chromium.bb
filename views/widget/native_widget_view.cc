// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/native_widget_view.h"

#include "ui/gfx/canvas.h"

namespace views {
namespace internal {

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetView, public:

// static
const char NativeWidgetView::kViewClassName[] = "views/NativeWidgetView";

NativeWidgetView::NativeWidgetView(NativeWidgetViews* native_widget)
    : native_widget_(native_widget) {
}

NativeWidgetView::~NativeWidgetView() {
}

Widget* NativeWidgetView::GetAssociatedWidget() {
  return native_widget_->delegate()->AsWidget();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWidgetView, View overrides:

void NativeWidgetView::SchedulePaintInternal(const gfx::Rect& r) {
  View::SchedulePaintInternal(r);
}

void NativeWidgetView::ViewHierarchyChanged(bool is_add, View* parent,
                                            View* child) {
  if (is_add && child == this)
    delegate()->OnNativeWidgetCreated();
}

void NativeWidgetView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  delegate()->OnNativeWidgetSizeChanged(size());
}

void NativeWidgetView::OnPaint(gfx::Canvas* canvas) {
  canvas->FillRectInt(SK_ColorRED, 0, 0, width(), height());
  delegate()->OnNativeWidgetPaint(canvas);
}

bool NativeWidgetView::OnMousePressed(const MouseEvent& event) {
  MouseEvent e(event, this);
  return delegate()->OnMouseEvent(event);
}

bool NativeWidgetView::OnMouseDragged(const MouseEvent& event) {
  MouseEvent e(event, this);
  return delegate()->OnMouseEvent(event);
}

void NativeWidgetView::OnMouseReleased(const MouseEvent& event) {
  MouseEvent e(event, this);
  delegate()->OnMouseEvent(event);
}

void NativeWidgetView::OnMouseCaptureLost() {
  delegate()->OnMouseCaptureLost();
}

void NativeWidgetView::OnMouseMoved(const MouseEvent& event) {
  MouseEvent e(event, this);
  delegate()->OnMouseEvent(event);
}

void NativeWidgetView::OnMouseEntered(const MouseEvent& event) {
  MouseEvent e(event, this);
  delegate()->OnMouseEvent(event);
}

void NativeWidgetView::OnMouseExited(const MouseEvent& event) {
  MouseEvent e(event, this);
  delegate()->OnMouseEvent(event);
}

#if defined(TOUCH_UI)
ui::TouchStatus NativeWidgetView::OnTouchEvent(const TouchEvent& event) {
  return delegate()->OnTouchEvent(event);
}
#endif

bool NativeWidgetView::OnKeyPressed(const KeyEvent& event) {
  return delegate()->OnKeyEvent(event);
}

bool NativeWidgetView::OnKeyReleased(const KeyEvent& event) {
  return delegate()->OnKeyEvent(event);
}

bool NativeWidgetView::OnMouseWheel(const MouseWheelEvent& event) {
  MouseWheelEvent e(event, this);
  return delegate()->OnMouseEvent(event);
}

void NativeWidgetView::OnFocus() {
  // TODO(beng): check if we have to do this.
  //delegate()->OnNativeFocus(NULL);
}

void NativeWidgetView::OnBlur() {
  // TODO(beng): check if we have to do this.
  //delegate()->OnNativeBlur(NULL);
}

std::string NativeWidgetView::GetClassName() const {
  return kViewClassName;
}


}  // namespace internal
}  // namespace views

