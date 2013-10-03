// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/autoclick/autoclick_controller.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ui/aura/test/event_generator.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/event_handler.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace ash {

class MouseEventCapturer : public ui::EventHandler {
 public:
  MouseEventCapturer() { Reset(); }

  void Reset() {
    events_.clear();
  }

  virtual void OnMouseEvent(ui::MouseEvent* event) OVERRIDE {
    if (!(event->flags() & ui::EF_LEFT_MOUSE_BUTTON))
      return;
    // Filter out extraneous mouse events like mouse entered, exited,
    // capture changed, etc.
    ui::EventType type = event->type();
    if (type == ui::ET_MOUSE_MOVED || type == ui::ET_MOUSE_PRESSED ||
        type == ui::ET_MOUSE_RELEASED) {
      events_.push_back(ui::MouseEvent(
          event->type(),
          event->location(),
          event->root_location(),
          event->flags()));
    }
  }

  const std::vector<ui::MouseEvent>& captured_events() const {
    return events_;
  }

 private:
  std::vector<ui::MouseEvent> events_;

  DISALLOW_COPY_AND_ASSIGN(MouseEventCapturer);
};

class AutoclickTest : public test::AshTestBase {
 public:
  AutoclickTest() {}
  virtual ~AutoclickTest() {}

  virtual void SetUp() OVERRIDE {
    test::AshTestBase::SetUp();
    Shell::GetInstance()->AddPreTargetHandler(&mouse_event_capturer_);
    GetAutoclickController()->SetClickWaitTime(0);

    // Move mouse to deterministic location at the start of each test.
    GetEventGenerator().MoveMouseTo(10, 10);
  }

  virtual void TearDown() OVERRIDE {
    Shell::GetInstance()->RemovePreTargetHandler(&mouse_event_capturer_);
    test::AshTestBase::TearDown();
  }

  void MoveMouseWithFlagsTo(int x, int y, ui::EventFlags flags) {
    GetEventGenerator().set_flags(flags);
    GetEventGenerator().MoveMouseTo(x, y);
    GetEventGenerator().set_flags(ui::EF_NONE);
  }

  const std::vector<ui::MouseEvent>& WaitForMouseEvents() {
    mouse_event_capturer_.Reset();
    RunAllPendingInMessageLoop();
    return mouse_event_capturer_.captured_events();
  }

  AutoclickController* GetAutoclickController() {
    return Shell::GetInstance()->autoclick_controller();
  }

 private:
  MouseEventCapturer mouse_event_capturer_;

  DISALLOW_COPY_AND_ASSIGN(AutoclickTest);
};

TEST_F(AutoclickTest, ToggleEnabled) {
  std::vector<ui::MouseEvent> events;

  // We should not see any events initially.
  EXPECT_FALSE(GetAutoclickController()->IsEnabled());
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());

  // Enable autoclick, and we should see a mouse pressed and
  // a mouse released event, simulating a click.
  GetAutoclickController()->SetEnabled(true);
  GetEventGenerator().MoveMouseTo(0, 0);
  EXPECT_TRUE(GetAutoclickController()->IsEnabled());
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, events[0].type());
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, events[1].type());

  // We should not get any more clicks until we move the mouse.
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());
  GetEventGenerator().MoveMouseTo(0, 1);
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(ui::ET_MOUSE_PRESSED, events[0].type());
  EXPECT_EQ(ui::ET_MOUSE_RELEASED, events[1].type());

  // Disable autoclick, and we should see the original behaviour.
  GetAutoclickController()->SetEnabled(false);
  EXPECT_FALSE(GetAutoclickController()->IsEnabled());
  events = WaitForMouseEvents();
  EXPECT_EQ(0u, events.size());
}

#if defined(OS_WIN)
// On Windows, we are getting unexpected mouse drag events that
// are breaking this test. See http://crbug.com/303830.
#define MAYBE_MouseMovement \
        DISABLED_MouseMovement
#else
#define MAYBE_MouseMovement \
        MouseMovement
