// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_BOUNDED_LABEL_H_
#define UI_MESSAGE_CENTER_BOUNDED_LABEL_H_

#include <list>
#include <map>

#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/message_center/message_center_export.h"
#include "ui/views/view.h"

namespace gfx {

class Font;

}  // namespace gfx

namespace message_center {

class InnerBoundedLabel;

// BoundedLabels display left aligned text up to a maximum number of lines, with
// ellipsis at the end of the last line for any omitted text. BoundedLabel is a
// direct subclass of views::Views rather than a subclass of views::Label
// because of limitations in views::Label's implementation. See the description
// of InnerBoundedLabel in the .cc file for details.
class MESSAGE_CENTER_EXPORT BoundedLabel : public views::View {
 public:
  BoundedLabel(const string16& text, gfx::Font font, size_t line_limit);
  BoundedLabel(const string16& text, size_t line_limit);
  virtual ~BoundedLabel();

  void SetLineLimit(size_t lines);
  size_t GetLinesForWidth(int width);
  size_t GetPreferredLines();
  size_t GetActualLines();

  void SetColors(SkColor textColor, SkColor backgroundColor);

  // Overridden from views::View.
  virtual int GetBaseline() const OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual int GetHeightForWidth(int w) OVERRIDE;
  virtual void Paint(gfx::Canvas* canvas);
  virtual bool HitTestRect(const gfx::Rect& rect) const OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

 protected:
  // Overridden from views::View.
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual void OnNativeThemeChanged(const ui::NativeTheme* theme) OVERRIDE;

 private:
  friend class BoundedLabelTest;

  string16 GetWrappedTextForTest(int width, size_t line_limit);

  scoped_ptr<InnerBoundedLabel> label_;

  DISALLOW_COPY_AND_ASSIGN(BoundedLabel);
};

}  // namespace message_center

#endif  // UI_MESSAGE_CENTER_BOUNDED_LABEL_H_
