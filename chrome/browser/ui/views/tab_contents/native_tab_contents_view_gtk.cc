// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_gtk.h"

#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/web_drag_dest_gtk.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/gtk/tab_contents_drag_source.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_delegate.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;
using WebKit::WebInputEvent;

namespace {

// Called when the content view gtk widget is tabbed to, or after the call to
// gtk_widget_child_focus() in TakeFocus(). We return true
// and grab focus if we don't have it. The call to
// FocusThroughTabTraversal(bool) forwards the "move focus forward" effect to
// webkit.
gboolean OnFocus(GtkWidget* widget, GtkDirectionType focus,
                 TabContents* tab_contents) {
  // If we already have focus, let the next widget have a shot at it. We will
  // reach this situation after the call to gtk_widget_child_focus() in
  // TakeFocus().
  if (gtk_widget_is_focus(widget))
    return FALSE;

  gtk_widget_grab_focus(widget);
  bool reverse = focus == GTK_DIR_TAB_BACKWARD;
  tab_contents->FocusThroughTabTraversal(reverse);
  return TRUE;
}

// See tab_contents_view_gtk.cc for discussion of mouse scroll zooming.
gboolean OnMouseScroll(GtkWidget* widget, GdkEventScroll* event,
                       internal::NativeTabContentsViewDelegate* delegate) {
  if ((event->state & gtk_accelerator_get_default_mod_mask()) ==
      GDK_CONTROL_MASK) {
    if (event->direction == GDK_SCROLL_DOWN) {
      delegate->OnNativeTabContentsViewWheelZoom(false);
      return TRUE;
    }
    if (event->direction == GDK_SCROLL_UP) {
      delegate->OnNativeTabContentsViewWheelZoom(true);
      return TRUE;
    }
  }

  return FALSE;
}

gfx::NativeView GetHiddenTabHostWindow() {
  static views::Widget* widget = NULL;

  if (!widget) {
    widget = new views::Widget;
    // We don't want this widget to be closed automatically, this causes
    // problems in tests that close the last non-secondary window.
    widget->set_is_secondary_widget(false);
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    widget->Init(params);
  }

  return static_cast<views::WidgetGtk*>(widget->native_widget())->
      window_contents();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewGtk, public:

NativeTabContentsViewGtk::NativeTabContentsViewGtk(
    internal::NativeTabContentsViewDelegate* delegate)
    : views::WidgetGtk(delegate->AsNativeWidgetDelegate()),
      delegate_(delegate),
      ignore_next_char_event_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(drag_source_(
          new TabContentsDragSource(delegate->GetTabContents()->view()))) {
}

NativeTabContentsViewGtk::~NativeTabContentsViewGtk() {
  delegate_ = NULL;
  CloseNow();
}

void NativeTabContentsViewGtk::AttachConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  DCHECK(find(constrained_windows_.begin(), constrained_windows_.end(),
              constrained_window) == constrained_windows_.end());

  constrained_windows_.push_back(constrained_window);
  AddChild(constrained_window->widget());

  gfx::Size requested_size;
  views::WidgetGtk::GetRequestedSize(&requested_size);
  PositionConstrainedWindows(requested_size);
}

void NativeTabContentsViewGtk::RemoveConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  std::vector<ConstrainedWindowGtk*>::iterator item =
      find(constrained_windows_.begin(), constrained_windows_.end(),
           constrained_window);
  DCHECK(item != constrained_windows_.end());
  RemoveChild((*item)->widget());
  constrained_windows_.erase(item);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewGtk, NativeTabContentsView implementation:

void NativeTabContentsViewGtk::InitNativeTabContentsView() {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.native_widget = this;
  params.delete_on_destroy = false;
  GetWidget()->Init(params);

  // We need to own the widget in order to attach/detach the native view to a
  // container.
  gtk_object_ref(GTK_OBJECT(GetWidget()->GetNativeView()));
}

void NativeTabContentsViewGtk::Unparent() {
  // Note that we do not DCHECK on focus_manager_ as it may be NULL when used
  // with an external tab container.
  NativeWidget::ReparentNativeView(GetNativeView(), GetHiddenTabHostWindow());
}

RenderWidgetHostView* NativeTabContentsViewGtk::CreateRenderWidgetHostView(
    RenderWidgetHost* render_widget_host) {
  RenderWidgetHostViewGtk* view =
      new RenderWidgetHostViewGtk(render_widget_host);
  view->InitAsChild();
  g_signal_connect(view->native_view(), "focus",
                   G_CALLBACK(OnFocus), delegate_->GetTabContents());
  g_signal_connect(view->native_view(), "scroll-event",
                   G_CALLBACK(OnMouseScroll), delegate_);

  // Let widget know that the tab contents has been painted.
  views::WidgetGtk::RegisterChildExposeHandler(view->native_view());

  // Renderer target DnD.
  if (delegate_->GetTabContents()->ShouldAcceptDragAndDrop())
    drag_dest_.reset(new WebDragDestGtk(delegate_->GetTabContents(),
                                        view->native_view()));

  gtk_fixed_put(GTK_FIXED(GetWidget()->GetNativeView()), view->native_view(), 0,
                0);
  return view;
}

gfx::NativeWindow NativeTabContentsViewGtk::GetTopLevelNativeWindow() const {
  GtkWidget* window = gtk_widget_get_ancestor(GetWidget()->GetNativeView(),
                                              GTK_TYPE_WINDOW);
  return window ? GTK_WINDOW(window) : NULL;
}

void NativeTabContentsViewGtk::SetPageTitle(const std::wstring& title) {
  // Set the window name to include the page title so it's easier to spot
  // when debugging (e.g. via xwininfo -tree).
  if (GDK_IS_WINDOW(GetNativeView()->window))
    gdk_window_set_title(GetNativeView()->window, WideToUTF8(title).c_str());
}

void NativeTabContentsViewGtk::StartDragging(const WebDropData& drop_data,
                                             WebKit::WebDragOperationsMask ops,
                                             const SkBitmap& image,
                                             const gfx::Point& image_offset) {
  drag_source_->StartDragging(drop_data, ops, &last_mouse_down_,
                              image, image_offset);
}

void NativeTabContentsViewGtk::CancelDrag() {
}

bool NativeTabContentsViewGtk::IsDoingDrag() const {
  return false;
}

void NativeTabContentsViewGtk::SetDragCursor(
    WebKit::WebDragOperation operation) {
  if (drag_dest_.get())
    drag_dest_->UpdateDragStatus(operation);
}

views::NativeWidget* NativeTabContentsViewGtk::AsNativeWidget() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewGtk, views::WidgetGtk overrides:

// Called when the mouse moves within the widget. We notify SadTabView if it's
// not NULL, else our delegate.
gboolean NativeTabContentsViewGtk::OnMotionNotify(GtkWidget* widget,
                                                  GdkEventMotion* event) {
  if (delegate_->IsShowingSadTab())
    return views::WidgetGtk::OnMotionNotify(widget, event);

  delegate_->OnNativeTabContentsViewMouseMove(true);
  return FALSE;
}

gboolean NativeTabContentsViewGtk::OnLeaveNotify(GtkWidget* widget,
                                                 GdkEventCrossing* event) {
  if (delegate_->IsShowingSadTab())
    return views::WidgetGtk::OnLeaveNotify(widget, event);

  delegate_->OnNativeTabContentsViewMouseMove(false);
  return FALSE;
}

gboolean NativeTabContentsViewGtk::OnButtonPress(GtkWidget* widget,
                                                 GdkEventButton* event) {
  if (delegate_->IsShowingSadTab())
    return views::WidgetGtk::OnButtonPress(widget, event);
  last_mouse_down_ = *event;
  return views::WidgetGtk::OnButtonPress(widget, event);
}

void NativeTabContentsViewGtk::OnSizeAllocate(GtkWidget* widget,
                                              GtkAllocation* allocation) {
  gfx::Size size(allocation->width, allocation->height);
  delegate_->OnNativeTabContentsViewSized(size);
  if (size != size_)
    PositionConstrainedWindows(size);
  size_ = size;
  views::WidgetGtk::OnSizeAllocate(widget, allocation);
}

void NativeTabContentsViewGtk::OnShow(GtkWidget* widget) {
  delegate_->OnNativeTabContentsViewShown();
  views::WidgetGtk::OnShow(widget);
}

void NativeTabContentsViewGtk::OnHide(GtkWidget* widget) {
  // OnHide can be called during widget destruction (gtk_widget_dispose calls
  // gtk_widget_hide) so we make sure we do not call back through to the
  // delegate after it's already deleted.
  if (delegate_)
    delegate_->OnNativeTabContentsViewHidden();
  views::WidgetGtk::OnHide(widget);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewGtk, private:

void NativeTabContentsViewGtk::PositionConstrainedWindows(
    const gfx::Size& view_size) {
  // Place each ConstrainedWindow in the center of the view.
  int half_view_width = view_size.width() / 2;

  typedef std::vector<ConstrainedWindowGtk*>::iterator iterator;

  for (iterator f = constrained_windows_.begin(),
                l = constrained_windows_.end(); f != l; ++f) {
    GtkWidget* widget = (*f)->widget();

    GtkRequisition requisition;
    gtk_widget_size_request(widget, &requisition);

    int child_x = std::max(half_view_width - (requisition.width / 2), 0);
    PositionChild(widget, child_x, 0, 0, 0);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsView, public:

// static
NativeTabContentsView* NativeTabContentsView::CreateNativeTabContentsView(
    internal::NativeTabContentsViewDelegate* delegate) {
  return new NativeTabContentsViewGtk(delegate);
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_gtk.h"

#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/web_drag_dest_gtk.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/gtk/tab_contents_drag_source.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_delegate.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;
using WebKit::WebInputEvent;

namespace {

// Called when the content view gtk widget is tabbed to, or after the call to
// gtk_widget_child_focus() in TakeFocus(). We return true
// and grab focus if we don't have it. The call to
// FocusThroughTabTraversal(bool) forwards the "move focus forward" effect to
// webkit.
gboolean OnFocus(GtkWidget* widget, GtkDirectionType focus,
                 TabContents* tab_contents) {
  // If we already have focus, let the next widget have a shot at it. We will
  // reach this situation after the call to gtk_widget_child_focus() in
  // TakeFocus().
  if (gtk_widget_is_focus(widget))
    return FALSE;

  gtk_widget_grab_focus(widget);
  bool reverse = focus == GTK_DIR_TAB_BACKWARD;
  tab_contents->FocusThroughTabTraversal(reverse);
  return TRUE;
}

// See tab_contents_view_gtk.cc for discussion of mouse scroll zooming.
gboolean OnMouseScroll(GtkWidget* widget, GdkEventScroll* event,
                       internal::NativeTabContentsViewDelegate* delegate) {
  if ((event->state & gtk_accelerator_get_default_mod_mask()) ==
      GDK_CONTROL_MASK) {
    if (event->direction == GDK_SCROLL_DOWN) {
      delegate->OnNativeTabContentsViewWheelZoom(false);
      return TRUE;
    }
    if (event->direction == GDK_SCROLL_UP) {
      delegate->OnNativeTabContentsViewWheelZoom(true);
      return TRUE;
    }
  }

  return FALSE;
}

gfx::NativeView GetHiddenTabHostWindow() {
  static views::Widget* widget = NULL;

  if (!widget) {
    widget = new views::Widget;
    // We don't want this widget to be closed automatically, this causes
    // problems in tests that close the last non-secondary window.
    widget->set_is_secondary_widget(false);
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    widget->Init(params);
  }

  return static_cast<views::WidgetGtk*>(widget->native_widget())->
      window_contents();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewGtk, public:

NativeTabContentsViewGtk::NativeTabContentsViewGtk(
    internal::NativeTabContentsViewDelegate* delegate)
    : views::WidgetGtk(delegate->AsNativeWidgetDelegate()),
      delegate_(delegate),
      ignore_next_char_event_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(drag_source_(
          new TabContentsDragSource(delegate->GetTabContents()->view()))) {
}

NativeTabContentsViewGtk::~NativeTabContentsViewGtk() {
  delegate_ = NULL;
  CloseNow();
}

void NativeTabContentsViewGtk::AttachConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  DCHECK(find(constrained_windows_.begin(), constrained_windows_.end(),
              constrained_window) == constrained_windows_.end());

  constrained_windows_.push_back(constrained_window);
  AddChild(constrained_window->widget());

  gfx::Size requested_size;
  views::WidgetGtk::GetRequestedSize(&requested_size);
  PositionConstrainedWindows(requested_size);
}

void NativeTabContentsViewGtk::RemoveConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  std::vector<ConstrainedWindowGtk*>::iterator item =
      find(constrained_windows_.begin(), constrained_windows_.end(),
           constrained_window);
  DCHECK(item != constrained_windows_.end());
  RemoveChild((*item)->widget());
  constrained_windows_.erase(item);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewGtk, NativeTabContentsView implementation:

void NativeTabContentsViewGtk::InitNativeTabContentsView() {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.native_widget = this;
  params.delete_on_destroy = false;
  GetWidget()->Init(params);

  // We need to own the widget in order to attach/detach the native view to a
  // container.
  gtk_object_ref(GTK_OBJECT(GetWidget()->GetNativeView()));
}

void NativeTabContentsViewGtk::Unparent() {
  // Note that we do not DCHECK on focus_manager_ as it may be NULL when used
  // with an external tab container.
  NativeWidget::ReparentNativeView(GetNativeView(), GetHiddenTabHostWindow());
}

RenderWidgetHostView* NativeTabContentsViewGtk::CreateRenderWidgetHostView(
    RenderWidgetHost* render_widget_host) {
  RenderWidgetHostViewGtk* view =
      new RenderWidgetHostViewGtk(render_widget_host);
  view->InitAsChild();
  g_signal_connect(view->native_view(), "focus",
                   G_CALLBACK(OnFocus), delegate_->GetTabContents());
  g_signal_connect(view->native_view(), "scroll-event",
                   G_CALLBACK(OnMouseScroll), delegate_);

  // Let widget know that the tab contents has been painted.
  views::WidgetGtk::RegisterChildExposeHandler(view->native_view());

  // Renderer target DnD.
  if (delegate_->GetTabContents()->ShouldAcceptDragAndDrop())
    drag_dest_.reset(new WebDragDestGtk(delegate_->GetTabContents(),
                                        view->native_view()));

  gtk_fixed_put(GTK_FIXED(GetWidget()->GetNativeView()), view->native_view(), 0,
                0);
  return view;
}

gfx::NativeWindow NativeTabContentsViewGtk::GetTopLevelNativeWindow() const {
  GtkWidget* window = gtk_widget_get_ancestor(GetWidget()->GetNativeView(),
                                              GTK_TYPE_WINDOW);
  return window ? GTK_WINDOW(window) : NULL;
}

void NativeTabContentsViewGtk::SetPageTitle(const std::wstring& title) {
  // Set the window name to include the page title so it's easier to spot
  // when debugging (e.g. via xwininfo -tree).
  if (GDK_IS_WINDOW(GetNativeView()->window))
    gdk_window_set_title(GetNativeView()->window, WideToUTF8(title).c_str());
}

void NativeTabContentsViewGtk::StartDragging(const WebDropData& drop_data,
                                             WebKit::WebDragOperationsMask ops,
                                             const SkBitmap& image,
                                             const gfx::Point& image_offset) {
  drag_source_->StartDragging(drop_data, ops, &last_mouse_down_,
                              image, image_offset);
}

void NativeTabContentsViewGtk::CancelDrag() {
}

bool NativeTabContentsViewGtk::IsDoingDrag() const {
  return false;
}

void NativeTabContentsViewGtk::SetDragCursor(
    WebKit::WebDragOperation operation) {
  if (drag_dest_.get())
    drag_dest_->UpdateDragStatus(operation);
}

views::NativeWidget* NativeTabContentsViewGtk::AsNativeWidget() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewGtk, views::WidgetGtk overrides:

// Called when the mouse moves within the widget. We notify SadTabView if it's
// not NULL, else our delegate.
gboolean NativeTabContentsViewGtk::OnMotionNotify(GtkWidget* widget,
                                                  GdkEventMotion* event) {
  if (delegate_->IsShowingSadTab())
    return views::WidgetGtk::OnMotionNotify(widget, event);

  delegate_->OnNativeTabContentsViewMouseMove(true);
  return FALSE;
}

gboolean NativeTabContentsViewGtk::OnLeaveNotify(GtkWidget* widget,
                                                 GdkEventCrossing* event) {
  if (delegate_->IsShowingSadTab())
    return views::WidgetGtk::OnLeaveNotify(widget, event);

  delegate_->OnNativeTabContentsViewMouseMove(false);
  return FALSE;
}

gboolean NativeTabContentsViewGtk::OnButtonPress(GtkWidget* widget,
                                                 GdkEventButton* event) {
  if (delegate_->IsShowingSadTab())
    return views::WidgetGtk::OnButtonPress(widget, event);
  last_mouse_down_ = *event;
  return views::WidgetGtk::OnButtonPress(widget, event);
}

void NativeTabContentsViewGtk::OnSizeAllocate(GtkWidget* widget,
                                              GtkAllocation* allocation) {
  gfx::Size size(allocation->width, allocation->height);
  delegate_->OnNativeTabContentsViewSized(size);
  if (size != size_)
    PositionConstrainedWindows(size);
  size_ = size;
  views::WidgetGtk::OnSizeAllocate(widget, allocation);
}

void NativeTabContentsViewGtk::OnShow(GtkWidget* widget) {
  delegate_->OnNativeTabContentsViewShown();
  views::WidgetGtk::OnShow(widget);
}

void NativeTabContentsViewGtk::OnHide(GtkWidget* widget) {
  // OnHide can be called during widget destruction (gtk_widget_dispose calls
  // gtk_widget_hide) so we make sure we do not call back through to the
  // delegate after it's already deleted.
  if (delegate_)
    delegate_->OnNativeTabContentsViewHidden();
  views::WidgetGtk::OnHide(widget);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewGtk, private:

void NativeTabContentsViewGtk::PositionConstrainedWindows(
    const gfx::Size& view_size) {
  // Place each ConstrainedWindow in the center of the view.
  int half_view_width = view_size.width() / 2;

  typedef std::vector<ConstrainedWindowGtk*>::iterator iterator;

  for (iterator f = constrained_windows_.begin(),
                l = constrained_windows_.end(); f != l; ++f) {
    GtkWidget* widget = (*f)->widget();

    GtkRequisition requisition;
    gtk_widget_size_request(widget, &requisition);

    int child_x = std::max(half_view_width - (requisition.width / 2), 0);
    PositionChild(widget, child_x, 0, 0, 0);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsView, public:

// static
NativeTabContentsView* NativeTabContentsView::CreateNativeTabContentsView(
    internal::NativeTabContentsViewDelegate* delegate) {
  return new NativeTabContentsViewGtk(delegate);
}

// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_gtk.h"

#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/tab_contents/web_drag_dest_gtk.h"
#include "chrome/browser/ui/gtk/constrained_window_gtk.h"
#include "chrome/browser/ui/gtk/tab_contents_drag_source.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_delegate.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDragData.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebInputEvent.h"

using WebKit::WebDragOperation;
using WebKit::WebDragOperationsMask;
using WebKit::WebInputEvent;

namespace {

/*
// Called when the content view gtk widget is tabbed to, or after the call to
// gtk_widget_child_focus() in TakeFocus(). We return true
// and grab focus if we don't have it. The call to
// FocusThroughTabTraversal(bool) forwards the "move focus forward" effect to
// webkit.
gboolean OnFocus(GtkWidget* widget, GtkDirectionType focus,
                 TabContents* tab_contents) {
  // If we already have focus, let the next widget have a shot at it. We will
  // reach this situation after the call to gtk_widget_child_focus() in
  // TakeFocus().
  if (gtk_widget_is_focus(widget))
    return FALSE;

  gtk_widget_grab_focus(widget);
  bool reverse = focus == GTK_DIR_TAB_BACKWARD;
  tab_contents->FocusThroughTabTraversal(reverse);
  return TRUE;
}

// Called when the mouse leaves the widget. We notify our delegate.
// WidgetGtk also defines OnLeaveNotify, so we use the name OnLeaveNotify2
// here.
gboolean OnLeaveNotify2(GtkWidget* widget, GdkEventCrossing* event,
                        TabContents* tab_contents) {
  if (tab_contents->delegate())
    tab_contents->delegate()->ContentsMouseEvent(
        tab_contents, views::Screen::GetCursorScreenPoint(), false);
  return FALSE;
}

// Called when the mouse moves within the widget.
gboolean CallMouseMove(GtkWidget* widget, GdkEventMotion* event,
                       TabContentsViewGtk* tab_contents_view) {
  return tab_contents_view->OnMouseMove(widget, event);
}

// See tab_contents_view_gtk.cc for discussion of mouse scroll zooming.
gboolean OnMouseScroll(GtkWidget* widget, GdkEventScroll* event,
                       TabContents* tab_contents) {
  if ((event->state & gtk_accelerator_get_default_mod_mask()) ==
      GDK_CONTROL_MASK) {
    if (tab_contents->delegate()) {
      if (event->direction == GDK_SCROLL_DOWN) {
        tab_contents->delegate()->ContentsZoomChange(false);
        return TRUE;
      } else if (event->direction == GDK_SCROLL_UP) {
        tab_contents->delegate()->ContentsZoomChange(true);
        return TRUE;
      }
    }
  }

  return FALSE;
}
*/

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewGtk, public:

NativeTabContentsViewGtk::NativeTabContentsViewGtk(
    internal::NativeTabContentsViewDelegate* delegate)
    : views::WidgetGtk(delegate->AsNativeWidgetDelegate()),
      delegate_(delegate),
      ignore_next_char_event_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(drag_source_(
          new TabContentsDragSource(delegate->GetTabContents()->view()))) {
}

NativeTabContentsViewGtk::~NativeTabContentsViewGtk() {
  CloseNow();
}

void NativeTabContentsViewGtk::AttachConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  DCHECK(find(constrained_windows_.begin(), constrained_windows_.end(),
              constrained_window) == constrained_windows_.end());

  constrained_windows_.push_back(constrained_window);
  AddChild(constrained_window->widget());

  gfx::Size requested_size;
  views::WidgetGtk::GetRequestedSize(&requested_size);
  PositionConstrainedWindows(requested_size);
}

void NativeTabContentsViewGtk::RemoveConstrainedWindow(
    ConstrainedWindowGtk* constrained_window) {
  std::vector<ConstrainedWindowGtk*>::iterator item =
      find(constrained_windows_.begin(), constrained_windows_.end(),
           constrained_window);
  DCHECK(item != constrained_windows_.end());
  RemoveChild((*item)->widget());
  constrained_windows_.erase(item);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewGtk, NativeTabContentsView implementation:

void NativeTabContentsViewGtk::InitNativeTabContentsView() {
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.native_widget = this;
  params.delete_on_destroy = false;
  GetWidget()->Init(params);

  // We need to own the widget in order to attach/detach the native view to a
  // container.
  gtk_object_ref(GTK_OBJECT(GetWidget()->GetNativeView()));
}

void NativeTabContentsViewGtk::Unparent() {
}

RenderWidgetHostView* NativeTabContentsViewGtk::CreateRenderWidgetHostView(
    RenderWidgetHost* render_widget_host) {
  RenderWidgetHostViewGtk* view =
      new RenderWidgetHostViewGtk(render_widget_host);
  view->InitAsChild();
  /*
  g_signal_connect(view->native_view(), "focus",
                   G_CALLBACK(OnFocus), delegate_->GetTabContents());
  g_signal_connect(view->native_view(), "leave-notify-event",
                   G_CALLBACK(OnLeaveNotify2), delegate_->GetTabContents());
  g_signal_connect(view->native_view(), "motion-notify-event",
                   G_CALLBACK(CallMouseMove), this);
  g_signal_connect(view->native_view(), "scroll-event",
                   G_CALLBACK(OnMouseScroll), delegate_->GetTabContents());
                   */
  gtk_widget_add_events(view->native_view(), GDK_LEAVE_NOTIFY_MASK |
                        GDK_POINTER_MOTION_MASK);

  // Let widget know that the tab contents has been painted.
  views::WidgetGtk::RegisterChildExposeHandler(view->native_view());

  // Renderer target DnD.
  if (delegate_->GetTabContents()->ShouldAcceptDragAndDrop())
    drag_dest_.reset(new WebDragDestGtk(delegate_->GetTabContents(),
                                        view->native_view()));

  gtk_fixed_put(GTK_FIXED(GetWidget()->GetNativeView()), view->native_view(), 0,
                0);
  return view;
}

gfx::NativeWindow NativeTabContentsViewGtk::GetTopLevelNativeWindow() const {
  GtkWidget* window = gtk_widget_get_ancestor(GetWidget()->GetNativeView(),
                                              GTK_TYPE_WINDOW);
  return window ? GTK_WINDOW(window) : NULL;
}

void NativeTabContentsViewGtk::SetPageTitle(const std::wstring& title) {
  // Set the window name to include the page title so it's easier to spot
  // when debugging (e.g. via xwininfo -tree).
  gdk_window_set_title(GetNativeView()->window, WideToUTF8(title).c_str());
}

void NativeTabContentsViewGtk::StartDragging(const WebDropData& drop_data,
                                             WebKit::WebDragOperationsMask ops,
                                             const SkBitmap& image,
                                             const gfx::Point& image_offset) {
  drag_source_->StartDragging(drop_data, ops, &last_mouse_down_,
                              image, image_offset);
}

void NativeTabContentsViewGtk::CancelDrag() {
}

bool NativeTabContentsViewGtk::IsDoingDrag() const {
  return false;
}

void NativeTabContentsViewGtk::SetDragCursor(
    WebKit::WebDragOperation operation) {
  if (drag_dest_.get())
    drag_dest_->UpdateDragStatus(operation);
}

views::NativeWidget* NativeTabContentsViewGtk::AsNativeWidget() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewGtk, views::WidgetGtk overrides:

// Called when the mouse moves within the widget. We notify SadTabView if it's
// not NULL, else our delegate.
gboolean NativeTabContentsViewGtk::OnMotionNotify(GtkWidget* widget,
                                                  GdkEventMotion* event) {
  if (delegate_->IsShowingSadTab())
    return views::WidgetGtk::OnMotionNotify(widget, event);

  delegate_->OnNativeTabContentsViewMouseMove();
  return FALSE;
}

gboolean NativeTabContentsViewGtk::OnButtonPress(GtkWidget* widget,
                                                 GdkEventButton* event) {
  if (delegate_->IsShowingSadTab())
    return views::WidgetGtk::OnButtonPress(widget, event);
  last_mouse_down_ = *event;
  return views::WidgetGtk::OnButtonPress(widget, event);
}

void NativeTabContentsViewGtk::OnSizeAllocate(GtkWidget* widget,
                                              GtkAllocation* allocation) {
  gfx::Size size(allocation->width, allocation->height);
  delegate_->OnNativeTabContentsViewSized(size);
  if (size != size_)
    PositionConstrainedWindows(size);
  size_ = size;
  views::WidgetGtk::OnSizeAllocate(widget, allocation);
}

void NativeTabContentsViewGtk::OnShow(GtkWidget* widget) {
  delegate_->OnNativeTabContentsViewShown();
  views::WidgetGtk::OnShow(widget);
}

void NativeTabContentsViewGtk::OnHide(GtkWidget* widget) {
  delegate_->OnNativeTabContentsViewHidden();
  views::WidgetGtk::OnHide(widget);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsViewGtk, private:

void NativeTabContentsViewGtk::PositionConstrainedWindows(
    const gfx::Size& view_size) {
  // Place each ConstrainedWindow in the center of the view.
  int half_view_width = view_size.width() / 2;

  typedef std::vector<ConstrainedWindowGtk*>::iterator iterator;

  for (iterator f = constrained_windows_.begin(),
                l = constrained_windows_.end(); f != l; ++f) {
    GtkWidget* widget = (*f)->widget();

    GtkRequisition requisition;
    gtk_widget_size_request(widget, &requisition);

    int child_x = std::max(half_view_width - (requisition.width / 2), 0);
    PositionChild(widget, child_x, 0, 0, 0);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsView, public:

// static
NativeTabContentsView* NativeTabContentsView::CreateNativeTabContentsView(
    internal::NativeTabContentsViewDelegate* delegate) {
  return new NativeTabContentsViewGtk(delegate);
}

