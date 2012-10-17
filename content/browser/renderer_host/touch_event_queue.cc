// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/touch_event_queue.h"

#include "content/browser/renderer_host/render_widget_host_impl.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/port/browser/render_widget_host_view_port.h"

namespace content {

TouchEventQueue::TouchEventQueue(RenderWidgetHostImpl* host)
    : render_widget_host_(host) {
}

TouchEventQueue::~TouchEventQueue() {
}

void TouchEventQueue::QueueEvent(const WebKit::WebTouchEvent& event) {
  if (touch_queue_.empty()) {
    // There is no touch event in the queue. Forward it to the renderer
    // immediately.
    touch_queue_.push_back(event);
    render_widget_host_->ForwardTouchEventImmediately(event);
    return;
  }

  // TODO(sad): Coalesce with |touch_queue_.back()| if appropriate.
  // http://crbug.com/110231
  touch_queue_.push_back(event);
}

void TouchEventQueue::ProcessTouchAck(bool processed) {
  CHECK(!touch_queue_.empty());
  WebKit::WebTouchEvent acked_event = touch_queue_.front();
  touch_queue_.pop_front();

  // Note that acking the touch-event may result in multiple gestures being sent
  // to the renderer.
  RenderWidgetHostViewPort* view = RenderWidgetHostViewPort::FromRWHV(
      render_widget_host_->GetView());
  view->ProcessAckedTouchEvent(acked_event, processed);

  // If there's a queued touch-event, then forward it to the renderer now.
  if (!touch_queue_.empty())
    render_widget_host_->ForwardTouchEventImmediately(touch_queue_.front());
}

}  // namespace content
