// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/login_utils.h"

#include <vector>

#include "base/command_line.h"
#include "base/compiler_specific.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/path_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_restrictions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/login_library.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_util.h"
#include "chrome/browser/chromeos/login/background_view.h"
#include "chrome/browser/chromeos/login/cookie_fetcher.h"
#include "chrome/browser/chromeos/login/google_authenticator.h"
#include "chrome/browser/chromeos/login/language_switch_menu.h"
#include "chrome/browser/chromeos/login/login_display_host.h"
#include "chrome/browser/chromeos/login/ownership_service.h"
#include "chrome/browser/chromeos/login/parallel_authenticator.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/net/chrome_url_request_context.h"
#include "chrome/browser/net/gaia/gaia_oauth_consumer.h"
#include "chrome/browser/net/gaia/gaia_oauth_fetcher.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/net/preconnect.h"
#include "chrome/browser/plugin_updater.h"
#include "chrome/browser/policy/browser_policy_connector.h"
#include "chrome/browser/prefs/pref_member.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/ui/browser_init.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/logging_chrome.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/gaia_urls.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "net/base/cookie_monster.h"
#include "net/base/cookie_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/gfx/gl/gl_switches.h"

namespace chromeos {

namespace {

// Affixes for Auth token received from ClientLogin request.
const char kAuthPrefix[] = "Auth=";
const char kAuthSuffix[] = "\n";

// Increase logging level for Guest mode to avoid LOG(INFO) messages in logs.
const char kGuestModeLoggingLevel[] = "1";

// Format of command line switch.
const char kSwitchFormatString[] = " --%s=\"%s\"";

// User name which is used in the Guest session.
const char kGuestUserName[] = "";

// The service scope of the OAuth v2 token that ChromeOS login will be
// requesting.
// TODO(zelidrag): Figure out if we need to add more services here.
const char kServiceScopeChromeOS[] =
    "https://www.googleapis.com/auth/chromesync";
}  // namespace

// Task for fetching tokens from UI thread.
class FetchTokensOnUIThreadTask : public Task {
 public:
  FetchTokensOnUIThreadTask(
      Profile* profile,
      const GaiaAuthConsumer::ClientLoginResult& credentials)
      : profile_(profile),
        credentials_(credentials) {}
  virtual ~FetchTokensOnUIThreadTask() {}
  // Task override.
  virtual void Run() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
    LoginUtils::Get()->FetchTokens(profile_, credentials_);
  }

 private:
  Profile* profile_;
  GaiaAuthConsumer::ClientLoginResult credentials_;
};

// Transfers initial set of Profile cookies form the default profile.
class TransferDefaultCookiesOnIOThreadTask : public Task {
 public:
  TransferDefaultCookiesOnIOThreadTask(
      net::URLRequestContextGetter* auth_context, Profile* new_profile,
      net::URLRequestContextGetter* new_context)
          : auth_context_(auth_context),
            new_profile_(new_profile),
            new_context_(new_context) {}
  virtual ~TransferDefaultCookiesOnIOThreadTask() {}

  // Task override.
  virtual void Run() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    net::CookieStore* default_store =
        auth_context_->GetURLRequestContext()->cookie_store();
    net::CookieMonster* default_monster = default_store->GetCookieMonster();
    default_monster->SetKeepExpiredCookies();
    net::CookieStore* new_store =
        new_context_->GetURLRequestContext()->cookie_store();
    net::CookieMonster* new_monster = new_store->GetCookieMonster();

    // Check that existing store does not already have some cookies.
    // If it does, we are probably dealing with previously created profile
    // and no transfer will be necessary.
    // TOOD(zelidrag): Investigate perf impact of the next operation
    // in case when we have a profile with bunch of cookies.
    net::CookieList existing_cookies = new_monster->GetAllCookies();
    if (existing_cookies.size())
      return;

    if (!new_monster->InitializeFrom(default_monster)) {
      LOG(WARNING) << "Failed initial cookie transfer.";
    }

