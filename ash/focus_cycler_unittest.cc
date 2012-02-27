// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/focus_cycler.h"

#include "ash/launcher/launcher.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/window_util.h"
#include "ash/test/ash_test_base.h"
#include "ash/shell_factory.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace test {

using aura::test::CreateTestWindowWithId;
using aura::Window;
using internal::FocusCycler;

typedef AshTestBase FocusCyclerTest;

TEST_F(FocusCyclerTest, CycleFocusBrowserOnly) {
  scoped_ptr<FocusCycler> focus_cycler(new FocusCycler());

  // Create a single test window.
  Window* default_container =
      ash::Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_DefaultContainer);
  scoped_ptr<Window> window0(CreateTestWindowWithId(0, default_container));
  ActivateWindow(window0.get());
  EXPECT_TRUE(IsActiveWindow(window0.get()));

  // Cycle the window
  focus_cycler->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, CycleFocusForward) {
  scoped_ptr<FocusCycler> focus_cycler(new FocusCycler());

  // Add the Status area
  views::Widget* status_widget = internal::CreateStatusArea();
  ASSERT_TRUE(status_widget);
  focus_cycler->AddWidget(status_widget);

  // Add a mock button to the status area.
  status_widget->GetContentsView()->AddChildView(
      new views::MenuButton(NULL, string16(), NULL, false));

  // Add the launcher
  Launcher* launcher = Shell::GetInstance()->launcher();
  ASSERT_TRUE(launcher);
  views::Widget* launcher_widget = launcher->widget();
  ASSERT_TRUE(launcher_widget);
  focus_cycler->AddWidget(launcher_widget);
  launcher->SetFocusCycler(focus_cycler.get());

  // Create a single test window.
  Window* default_container =
      ash::Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_DefaultContainer);
  scoped_ptr<Window> window0(CreateTestWindowWithId(0, default_container));
  ActivateWindow(window0.get());
  EXPECT_TRUE(IsActiveWindow(window0.get()));

  // Cycle focus to the status area
  focus_cycler->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(status_widget->IsActive());

  // Cycle focus to the launcher
  focus_cycler->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(launcher_widget->IsActive());

  // Cycle focus to the browser
  focus_cycler->RotateFocus(FocusCycler::FORWARD);
  EXPECT_TRUE(IsActiveWindow(window0.get()));
}

TEST_F(FocusCyclerTest, CycleFocusBackward) {
  scoped_ptr<FocusCycler> focus_cycler(new FocusCycler());

  // Add the Status area
  views::Widget* status_widget = internal::CreateStatusArea();
  ASSERT_TRUE(status_widget);
  focus_cycler->AddWidget(status_widget);

  // Add a mock button to the status area.
  status_widget->GetContentsView()->AddChildView(
      new views::MenuButton(NULL, string16(), NULL, false));

  // Add the launcher
  Launcher* launcher = Shell::GetInstance()->launcher();
  ASSERT_TRUE(launcher);
  views::Widget* launcher_widget = launcher->widget();
  ASSERT_TRUE(launcher_widget);
  focus_cycler->AddWidget(launcher_widget);
  launcher->SetFocusCycler(focus_cycler.get());

  // Create a single test window.
  Window* default_container =
      ash::Shell::GetInstance()->GetContainer(
          internal::kShellWindowId_DefaultContainer);
  scoped_ptr<Window> window0(CreateTestWindowWithId(0, default_container));
  ActivateWindow(window0.get());
  EXPECT_TRUE(IsActiveWindow(window0.get()));

  // Cycle focus to the launcher
  focus_cycler->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(launcher_widget->IsActive());

  // Cycle focus to the status area
  focus_cycler->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(status_widget->IsActive());

  // Cycle focus to the browser
  focus_cycler->RotateFocus(FocusCycler::BACKWARD);
  EXPECT_TRUE(IsActiveWindow(window0.get()));
}

}  // namespace test
}  // namespace ash
