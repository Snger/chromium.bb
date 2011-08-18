// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AURA_DESKTOP_H_
#define AURA_DESKTOP_H_
#pragma once

#include "aura/window.h"
#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Size;
}

namespace ui {
class Compositor;
}

namespace aura {

class Window;

// Desktop is responsible for hosting a set of windows.
class Desktop {
 public:
  Desktop(gfx::AcceleratedWidget widget, const gfx::Size& size);
  ~Desktop();

  // Draws the necessary set of windows.
  void Draw();

  // Compositor we're drawing to.
  ui::Compositor* compositor() { return compositor_.get(); }

  Window* window() { return window_.get(); }

 private:
  scoped_refptr<ui::Compositor> compositor_;

  scoped_ptr<Window> window_;

  DISALLOW_COPY_AND_ASSIGN(Desktop);
};

}  // namespace aura

#endif  // AURA_DESKTOP_H_