    // TODO(zelidrag): Once sync is OAuth happy, remove this part.
    GaiaAuthConsumer::ClientLoginResult credentials;
    GetCredentialsFromCookieJar(default_monster->GetAllCookies(), &credentials);
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            new FetchTokensOnUIThreadTask(
                                new_profile_,
                                credentials));
  }

  // TODO(zelidrag): Once sync is OAuth happy, remove this function.
  void GetCredentialsFromCookieJar(const net::CookieList& cookies,
      GaiaAuthConsumer::ClientLoginResult* credentials) {
    // At this point we should only have GAIA's cookies in place,
    // so we
    for (net::CookieList::const_iterator iter = cookies.begin();
         iter != cookies.end(); ++iter) {
      const net::CookieMonster::CanonicalCookie& cookie = *iter;
      if (cookie.Name() == "SID") {
        credentials->sid = cookie.Value();
      } else if (cookie.Name() == "LSID") {
        credentials->lsid = cookie.Value();
      }
    }
  }

 private:
  net::URLRequestContextGetter* auth_context_;
  Profile* new_profile_;
  net::URLRequestContextGetter* new_context_;

  DISALLOW_COPY_AND_ASSIGN(TransferDefaultCookiesOnIOThreadTask);
};

class LoginUtilsImpl : public LoginUtils,
                       public ProfileManagerObserver,
                       public GaiaOAuthConsumer {
 public:
  LoginUtilsImpl()
      : background_view_(NULL) {
  }

  virtual void PrepareProfile(
      const std::string& username,
      const std::string& password,
      const GaiaAuthConsumer::ClientLoginResult& credentials,
      bool pending_requests,
      LoginUtils::Delegate* delegate) OVERRIDE;

  // Invoked after the tmpfs is successfully mounted.
  // Launches a browser in the incognito mode.
  virtual void CompleteOffTheRecordLogin(const GURL& start_url) OVERRIDE;

  // Invoked when the user is logging in for the first time, or is logging in as
  // a guest user.
  virtual void SetFirstLoginPrefs(PrefService* prefs) OVERRIDE;

  // Creates and returns the authenticator to use. The caller owns the returned
  // Authenticator and must delete it when done.
  virtual Authenticator* CreateAuthenticator(
      LoginStatusConsumer* consumer) OVERRIDE;

  // Warms the url used by authentication.
  virtual void PrewarmAuthentication() OVERRIDE;

  // Given the authenticated credentials from the cookie jar, try to exchange
  // fetch OAuth request, v1 and v2 tokens.
  void FetchOAuthTokens(Profile* profile);

  // Given the credentials try to exchange them for
  // full-fledged Google authentication cookies.
  virtual void FetchCookies(
      Profile* profile,
      const GaiaAuthConsumer::ClientLoginResult& credentials) OVERRIDE;

  // Supply credentials for sync and others to use.
  virtual void FetchTokens(
      Profile* profile,
      const GaiaAuthConsumer::ClientLoginResult& credentials) OVERRIDE;

  // Sets the current background view.
  virtual void SetBackgroundView(
      chromeos::BackgroundView* background_view) OVERRIDE;

  // Gets the current background view.
  virtual chromeos::BackgroundView* GetBackgroundView() OVERRIDE;

  // Transfers cookies from the |default_profile| into the |new_profile|.
  // If authentication was performed by an extension, then
  // the set of cookies that was acquired through such that process will be
  // automatically transfered into the profile. Returns true if cookie transfer
  // was performed successfully.
  virtual bool TransferDefaultCookies(Profile* default_profile,
                                      Profile* new_profile) OVERRIDE;

  // ProfileManagerObserver implementation:
  virtual void OnProfileCreated(Profile* profile) OVERRIDE;

  // GaiaOAuthConsumer overrides.
  virtual void OnGetOAuthTokenSuccess(const std::string& oauth_token) OVERRIDE;
  virtual void OnGetOAuthTokenFailure() OVERRIDE;
  virtual void OnOAuthGetAccessTokenSuccess(const std::string& token,
                                            const std::string& secret) OVERRIDE;
  virtual void OnOAuthGetAccessTokenFailure(
      const GoogleServiceAuthError& error) OVERRIDE;
  virtual void OnOAuthWrapBridgeSuccess(const std::string& token,
                                        const std::string& expires_in) OVERRIDE;
  virtual void OnOAuthWrapBridgeFailure(
      const GoogleServiceAuthError& error) OVERRIDE;

 protected:
  virtual std::string GetOffTheRecordCommandLine(
      const GURL& start_url,
      const CommandLine& base_command_line,
      CommandLine *command_line);

 private:
  // Check user's profile for kApplicationLocale setting.
  void RespectLocalePreference(Profile* pref);

  // The current background view.
  chromeos::BackgroundView* background_view_;

  std::string username_;
  std::string password_;
  GaiaAuthConsumer::ClientLoginResult credentials_;
  bool pending_requests_;
  scoped_refptr<Authenticator> authenticator_;
  scoped_ptr<GaiaOAuthFetcher> oauth_fetcher_;

  // Delegate to be fired when the profile will be prepared.
  LoginUtils::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(LoginUtilsImpl);
};

