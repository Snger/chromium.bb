// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_setup_flow.h"

#include "base/callback.h"
#include "base/command_line.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/sync_setup_flow_handler.h"
#include "chrome/browser/sync/user_selectable_sync_type.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "sync/syncable/model_type.h"

namespace {

// Helper function to disable password sync.
void DisablePasswordSync(ProfileSyncService* service) {
  syncable::ModelTypeSet types = service->GetPreferredDataTypes();
  types.Remove(syncable::PASSWORDS);
  service->OnUserChoseDatatypes(false, types);
}

bool HasConfigurationChanged(const SyncConfiguration& configuration,
                             Profile* profile) {
  CHECK(profile);

  // This function must be updated every time a new sync datatype is added to
  // the sync preferences page.
  COMPILE_ASSERT(17 == syncable::MODEL_TYPE_COUNT,
                 UpdateCustomConfigHistogram);

  // If service is null or if this is a first time configuration, return true.
  ProfileSyncService* service =
      ProfileSyncServiceFactory::GetInstance()->GetForProfile(profile);
  if (!service || !service->HasSyncSetupCompleted())
    return true;

  if (configuration.set_secondary_passphrase !=
      service->IsUsingSecondaryPassphrase()) {
    return true;
  }

  if (configuration.encrypt_all != service->EncryptEverythingEnabled())
    return true;

  PrefService* pref_service = profile->GetPrefs();
  CHECK(pref_service);

  if (configuration.sync_everything !=
      pref_service->GetBoolean(prefs::kSyncKeepEverythingSynced)) {
    return true;
  }

  // Only check the data types that are explicitly listed on the sync
  // preferences page.
  const syncable::ModelTypeSet types = configuration.data_types;
  if (((types.Has(syncable::BOOKMARKS)) !=
       pref_service->GetBoolean(prefs::kSyncBookmarks)) ||
      ((types.Has(syncable::PREFERENCES)) !=
       pref_service->GetBoolean(prefs::kSyncPreferences)) ||
      ((types.Has(syncable::THEMES)) !=
       pref_service->GetBoolean(prefs::kSyncThemes)) ||
      ((types.Has(syncable::PASSWORDS)) !=
       pref_service->GetBoolean(prefs::kSyncPasswords)) ||
      ((types.Has(syncable::AUTOFILL)) !=
       pref_service->GetBoolean(prefs::kSyncAutofill)) ||
      ((types.Has(syncable::EXTENSIONS)) !=
       pref_service->GetBoolean(prefs::kSyncExtensions)) ||
      ((types.Has(syncable::TYPED_URLS)) !=
       pref_service->GetBoolean(prefs::kSyncTypedUrls)) ||
      ((types.Has(syncable::SESSIONS)) !=
       pref_service->GetBoolean(prefs::kSyncSessions)) ||
      ((types.Has(syncable::APPS)) !=
       pref_service->GetBoolean(prefs::kSyncApps))) {
    return true;
  }

  return false;
}

void UpdateHistogram(const SyncConfiguration& configuration,
                     const ProfileSyncService* service) {
  if (!service)
    return;
  Profile* profile = service->profile();
  if (HasConfigurationChanged(configuration, profile)) {
    UMA_HISTOGRAM_BOOLEAN("Sync.SyncEverything",
                          configuration.sync_everything);
    if (!configuration.sync_everything) {
      // Only log the data types that are explicitly listed on the sync
      // preferences page.
      const syncable::ModelTypeSet types = configuration.data_types;
      if (types.Has(syncable::BOOKMARKS)) {
        UMA_HISTOGRAM_ENUMERATION(
            "Sync.CustomSync",
            browser_sync::user_selectable_type::BOOKMARKS,
            browser_sync::user_selectable_type::SELECTABLE_DATATYPE_COUNT + 1);
      }
      if (types.Has(syncable::PREFERENCES)) {
        UMA_HISTOGRAM_ENUMERATION(
            "Sync.CustomSync",
            browser_sync::user_selectable_type::PREFERENCES,
            browser_sync::user_selectable_type::SELECTABLE_DATATYPE_COUNT + 1);
      }
      if (types.Has(syncable::PASSWORDS)) {
        UMA_HISTOGRAM_ENUMERATION(
            "Sync.CustomSync",
            browser_sync::user_selectable_type::PASSWORDS,
            browser_sync::user_selectable_type::SELECTABLE_DATATYPE_COUNT + 1);
      }
      if (types.Has(syncable::AUTOFILL)) {
        UMA_HISTOGRAM_ENUMERATION(
            "Sync.CustomSync",
            browser_sync::user_selectable_type::AUTOFILL,
            browser_sync::user_selectable_type::SELECTABLE_DATATYPE_COUNT + 1);
      }
      if (types.Has(syncable::THEMES)) {
        UMA_HISTOGRAM_ENUMERATION(
            "Sync.CustomSync",
            browser_sync::user_selectable_type::THEMES,
            browser_sync::user_selectable_type::SELECTABLE_DATATYPE_COUNT + 1);
      }
      if (types.Has(syncable::TYPED_URLS)) {
        UMA_HISTOGRAM_ENUMERATION(
            "Sync.CustomSync",
            browser_sync::user_selectable_type::TYPED_URLS,
            browser_sync::user_selectable_type::SELECTABLE_DATATYPE_COUNT + 1);
      }
      if (types.Has(syncable::EXTENSIONS)) {
        UMA_HISTOGRAM_ENUMERATION(
            "Sync.CustomSync",
            browser_sync::user_selectable_type::EXTENSIONS,
            browser_sync::user_selectable_type::SELECTABLE_DATATYPE_COUNT + 1);
      }
      if (types.Has(syncable::SESSIONS)) {
        UMA_HISTOGRAM_ENUMERATION(
            "Sync.CustomSync",
            browser_sync::user_selectable_type::SESSIONS,
            browser_sync::user_selectable_type::SELECTABLE_DATATYPE_COUNT + 1);
      }
      if (types.Has(syncable::APPS)) {
        UMA_HISTOGRAM_ENUMERATION(
            "Sync.CustomSync",
            browser_sync::user_selectable_type::APPS,
            browser_sync::user_selectable_type::SELECTABLE_DATATYPE_COUNT + 1);
      }
      COMPILE_ASSERT(17 == syncable::MODEL_TYPE_COUNT,
                     UpdateCustomConfigHistogram);
      COMPILE_ASSERT(
          9 == browser_sync::user_selectable_type::SELECTABLE_DATATYPE_COUNT,
          UpdateCustomConfigHistogram);
    }
    UMA_HISTOGRAM_BOOLEAN("Sync.EncryptAllData", configuration.encrypt_all);
    UMA_HISTOGRAM_BOOLEAN("Sync.CustomPassphrase",
                          configuration.set_secondary_passphrase);
  }
}

}  // namespace

