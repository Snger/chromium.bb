// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_LAUNCHER_CONTEXT_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_LAUNCHER_CONTEXT_MENU_H_
#pragma once

#include "ash/launcher/launcher_types.h"
#include "base/basictypes.h"
#include "ui/base/models/simple_menu_model.h"

namespace gfx {
class Point;
}

namespace views {
class MenuRunner;
class View;
}

class ChromeLauncherDelegate;

// Context menu shown for a launcher item.
class LauncherContextMenu : public ui::SimpleMenuModel,
                            public ui::SimpleMenuModel::Delegate {
 public:
  LauncherContextMenu(ChromeLauncherDelegate* delegate, ash::LauncherID id);
  virtual ~LauncherContextMenu();

  // ID of the item we're showing the context menu for.
  ash::LauncherID id() const { return id_; }

  // ui::SimpleMenuModel::Delegate overrides:
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

 private:
  enum MenuItem {
    MENU_OPEN,
    MENU_PIN,
    MENU_CLOSE
  };

  ChromeLauncherDelegate* delegate_;
  const ash::LauncherID id_;

  DISALLOW_COPY_AND_ASSIGN(LauncherContextMenu);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_LAUNCHER_LAUNCHER_CONTEXT_MENU_H_