class LoginUtilsWrapper {
 public:
  static LoginUtilsWrapper* GetInstance() {
    return Singleton<LoginUtilsWrapper>::get();
  }

  LoginUtils* get() {
    base::AutoLock create(create_lock_);
    if (!ptr_.get())
      reset(new LoginUtilsImpl);
    return ptr_.get();
  }

  void reset(LoginUtils* ptr) {
    ptr_.reset(ptr);
  }

 private:
  friend struct DefaultSingletonTraits<LoginUtilsWrapper>;

  LoginUtilsWrapper() {}

  base::Lock create_lock_;
  scoped_ptr<LoginUtils> ptr_;

  DISALLOW_COPY_AND_ASSIGN(LoginUtilsWrapper);
};

void LoginUtilsImpl::PrepareProfile(
    const std::string& username,
    const std::string& password,
    const GaiaAuthConsumer::ClientLoginResult& credentials,
    bool pending_requests,
    LoginUtils::Delegate* delegate) {
  BootTimesLoader* btl = BootTimesLoader::Get();

  VLOG(1) << "Completing login for " << username;
  btl->AddLoginTimeMarker("CompletingLogin", false);

  if (CrosLibrary::Get()->EnsureLoaded()) {
    CrosLibrary::Get()->GetLoginLibrary()->StartSession(username, "");
    btl->AddLoginTimeMarker("StartedSession", false);
  }

  UserManager::Get()->UserLoggedIn(username);
  btl->AddLoginTimeMarker("UserLoggedIn", false);

  // Switch log file as soon as possible.
  logging::RedirectChromeLogging(*(CommandLine::ForCurrentProcess()));
  btl->AddLoginTimeMarker("LoggingRedirected", false);

  username_ = username;
  password_ = password;

  credentials_ = credentials;
  pending_requests_ = pending_requests;
  delegate_ = delegate;

  // The default profile will have been changed because the ProfileManager
  // will process the notification that the UserManager sends out.
  ProfileManager::CreateDefaultProfileAsync(this);
}