SyncConfiguration::SyncConfiguration()
    : encrypt_all(false),
      sync_everything(false),
      set_secondary_passphrase(false),
      set_gaia_passphrase(false) {
}

SyncConfiguration::~SyncConfiguration() {}

SyncSetupFlow::~SyncSetupFlow() {
  flow_handler_->SetFlow(NULL);
}

// static
SyncSetupFlow* SyncSetupFlow::Run(ProfileSyncService* service,
                                  SyncSetupFlowContainer* container,
                                  SyncSetupWizard::State start,
                                  SyncSetupWizard::State end) {
  if ((start == SyncSetupWizard::CONFIGURE ||
       start == SyncSetupWizard::SYNC_EVERYTHING ||
       start == SyncSetupWizard::ENTER_PASSPHRASE) &&
      !service->sync_initialized()) {
    // We are trying to open configuration window, but the backend isn't ready.
    // We just return NULL. This has the effect of the flow getting reset, and
    // the user's action has no effect.
    LOG(ERROR) << "Attempted to show sync configure before backend ready.";
    return NULL;
  }
  return new SyncSetupFlow(start, end, container, service);
}

void SyncSetupFlow::GetArgsForConfigure(DictionaryValue* args) {
  // The SYNC_EVERYTHING case will set this to true.
  args->SetBoolean("showSyncEverythingPage", false);

  args->SetBoolean("syncAllDataTypes",
      service_->profile()->GetPrefs()->GetBoolean(
          prefs::kSyncKeepEverythingSynced));

  // Bookmarks, Preferences, and Themes are launched for good, there's no
  // going back now.  Check if the other data types are registered though.
  const syncable::ModelTypeSet registered_types =
      service_->GetRegisteredDataTypes();
  const syncable::ModelTypeSet preferred_types =
      service_->GetPreferredDataTypes();
  args->SetBoolean("passwordsRegistered",
      registered_types.Has(syncable::PASSWORDS));
  args->SetBoolean("autofillRegistered",
      registered_types.Has(syncable::AUTOFILL));
  args->SetBoolean("extensionsRegistered",
      registered_types.Has(syncable::EXTENSIONS));
  args->SetBoolean("typedUrlsRegistered",
      registered_types.Has(syncable::TYPED_URLS));
  args->SetBoolean("appsRegistered",
      registered_types.Has(syncable::APPS));
  args->SetBoolean("sessionsRegistered",
      registered_types.Has(syncable::SESSIONS));
  args->SetBoolean("syncBookmarks",
                   preferred_types.Has(syncable::BOOKMARKS));
  args->SetBoolean("syncPreferences",
                   preferred_types.Has(syncable::PREFERENCES));
  args->SetBoolean("syncThemes",
                   preferred_types.Has(syncable::THEMES));
  args->SetBoolean("syncPasswords",
                   preferred_types.Has(syncable::PASSWORDS));
  args->SetBoolean("syncAutofill",
                   preferred_types.Has(syncable::AUTOFILL));
  args->SetBoolean("syncExtensions",
                   preferred_types.Has(syncable::EXTENSIONS));
  args->SetBoolean("syncSessions",
                   preferred_types.Has(syncable::SESSIONS));
  args->SetBoolean("syncTypedUrls",
                   preferred_types.Has(syncable::TYPED_URLS));
  args->SetBoolean("syncApps",
                   preferred_types.Has(syncable::APPS));

  args->SetBoolean("encryptionEnabled",
      !CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kDisableSyncEncryption));

  bool encrypt_all = service_->EncryptEverythingEnabled();
  if (service_->encryption_pending())
    encrypt_all = true;
  args->SetBoolean("encryptAllData", encrypt_all);

  // Load the parameters for the encryption tab.
  args->SetBoolean("usePassphrase", service_->IsUsingSecondaryPassphrase());

  // Determine if we need a passphrase or not, and if so, prompt the user.
  if (service_->IsPassphraseRequiredForDecryption()) {
    // We need a passphrase, so we have to prompt the user.
    args->SetBoolean("show_passphrase", true);
    // Tell the UI layer what kind of passphrase we need.
    args->SetBoolean("need_google_passphrase",
                     !service_->IsUsingSecondaryPassphrase());
    args->SetBoolean("passphrase_creation_rejected",
                     user_tried_creating_explicit_passphrase_);
    args->SetBoolean("passphrase_setting_rejected",
                     user_tried_setting_passphrase_);
  }
}

