// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ui/message_center/cocoa/notification_controller.h"

#include "base/strings/sys_string_conversions.h"
#include "grit/ui_resources.h"
#include "skia/ext/skia_utils_mac.h"
#import "third_party/GTM/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#import "ui/base/cocoa/hover_image_button.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_constants.h"
#include "ui/message_center/notification.h"

@interface MCNotificationController (Private)
// Configures a NSBox to be borderless, titleless, and otherwise appearance-
// free.
- (void)configureCustomBox:(NSBox*)box;

// Initializes the icon_ ivar and returns the view to insert into the hierarchy.
- (NSView*)createImageView;

// Initializes the closeButton_ ivar with the configured button.
- (void)configureCloseButtonInFrame:(NSRect)rootFrame;

// Initializes title_ in the given frame.
- (void)configureTitleInFrame:(NSRect)rootFrame;

// Initializes message_ in the given frame.
- (void)configureBodyInFrame:(NSRect)rootFrame;

// Creates a NSTextField that the caller owns configured as a label in a
// notification.
- (NSTextField*)newLabelWithFrame:(NSRect)frame;

// Gets the rectangle in which notification content should be placed. This
// rectangle is to the right of the icon and left of the control buttons.
// This depends on the icon_ and closeButton_ being initialized.
- (NSRect)currentContentRect;
@end

@implementation MCNotificationController

- (id)initWithNotification:(const message_center::Notification*)notification
    messageCenter:(message_center::MessageCenter*)messageCenter {
  if ((self = [super initWithNibName:nil bundle:nil])) {
    notification_ = notification;
    notificationID_ = notification_->id();
    messageCenter_ = messageCenter;
  }
  return self;
}

- (void)loadView {
  // Create the root view of the notification.
  NSRect rootFrame = NSMakeRect(0, 0,
      message_center::kNotificationPreferredImageSize,
      message_center::kNotificationIconSize);
  scoped_nsobject<NSBox> rootView([[NSBox alloc] initWithFrame:rootFrame]);
  [self configureCustomBox:rootView];
  [rootView setFillColor:gfx::SkColorToCalibratedNSColor(
      message_center::kNotificationBackgroundColor)];
  [self setView:rootView];

  [rootView addSubview:[self createImageView]];

  // Create the close button.
  [self configureCloseButtonInFrame:rootFrame];
  [rootView addSubview:closeButton_];

  // Create the title.
  [self configureTitleInFrame:rootFrame];
  [rootView addSubview:title_];

  // Create the message body.
  [self configureBodyInFrame:rootFrame];
  [rootView addSubview:message_];

  // Populate the data.
  [self updateNotification:notification_];
}

- (NSRect)updateNotification:(const message_center::Notification*)notification {
  DCHECK_EQ(notification->id(), notificationID_);
  notification_ = notification;

  NSRect rootFrame = NSMakeRect(0, 0,
      message_center::kNotificationPreferredImageSize,
      message_center::kNotificationIconSize);

  // Update the icon.
  [icon_ setImage:notification_->icon().AsNSImage()];

  // Set the title and recalculate the frame.
  [title_ setStringValue:base::SysUTF16ToNSString(notification_->title())];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:title_];
  NSRect titleFrame = [title_ frame];
  titleFrame.origin.y = NSMaxY(rootFrame) - message_center::kTextTopPadding -
                        NSHeight(titleFrame);

  // Set the message and recalculate the frame.
  [message_ setStringValue:base::SysUTF16ToNSString(notification_->message())];
  [GTMUILocalizerAndLayoutTweaker sizeToFitFixedWidthTextField:message_];
  NSRect messageFrame = [message_ frame];
  messageFrame.origin.y = NSMinY(titleFrame) - message_center::kTextTopPadding -
                          NSHeight(messageFrame);
  messageFrame.size.height = NSHeight([message_ frame]);

  // In this basic notification UI, the message body is the bottom-most
  // vertical element. If it is out of the rootView's bounds, resize the view.
  if (NSMinY(messageFrame) < message_center::kTextTopPadding) {
    CGFloat delta = message_center::kTextTopPadding - NSMinY(messageFrame);
    rootFrame.size.height += delta;
    titleFrame.origin.y += delta;
    messageFrame.origin.y += delta;
  }

  [[self view] setFrame:rootFrame];
  [title_ setFrame:titleFrame];
  [message_ setFrame:messageFrame];

  return rootFrame;
}

