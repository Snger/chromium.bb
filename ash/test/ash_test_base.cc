// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_base.h"

#include <string>
#include <vector>

#include "ash/display/display_controller.h"
#include "ash/display/display_manager.h"
#include "ash/shell.h"
#include "ash/test/display_manager_test_api.h"
#include "ash/test/test_shell_delegate.h"
#include "base/run_loop.h"
#include "content/public/test/web_contents_tester.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/ime/text_input_test_support.h"
#include "ui/compositor/layer_animator.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace test {

content::WebContents* AshTestViewsDelegate::CreateWebContents(
    content::BrowserContext* browser_context,
    content::SiteInstance* site_instance) {
  return content::WebContentsTester::CreateTestWebContents(browser_context,
                                                           site_instance);
}

AshTestBase::AshTestBase() : test_shell_delegate_(NULL) {
}

AshTestBase::~AshTestBase() {
}

void AshTestBase::SetUp() {
  // Disable animations during tests.
  ui::LayerAnimator::set_disable_animations_for_test(true);
  ui::TextInputTestSupport::Initialize();
  // Creates Shell and hook with Desktop.
  test_shell_delegate_ = new TestShellDelegate;
  ash::Shell::CreateInstance(test_shell_delegate_);
  Shell::GetPrimaryRootWindow()->Show();
  // Move the mouse cursor to far away so that native events doesn't
  // interfere test expectations.
  Shell::GetPrimaryRootWindow()->MoveCursorTo(gfx::Point(-1000, -1000));
  UpdateDisplay("800x600");
  Shell::GetInstance()->cursor_manager()->ShowCursor(true);
}

void AshTestBase::TearDown() {
  // Flush the message loop to finish pending release tasks.
  RunAllPendingInMessageLoop();

  // Tear down the shell.
  Shell::DeleteInstance();
  aura::Env::DeleteInstance();
  ui::TextInputTestSupport::Shutdown();
}

void AshTestBase::ChangeDisplayConfig(float scale,
                                      const gfx::Rect& bounds_in_pixel) {
  gfx::Display display =
      gfx::Display(Shell::GetScreen()->GetPrimaryDisplay().id());
  display.SetScaleAndBounds(scale, bounds_in_pixel);
  std::vector<gfx::Display> displays;
  displays.push_back(display);
  Shell::GetInstance()->display_manager()->OnNativeDisplaysChanged(displays);
}

void AshTestBase::UpdateDisplay(const std::string& display_specs) {
  DisplayManagerTestApi display_manager_test_api(
      Shell::GetInstance()->display_manager());
  display_manager_test_api.UpdateDisplay(display_specs);
}

aura::Window* AshTestBase::CreateTestWindowInShellWithId(int id) {
  return CreateTestWindowInShellWithDelegate(NULL, id, gfx::Rect());
}

aura::Window* AshTestBase::CreateTestWindowInShellWithBounds(
    const gfx::Rect& bounds) {
  return CreateTestWindowInShellWithDelegate(NULL, 0, bounds);
}

aura::Window* AshTestBase::CreateTestWindowInShell(SkColor color,
                                                   int id,
                                                   const gfx::Rect& bounds) {
  return CreateTestWindowInShellWithDelegate(
      new aura::test::ColorTestWindowDelegate(color), id, bounds);
}

aura::Window* AshTestBase::CreateTestWindowInShellWithDelegate(
    aura::WindowDelegate* delegate,
    int id,
    const gfx::Rect& bounds) {
  return CreateTestWindowInShellWithDelegateAndType(
      delegate,
      aura::client::WINDOW_TYPE_NORMAL,
      id,
      bounds);
}

aura::Window* AshTestBase::CreateTestWindowInShellWithDelegateAndType(
    aura::WindowDelegate* delegate,
    aura::client::WindowType type,
    int id,
    const gfx::Rect& bounds) {
  aura::Window* window = new aura::Window(delegate);
  window->set_id(id);
  window->SetType(type);
  window->Init(ui::LAYER_TEXTURED);
  window->SetBounds(bounds);
  window->Show();
  SetDefaultParentByPrimaryRootWindow(window);
  window->SetProperty(aura::client::kCanMaximizeKey, true);
  return window;
}

void AshTestBase::SetDefaultParentByPrimaryRootWindow(aura::Window* window) {
  window->SetDefaultParentByRootWindow(
      Shell::GetPrimaryRootWindow(), gfx::Rect());
}

void AshTestBase::RunAllPendingInMessageLoop() {
#if !defined(OS_MACOSX)
  DCHECK(MessageLoopForUI::current() == &message_loop_);
  base::RunLoop run_loop(aura::Env::GetInstance()->GetDispatcher());
  run_loop.RunUntilIdle();
#endif
}

void AshTestBase::SetSessionStarted(bool session_started) {
  test_shell_delegate_->SetSessionStarted(session_started);
}

void AshTestBase::SetUserLoggedIn(bool user_logged_in) {
  test_shell_delegate_->SetUserLoggedIn(user_logged_in);
}

void AshTestBase::SetCanLockScreen(bool can_lock_screen) {
  test_shell_delegate_->SetCanLockScreen(can_lock_screen);
}

}  // namespace test
}  // namespace ash