bool SyncSetupFlow::AttachSyncSetupHandler(SyncSetupFlowHandler* handler) {
  if (flow_handler_)
    return false;

  flow_handler_ = handler;
  handler->SetFlow(this);
  ActivateState(current_state_);
  return true;
}

bool SyncSetupFlow::IsAttached() const {
  return flow_handler_ != NULL;
}

void SyncSetupFlow::Advance(SyncSetupWizard::State advance_state) {
  if (!ShouldAdvance(advance_state)) {
    LOG(WARNING) << "Invalid state change from "
                 << current_state_ << " to " << advance_state;
    return;
  }

  if (flow_handler_)
    ActivateState(advance_state);
}

void SyncSetupFlow::Focus() {
  // This gets called from SyncSetupWizard::Focus(), and might get called
  // before flow_handler_ is set in AttachSyncSetupHandler() (which gets
  // called asynchronously after the UI initializes).
  if (flow_handler_)
    flow_handler_->Focus();
}

// A callback to notify the delegate that the dialog closed.
void SyncSetupFlow::OnDialogClosed(const std::string& json_retval) {
  DCHECK(json_retval.empty());
  container_->set_flow(NULL);  // Sever ties from the wizard.
  // If we've reached the end, mark it.  This could be a discrete run, in which
  // case it's already set, but it simplifes the logic to do it this way.
  if (current_state_ == end_state_)
    service_->SetSyncSetupCompleted();

  // Record the state at which the user cancelled the signon dialog.
  switch (current_state_) {
    case SyncSetupWizard::CONFIGURE:
    case SyncSetupWizard::ENTER_PASSPHRASE:
    case SyncSetupWizard::SETTING_UP:
      // TODO(atwilson): Treat a close during ENTER_PASSPHRASE like a
      // Cancel + Skip (i.e. call OnPassphraseCancel()). http://crbug.com/74645
      ProfileSyncService::SyncEvent(
          ProfileSyncService::CANCEL_DURING_CONFIGURE);
      break;
    default:
      break;
  }

  service_->OnUserCancelledDialog();
  delete this;
}

