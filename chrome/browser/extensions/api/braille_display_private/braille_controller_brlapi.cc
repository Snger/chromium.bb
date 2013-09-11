// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/braille_display_private/braille_controller.h"

#include <algorithm>
#include <cerrno>
#include <cstring>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file_path_watcher.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/braille_display_private/brlapi_connection.h"
#include "content/public/browser/browser_thread.h"
#include "library_loaders/libbrlapi.h"

namespace extensions {
using content::BrowserThread;
using base::TimeDelta;
namespace api {
namespace braille_display_private {

namespace {
// Delay between detecting a directory update and trying to connect
// to the brlapi.
const int64 kConnectionDelayMs = 1000;
}

class BrailleControllerImpl : public BrailleController {
 public:
  static BrailleControllerImpl* GetInstance();
  virtual scoped_ptr<base::DictionaryValue> GetDisplayState() OVERRIDE;
  virtual void WriteDots(const std::string& cells) OVERRIDE;
  virtual void AddObserver(BrailleObserver* observer) OVERRIDE;
  virtual void RemoveObserver(BrailleObserver* observer) OVERRIDE;
  virtual void SetCreateBrlapiConnectionForTesting(
      const CreateBrlapiConnectionFunction& callback) OVERRIDE;

 private:
  BrailleControllerImpl();
  virtual ~BrailleControllerImpl();
  void TryLoadLibBrlApi();
  void StartConnecting();
  void OnSocketDirChanged(const base::FilePath& path, bool error);
  void TryToConnect();
  scoped_ptr<BrlapiConnection> CreateBrlapiConnection();
  void DispatchKeys();
  scoped_ptr<KeyEvent> MapKeyCode(brlapi_keyCode_t code);
  void DispatchKeyEvent(scoped_ptr<KeyEvent> event);

  LibBrlapiLoader libbrlapi_loader_;
  CreateBrlapiConnectionFunction create_brlapi_connection_function_;

  // Manipulated on the IO thread.
  scoped_ptr<BrlapiConnection> connection_;
  base::FilePathWatcher file_path_watcher_;

  // Manipulated on the UI thread.
  ObserverList<BrailleObserver> observers_;
  bool watching_dir_;

  friend struct DefaultSingletonTraits<BrailleControllerImpl>;

  DISALLOW_COPY_AND_ASSIGN(BrailleControllerImpl);
};

BrailleController::BrailleController() {
}

BrailleController::~BrailleController() {
}

// static
BrailleController* BrailleController::GetInstance() {
  return BrailleControllerImpl::GetInstance();
}

// static
BrailleControllerImpl* BrailleControllerImpl::GetInstance() {
  return Singleton<BrailleControllerImpl,
                   LeakySingletonTraits<BrailleControllerImpl> >::get();
}

BrailleControllerImpl::BrailleControllerImpl()
    : watching_dir_(false) {
  create_brlapi_connection_function_ = base::Bind(
      &BrailleControllerImpl::CreateBrlapiConnection,
      base::Unretained(this));
}

BrailleControllerImpl::~BrailleControllerImpl() {
}

void BrailleControllerImpl::TryLoadLibBrlApi() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  if (libbrlapi_loader_.loaded())
    return;
  // These versions of libbrlapi work the same for the functions we
  // are using.  (0.6.0 adds brlapi_writeWText).
  static const char* kSupportedVersions[] = {
    "libbrlapi.so.0.5",
    "libbrlapi.so.0.6"
  };
  for (size_t i = 0; i < arraysize(kSupportedVersions); ++i) {
    if (libbrlapi_loader_.Load(kSupportedVersions[i]))
      return;
  }
  LOG(WARNING) << "Couldn't load libbrlapi: " << strerror(errno);
}

scoped_ptr<base::DictionaryValue> BrailleControllerImpl::GetDisplayState() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  if (!watching_dir_) {
    watching_dir_ = true;
    StartConnecting();
  }
  DisplayState displayState;
  if (connection_.get() && connection_->Connected()) {
    size_t size;
    if (!connection_->GetDisplaySize(&size)) {
      connection_->Disconnect();
    } else if (size > 0) {  // size == 0 means no display present.
      displayState.available = true;
      displayState.text_cell_count.reset(new int(size));
    }
  }
  return displayState.ToValue().Pass();
}

void BrailleControllerImpl::WriteDots(const std::string& cells) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  if (connection_ && connection_->Connected()) {
    size_t size;
    if (!connection_->GetDisplaySize(&size)) {
      connection_->Disconnect();
    }
    std::vector<unsigned char> sizedCells(size);
    std::memcpy(&sizedCells[0], cells.data(), std::min(cells.size(), size));
    if (size > cells.size())
      std::fill(sizedCells.begin() + cells.size(), sizedCells.end(), 0);
    if (!connection_->WriteDots(&sizedCells[0]))
      connection_->Disconnect();
  }
}

void BrailleControllerImpl::AddObserver(BrailleObserver* observer) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  observers_.AddObserver(observer);
}

void BrailleControllerImpl::RemoveObserver(BrailleObserver* observer) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  observers_.RemoveObserver(observer);
}

void BrailleControllerImpl::SetCreateBrlapiConnectionForTesting(
    const CreateBrlapiConnectionFunction& function) {
  if (function.is_null()) {
    create_brlapi_connection_function_ = base::Bind(
        &BrailleControllerImpl::CreateBrlapiConnection,
        base::Unretained(this));
  } else {
    create_brlapi_connection_function_ = function;
  }
}