void LoginUtilsImpl::OnProfileCreated(Profile* profile) {
  CHECK(profile);

  // Initialize the user-policy backend.
  policy::BrowserPolicyConnector* browser_policy_connector =
      g_browser_process->browser_policy_connector();
  browser_policy_connector->InitializeUserPolicy(username_,
                                                 profile->GetPath(),
                                                 profile->GetTokenService());

  BootTimesLoader* btl = BootTimesLoader::Get();
  btl->AddLoginTimeMarker("UserProfileGotten", false);

  // Since we're doing parallel authentication, only new user sign in
  // would perform online auth before calling PrepareProfile.
  // For existing users there's usually a pending online auth request.
  // Cookies will be fetched after it's is succeeded.
  if (!pending_requests_) {
    if (CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kWebUIGaiaLogin)) {
      // Transfer cookies from the profile that was used for authentication.
      // This profile contains cookies that auth extension should have already
      // put in place that will ensure that the newly created session is
      // authenticated for the websites that work with the used authentication
      // schema.
      if (!TransferDefaultCookies(authenticator_->AuthenticationProfile(),
                                  profile)) {
        LOG(WARNING) << "Cookie transfer from the default profile failed!";
      }
      // Fetch OAuth tokens. Use off-the-record profile that was used for
      // authentication step. It should already contain all needed cookies
      // that will let us skip GAIA's user authentication.
      FetchOAuthTokens(authenticator_->AuthenticationProfile());
    } else {
      FetchCookies(profile, credentials_);
    }
  }

  // Init extension event routers; this normally happens in browser_main
  // but on Chrome OS it has to be deferred until the user finishes
  // logging in and the profile is not OTR.
  if (profile->GetExtensionService() &&
      profile->GetExtensionService()->extensions_enabled()) {
    profile->GetExtensionService()->InitEventRouters();
  }
  btl->AddLoginTimeMarker("ExtensionsServiceStarted", false);

  // Supply credentials for sync and others to use. Load tokens from disk.
  TokenService* token_service = profile->GetTokenService();
  token_service->Initialize(GaiaConstants::kChromeOSSource,
                            profile);
  token_service->LoadTokensFromDB();

  // For existing users there's usually a pending online auth request.
  // Tokens will be fetched after it's is succeeded.
  if (!pending_requests_) {
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kWebUIGaiaLogin)) {
      FetchTokens(profile, credentials_);
    }
  }
  btl->AddLoginTimeMarker("TokensGotten", false);

  // Set the CrOS user by getting this constructor run with the
  // user's email on first retrieval.
  profile->GetProfileSyncService(username_)->SetPassphrase(password_,
                                                           false,
                                                           true);
  btl->AddLoginTimeMarker("SyncStarted", false);

  // Own TPM device if, for any reason, it has not been done in EULA
  // wizard screen.
  if (CrosLibrary::Get()->EnsureLoaded()) {
    CryptohomeLibrary* cryptohome = CrosLibrary::Get()->GetCryptohomeLibrary();
    if (cryptohome->TpmIsEnabled() && !cryptohome->TpmIsBeingOwned()) {
      if (cryptohome->TpmIsOwned()) {
        cryptohome->TpmClearStoredPassword();
      } else {
        cryptohome->TpmCanAttemptOwnership();
      }
    }
  }
  btl->AddLoginTimeMarker("TPMOwned", false);

  RespectLocalePreference(profile);

  if (UserManager::Get()->current_user_is_new()) {
    SetFirstLoginPrefs(profile->GetPrefs());
  }

  // Enable/disable plugins based on user preferences.
  PluginUpdater::GetInstance()->SetProfile(profile);
  btl->AddLoginTimeMarker("PluginsStateUpdated", false);

  // We suck. This is a hack since we do not have the enterprise feature
  // done yet to pull down policies from the domain admin. We'll take this
  // out when we get that done properly.
  // TODO(xiyuan): Remove this once enterprise feature is ready.
  if (EndsWith(username_, "@google.com", true)) {
    PrefService* pref_service = profile->GetPrefs();
    pref_service->SetBoolean(prefs::kEnableScreenLock, true);
  }

  profile->OnLogin();

  delegate_->OnProfilePrepared(profile);

  // TODO(altimofeev): Need to sanitize memory used to store password.
  password_ = "";
  username_ = "";
  credentials_ = GaiaAuthConsumer::ClientLoginResult();
}

void LoginUtilsImpl::FetchOAuthTokens(Profile* profile) {
  oauth_fetcher_.reset(new GaiaOAuthFetcher(this,
                                            profile->GetRequestContext(),
                                            profile,
                                            kServiceScopeChromeOS));
  // We don't care about everything this class can get right now, just
  // about OAuth tokens for now.
  oauth_fetcher_->SetAutoFetchMask(
      GaiaOAuthFetcher::OAUTH1_REQUEST_TOKEN |
      GaiaOAuthFetcher::OAUTH1_ALL_ACCESS_TOKEN);
  oauth_fetcher_->StartGetOAuthTokenRequest();
}

