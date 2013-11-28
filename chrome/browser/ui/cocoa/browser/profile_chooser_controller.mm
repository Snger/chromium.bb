// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#import "chrome/browser/ui/cocoa/browser/profile_chooser_controller.h"

#include "base/mac/bundle_locations.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/avatar_menu.h"
#include "chrome/browser/profiles/profile_info_cache.h"
#include "chrome/browser/profiles/profile_info_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#import "chrome/browser/ui/cocoa/info_bubble_view.h"
#import "chrome/browser/ui/cocoa/info_bubble_window.h"
#include "chrome/browser/ui/singleton_tabs.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#import "ui/base/cocoa/cocoa_event_utils.h"
#include "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {

// Constants taken from the Windows/Views implementation at:
// chrome/browser/ui/views/profile_chooser_view.cc
const int kLargeImageSide = 64;
const int kSmallImageSide = 32;

const CGFloat kMinMenuWidth = 250;
const CGFloat kVerticalSpacing = 20.0;
const CGFloat kSmallVerticalSpacing = 10.0;
const CGFloat kHorizontalSpacing = 20.0;
const CGFloat kTitleFontSize = 15.0;

gfx::Image CreateProfileImage(const gfx::Image& icon, int imageSize) {
  return profiles::GetSizedAvatarIconWithBorder(
      icon, true /* image is a square */,
      imageSize + profiles::kAvatarIconPadding,
      imageSize + profiles::kAvatarIconPadding);
}

// Should only be called before the window is shown, as that sets the window
// position.
void SetWindowSize(NSWindow* window, NSSize size) {
  NSRect frame = [window frame];
  frame.size = size;
  [window setFrame:frame display:YES];
}

}  // namespace

@interface ProfileChooserController (Private)
// Creates the main profile card for the profile |itemIndex| at the top of
// the bubble.
- (NSView*)createCurrentProfileView:(int)itemIndex;

// Creates an item for the profile |itemIndex| that is used in the fast profile
// switcher in the middle of the bubble.
- (NSButton*)createOtherProfileView:(int)itemIndex;

// Creates the Guest / Add person / View all persons buttons. |isGuestView| is
// used to determine the text and functionality of the Guest button.
- (NSView*)createOptionsViewWithRect:(NSRect)rect
                         isGuestView:(BOOL)isGuestView;

// Creates a generic button with text given by |textResourceId| and an icon
// given by |imageResourceId|.
- (NSButton*)makeButtonWithRect:(NSRect)rect
             withTextResourceId:(int)textResourceId
                 withResourceId:(int)imageResourceId;

// Creates all the subviews of the avatar bubble.
- (void)initMenuContents;
@end

@implementation ProfileChooserController

- (IBAction)addNewProfile:(id)sender {
  profiles::CreateAndSwitchToNewProfile(
      browser_->host_desktop_type(),
      profiles::ProfileSwitchingDoneCallback());
}

- (IBAction)switchToProfile:(id)sender {
  // Check the event flags to see if a new window should be created.
  bool always_create = ui::WindowOpenDispositionFromNSEvent(
      [NSApp currentEvent]) == NEW_WINDOW;
  avatarMenu_->SwitchToProfile([sender tag], always_create);
}

- (IBAction)showUserManager:(id)sender {
  // TODO(noms): This should use chrome::ShowUserManager() when the
  // implementation is ready.
  chrome::ShowSingletonTab(browser_, GURL(chrome::kChromeUIUserManagerURL));
}

- (IBAction)switchToGuestProfile:(id)sender {
  profiles::SwitchToGuestProfile(browser_->host_desktop_type(),
                                 profiles::ProfileSwitchingDoneCallback());
}

- (IBAction)exitGuestProfile:(id)sender {
  profiles::CloseGuestProfileWindows();
}

