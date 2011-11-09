// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_settings_frontend.h"

#include "base/bind.h"
#include "base/file_path.h"
#include "chrome/browser/extensions/extension_event_names.h"
#include "chrome/browser/extensions/extension_event_router.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_settings_backend.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"

using content::BrowserThread;

namespace {

struct Backends {
  Backends(
      const FilePath& profile_path,
      const scoped_refptr<ExtensionSettingsObserverList>& observers)
    : extensions_backend_(
          profile_path.AppendASCII(
              ExtensionService::kExtensionSettingsDirectoryName),
          observers),
      apps_backend_(
          profile_path.AppendASCII(
              ExtensionService::kAppSettingsDirectoryName),
          observers) {}

  ExtensionSettingsBackend extensions_backend_;
  ExtensionSettingsBackend apps_backend_;
};

static void CallbackWithExtensionsBackend(
    const ExtensionSettingsFrontend::SyncableServiceCallback& callback,
    Backends* backends) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  callback.Run(&backends->extensions_backend_);
}

void CallbackWithAppsBackend(
    const ExtensionSettingsFrontend::SyncableServiceCallback& callback,
    Backends* backends) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  callback.Run(&backends->apps_backend_);
}

void CallbackWithExtensionsStorage(
    const std::string& extension_id,
    const ExtensionSettingsFrontend::StorageCallback& callback,
    Backends* backends) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  callback.Run(backends->extensions_backend_.GetStorage(extension_id));
}

void CallbackWithAppsStorage(
    const std::string& extension_id,
    const ExtensionSettingsFrontend::StorageCallback& callback,
    Backends* backends) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  callback.Run(backends->apps_backend_.GetStorage(extension_id));
}

void CallbackWithNullStorage(
    const ExtensionSettingsFrontend::StorageCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  callback.Run(NULL);
}

void DeleteStorageOnFileThread(
    const std::string& extension_id, Backends* backends) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  backends->extensions_backend_.DeleteStorage(extension_id);
  backends->apps_backend_.DeleteStorage(extension_id);
}

}  // namespace

// DefaultObserver.

ExtensionSettingsFrontend::DefaultObserver::DefaultObserver(Profile* profile)
    : profile_(profile) {}

ExtensionSettingsFrontend::DefaultObserver::~DefaultObserver() {}

void ExtensionSettingsFrontend::DefaultObserver::OnSettingsChanged(
    const std::string& extension_id, const std::string& changes_json) {
  profile_->GetExtensionEventRouter()->DispatchEventToExtension(
      extension_id,
      extension_event_names::kOnSettingsChanged,
      // This is the list of function arguments to pass to the onChanged
      // handler of extensions, a single argument with the list of changes.
      std::string("[") + changes_json + "]",
      NULL,
      GURL());
}

class ExtensionSettingsFrontend::Core
    : public base::RefCountedThreadSafe<Core> {
 public:
  explicit Core(
      const scoped_refptr<ExtensionSettingsObserverList>& observers)
      : observers_(observers), backends_(NULL) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  }

  typedef base::Callback<void(Backends*)> BackendsCallback;

  // Does any FILE thread specific initialization, such as construction of
  // |backend_|.  Must be called before any call to
  // RunWithBackendOnFileThread().
  void InitOnFileThread(const FilePath& profile_path) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    DCHECK(!backends_);
    backends_ = new Backends(profile_path, observers_);
  }

  // Runs |callback| with both the extensions and apps settings on the FILE
  // thread.
  void RunWithBackends(const BackendsCallback& callback) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(
            &ExtensionSettingsFrontend::Core::RunWithBackendsOnFileThread,
            this,
            callback));
  }

 private:
  void RunWithBackendsOnFileThread(const BackendsCallback& callback) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    DCHECK(backends_);
    callback.Run(backends_);
  }

  virtual ~Core() {
    if (BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
      delete backends_;
    } else if (BrowserThread::CurrentlyOn(BrowserThread::UI)) {
      BrowserThread::DeleteSoon(BrowserThread::FILE, FROM_HERE, backends_);
    } else {
      NOTREACHED();
    }
  }

  friend class base::RefCountedThreadSafe<Core>;

  // Observers to settings changes (thread safe).
  scoped_refptr<ExtensionSettingsObserverList> observers_;

  // Backends for extensions and apps settings.  Lives on FILE thread.
  Backends* backends_;

  DISALLOW_COPY_AND_ASSIGN(Core);
};

ExtensionSettingsFrontend::ExtensionSettingsFrontend(Profile* profile)
    : profile_(profile),
      observers_(new ExtensionSettingsObserverList()),
      default_observer_(profile),
      core_(new ExtensionSettingsFrontend::Core(observers_)) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  DCHECK(!profile->IsOffTheRecord());

  observers_->AddObserver(&default_observer_);

  BrowserThread::PostTask(
      BrowserThread::FILE,
      FROM_HERE,
      base::Bind(
          &ExtensionSettingsFrontend::Core::InitOnFileThread,
          core_.get(),
          profile->GetPath()));
}

ExtensionSettingsFrontend::~ExtensionSettingsFrontend() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  observers_->RemoveObserver(&default_observer_);
}

void ExtensionSettingsFrontend::RunWithSyncableService(
    syncable::ModelType model_type, const SyncableServiceCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  switch (model_type) {
    case syncable::EXTENSION_SETTINGS:
      core_->RunWithBackends(
          base::Bind(&CallbackWithExtensionsBackend, callback));
      break;
    case syncable::APP_SETTINGS:
      core_->RunWithBackends(
          base::Bind(&CallbackWithAppsBackend, callback));
      break;
    default:
      NOTREACHED();
  }
}

void ExtensionSettingsFrontend::RunWithStorage(
    const std::string& extension_id,
    const StorageCallback& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  const Extension* extension =
      profile_->GetExtensionService()->GetExtensionById(extension_id, true);
  if (!extension) {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&CallbackWithNullStorage, callback));
    return;
  }

  if (extension->is_app()) {
    core_->RunWithBackends(
        base::Bind(&CallbackWithAppsStorage, extension_id, callback));
  } else {
    core_->RunWithBackends(
        base::Bind(&CallbackWithExtensionsStorage, extension_id, callback));
  }
}

void ExtensionSettingsFrontend::DeleteStorageSoon(
    const std::string& extension_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  core_->RunWithBackends(base::Bind(&DeleteStorageOnFileThread, extension_id));
}

scoped_refptr<ExtensionSettingsObserverList>
ExtensionSettingsFrontend::GetObservers() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  return observers_;
}