void LoginUtilsImpl::FetchCookies(
    Profile* profile,
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  // Take the credentials passed in and try to exchange them for
  // full-fledged Google authentication cookies.  This is
  // best-effort; it's possible that we'll fail due to network
  // troubles or some such.
  // CookieFetcher will delete itself once done.
  CookieFetcher* cf = new CookieFetcher(profile);
  cf->AttemptFetch(credentials.data);
  BootTimesLoader::Get()->AddLoginTimeMarker("CookieFetchStarted", false);
}

void LoginUtilsImpl::FetchTokens(
    Profile* profile,
    const GaiaAuthConsumer::ClientLoginResult& credentials) {
  TokenService* token_service = profile->GetTokenService();
  token_service->UpdateCredentials(credentials);
  if (token_service->AreCredentialsValid()) {
    token_service->StartFetchingTokens();
  }
}

void LoginUtilsImpl::RespectLocalePreference(Profile* profile) {
  DCHECK(profile != NULL);
  PrefService* prefs = profile->GetPrefs();
  DCHECK(prefs != NULL);
  if (g_browser_process == NULL)
    return;

  std::string pref_locale = prefs->GetString(prefs::kApplicationLocale);
  if (pref_locale.empty())
    pref_locale = prefs->GetString(prefs::kApplicationLocaleBackup);
  if (pref_locale.empty())
    pref_locale = g_browser_process->GetApplicationLocale();
  DCHECK(!pref_locale.empty());
  profile->ChangeAppLocale(pref_locale, Profile::APP_LOCALE_CHANGED_VIA_LOGIN);
  // Here we don't enable keyboard layouts. Input methods are set up when
  // the user first logs in. Then the user may customize the input methods.
  // Hence changing input methods here, just because the user's UI language
  // is different from the login screen UI language, is not desirable. Note
  // that input method preferences are synced, so users can use their
  // farovite input methods as soon as the preferences are synced.
  LanguageSwitchMenu::SwitchLanguage(pref_locale);
}

void LoginUtilsImpl::CompleteOffTheRecordLogin(const GURL& start_url) {
  VLOG(1) << "Completing incognito login";

  UserManager::Get()->OffTheRecordUserLoggedIn();

  if (CrosLibrary::Get()->EnsureLoaded()) {
    // Session Manager may kill the chrome anytime after this point.
    // Write exit_cleanly and other stuff to the disk here.
    g_browser_process->EndSession();

    // For guest session we ask session manager to restart Chrome with --bwsi
    // flag. We keep only some of the arguments of this process.
    const CommandLine& browser_command_line = *CommandLine::ForCurrentProcess();
    CommandLine command_line(browser_command_line.GetProgram());
    std::string cmd_line_str =
        GetOffTheRecordCommandLine(start_url,
                                   browser_command_line,
                                   &command_line);

    CrosLibrary::Get()->GetLoginLibrary()->RestartJob(getpid(), cmd_line_str);
  }
}

std::string LoginUtilsImpl::GetOffTheRecordCommandLine(
    const GURL& start_url,
    const CommandLine& base_command_line,
    CommandLine* command_line) {
  static const char* kForwardSwitches[] = {
      switches::kEnableLogging,
      switches::kEnableAcceleratedPlugins,
      switches::kUseGL,
      switches::kUserDataDir,
      switches::kScrollPixels,
      switches::kEnableGView,
      switches::kNoFirstRun,
      switches::kLoginProfile,
      switches::kCompressSystemFeedback,
      switches::kDisableSeccompSandbox,
      switches::kPpapiFlashInProcess,
      switches::kPpapiFlashPath,
      switches::kPpapiFlashVersion,
#if defined(TOUCH_UI)
      switches::kTouchDevices,
      // The virtual keyboard extension for TOUCH_UI (chrome://keyboard) highly
      // relies on experimental APIs.
      switches::kEnableExperimentalExtensionApis,
#endif
  };
  command_line->CopySwitchesFrom(base_command_line,
                                 kForwardSwitches,
                                 arraysize(kForwardSwitches));
  command_line->AppendSwitch(switches::kGuestSession);
  command_line->AppendSwitch(switches::kIncognito);
  command_line->AppendSwitchASCII(switches::kLoggingLevel,
                                 kGuestModeLoggingLevel);

  command_line->AppendSwitchASCII(switches::kLoginUser, kGuestUserName);

  if (start_url.is_valid())
    command_line->AppendArg(start_url.spec());

  // Override the value of the homepage that is set in first run mode.
  // TODO(altimofeev): extend action of the |kNoFirstRun| to cover this case.
  command_line->AppendSwitchASCII(
      switches::kHomePage,
      GURL(chrome::kChromeUINewTabURL).spec());

  std::string cmd_line_str = command_line->GetCommandLineString();
  // Special workaround for the arguments that should be quoted.
  // Copying switches won't be needed when Guest mode won't need restart
  // http://crosbug.com/6924
  if (base_command_line.HasSwitch(switches::kRegisterPepperPlugins)) {
    cmd_line_str += base::StringPrintf(
        kSwitchFormatString,
        switches::kRegisterPepperPlugins,
        base_command_line.GetSwitchValueNative(
            switches::kRegisterPepperPlugins).c_str());
  }

  return cmd_line_str;
}

