// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_DELEGATE_H_
#define CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_DELEGATE_H_

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "ui/base/window_open_disposition.h"

namespace content {
class WebContents;
}

namespace gfx {
class Image;
}

// Operations to be performed on the dialog by the
// TabModalConfirmDialogDelegate.
class TabModalConfirmDialogOperationsDelegate {
 public:
  TabModalConfirmDialogOperationsDelegate() {}
  virtual ~TabModalConfirmDialogOperationsDelegate() {}

  virtual void CloseDialog() = 0;
  virtual void SetPreventCloseOnLoadStart(bool prevent) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabModalConfirmDialogOperationsDelegate);
};

// This class acts as the delegate for a simple tab-modal dialog confirming
// whether the user wants to execute a certain action.
class TabModalConfirmDialogDelegate {
 public:
  TabModalConfirmDialogDelegate();
  virtual ~TabModalConfirmDialogDelegate();

  void set_operations_delegate(
      TabModalConfirmDialogOperationsDelegate* operations_delegate) {
    operations_delegate_ = operations_delegate;
  }

  // Accepts the confirmation prompt and calls |OnAccepted|.
  // This method is safe to call even from an |OnAccepted| or |OnCanceled|
  // callback.
  void Accept();

  // Cancels the confirmation prompt and calls |OnCanceled|.
  // This method is safe to call even from an |OnAccepted| or |OnCanceled|
  // callback.
  void Cancel();

  // Called when the link (if any) is clicked. Calls |OnLinkClicked| and closes
  // the dialog. The |disposition| specifies how the resulting document should
  // be loaded (based on the event flags present when the link was clicked).
  void LinkClicked(WindowOpenDisposition disposition);

  // The title of the dialog. Note that the title is not shown on all platforms.
  virtual string16 GetTitle() = 0;
  virtual string16 GetMessage() = 0;

  // Icon to show for the dialog. If this method is not overridden, a default
  // icon (like the application icon) is shown.
  virtual gfx::Image* GetIcon();

  // Title for the accept and the cancel buttons.
  // The default implementation uses IDS_OK and IDS_CANCEL.
  virtual string16 GetAcceptButtonTitle();
  virtual string16 GetCancelButtonTitle();

  // Returns the text of the link to be displayed, if any. Otherwise returns
  // an empty string.
  virtual string16 GetLinkText() const;

  // GTK stock icon names for the accept and cancel buttons, respectively.
  // The icons are only used on GTK. If these methods are not overriden,
  // the buttons have no stock icons.
  virtual const char* GetAcceptButtonIcon();
  virtual const char* GetCancelButtonIcon();

 protected:
  TabModalConfirmDialogOperationsDelegate* operations_delegate() {
    return operations_delegate_;
  }

 private:
  // It is guaranteed that exactly one of |OnAccepted|, |OnCanceled| or
  // |OnLinkClicked| is eventually called. These method are private to
  // enforce this guarantee. Access to them is controlled by |Accept|,
  // |Cancel| and |LinkClicked|.

  // Called when the user accepts or cancels the dialog, respectively.
  virtual void OnAccepted();
  virtual void OnCanceled();

  // Called when the user clicks on the link (if any).
  virtual void OnLinkClicked(WindowOpenDisposition disposition);

  // Close the dialog.
  void CloseDialog();

  TabModalConfirmDialogOperationsDelegate* operations_delegate_;
  // True iff we are in the process of closing, to avoid running callbacks
  // multiple times.
  bool closing_;

  DISALLOW_COPY_AND_ASSIGN(TabModalConfirmDialogDelegate);
};

#endif  // CHROME_BROWSER_UI_TAB_MODAL_CONFIRM_DIALOG_DELEGATE_H_
