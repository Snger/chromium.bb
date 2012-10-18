// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_WEB_NOTIFICATION_MESSAGE_CENTER_BUBBLE_H_
#define ASH_SYSTEM_WEB_NOTIFICATION_MESSAGE_CENTER_BUBBLE_H_

#include "ash/ash_export.h"
#include "ash/system/web_notification/web_notification_bubble.h"
#include "ash/system/web_notification/web_notification_list.h"

namespace message_center {

class MessageCenterContentsView;

// Bubble for message center.
class ASH_EXPORT MessageCenterBubble : public WebNotificationBubble {
 public:
  explicit MessageCenterBubble(WebNotificationList::Delegate* delegate);

  virtual ~MessageCenterBubble();

  // Overridden from WebNotificationBubble.
  virtual TrayBubbleView::InitParams GetInitParams(
      TrayBubbleView::AnchorAlignment anchor_alignment) OVERRIDE;
  virtual void InitializeContents(TrayBubbleView* bubble_view) OVERRIDE;
  virtual void OnBubbleViewDestroyed() OVERRIDE;
  virtual void UpdateBubbleView() OVERRIDE;
  virtual void OnMouseEnteredView() OVERRIDE;
  virtual void OnMouseExitedView() OVERRIDE;

  size_t NumMessageViewsForTest() const;

 private:
  MessageCenterContentsView* contents_view_;

  DISALLOW_COPY_AND_ASSIGN(MessageCenterBubble);
};

}  // namespace message_center

#endif // ASH_SYSTEM_WEB_NOTIFICATION_MESSAGE_CENTER_BUBBLE_H_