void SyncSetupFlow::OnUserConfigured(const SyncConfiguration& configuration) {
  // Update sync histograms. This is a no-op if |configuration| has not changed.
  UpdateHistogram(configuration, service_);

  // Go to the "loading..." screen.
  Advance(SyncSetupWizard::SETTING_UP);

  // Note: encryption will not occur until OnUserChoseDatatypes is called.
  if (configuration.encrypt_all)
    service_->EnableEncryptEverything();

  bool set_new_decryption_passphrase = false;
  if (configuration.set_gaia_passphrase &&
      !configuration.gaia_passphrase.empty()) {
    // Caller passed a gaia passphrase. This is illegal if we are currently
    // using a secondary passphrase.
    DCHECK(!service_->IsUsingSecondaryPassphrase());
    service_->SetDecryptionPassphrase(configuration.gaia_passphrase);
    // Since the user entered the passphrase manually, set this flag so we can
    // report an error if the passphrase setting failed.
    user_tried_setting_passphrase_ = true;
    set_new_decryption_passphrase = true;
  }

  // Set the secondary passphrase, either as a decryption passphrase, or
  // as an attempt to encrypt the user's data using this new passphrase.
  if (configuration.set_secondary_passphrase &&
      !configuration.secondary_passphrase.empty()) {
    // If we are signing in when an explicit password has already been set,
    // we must call SetDecryptionPassphrase, which will first try decrypting
    // the cached pending keys with the passphrase on the UI thread. If
    // decryption fails, we can immediately show an error and go back to the
    // "enter passphrase" dialog without sending the passphrase to the syncer
    // thread.
    if (service_->IsPassphraseRequiredForDecryption()) {
      if (!service_->SetDecryptionPassphrase(
          configuration.secondary_passphrase)) {
        user_tried_setting_passphrase_ = true;
        Advance(SyncSetupWizard::ENTER_PASSPHRASE);
        return;
      }
    } else {
      service_->SetEncryptionPassphrase(configuration.secondary_passphrase,
                                        ProfileSyncService::EXPLICIT);
    }
    if (service_->IsUsingSecondaryPassphrase()) {
      user_tried_setting_passphrase_ = true;
      set_new_decryption_passphrase = true;
    } else {
      user_tried_creating_explicit_passphrase_ = true;
    }
  }

  service_->OnUserChoseDatatypes(configuration.sync_everything,
                                 configuration.data_types);

  // See if we are done configuring (if we don't need a passphrase, and don't
  // need to hang around waiting for encryption to happen, just exit). This call
  // to IsPassphraseRequiredForDecryption() takes into account the data types
  // we just enabled/disabled.
  if (!service_->IsPassphraseRequiredForDecryption() &&
      !service_->encryption_pending()) {
    Advance(SyncSetupWizard::DONE);
  } else if (!set_new_decryption_passphrase &&
             service_->IsPassphraseRequiredForDecryption()) {
    // We need a passphrase, but the user did not provide one, so transition
    // directly to ENTER_PASSPHRASE (otherwise we'll have to wait until
    // the sync engine generates another OnPassphraseRequired() at the end of
    // the sync cycle which can take a long time).
    Advance(SyncSetupWizard::ENTER_PASSPHRASE);
  }
}

