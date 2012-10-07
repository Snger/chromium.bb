// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#include "chrome/browser/ui/cocoa/run_loop_testing.h"
#import "testing/gtest_mac.h"

class ConstrainedWindowSheetControllerTest : public CocoaTest {
 protected:
  ~ConstrainedWindowSheetControllerTest() {
  }

  virtual void SetUp() OVERRIDE {
    CocoaTest::SetUp();

    // Center the window so that the sheet doesn't go offscreen.
    [test_window() center];

    // Create two dummy tabs and make the first one active.
    NSRect dummyRect = NSMakeRect(0, 0, 50, 50);
    tab_views_.reset([[NSMutableArray alloc] init]);
    for (int i = 0; i < 2; ++i) {
      scoped_nsobject<NSView> view([[NSView alloc] initWithFrame:dummyRect]);
      [tab_views_ addObject:view];
    }
    ActivateTabView([tab_views_ objectAtIndex:0]);

    // Create a test sheet.
    sheet_.reset([[NSWindow alloc] initWithContentRect:dummyRect
                                             styleMask:NSTitledWindowMask
                                               backing:NSBackingStoreBuffered
                                                 defer:NO]);
    [sheet_ setReleasedWhenClosed:NO];

    EXPECT_FALSE([ConstrainedWindowSheetController controllerForSheet:sheet_]);
  }

  virtual void TearDown() OVERRIDE {
    sheet_.reset();
    CocoaTest::TearDown();
  }

  void ActivateTabView(NSView* tab_view) {
    for (NSView* view in tab_views_.get()) {
      [view removeFromSuperview];
    }
    [[test_window() contentView] addSubview:tab_view];
    active_tab_view_.reset([tab_view retain]);

    ConstrainedWindowSheetController* controller =
        [ConstrainedWindowSheetController
            controllerForParentWindow:test_window()];
    EXPECT_TRUE(controller);
    [controller parentViewDidBecomeActive:active_tab_view_];
  }

  NSRect GetViewFrameInScreenCoordinates(NSView* view) {
    NSRect rect = [view convertRect:[view bounds] toView:nil];
    rect.origin = [[view window] convertBaseToScreen:rect.origin];
    return rect;
  }

  scoped_nsobject<NSWindow> sheet_;
  scoped_nsobject<NSMutableArray> tab_views_;
  scoped_nsobject<NSView> active_tab_view_;
};

// Test showing then hiding the sheet.
TEST_F(ConstrainedWindowSheetControllerTest, ShowHide) {
  ConstrainedWindowSheetController* controller =
      [ConstrainedWindowSheetController
          controllerForParentWindow:test_window()];
  EXPECT_TRUE(controller);

  EXPECT_FALSE([sheet_ isVisible]);
  [controller showSheet:sheet_ forParentView:active_tab_view_];
  EXPECT_TRUE([ConstrainedWindowSheetController controllerForSheet:sheet_]);
  EXPECT_TRUE([sheet_ isVisible]);

  [controller closeSheet:sheet_];
  [controller endAnimationForSheet:sheet_];
  chrome::testing::NSRunLoopRunAllPending();
  [controller endAnimationForSheet:sheet_];
  chrome::testing::NSRunLoopRunAllPending();
  EXPECT_FALSE([ConstrainedWindowSheetController controllerForSheet:sheet_]);
  EXPECT_FALSE([sheet_ isVisible]);
}

// Test that switching tabs correctly hides the inactive tab's sheet.
TEST_F(ConstrainedWindowSheetControllerTest, SwitchTabs) {
  ConstrainedWindowSheetController* controller =
      [ConstrainedWindowSheetController
          controllerForParentWindow:test_window()];
  [controller showSheet:sheet_ forParentView:active_tab_view_];

  EXPECT_TRUE([sheet_ isVisible]);
  EXPECT_EQ(1.0, [sheet_ alphaValue]);
  ActivateTabView([tab_views_ objectAtIndex:1]);
  EXPECT_TRUE([sheet_ isVisible]);
  EXPECT_EQ(0.0, [sheet_ alphaValue]);
  ActivateTabView([tab_views_ objectAtIndex:0]);
  EXPECT_TRUE([sheet_ isVisible]);
  EXPECT_EQ(1.0, [sheet_ alphaValue]);
}

