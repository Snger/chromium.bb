// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/file_manager_dialog.h"

#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "chrome/browser/extensions/extension_file_browser_private_api.h"
#include "chrome/browser/extensions/file_manager_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/views/extensions/extension_dialog.h"
#include "chrome/browser/ui/views/window.h"
#include "content/browser/browser_thread.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "views/window/window.h"

namespace {

const int kFileManagerWidth = 720;  // pixels
const int kFileManagerHeight = 580;  // pixels

}

// Linking this implementation of SelectFileDialog::Create into the target
// selects FileManagerDialog as the dialog of choice.
// static
SelectFileDialog* SelectFileDialog::Create(Listener* listener) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return new FileManagerDialog(listener);
}

/////////////////////////////////////////////////////////////////////////////

FileManagerDialog::FileManagerDialog(Listener* listener)
    : SelectFileDialog(listener),
      tab_id_(0),
      owner_window_(0) {
}

FileManagerDialog::~FileManagerDialog() {
  if (extension_dialog_)
    extension_dialog_->ObserverDestroyed();
  FileDialogFunction::Callback::Remove(tab_id_);
}

bool FileManagerDialog::IsRunning(gfx::NativeWindow owner_window) const {
  return owner_window_ == owner_window;
}

void FileManagerDialog::ListenerDestroyed() {
  listener_ = NULL;
  FileDialogFunction::Callback::Remove(tab_id_);
}

void FileManagerDialog::ExtensionDialogIsClosing(ExtensionDialog* dialog) {
  owner_window_ = NULL;
  // Release our reference to the dialog to allow it to close.
  extension_dialog_ = NULL;
  FileDialogFunction::Callback::Remove(tab_id_);
}

RenderViewHost* FileManagerDialog::GetRenderViewHost() {
  if (extension_dialog_)
    return extension_dialog_->host()->render_view_host();
  return NULL;
}

void FileManagerDialog::SelectFileImpl(
    Type type,
    const string16& title,
    const FilePath& default_path,
    const FileTypeInfo* file_types,
    int file_type_index,
    const FilePath::StringType& default_extension,
    gfx::NativeWindow owner_window,
    void* params) {
  if (owner_window_) {
    LOG(ERROR) << "File dialog already in use!";
    return;
  }
  Browser* active_browser = BrowserList::GetLastActive();
  if (!active_browser)
    return;

  GURL file_browser_url = FileManagerUtil::GetFileBrowserUrlWithParams(
      type, title, default_path, file_types, file_type_index,
      default_extension);
  extension_dialog_ = ExtensionDialog::Show(file_browser_url,
      active_browser, kFileManagerWidth, kFileManagerHeight,
      this /* ExtensionDialog::Observer */);

  // Connect our listener to FileDialogFunction's per-tab callbacks.
  Browser* extension_browser = extension_dialog_->host()->view()->browser();
  TabContents* contents = extension_browser->GetSelectedTabContents();
  int32 tab_id = (contents ? contents->controller().session_id().id() : 0);
  FileDialogFunction::Callback::Add(tab_id, listener_, params);

  tab_id_ = tab_id;
  owner_window_ = owner_window;
}