void LoginUtilsImpl::SetFirstLoginPrefs(PrefService* prefs) {
  VLOG(1) << "Setting first login prefs";
  BootTimesLoader* btl = BootTimesLoader::Get();
  std::string locale = g_browser_process->GetApplicationLocale();

  // First, we'll set kLanguagePreloadEngines.
  input_method::InputMethodManager* manager =
      input_method::InputMethodManager::GetInstance();
  std::vector<std::string> input_method_ids;
  input_method::GetFirstLoginInputMethodIds(locale,
                                            manager->current_input_method(),
                                            &input_method_ids);
  // Save the input methods in the user's preferences.
  StringPrefMember language_preload_engines;
  language_preload_engines.Init(prefs::kLanguagePreloadEngines,
                                prefs, NULL);
  language_preload_engines.SetValue(JoinString(input_method_ids, ','));
  btl->AddLoginTimeMarker("IMEStarted", false);

  // Second, we'll set kLanguagePreferredLanguages.
  std::vector<std::string> language_codes;
  // The current locale should be on the top.
  language_codes.push_back(locale);

  // Add input method IDs based on the input methods, as there may be
  // input methods that are unrelated to the current locale. Example: the
  // hardware keyboard layout xkb:us::eng is used for logging in, but the
  // UI language is set to French. In this case, we should set "fr,en"
  // to the preferred languages preference.
  std::vector<std::string> candidates;
  input_method::GetLanguageCodesFromInputMethodIds(
      input_method_ids, &candidates);
  for (size_t i = 0; i < candidates.size(); ++i) {
    const std::string& candidate = candidates[i];
    // Skip if it's already in language_codes.
    if (std::count(language_codes.begin(), language_codes.end(),
                   candidate) == 0) {
      language_codes.push_back(candidate);
    }
  }
  // Save the preferred languages in the user's preferences.
  StringPrefMember language_preferred_languages;
  language_preferred_languages.Init(prefs::kLanguagePreferredLanguages,
                                    prefs, NULL);
  language_preferred_languages.SetValue(JoinString(language_codes, ','));
  prefs->ScheduleSavePersistentPrefs();
}

Authenticator* LoginUtilsImpl::CreateAuthenticator(
    LoginStatusConsumer* consumer) {
  if (!authenticator_.get()) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kParallelAuth))
    authenticator_ = new ParallelAuthenticator(consumer);
  else
    authenticator_ = new GoogleAuthenticator(consumer);
  }
  return authenticator_.get();
}

// We use a special class for this so that it can be safely leaked if we
// never connect. At shutdown the order is not well defined, and it's possible
// for the infrastructure needed to unregister might be unstable and crash.
class WarmingObserver : public NetworkLibrary::NetworkManagerObserver {
 public:
  WarmingObserver() {
    NetworkLibrary *netlib = CrosLibrary::Get()->GetNetworkLibrary();
    netlib->AddNetworkManagerObserver(this);
  }

  virtual ~WarmingObserver() {}