#endif
TEST_F(AutoclickTest, MAYBE_MouseMovement) {
  std::vector<ui::MouseEvent> events;
  GetAutoclickController()->SetEnabled(true);

  gfx::Point p1(1, 1);
  gfx::Point p2(2, 2);
  gfx::Point p3(3, 3);

  // Move mouse to p1.
  GetEventGenerator().MoveMouseTo(p1);
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(p1.ToString(), events[0].root_location().ToString());
  EXPECT_EQ(p1.ToString(), events[1].root_location().ToString());

  // Move mouse to multiple locations and finally arrive at p3.
  GetEventGenerator().MoveMouseTo(p2);
  GetEventGenerator().MoveMouseTo(p1);
  GetEventGenerator().MoveMouseTo(p3);
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(p3.ToString(), events[0].root_location().ToString());
  EXPECT_EQ(p3.ToString(), events[1].root_location().ToString());
}

TEST_F(AutoclickTest, SingleKeyModifier) {
  GetAutoclickController()->SetEnabled(true);
  MoveMouseWithFlagsTo(20, 20, ui::EF_SHIFT_DOWN);
  std::vector<ui::MouseEvent> events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(ui::EF_SHIFT_DOWN, events[0].flags() & ui::EF_SHIFT_DOWN);
  EXPECT_EQ(ui::EF_SHIFT_DOWN, events[1].flags() & ui::EF_SHIFT_DOWN);
}

TEST_F(AutoclickTest, MultipleKeyModifiers) {
  GetAutoclickController()->SetEnabled(true);
  ui::EventFlags modifier_flags = static_cast<ui::EventFlags>(
      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  MoveMouseWithFlagsTo(30, 30, modifier_flags);
  std::vector<ui::MouseEvent> events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(modifier_flags, events[0].flags() & modifier_flags);
  EXPECT_EQ(modifier_flags, events[1].flags() & modifier_flags);
}

#if defined(OS_WIN)
// Multiple displays are not supported on Windows Ash. http://crbug.com/165962
#define MAYBE_ExtendedDisplay \
        DISABLED_ExtendedDisplay
#else
#define MAYBE_ExtendedDisplay \
        ExtendedDisplay
#endif
TEST_F(AutoclickTest, KeyModifiersReleased) {
  GetAutoclickController()->SetEnabled(true);

  ui::EventFlags modifier_flags = static_cast<ui::EventFlags>(
      ui::EF_CONTROL_DOWN | ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  MoveMouseWithFlagsTo(12, 12, modifier_flags);

  // Simulate releasing key modifiers by sending key released events.
  GetEventGenerator().ReleaseKey(ui::VKEY_CONTROL,
      static_cast<ui::EventFlags>(ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN));
  GetEventGenerator().ReleaseKey(ui::VKEY_SHIFT, ui::EF_ALT_DOWN);

  std::vector<ui::MouseEvent> events;
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(0, events[0].flags() & ui::EF_CONTROL_DOWN);
  EXPECT_EQ(0, events[0].flags() & ui::EF_SHIFT_DOWN);
  EXPECT_EQ(ui::EF_ALT_DOWN, events[0].flags() & ui::EF_ALT_DOWN);
}

#if defined(OS_WIN)
// Multiple displays are not supported on Windows Ash. http://crbug.com/165962
#define MAYBE_ExtendedDisplay \
        DISABLED_ExtendedDisplay
#else
#define MAYBE_ExtendedDisplay \
        ExtendedDisplay
#endif
TEST_F(AutoclickTest, MAYBE_ExtendedDisplay) {
  UpdateDisplay("1280x1024,800x600");
  RunAllPendingInMessageLoop();
  Shell::RootWindowList root_windows = Shell::GetAllRootWindows();
  EXPECT_EQ(2u, root_windows.size());

  GetAutoclickController()->SetEnabled(true);
  std::vector<ui::MouseEvent> events;

  // Test first root window.
  aura::test::EventGenerator generator1(root_windows[0]);
  generator1.MoveMouseTo(100, 200);
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(100, events[0].root_location().x());
  EXPECT_EQ(200, events[0].root_location().y());

  // Test second root window.
  aura::test::EventGenerator generator2(root_windows[1]);
  generator2.MoveMouseTo(300, 400);
  events = WaitForMouseEvents();
  EXPECT_EQ(2u, events.size());
  EXPECT_EQ(300, events[0].root_location().x());
  EXPECT_EQ(400, events[0].root_location().y());
}

}  // namespace ash
