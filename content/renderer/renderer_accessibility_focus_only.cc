// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_accessibility_focus_only.h"

#include "content/renderer/render_view_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDocument.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebNode.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebView.h"
#include "webkit/glue/webaccessibility.h"

using WebKit::WebDocument;
using WebKit::WebFrame;
using WebKit::WebNode;
using WebKit::WebView;
using webkit_glue::WebAccessibility;

namespace {
// The root node will always have id 1. Let each child node have a new
// id starting with 2.
const int kInitialId = 2;
}

namespace content {

RendererAccessibilityFocusOnly::RendererAccessibilityFocusOnly(
    RenderViewImpl* render_view)
    : RendererAccessibility(render_view),
      next_id_(kInitialId) {
}

RendererAccessibilityFocusOnly::~RendererAccessibilityFocusOnly() {
}

void RendererAccessibilityFocusOnly::HandleWebAccessibilityNotification(
    const WebKit::WebAccessibilityObject& obj,
    WebKit::WebAccessibilityNotification notification) {
  // Do nothing.
}

void RendererAccessibilityFocusOnly::FocusedNodeChanged(const WebNode& node) {
  // Send the new accessible tree and post a native focus event.
  HandleFocusedNodeChanged(node, true);
}

void RendererAccessibilityFocusOnly::DidFinishLoad(WebKit::WebFrame* frame) {
  WebView* view = render_view()->GetWebView();
  if (view->focusedFrame() != frame)
    return;

  WebDocument document = frame->document();
  // Send an accessible tree to the browser, but do not post a native
  // focus event. This is important so that if focus is initially in an
  // editable text field, Windows will know to pop up the keyboard if the
  // user touches it and focus doesn't change.
  HandleFocusedNodeChanged(document.focusedNode(), false);
}

void RendererAccessibilityFocusOnly::HandleFocusedNodeChanged(
    const WebNode& node,
    bool sent_focus_event) {
  const WebDocument& document = GetMainDocument();
  if (document.isNull())
    return;

  std::vector<AccessibilityHostMsg_NotificationParams> notifications;
  notifications.push_back(AccessibilityHostMsg_NotificationParams());
  AccessibilityHostMsg_NotificationParams& notification = notifications[0];

  // If we want to update the browser's accessibility tree but not send a
  // native focus changed notification, we can send a LayoutComplete
  // notification, which doesn't post a native event on Windows.
  notification.notification_type =
      sent_focus_event ?
      AccessibilityNotificationFocusChanged :
      AccessibilityNotificationLayoutComplete;

  // This means that the new tree we send supercedes any previous tree,
  // not just a previous node.
  notification.includes_children = true;

  // Set the id that the notification applies to: the root node if nothing
  // has focus, otherwise the focused node.
  notification.id = node.isNull() ? 1 : next_id_;

  // Always include the root of the tree, the document. It always has id 1.
  notification.acc_tree.id = 1;
  notification.acc_tree.role = WebAccessibility::ROLE_ROOT_WEB_AREA;
  notification.acc_tree.state =
      (1 << WebAccessibility::STATE_READONLY) |
      (1 << WebAccessibility::STATE_FOCUSABLE);
  if (node.isNull())
    notification.acc_tree.state |= (1 << WebAccessibility::STATE_FOCUSED);
  notification.acc_tree.location = gfx::Rect(render_view_->size());

  notification.acc_tree.children.push_back(WebAccessibility());
  WebAccessibility& child = notification.acc_tree.children[0];
  child.id = next_id_;
  child.role = WebAccessibility::ROLE_GROUP;
  child.location = gfx::Rect(render_view_->size());
  if (!node.isNull()) {
    child.state =
        (1 << WebAccessibility::STATE_FOCUSABLE) |
        (1 << WebAccessibility::STATE_FOCUSED);
    if (!render_view_->IsEditableNode(node))
      child.state |= (1 << WebAccessibility::STATE_READONLY);
  }

#ifndef NDEBUG
  if (logging_) {
    LOG(INFO) << "Accessibility update: \n"
        << "routing id=" << routing_id()
        << " notification="
        << AccessibilityNotificationToString(notification.notification_type)
        << "\n" << notification.acc_tree.DebugString(true);
  }
#endif

  Send(new AccessibilityHostMsg_Notifications(routing_id(), notifications));

  // Increment the id, wrap back when we get past a million.
  next_id_++;
  if (next_id_ > 1000000)
    next_id_ = kInitialId;
}

}  // namespace content
