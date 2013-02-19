// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_TEXTINPUT_TEST_HELPER_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_TEXTINPUT_TEST_HELPER_H_

#include "chrome/test/base/in_process_browser_test.h"
#include "ui/base/ime/mock_input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/range/range.h"
#include "ui/gfx/rect.h"

namespace chromeos {

// The base class of text input testing.
class TextInputTestBase : public InProcessBrowserTest {
 public:
  TextInputTestBase() {}
  virtual ~TextInputTestBase() {}

  virtual void SetUpInProcessBrowserTestFixture() OVERRIDE;
 private:
  DISALLOW_COPY_AND_ASSIGN(TextInputTestBase);
};

// Provides text input test utilities.
class TextInputTestHelper : public ui::MockInputMethod::Observer {
 public:
  TextInputTestHelper();
  virtual ~TextInputTestHelper();

  // Returns the latest status notified to ui::InputMethod
  std::string GetSurroundingText() const;
  gfx::Rect GetCaretRect() const;
  gfx::Rect GetCompositionHead() const;
  ui::Range GetSelectionRange() const;
  bool GetFocusState() const;
  ui::TextInputType GetTextInputType() const;

  // Waiting function for each input method events. These functions runs message
  // loop until the expected event comes.
  void WaitForTextInputStateChanged(ui::TextInputType expected_type);
  void WaitForFocus();
  void WaitForBlur();
  void WaitForCaretBoundsChanged(const gfx::Rect& expected_caret_rect,
                                 const gfx::Rect& expected_composition_head);
  void WaitForSurroundingTextChanged(const std::string& expected_text,
                                     const ui::Range& expected_selection);

 private:
  enum WaitImeEventType {
    NO_WAIT,
    WAIT_ON_BLUR,
    WAIT_ON_CARET_BOUNDS_CHANGED,
    WAIT_ON_FOCUS,
    WAIT_ON_TEXT_INPUT_TYPE_CHANGED,
  };

  // ui::MockInputMethod::Observer overrides.
  virtual void OnTextInputTypeChanged(
      const ui::TextInputClient* client) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual void OnCaretBoundsChanged(const ui::TextInputClient* client) OVERRIDE;

  // Represents waiting type of text input event.
  WaitImeEventType waiting_type_;

  std::string surrounding_text_;
  gfx::Rect caret_rect_;
  gfx::Rect composition_head_;
  ui::Range selection_range_;
  bool focus_state_;
  ui::TextInputType latest_text_input_type_;

  DISALLOW_COPY_AND_ASSIGN(TextInputTestHelper);
};

} // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_TEXTINPUT_TEST_HELPER_H_