  // If we're now connected, prewarm the auth url.
  virtual void OnNetworkManagerChanged(NetworkLibrary* netlib) {
    if (netlib->Connected()) {
      const int kConnectionsNeeded = 1;
      chrome_browser_net::PreconnectOnUIThread(
          GURL(GaiaUrls::GetInstance()->client_login_url()),
          chrome_browser_net::UrlInfo::EARLY_LOAD_MOTIVATED,
          kConnectionsNeeded);
      netlib->RemoveNetworkManagerObserver(this);
      delete this;
    }
  }
};

void LoginUtilsImpl::PrewarmAuthentication() {
  if (CrosLibrary::Get()->EnsureLoaded()) {
    NetworkLibrary *network = CrosLibrary::Get()->GetNetworkLibrary();
    if (network->Connected()) {
      const int kConnectionsNeeded = 1;
      chrome_browser_net::PreconnectOnUIThread(
          GURL(GaiaUrls::GetInstance()->client_login_url()),
          chrome_browser_net::UrlInfo::EARLY_LOAD_MOTIVATED,
          kConnectionsNeeded);
    } else {
      new WarmingObserver();
    }
  }
}

void LoginUtilsImpl::SetBackgroundView(BackgroundView* background_view) {
  background_view_ = background_view;
}

BackgroundView* LoginUtilsImpl::GetBackgroundView() {
  return background_view_;
}

bool LoginUtilsImpl::TransferDefaultCookies(Profile* default_profile,
                                            Profile* profile) {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          new TransferDefaultCookiesOnIOThreadTask(
                              default_profile->GetRequestContext(),
                              profile,
                              profile->GetRequestContext()));
  return true;
}

void LoginUtilsImpl::OnGetOAuthTokenSuccess(const std::string& oauth_token) {
  VLOG(1) << "Got OAuth request token!";
}

void LoginUtilsImpl::OnGetOAuthTokenFailure() {
  // TODO(zelidrag): Pop up sync setup UI here?
  LOG(WARNING) << "Failed fetching OAuth request token";
}

void LoginUtilsImpl::OnOAuthGetAccessTokenSuccess(const std::string& token,
                                                  const std::string& secret) {
  // TODO(zelidrag): OK, now we have OAuth1 token in place. Where do I stick it?
  VLOG(1) << "Got OAuth v1 token!";
}

void LoginUtilsImpl::OnOAuthGetAccessTokenFailure(
    const GoogleServiceAuthError& error) {
  // TODO(zelidrag): Pop up sync setup UI here?
  LOG(WARNING) << "Failed fetching OAuth v1 token, error: " << error.state();
}

void LoginUtilsImpl::OnOAuthWrapBridgeSuccess(const std::string& token,
    const std::string& expires_in) {
  // TODO(zelidrag): OK, now we have OAuth2 token in place. Where do I stick it?
  VLOG(1) << "Got OAuth v2 token!";
}

void LoginUtilsImpl::OnOAuthWrapBridgeFailure(
    const GoogleServiceAuthError& error) {
  // TODO(zelidrag): Pop up sync setup UI here?
  LOG(WARNING) << "Failed fetching OAuth v2 token, error: " << error.state();
}


LoginUtils* LoginUtils::Get() {
  return LoginUtilsWrapper::GetInstance()->get();
}

void LoginUtils::Set(LoginUtils* mock) {
  LoginUtilsWrapper::GetInstance()->reset(mock);
}

void LoginUtils::DoBrowserLaunch(Profile* profile,
                                 LoginDisplayHost* login_host) {
  BootTimesLoader::Get()->AddLoginTimeMarker("BrowserLaunched", false);

  VLOG(1) << "Launching browser...";
  BrowserInit browser_init;
  int return_code;
  browser_init.LaunchBrowser(*CommandLine::ForCurrentProcess(),
                             profile,
                             FilePath(),
                             true,
                             &return_code);

  // Mark login host for deletion after browser starts.  This
  // guarantees that the message loop will be referenced by the
  // browser before it is dereferenced by the login host.
  if (login_host) {
    login_host->OnSessionStart();
    login_host = NULL;
  }
}

}  // namespace chromeos