// Test that adding a sheet to an inactive view doesn't show it.
TEST_F(ConstrainedWindowSheetControllerTest, AddToInactiveTab) {
  ConstrainedWindowSheetController* controller =
      [ConstrainedWindowSheetController
          controllerForParentWindow:test_window()];
  NSView* tab0 = [tab_views_ objectAtIndex:0];
  NSView* tab1 = [tab_views_ objectAtIndex:1];

  ActivateTabView(tab0);
  [controller showSheet:sheet_ forParentView:tab1];
  EXPECT_EQ(0.0, [sheet_ alphaValue]);

  ActivateTabView(tab1);
  EXPECT_EQ(1.0, [sheet_ alphaValue]);
}

// Test that two parent windows with two sheet controllers don't conflict.
TEST_F(ConstrainedWindowSheetControllerTest, TwoParentWindows) {
  ConstrainedWindowSheetController* controller1 =
      [ConstrainedWindowSheetController
          controllerForParentWindow:test_window()];
  EXPECT_TRUE(controller1);

  scoped_nsobject<NSWindow> parent_window2([[NSWindow alloc]
      initWithContentRect:NSMakeRect(0, 0, 30, 30)
                styleMask:NSTitledWindowMask
                  backing:NSBackingStoreBuffered
                    defer:NO]);
  [parent_window2 setReleasedWhenClosed:NO];

  ConstrainedWindowSheetController* controller2 =
      [ConstrainedWindowSheetController
          controllerForParentWindow:parent_window2];
  EXPECT_TRUE(controller2);
  EXPECT_NSNE(controller1, controller2);

  [controller2 showSheet:sheet_ forParentView:[parent_window2 contentView]];
  EXPECT_NSEQ(controller2,
              [ConstrainedWindowSheetController controllerForSheet:sheet_]);

  [parent_window2 close];
}

// Test that using a top level parent view works.
TEST_F(ConstrainedWindowSheetControllerTest, TopLevelView) {
  ConstrainedWindowSheetController* controller =
      [ConstrainedWindowSheetController
          controllerForParentWindow:test_window()];
  EXPECT_TRUE(controller);

  NSView* parentView = [[test_window() contentView] superview];
  [controller parentViewDidBecomeActive:parentView];

  EXPECT_FALSE([sheet_ isVisible]);
  [controller showSheet:sheet_ forParentView:parentView];
  EXPECT_TRUE([ConstrainedWindowSheetController controllerForSheet:sheet_]);
  EXPECT_TRUE([sheet_ isVisible]);

  NSRect parent_frame = GetViewFrameInScreenCoordinates(parentView);
  NSRect sheet_frame = [sheet_ frame];
  CGFloat expected_x = NSMinX(parent_frame) +
      (NSWidth(parent_frame) - NSWidth(sheet_frame)) / 2.0;
  EXPECT_EQ(expected_x, NSMinX(sheet_frame));
}

// Test that resizing sheet works.
TEST_F(ConstrainedWindowSheetControllerTest, Resize) {
  ConstrainedWindowSheetController* controller =
      [ConstrainedWindowSheetController
          controllerForParentWindow:test_window()];
  [controller showSheet:sheet_ forParentView:active_tab_view_];

  NSRect old_frame = [sheet_ frame];
  NSSize desired_size = NSMakeSize(NSWidth(old_frame) + 100,
                                   NSHeight(old_frame) + 50);
  [controller setSheet:sheet_
            windowSize:desired_size];

  NSRect sheet_frame = [sheet_ frame];
  EXPECT_EQ(NSWidth(sheet_frame), desired_size.width);
  EXPECT_EQ(NSHeight(sheet_frame), desired_size.height);

  // Y pos should not have changed.
  EXPECT_EQ(NSMaxY(sheet_frame), NSMaxY(old_frame));

  // X pos should be centered on parent view.
  NSRect parent_frame = GetViewFrameInScreenCoordinates(active_tab_view_);
  CGFloat expected_x = NSMinX(parent_frame) +
      (NSWidth(parent_frame) - NSWidth(sheet_frame)) / 2.0;
  EXPECT_EQ(expected_x, NSMinX(sheet_frame));
}