void BrailleControllerImpl::StartConnecting() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  TryLoadLibBrlApi();
  if (!libbrlapi_loader_.loaded()) {
    return;
  }
  base::FilePath brlapi_dir(BRLAPI_SOCKETPATH);
  if (!file_path_watcher_.Watch(
          brlapi_dir, false, base::Bind(
              &BrailleControllerImpl::OnSocketDirChanged,
              base::Unretained(this)))) {
    LOG(WARNING) << "Couldn't watch brlapi directory " << BRLAPI_SOCKETPATH;
    return;
  }
  TryToConnect();
}

void BrailleControllerImpl::OnSocketDirChanged(const base::FilePath& path,
                                               bool error) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(libbrlapi_loader_.loaded());
  if (error) {
    LOG(ERROR) << "Error watching brlapi directory: " << path.value();
    return;
  }
  LOG(INFO) << "BrlAPI directory changed";
  BrowserThread::PostDelayedTask(BrowserThread::IO, FROM_HERE,
                                 base::Bind(
                                     &BrailleControllerImpl::TryToConnect,
                                     base::Unretained(this)),
                                 TimeDelta::FromMilliseconds(
                                     kConnectionDelayMs));
}

void BrailleControllerImpl::TryToConnect() {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::IO));
  DCHECK(libbrlapi_loader_.loaded());
  if (!connection_.get())
    connection_ = create_brlapi_connection_function_.Run();
  if (connection_.get() && !connection_->Connected()) {
    if (!connection_->Connect(base::Bind(
            &BrailleControllerImpl::DispatchKeys,
            base::Unretained(this))))
      LOG(WARNING) << "Couldn't connect to brlapi";
  }
}

scoped_ptr<BrlapiConnection> BrailleControllerImpl::CreateBrlapiConnection() {
  DCHECK(libbrlapi_loader_.loaded());
  return BrlapiConnection::Create(&libbrlapi_loader_);
}

scoped_ptr<KeyEvent> BrailleControllerImpl::MapKeyCode(brlapi_keyCode_t code) {
  brlapi_expandedKeyCode_t expanded;
  if (libbrlapi_loader_.brlapi_expandKeyCode(code, &expanded) != 0) {
    LOG(ERROR) << "Couldn't expand key code " << code;
    return scoped_ptr<KeyEvent>();
  }
  scoped_ptr<KeyEvent> result(new KeyEvent);
  result->command = KEY_COMMAND_NONE;
  switch (expanded.type) {
    case BRLAPI_KEY_TYPE_CMD:
      switch (expanded.command) {
        case BRLAPI_KEY_CMD_LNUP:
          result->command = KEY_COMMAND_LINE_UP;
          break;
        case BRLAPI_KEY_CMD_LNDN:
          result->command = KEY_COMMAND_LINE_DOWN;
          break;
        case BRLAPI_KEY_CMD_FWINLT:
          result->command = KEY_COMMAND_PAN_LEFT;
          break;
        case BRLAPI_KEY_CMD_FWINRT:
          result->command = KEY_COMMAND_PAN_RIGHT;
          break;
        case BRLAPI_KEY_CMD_TOP:
          result->command = KEY_COMMAND_TOP;
          break;
        case BRLAPI_KEY_CMD_BOT:
          result->command = KEY_COMMAND_BOTTOM;
          break;
        case BRLAPI_KEY_CMD_ROUTE:
          result->command = KEY_COMMAND_ROUTING;
          result->display_position.reset(new int(expanded.argument));
          break;
        case BRLAPI_KEY_CMD_PASSDOTS:
          result->command = KEY_COMMAND_DOTS;
          // The 8 low-order bits in the argument contains the dots.
          result->braille_dots.reset(new int(expanded.argument & 0xf));
          if ((expanded.argument & BRLAPI_DOTC) != 0)
            result->space_key.reset(new bool(true));
          break;
      }
      break;
  }
  if (result->command == KEY_COMMAND_NONE)
    result.reset();
  return result.Pass();
}

void BrailleControllerImpl::DispatchKeys() {
  DCHECK(connection_.get());
  brlapi_keyCode_t code;
  while (true) {
    int result = connection_->ReadKey(&code);
    if (result < 0) {  // Error.
      brlapi_error_t* err = connection_->BrlapiError();
      if (err->brlerrno == BRLAPI_ERROR_LIBCERR && err->libcerrno == EINTR)
        continue;
      // Disconnect on other errors.
      LOG(ERROR) << "BrlAPI error: " << connection_->BrlapiStrError();
      connection_->Disconnect();
      return;
    } else if (result == 0) { // No more data.
      return;
    }
    scoped_ptr<KeyEvent> event = MapKeyCode(code);
    if (event)
      DispatchKeyEvent(event.Pass());
  }
}

void BrailleControllerImpl::DispatchKeyEvent(scoped_ptr<KeyEvent> event) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(
                                &BrailleControllerImpl::DispatchKeyEvent,
                                base::Unretained(this),
                                base::Passed(&event)));
    return;
  }
  FOR_EACH_OBSERVER(BrailleObserver, observers_, OnKeyEvent(*event));
}

}  // namespace braille_display_private
}  // namespace api
}  // namespace extensions