- (void)close:(id)sender {
  messageCenter_->RemoveNotification(notification_->id(), /*by_user=*/true);
}

- (const message_center::Notification*)notification {
  return notification_;
}

- (const std::string&)notificationID {
  return notificationID_;
}

// Private /////////////////////////////////////////////////////////////////////

- (void)configureCustomBox:(NSBox*)box {
  [box setBoxType:NSBoxCustom];
  [box setBorderType:NSNoBorder];
  [box setTitlePosition:NSNoTitle];
  [box setContentViewMargins:NSZeroSize];
}

- (NSView*)createImageView {
  // Create another box that shows a background color when the icon is not
  // big enough to fill the space.
  NSRect imageFrame = NSMakeRect(0, 0,
       message_center::kNotificationIconSize,
       message_center::kNotificationIconSize);
  scoped_nsobject<NSBox> imageBox([[NSBox alloc] initWithFrame:imageFrame]);
  [self configureCustomBox:imageBox];
  [imageBox setFillColor:gfx::SkColorToCalibratedNSColor(
      message_center::kLegacyIconBackgroundColor)];
  [imageBox setAutoresizingMask:NSViewMinYMargin];

  // Inside the image box put the actual icon view.
  icon_.reset([[NSImageView alloc] initWithFrame:imageFrame]);
  [imageBox setContentView:icon_];

  return imageBox.autorelease();
}

- (void)configureCloseButtonInFrame:(NSRect)rootFrame {
  closeButton_.reset([[HoverImageButton alloc] initWithFrame:NSMakeRect(
      NSMaxX(rootFrame) - message_center::kControlButtonSize,
      NSMaxY(rootFrame) - message_center::kControlButtonSize,
      message_center::kControlButtonSize,
      message_center::kControlButtonSize)]);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  [closeButton_ setDefaultImage:
      rb.GetNativeImageNamed(IDR_NOTIFICATION_CLOSE).ToNSImage()];
  [closeButton_ setDefaultOpacity:1];
  [closeButton_ setHoverImage:
      rb.GetNativeImageNamed(IDR_NOTIFICATION_CLOSE_HOVER).ToNSImage()];
  [closeButton_ setHoverOpacity:1];
  [closeButton_ setPressedImage:
      rb.GetNativeImageNamed(IDR_NOTIFICATION_CLOSE_PRESSED).ToNSImage()];
  [closeButton_ setPressedOpacity:1];
  [[closeButton_ cell] setHighlightsBy:NSOnState];
  [closeButton_ setTrackingEnabled:YES];
  [closeButton_ setBordered:NO];
  [closeButton_ setAutoresizingMask:NSViewMinYMargin];
  [closeButton_ setTarget:self];
  [closeButton_ setAction:@selector(close:)];
}

- (void)configureTitleInFrame:(NSRect)rootFrame {
  NSRect frame = [self currentContentRect];
  frame.size.height = 0;
  title_.reset([self newLabelWithFrame:frame]);
  [title_ setAutoresizingMask:NSViewMinYMargin];
  [title_ setFont:[NSFont messageFontOfSize:message_center::kTitleFontSize]];
}

- (void)configureBodyInFrame:(NSRect)rootFrame {
  NSRect frame = [self currentContentRect];
  frame.size.height = 0;
  message_.reset([self newLabelWithFrame:frame]);
  [message_ setAutoresizingMask:NSViewMinYMargin];
  [message_ setFont:
      [NSFont messageFontOfSize:message_center::kMessageFontSize]];
}

- (NSTextField*)newLabelWithFrame:(NSRect)frame {
  NSTextField* label = [[NSTextField alloc] initWithFrame:frame];
  [label setDrawsBackground:NO];
  [label setBezeled:NO];
  [label setEditable:NO];
  [label setSelectable:NO];
  [label setTextColor:gfx::SkColorToCalibratedNSColor(
      message_center::kRegularTextColor)];
  return label;
}

- (NSRect)currentContentRect {
  DCHECK(icon_);
  DCHECK(closeButton_);

  NSRect iconFrame, contentFrame;
  NSDivideRect([[self view] bounds], &iconFrame, &contentFrame,
      NSWidth([icon_ frame]) + message_center::kIconToTextPadding,
      NSMinXEdge);
  contentFrame.size.width -= NSWidth([closeButton_ frame]);
  return contentFrame;
}

@end