void SyncSetupFlow::OnPassphraseEntry(const std::string& passphrase) {
  Advance(SyncSetupWizard::SETTING_UP);
  service_->SetDecryptionPassphrase(passphrase);
  user_tried_setting_passphrase_ = true;
}

void SyncSetupFlow::OnPassphraseCancel() {
  // If the user cancels when being asked for the passphrase,
  // just disable encrypted sync and continue setting up.
  if (current_state_ == SyncSetupWizard::ENTER_PASSPHRASE)
    DisablePasswordSync(service_);

  Advance(SyncSetupWizard::SETTING_UP);
}

// Use static Run method to get an instance.
SyncSetupFlow::SyncSetupFlow(SyncSetupWizard::State start_state,
                             SyncSetupWizard::State end_state,
                             SyncSetupFlowContainer* container,
                             ProfileSyncService* service)
    : container_(container),
      current_state_(start_state),
      end_state_(end_state),
      flow_handler_(NULL),
      service_(service),
      user_tried_creating_explicit_passphrase_(false),
      user_tried_setting_passphrase_(false) {
}

// Returns true if the flow should advance to |state| based on |current_state_|.
bool SyncSetupFlow::ShouldAdvance(SyncSetupWizard::State state) {
  switch (state) {
    case SyncSetupWizard::SYNC_EVERYTHING:  // Intentionally fall through.
    case SyncSetupWizard::CONFIGURE:
      return current_state_ != SyncSetupWizard::SETTING_UP;
    case SyncSetupWizard::ENTER_PASSPHRASE:
      return current_state_ == SyncSetupWizard::SYNC_EVERYTHING ||
             current_state_ == SyncSetupWizard::CONFIGURE ||
             current_state_ == SyncSetupWizard::SETTING_UP;
    case SyncSetupWizard::SETTING_UP:
      return current_state_ == SyncSetupWizard::SYNC_EVERYTHING ||
             current_state_ == SyncSetupWizard::CONFIGURE ||
             current_state_ == SyncSetupWizard::ENTER_PASSPHRASE;
    case SyncSetupWizard::FATAL_ERROR:
      return current_state_ != SyncSetupWizard::ABORT;
    case SyncSetupWizard::ABORT:
      return true;
    case SyncSetupWizard::DONE:
      return current_state_ == SyncSetupWizard::SETTING_UP ||
             current_state_ == SyncSetupWizard::ENTER_PASSPHRASE;
    default:
      NOTREACHED() << "Unhandled State: " << state;
      return false;
  }
}

void SyncSetupFlow::ActivateState(SyncSetupWizard::State state) {
  DCHECK(flow_handler_);

  current_state_ = state;

  switch (state) {
    case SyncSetupWizard::SYNC_EVERYTHING: {
      DictionaryValue args;
      GetArgsForConfigure(&args);
      args.SetBoolean("showSyncEverythingPage", true);
      flow_handler_->ShowConfigure(args);
      break;
    }
    case SyncSetupWizard::CONFIGURE: {
      DictionaryValue args;
      GetArgsForConfigure(&args);
      flow_handler_->ShowConfigure(args);
      break;
    }
    case SyncSetupWizard::ENTER_PASSPHRASE: {
      DictionaryValue args;
      GetArgsForConfigure(&args);
      // TODO(atwilson): Remove ShowPassphraseEntry in favor of using
      // ShowConfigure() - http://crbug.com/90786.
      flow_handler_->ShowPassphraseEntry(args);
      break;
    }
    case SyncSetupWizard::SETTING_UP: {
      flow_handler_->ShowSettingUp();
      break;
    }
    case SyncSetupWizard::FATAL_ERROR: {
      // This shows the user the "Could not connect to server" error.
      // TODO(sync): Update this error handling to allow different platforms to
      // display the error appropriately (http://crbug.com/92722).
      flow_handler_->ShowFatalError();
      break;
    }
    case SyncSetupWizard::DONE:
    case SyncSetupWizard::ABORT:
      flow_handler_->ShowSetupDone(UTF8ToUTF16(
          service_->profile()->GetPrefs()->GetString(
          prefs::kGoogleServicesUsername)));
      break;
    default:
      NOTREACHED() << "Invalid advance state: " << state;
  }
}