- (id)initWithBrowser:(Browser*)browser anchoredAt:(NSPoint)point {
  browser_ = browser;
  // TODO(noms): Add an observer when profile name editing is implemented.
  avatarMenu_.reset(new AvatarMenu(
      &g_browser_process->profile_manager()->GetProfileInfoCache(),
      NULL,
      browser));
  avatarMenu_->RebuildMenu();

  base::scoped_nsobject<InfoBubbleWindow> window([[InfoBubbleWindow alloc]
      initWithContentRect:ui::kWindowSizeDeterminedLater
                styleMask:NSBorderlessWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  if ((self = [super initWithWindow:window
                       parentWindow:browser->window()->GetNativeWindow()
                         anchoredAt:point])) {
    [[self bubble] setArrowLocation:info_bubble::kTopRight];
    [self initMenuContents];
  }
  return self;
}

- (void)initMenuContents {
  NSView* contentView = [[self window] contentView];

  // Separate items into active and other profiles.
  NSView* currentProfileView = nil;
  base::scoped_nsobject<NSMutableArray> otherProfiles(
      [[NSMutableArray alloc] init]);
  BOOL isGuestView = YES;

  for (size_t i = 0; i < avatarMenu_->GetNumberOfItems(); ++i) {
    if (avatarMenu_->GetItemAt(i).active) {
      currentProfileView = [self createCurrentProfileView:i];
      // The avatar menu only contains non-guest profiles, so an active profile
      // implies this is not a guest session browser.
      isGuestView = NO;
    } else {
      [otherProfiles addObject:[self createOtherProfileView:i]];
    }
  }

  CGFloat updatedMenuWidth =
      std::max(kMinMenuWidth, NSWidth([currentProfileView frame]));

  // |yOffset| is the next position at which to draw in |contentView|
  // coordinates.
  CGFloat yOffset = kVerticalSpacing;

  // Guest / Add Person / View All Persons buttons.
  NSRect viewRect = NSMakeRect(kHorizontalSpacing, yOffset,
                               updatedMenuWidth, 0);
  NSView* optionsView = [self createOptionsViewWithRect:viewRect
                                            isGuestView:isGuestView];
  [contentView addSubview:optionsView];
  yOffset = NSMaxY([optionsView frame]) + kVerticalSpacing;

  NSBox* separator =
      [self separatorWithFrame:NSMakeRect(0, yOffset, updatedMenuWidth, 0)];
  [contentView addSubview:separator];
  yOffset = NSMaxY([separator frame]) + kVerticalSpacing;

  // Other profiles switcher.
  for (NSView *otherProfileView in otherProfiles.get()) {
    [otherProfileView setFrameOrigin:NSMakePoint(kHorizontalSpacing, yOffset)];
    [contentView addSubview:otherProfileView];
    yOffset = NSMaxY([otherProfileView frame]) + kSmallVerticalSpacing;
  }

  // If we displayed other profiles, ensure the spacing between the last item
  // and the active profile card is the same as the spacing between the active
  // profile card and the bottom of the bubble.
  if ([otherProfiles.get() count] > 0)
    yOffset += kSmallVerticalSpacing;

  // Active profile card.
  if (currentProfileView) {
    // Don't need to size this view, as it was done at its creation.
    [currentProfileView setFrameOrigin:NSMakePoint(0, yOffset)];
    [contentView addSubview:currentProfileView];
    yOffset = NSMaxY([currentProfileView frame]) + kVerticalSpacing;
  }

  SetWindowSize([self window], NSMakeSize(updatedMenuWidth, yOffset));
}

- (NSView*)createCurrentProfileView:(int)itemIndex {
  const AvatarMenu::Item& item = avatarMenu_->GetItemAt(itemIndex);
  base::scoped_nsobject<NSView> container([[NSView alloc]
      initWithFrame:NSZeroRect]);

  // Profile icon.
  base::scoped_nsobject<NSImageView> iconView([[NSImageView alloc]
      initWithFrame:NSMakeRect(0, 0, kLargeImageSide, kLargeImageSide)]);
  [iconView setImage:CreateProfileImage(
      item.icon, kLargeImageSide).ToNSImage()];
  // Position the image correctly so that the width of the container can be
  // used to correctly resize the bubble if needed.
  [iconView setFrameOrigin:NSMakePoint(kHorizontalSpacing, 0)];
  [container addSubview:iconView];

  // Profile name.
  base::scoped_nsobject<NSTextField> profileName([[NSTextField alloc]
      initWithFrame:NSZeroRect]);
  [profileName setStringValue:base::SysUTF16ToNSString(item.name)];
  [profileName setFont:[NSFont labelFontOfSize:kTitleFontSize]];
  [profileName setEditable:NO];
  [profileName setDrawsBackground:NO];
  [profileName setBezeled:NO];
  [profileName setFrameOrigin:NSMakePoint(
      NSMaxX([iconView frame]) + kHorizontalSpacing, kVerticalSpacing)];
  [profileName sizeToFit];

  [container setSubviews:@[iconView, profileName]];
  [container setFrameSize:NSMakeSize(
      NSMaxX([profileName frame]) + kHorizontalSpacing,
      NSHeight([iconView frame]))];
  return container.autorelease();
}

- (NSButton*)createOtherProfileView:(int)itemIndex {
  const AvatarMenu::Item& item = avatarMenu_->GetItemAt(itemIndex);
  base::scoped_nsobject<NSButton> profileButton([[NSButton alloc]
      initWithFrame:NSZeroRect]);

  // TODO(noms): Increase the spacing between the icon and the text to 10px;
  [profileButton setTitle:base::SysUTF16ToNSString(item.name)];
  [profileButton setImage:CreateProfileImage(
      item.icon, kSmallImageSide).ToNSImage()];
  [profileButton setImagePosition:NSImageLeft];
  [profileButton setBordered:NO];
  [profileButton setFont:[NSFont labelFontOfSize:kTitleFontSize]];
  [profileButton setTag:itemIndex];
  [profileButton setTarget:self];
  [profileButton setAction:@selector(switchToProfile:)];
  [profileButton sizeToFit];

  return profileButton.autorelease();
}

- (NSView*)createOptionsViewWithRect:(NSRect)rect
                         isGuestView:(BOOL)isGuestView {
  CGFloat yOffset = 0;
  base::scoped_nsobject<NSView> container([[NSView alloc] initWithFrame:rect]);

  NSButton* allUsersButton =
      [self makeButtonWithRect:NSMakeRect(0, yOffset, 0, 0)
            withTextResourceId:IDS_PROFILES_ALL_PEOPLE_BUTTON
                withResourceId:IDR_ICON_PROFILES_ADD_USER];
  [allUsersButton setTarget:self];
  [allUsersButton setAction:@selector(showUserManager:)];
  yOffset = NSMaxY([allUsersButton frame]) + kSmallVerticalSpacing;

  NSButton* addUserButton =
      [self makeButtonWithRect:NSMakeRect(0, yOffset, 0, 0)
            withTextResourceId:IDS_PROFILES_ADD_PERSON_BUTTON
                withResourceId:IDR_ICON_PROFILES_ADD_USER];
  [addUserButton setTarget:self];
  [addUserButton setAction:@selector(addNewProfile:)];
  yOffset = NSMaxY([addUserButton frame]) + kSmallVerticalSpacing;

  int guestButtonText =
      isGuestView ? IDS_PROFILES_EXIT_GUEST_BUTTON: IDS_PROFILES_GUEST_BUTTON;
  NSButton* guestButton =
      [self makeButtonWithRect:NSMakeRect(0, yOffset, 0, 0)
            withTextResourceId:guestButtonText
                withResourceId:IDR_ICON_PROFILES_BROWSE_GUEST];
  [guestButton setTarget:self];
  if (isGuestView)
    [guestButton setAction:@selector(exitGuestProfile:)];
  else
    [guestButton setAction:@selector(switchToGuestProfile:)];
  yOffset = NSMaxY([guestButton frame]);

  [container setSubviews:@[allUsersButton, addUserButton, guestButton]];
  [container setFrameSize:NSMakeSize(NSWidth([container frame]), yOffset)];
  return container.autorelease();
}

- (NSButton*)makeButtonWithRect:(NSRect)rect
             withTextResourceId:(int)textResourceId
                 withResourceId:(int)imageResourceId {
  base::scoped_nsobject<NSButton> button([[NSButton alloc] initWithFrame:rect]);

  // TODO(noms): Increase the spacing between the icon and the text to 10px;
  [button setTitle:l10n_util::GetNSString(textResourceId)];
  [button setImage:ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      imageResourceId).ToNSImage()];
  [button setImagePosition:NSImageLeft];
  [button setBordered:NO];
  [button sizeToFit];

  return button.autorelease();
}

@end

