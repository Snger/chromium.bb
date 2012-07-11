// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cookies_tree_model.h"

#include <algorithm>
#include <functional>
#include <vector>

#include "base/bind.h"
#include "base/memory/linked_ptr.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browsing_data_cookie_helper.h"
#include "chrome/browser/browsing_data_server_bound_cert_helper.h"
#include "chrome/browser/content_settings/cookie_settings.h"
#include "chrome/browser/extensions/extension_service.h"
#include "content/public/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "net/base/registry_controlled_domain.h"
#include "net/cookies/cookie_monster.h"
#include "net/url_request/url_request_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

namespace {

struct NodeTitleComparator {
  bool operator()(const CookieTreeNode* lhs, const CookieTreeNode* rhs) {
    return lhs->GetTitle() < rhs->GetTitle();
  }
};

// Comparison functor, for use in CookieTreeRootNode.
struct OriginNodeComparator {
  bool operator()(const CookieTreeNode* lhs, const CookieTreeNode* rhs) {
    // This comparator is only meant to compare CookieTreeOriginNode types. Make
    // sure we check this, as the static cast below is dangerous if we get the
    // wrong object type.
    CHECK_EQ(lhs->GetDetailedInfo().node_type,
             CookieTreeNode::DetailedInfo::TYPE_ORIGIN);
    CHECK_EQ(rhs->GetDetailedInfo().node_type,
             CookieTreeNode::DetailedInfo::TYPE_ORIGIN);

    const CookieTreeOriginNode* ltn =
        static_cast<const CookieTreeOriginNode*>(lhs);
    const CookieTreeOriginNode* rtn =
        static_cast<const CookieTreeOriginNode*>(rhs);

    if (ltn->app_id() != rtn->app_id()) {
      return (ltn->app_name() < rtn->app_name());
    }

    // We want to order by registry controlled domain, so we would get
    // google.com, ad.google.com, www.google.com,
    // microsoft.com, ad.microsoft.com. CanonicalizeHost transforms the origins
    // into a form like google.com.www so that string comparisons work.
    return (CanonicalizeHost(ltn->GetHost()) <
            CanonicalizeHost(rtn->GetHost()));
  }

  static std::string CanonicalizeHost(const std::string& host) {
    // The canonicalized representation makes the registry controlled domain
    // come first, and then adds subdomains in reverse order, e.g.
    // 1.mail.google.com would become google.com.mail.1, and then a standard
    // string comparison works to order hosts by registry controlled domain
    // first. Leading dots are ignored, ".google.com" is the same as
    // "google.com".

    std::string retval = net::RegistryControlledDomainService::
        GetDomainAndRegistry(host);
    if (!retval.length())  // Is an IP address or other special origin.
      return host;

    std::string::size_type position = host.rfind(retval);

    // The host may be the registry controlled domain, in which case fail fast.
    if (position == 0 || position == std::string::npos)
      return host;

    // If host is www.google.com, retval will contain google.com at this point.
    // Start operating to the left of the registry controlled domain, e.g. in
    // the www.google.com example, start at index 3.
    --position;

    // If position == 0, that means it's a dot; this will be ignored to treat
    // ".google.com" the same as "google.com".
    while (position > 0) {
      retval += std::string(".");
      // Copy up to the next dot. host[position] is a dot so start after it.
      std::string::size_type next_dot = host.rfind(".", position - 1);
      if (next_dot == std::string::npos) {
        retval += host.substr(0, position);
        break;
      }
      retval += host.substr(next_dot + 1, position - (next_dot + 1));
      position = next_dot;
    }
    return retval;
  }
};

// This function returns the local data container associated with a leaf tree
// node. The app node is assumed to be 3 levels above the leaf because of the
// following structure:
//   root -> origin -> storage type -> leaf node
LocalDataContainer* GetLocalDataContainerForNode(CookieTreeNode* node) {
  CookieTreeOriginNode* origin = static_cast<CookieTreeOriginNode*>(
      node->parent()->parent());
  CHECK_EQ(origin->GetDetailedInfo().node_type,
           CookieTreeNode::DetailedInfo::TYPE_ORIGIN);
  return node->GetModel()->GetLocalDataContainer(origin->app_id());
}


} // namespace

///////////////////////////////////////////////////////////////////////////////
// CookieTreeNode, public:

void CookieTreeNode::DeleteStoredObjects() {
  std::for_each(children().begin(),
                children().end(),
                std::mem_fun(&CookieTreeNode::DeleteStoredObjects));
}

CookiesTreeModel* CookieTreeNode::GetModel() const {
  if (parent())
    return parent()->GetModel();
  else
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeCookieNode, public:

CookieTreeCookieNode::CookieTreeCookieNode(
    std::list<net::CookieMonster::CanonicalCookie>::iterator cookie)
    : CookieTreeNode(UTF8ToUTF16(cookie->Name())),
      cookie_(cookie) {
}

CookieTreeCookieNode::~CookieTreeCookieNode() {}

void CookieTreeCookieNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);
  CHECK(container);
  container->cookie_helper_->DeleteCookie(*cookie_);
  container->cookie_list_.erase(cookie_);
}

CookieTreeNode::DetailedInfo CookieTreeCookieNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitCookie(&*cookie_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeAppCacheNode, public:

CookieTreeAppCacheNode::CookieTreeAppCacheNode(
    const GURL& origin_url,
    std::list<appcache::AppCacheInfo>::iterator appcache_info)
    : CookieTreeNode(UTF8ToUTF16(appcache_info->manifest_url.spec())),
      origin_url_(origin_url),
      appcache_info_(appcache_info) {
}

CookieTreeAppCacheNode::~CookieTreeAppCacheNode() {
}

void CookieTreeAppCacheNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    DCHECK(container->appcache_helper_);
    container->appcache_helper_->DeleteAppCacheGroup(
        appcache_info_->manifest_url);
    container->appcache_info_[origin_url_].erase(appcache_info_);
  }
}

CookieTreeNode::DetailedInfo CookieTreeAppCacheNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitAppCache(
      &*appcache_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeDatabaseNode, public:

CookieTreeDatabaseNode::CookieTreeDatabaseNode(
    std::list<BrowsingDataDatabaseHelper::DatabaseInfo>::iterator database_info)
    : CookieTreeNode(database_info->database_name.empty() ?
          l10n_util::GetStringUTF16(IDS_COOKIES_WEB_DATABASE_UNNAMED_NAME) :
          UTF8ToUTF16(database_info->database_name)),
      database_info_(database_info) {
}

CookieTreeDatabaseNode::~CookieTreeDatabaseNode() {}

void CookieTreeDatabaseNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    container->database_helper_->DeleteDatabase(
        database_info_->origin_identifier, database_info_->database_name);
    container->database_info_list_.erase(database_info_);
  }
}

CookieTreeNode::DetailedInfo CookieTreeDatabaseNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitDatabase(
      &*database_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeLocalStorageNode, public:

CookieTreeLocalStorageNode::CookieTreeLocalStorageNode(
    std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>::iterator
        local_storage_info)
    : CookieTreeNode(UTF8ToUTF16(local_storage_info->origin_url.spec())),
      local_storage_info_(local_storage_info) {
}

CookieTreeLocalStorageNode::~CookieTreeLocalStorageNode() {}

void CookieTreeLocalStorageNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    container->local_storage_helper_->DeleteOrigin(
        local_storage_info_->origin_url);
    container->local_storage_info_list_.erase(local_storage_info_);
  }
}

CookieTreeNode::DetailedInfo
CookieTreeLocalStorageNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitLocalStorage(
      &*local_storage_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeSessionStorageNode, public:

CookieTreeSessionStorageNode::CookieTreeSessionStorageNode(
    std::list<BrowsingDataLocalStorageHelper::LocalStorageInfo>::iterator
        session_storage_info)
    : CookieTreeNode(UTF8ToUTF16(session_storage_info->origin_url.spec())),
      session_storage_info_(session_storage_info) {
}

CookieTreeSessionStorageNode::~CookieTreeSessionStorageNode() {}

void CookieTreeSessionStorageNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    container->session_storage_info_list_.erase(session_storage_info_);
  }
}

CookieTreeNode::DetailedInfo
CookieTreeSessionStorageNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitSessionStorage(
      &*session_storage_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeIndexedDBNode, public:

CookieTreeIndexedDBNode::CookieTreeIndexedDBNode(
    std::list<BrowsingDataIndexedDBHelper::IndexedDBInfo>::iterator
        indexed_db_info)
    : CookieTreeNode(UTF8ToUTF16(
          indexed_db_info->origin.spec())),
      indexed_db_info_(indexed_db_info) {
}

CookieTreeIndexedDBNode::~CookieTreeIndexedDBNode() {}

void CookieTreeIndexedDBNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    container->indexed_db_helper_->DeleteIndexedDB(
        indexed_db_info_->origin);
    container->indexed_db_info_list_.erase(indexed_db_info_);
  }
}

CookieTreeNode::DetailedInfo CookieTreeIndexedDBNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitIndexedDB(
      &*indexed_db_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeFileSystemNode, public:

CookieTreeFileSystemNode::CookieTreeFileSystemNode(
    std::list<BrowsingDataFileSystemHelper::FileSystemInfo>::iterator
        file_system_info)
    : CookieTreeNode(UTF8ToUTF16(
          file_system_info->origin.spec())),
      file_system_info_(file_system_info) {
}

CookieTreeFileSystemNode::~CookieTreeFileSystemNode() {}

void CookieTreeFileSystemNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    container->file_system_helper_->DeleteFileSystemOrigin(
        file_system_info_->origin);
    container->file_system_info_list_.erase(file_system_info_);
  }
}

CookieTreeNode::DetailedInfo CookieTreeFileSystemNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitFileSystem(
      &*file_system_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeQuotaNode, public:

CookieTreeQuotaNode::CookieTreeQuotaNode(
    std::list<BrowsingDataQuotaHelper::QuotaInfo>::iterator quota_info)
    : CookieTreeNode(UTF8ToUTF16(quota_info->host)),
      quota_info_(quota_info) {
}

CookieTreeQuotaNode::~CookieTreeQuotaNode() {}

void CookieTreeQuotaNode::DeleteStoredObjects() {
  // Calling this function may cause unexpected over-quota state of origin.
  // However, it'll caused no problem, just prevent usage growth of the origin.
  CookieTreeOriginNode* app = static_cast<CookieTreeOriginNode*>(parent());
  LocalDataContainer* container = GetModel()->GetLocalDataContainer(
      app->app_id());

  if (container) {
    container->quota_helper_->RevokeHostQuota(quota_info_->host);
    container->quota_info_list_.erase(quota_info_);
  }
}

CookieTreeNode::DetailedInfo CookieTreeQuotaNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitQuota(
      &*quota_info_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeServerBoundCertNode, public:

CookieTreeServerBoundCertNode::CookieTreeServerBoundCertNode(
      net::ServerBoundCertStore::ServerBoundCertList::iterator cert)
    : CookieTreeNode(ASCIIToUTF16(cert->server_identifier())),
      server_bound_cert_(cert) {
}

CookieTreeServerBoundCertNode::~CookieTreeServerBoundCertNode() {}

void CookieTreeServerBoundCertNode::DeleteStoredObjects() {
  LocalDataContainer* container = GetLocalDataContainerForNode(this);

  if (container) {
    container->server_bound_cert_helper_->DeleteServerBoundCert(
        server_bound_cert_->server_identifier());
    container->server_bound_cert_list_.erase(server_bound_cert_);
  }
}

CookieTreeNode::DetailedInfo
CookieTreeServerBoundCertNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->parent()->GetTitle()).InitServerBoundCert(
      &*server_bound_cert_);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeRootNode, public:

CookieTreeRootNode::CookieTreeRootNode(CookiesTreeModel* model)
    : model_(model) {
}

CookieTreeRootNode::~CookieTreeRootNode() {}

CookieTreeOriginNode* CookieTreeRootNode::GetOrCreateOriginNode(
    const GURL& url,
    const std::string& app_id,
    const std::string& app_name) {
  CookieTreeOriginNode origin_node(url, app_id, app_name);

  // First see if there is an existing match.
  std::vector<CookieTreeNode*>::iterator origin_node_iterator =
        std::lower_bound(children().begin(), children().end(), &origin_node,
                         OriginNodeComparator());
  if (origin_node_iterator != children().end() &&
      CookieTreeOriginNode::TitleForUrl(url, app_id, app_name) ==
      (*origin_node_iterator)->GetTitle())
    return static_cast<CookieTreeOriginNode*>(*origin_node_iterator);
  // Node doesn't exist, create a new one and insert it into the (ordered)
  // children.
  CookieTreeOriginNode* retval = new CookieTreeOriginNode(url, app_id,
                                                          app_name);
  DCHECK(model_);
  model_->Add(this, retval, (origin_node_iterator - children().begin()));
  return retval;
}

CookiesTreeModel* CookieTreeRootNode::GetModel() const {
  return model_;
}

CookieTreeNode::DetailedInfo CookieTreeRootNode::GetDetailedInfo() const {
  return DetailedInfo(string16()).Init(DetailedInfo::TYPE_ROOT);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeOriginNode, public:

// static
string16 CookieTreeOriginNode::TitleForUrl(
    const GURL& url,
    const std::string& app_id,
    const std::string& app_name) {
  std::string title;
  const std::string file_origin_node_name(
      std::string(chrome::kFileScheme) + content::kStandardSchemeSeparator);
  // Only prepend the app name, if the app id is not an empty string.
  if (!app_id.empty()) {
    title = app_name;
    title.append(", ");
  }
  title.append(url.SchemeIsFile() ? file_origin_node_name : url.host());
  return UTF8ToUTF16(title);
}

CookieTreeOriginNode::CookieTreeOriginNode(const GURL& url,
                                           const std::string& app_id,
                                           const std::string& name)
    : CookieTreeNode(TitleForUrl(url, app_id, name)),
      cookies_child_(NULL),
      databases_child_(NULL),
      local_storages_child_(NULL),
      session_storages_child_(NULL),
      appcaches_child_(NULL),
      indexed_dbs_child_(NULL),
      file_systems_child_(NULL),
      quota_child_(NULL),
      server_bound_certs_child_(NULL),
      app_id_(app_id),
      app_name_(name),
      url_(url) {}

CookieTreeOriginNode::~CookieTreeOriginNode() {}

const std::string CookieTreeOriginNode::GetHost() const {
  const std::string file_origin_node_name(
      std::string(chrome::kFileScheme) + content::kStandardSchemeSeparator);
  return url_.SchemeIsFile() ? file_origin_node_name : url_.host();
}

CookieTreeNode::DetailedInfo CookieTreeOriginNode::GetDetailedInfo() const {
  return DetailedInfo(GetTitle()).InitOrigin(app_id_, app_name_);
}

CookieTreeCookiesNode* CookieTreeOriginNode::GetOrCreateCookiesNode() {
  if (cookies_child_)
    return cookies_child_;
  cookies_child_ = new CookieTreeCookiesNode;
  AddChildSortedByTitle(cookies_child_);
  return cookies_child_;
}

CookieTreeDatabasesNode* CookieTreeOriginNode::GetOrCreateDatabasesNode() {
  if (databases_child_)
    return databases_child_;
  databases_child_ = new CookieTreeDatabasesNode;
  AddChildSortedByTitle(databases_child_);
  return databases_child_;
}

CookieTreeLocalStoragesNode*
    CookieTreeOriginNode::GetOrCreateLocalStoragesNode() {
  if (local_storages_child_)
    return local_storages_child_;
  local_storages_child_ = new CookieTreeLocalStoragesNode;
  AddChildSortedByTitle(local_storages_child_);
  return local_storages_child_;
}

CookieTreeSessionStoragesNode*
    CookieTreeOriginNode::GetOrCreateSessionStoragesNode() {
  if (session_storages_child_)
    return session_storages_child_;
  session_storages_child_ = new CookieTreeSessionStoragesNode;
  AddChildSortedByTitle(session_storages_child_);
  return session_storages_child_;
}

CookieTreeAppCachesNode* CookieTreeOriginNode::GetOrCreateAppCachesNode() {
  if (appcaches_child_)
    return appcaches_child_;
  appcaches_child_ = new CookieTreeAppCachesNode;
  AddChildSortedByTitle(appcaches_child_);
  return appcaches_child_;
}

CookieTreeIndexedDBsNode* CookieTreeOriginNode::GetOrCreateIndexedDBsNode() {
  if (indexed_dbs_child_)
    return indexed_dbs_child_;
  indexed_dbs_child_ = new CookieTreeIndexedDBsNode;
  AddChildSortedByTitle(indexed_dbs_child_);
  return indexed_dbs_child_;
}

CookieTreeFileSystemsNode* CookieTreeOriginNode::GetOrCreateFileSystemsNode() {
  if (file_systems_child_)
    return file_systems_child_;
  file_systems_child_ = new CookieTreeFileSystemsNode;
  AddChildSortedByTitle(file_systems_child_);
  return file_systems_child_;
}

CookieTreeQuotaNode* CookieTreeOriginNode::UpdateOrCreateQuotaNode(
    std::list<BrowsingDataQuotaHelper::QuotaInfo>::iterator quota_info) {
  if (quota_child_)
    return quota_child_;
  quota_child_ = new CookieTreeQuotaNode(quota_info);
  AddChildSortedByTitle(quota_child_);
  return quota_child_;
}

CookieTreeServerBoundCertsNode*
CookieTreeOriginNode::GetOrCreateServerBoundCertsNode() {
  if (server_bound_certs_child_)
    return server_bound_certs_child_;
  server_bound_certs_child_ = new CookieTreeServerBoundCertsNode;
  AddChildSortedByTitle(server_bound_certs_child_);
  return server_bound_certs_child_;
}

void CookieTreeOriginNode::CreateContentException(
    CookieSettings* cookie_settings, ContentSetting setting) const {
  DCHECK(setting == CONTENT_SETTING_ALLOW ||
         setting == CONTENT_SETTING_BLOCK ||
         setting == CONTENT_SETTING_SESSION_ONLY);
  if (CanCreateContentException()) {
    cookie_settings->ResetCookieSetting(
        ContentSettingsPattern::FromURLNoWildcard(url_),
        ContentSettingsPattern::Wildcard());
    cookie_settings->SetCookieSetting(
        ContentSettingsPattern::FromURL(url_),
        ContentSettingsPattern::Wildcard(), setting);
  }
}

bool CookieTreeOriginNode::CanCreateContentException() const {
  return !url_.SchemeIsFile();
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeCookiesNode, public:

CookieTreeCookiesNode::CookieTreeCookiesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_COOKIES)) {
}

CookieTreeCookiesNode::~CookieTreeCookiesNode() {
}

CookieTreeNode::DetailedInfo CookieTreeCookiesNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->GetTitle()).Init(DetailedInfo::TYPE_COOKIES);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeAppCachesNode, public:

CookieTreeAppCachesNode::CookieTreeAppCachesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(
                         IDS_COOKIES_APPLICATION_CACHES)) {
}

CookieTreeAppCachesNode::~CookieTreeAppCachesNode() {}

CookieTreeNode::DetailedInfo CookieTreeAppCachesNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->GetTitle()).Init(DetailedInfo::TYPE_APPCACHES);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeDatabasesNode, public:

CookieTreeDatabasesNode::CookieTreeDatabasesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_WEB_DATABASES)) {
}

CookieTreeDatabasesNode::~CookieTreeDatabasesNode() {}

CookieTreeNode::DetailedInfo CookieTreeDatabasesNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->GetTitle()).Init(DetailedInfo::TYPE_DATABASES);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeLocalStoragesNode, public:

CookieTreeLocalStoragesNode::CookieTreeLocalStoragesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_LOCAL_STORAGE)) {
}

CookieTreeLocalStoragesNode::~CookieTreeLocalStoragesNode() {}

CookieTreeNode::DetailedInfo
CookieTreeLocalStoragesNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->GetTitle()).Init(
      DetailedInfo::TYPE_LOCAL_STORAGES);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeSessionStoragesNode, public:

CookieTreeSessionStoragesNode::CookieTreeSessionStoragesNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_SESSION_STORAGE)) {
}

CookieTreeSessionStoragesNode::~CookieTreeSessionStoragesNode() {}

CookieTreeNode::DetailedInfo
CookieTreeSessionStoragesNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->GetTitle()).Init(
      DetailedInfo::TYPE_SESSION_STORAGES);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeIndexedDBsNode, public:

CookieTreeIndexedDBsNode::CookieTreeIndexedDBsNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_INDEXED_DBS)) {
}

CookieTreeIndexedDBsNode::~CookieTreeIndexedDBsNode() {}

CookieTreeNode::DetailedInfo
CookieTreeIndexedDBsNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->GetTitle()).Init(
      DetailedInfo::TYPE_INDEXED_DBS);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeFileSystemsNode, public:

CookieTreeFileSystemsNode::CookieTreeFileSystemsNode()
    : CookieTreeNode(l10n_util::GetStringUTF16(IDS_COOKIES_FILE_SYSTEMS)) {
}

CookieTreeFileSystemsNode::~CookieTreeFileSystemsNode() {}

CookieTreeNode::DetailedInfo
CookieTreeFileSystemsNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->GetTitle()).Init(
      DetailedInfo::TYPE_FILE_SYSTEMS);
}

///////////////////////////////////////////////////////////////////////////////
// CookieTreeServerBoundCertsNode, public:

CookieTreeServerBoundCertsNode::CookieTreeServerBoundCertsNode()
    : CookieTreeNode(
        l10n_util::GetStringUTF16(IDS_COOKIES_SERVER_BOUND_CERTS)) {
}

CookieTreeServerBoundCertsNode::~CookieTreeServerBoundCertsNode() {}

CookieTreeNode::DetailedInfo
CookieTreeServerBoundCertsNode::GetDetailedInfo() const {
  return DetailedInfo(parent()->GetTitle()).Init(
      DetailedInfo::TYPE_SERVER_BOUND_CERTS);
}

void CookieTreeNode::AddChildSortedByTitle(CookieTreeNode* new_child) {
  DCHECK(new_child);
  std::vector<CookieTreeNode*>::iterator iter =
      std::lower_bound(children().begin(), children().end(), new_child,
                       NodeTitleComparator());
  GetModel()->Add(this, new_child, iter - children().begin());
}

///////////////////////////////////////////////////////////////////////////////
// ScopedBatchUpdateNotifier
CookiesTreeModel::ScopedBatchUpdateNotifier::ScopedBatchUpdateNotifier(
  CookiesTreeModel* model, CookieTreeNode* node)
      : model_(model), node_(node), batch_in_progress_(false) {
}

CookiesTreeModel::ScopedBatchUpdateNotifier::~ScopedBatchUpdateNotifier() {
  if (batch_in_progress_) {
    model_->NotifyObserverTreeNodeChanged(node_);
    model_->NotifyObserverEndBatch();
  }
}

void CookiesTreeModel::ScopedBatchUpdateNotifier::StartBatchUpdate() {
  if (!batch_in_progress_) {
    model_->NotifyObserverBeginBatch();
    batch_in_progress_ = true;
  }
}

///////////////////////////////////////////////////////////////////////////////
// CookiesTreeModel, public:
CookiesTreeModel::CookiesTreeModel(const ContainerMap& apps_map,
                                   bool group_by_cookie_source)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::TreeNodeModel<CookieTreeNode>(
          new CookieTreeRootNode(this))),
      app_data_map_(apps_map),
      group_by_cookie_source_(group_by_cookie_source),
      batch_update_(0) {
  for (ContainerMap::const_iterator it = apps_map.begin();
       it != apps_map.end(); ++it) {
    it->second->Init(this);
  }
}

CookiesTreeModel::~CookiesTreeModel() {
  STLDeleteValues(&app_data_map_);
}

///////////////////////////////////////////////////////////////////////////////
// CookiesTreeModel, TreeModel methods (public):

// TreeModel methods:
// Returns the set of icons for the nodes in the tree. You only need override
// this if you don't want to use the default folder icons.
void CookiesTreeModel::GetIcons(std::vector<gfx::ImageSkia>* icons) {
  icons->push_back(*ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_OMNIBOX_HTTP));
  icons->push_back(*ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_COOKIE_ICON));
  icons->push_back(*ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      IDR_COOKIE_STORAGE_ICON));
}

// Returns the index of the icon to use for |node|. Return -1 to use the
// default icon. The index is relative to the list of icons returned from
// GetIcons.
int CookiesTreeModel::GetIconIndex(ui::TreeModelNode* node) {
  CookieTreeNode* ct_node = static_cast<CookieTreeNode*>(node);
  switch (ct_node->GetDetailedInfo().node_type) {
    case CookieTreeNode::DetailedInfo::TYPE_ORIGIN:
      return ORIGIN;
    case CookieTreeNode::DetailedInfo::TYPE_COOKIE:
      return COOKIE;
    case CookieTreeNode::DetailedInfo::TYPE_DATABASE:
      return DATABASE;
    case CookieTreeNode::DetailedInfo::TYPE_LOCAL_STORAGE:
      return DATABASE;  // close enough
    case CookieTreeNode::DetailedInfo::TYPE_SESSION_STORAGE:
      return DATABASE;  // ditto
    case CookieTreeNode::DetailedInfo::TYPE_APPCACHE:
      return DATABASE;  // ditto
    case CookieTreeNode::DetailedInfo::TYPE_INDEXED_DB:
      return DATABASE;  // ditto
    case CookieTreeNode::DetailedInfo::TYPE_FILE_SYSTEM:
      return DATABASE;  // ditto
    case CookieTreeNode::DetailedInfo::TYPE_QUOTA:
      return -1;
    case CookieTreeNode::DetailedInfo::TYPE_SERVER_BOUND_CERT:
      return COOKIE;  // It's kinda like a cookie?
    default:
      break;
  }
  return -1;
}

void CookiesTreeModel::DeleteAllStoredObjects() {
  NotifyObserverBeginBatch();
  CookieTreeNode* root = GetRoot();
  root->DeleteStoredObjects();
  int num_children = root->child_count();
  for (int i = num_children - 1; i >= 0; --i)
    delete Remove(root, root->GetChild(i));
  NotifyObserverTreeNodeChanged(root);
  NotifyObserverEndBatch();
}

void CookiesTreeModel::DeleteCookieNode(CookieTreeNode* cookie_node) {
  if (cookie_node == GetRoot())
    return;
  cookie_node->DeleteStoredObjects();
  CookieTreeNode* parent_node = cookie_node->parent();
  delete Remove(parent_node, cookie_node);
  if (parent_node->empty())
    DeleteCookieNode(parent_node);
}

void CookiesTreeModel::UpdateSearchResults(const string16& filter) {
  CookieTreeNode* root = GetRoot();
  ScopedBatchUpdateNotifier notifier(this, root);
  int num_children = root->child_count();
  notifier.StartBatchUpdate();
  for (int i = num_children - 1; i >= 0; --i)
    delete Remove(root, root->GetChild(i));

  for (ContainerMap::iterator app_iterator = app_data_map_.begin();
       app_iterator != app_data_map_.end(); ++app_iterator ) {
    LocalDataContainer* container = app_iterator->second;

    PopulateCookieInfoWithFilter(container, &notifier, filter);
    PopulateDatabaseInfoWithFilter(container, &notifier, filter);
    PopulateLocalStorageInfoWithFilter(container, &notifier, filter);
    PopulateSessionStorageInfoWithFilter(container, &notifier, filter);
    PopulateAppCacheInfoWithFilter(container, &notifier, filter);
    PopulateIndexedDBInfoWithFilter(container, &notifier, filter);
    PopulateFileSystemInfoWithFilter(container, &notifier, filter);
    PopulateQuotaInfoWithFilter(container, &notifier, filter);
    PopulateServerBoundCertInfoWithFilter(container, &notifier, filter);
  }
}

void CookiesTreeModel::AddCookiesTreeObserver(Observer* observer) {
  cookies_observer_list_.AddObserver(observer);
  // Call super so that TreeNodeModel can notify, too.
  ui::TreeNodeModel<CookieTreeNode>::AddObserver(observer);
}

void CookiesTreeModel::RemoveCookiesTreeObserver(Observer* observer) {
  cookies_observer_list_.RemoveObserver(observer);
  // Call super so that TreeNodeModel doesn't have dead pointers.
  ui::TreeNodeModel<CookieTreeNode>::RemoveObserver(observer);
}

void CookiesTreeModel::PopulateAppCacheInfo(LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateAppCacheInfoWithFilter(container, &notifier, string16());
}

void CookiesTreeModel::PopulateCookieInfo(LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateCookieInfoWithFilter(container, &notifier, string16());
}

void CookiesTreeModel::PopulateDatabaseInfo(LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateDatabaseInfoWithFilter(container, &notifier, string16());
}

void CookiesTreeModel::PopulateLocalStorageInfo(LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateLocalStorageInfoWithFilter(container, &notifier, string16());
}

void CookiesTreeModel::PopulateSessionStorageInfo(
      LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateSessionStorageInfoWithFilter(container, &notifier, string16());
}

void CookiesTreeModel::PopulateIndexedDBInfo(LocalDataContainer* container){
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateIndexedDBInfoWithFilter(container, &notifier, string16());
}

void CookiesTreeModel::PopulateFileSystemInfo(LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateFileSystemInfoWithFilter(container, &notifier, string16());
}

void CookiesTreeModel::PopulateQuotaInfo(LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateQuotaInfoWithFilter(container, &notifier, string16());
}

void CookiesTreeModel::PopulateServerBoundCertInfo(
      LocalDataContainer* container) {
  ScopedBatchUpdateNotifier notifier(this, GetRoot());
  PopulateServerBoundCertInfoWithFilter(container, &notifier, string16());
}

void CookiesTreeModel::PopulateAppCacheInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const string16& filter) {
  using appcache::AppCacheInfo;
  typedef std::map<GURL, std::list<AppCacheInfo> > InfoByOrigin;
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->appcache_info_.empty())
    return;

  notifier->StartBatchUpdate();
  for (InfoByOrigin::iterator origin = container->appcache_info_.begin();
       origin != container->appcache_info_.end(); ++origin) {
    string16 origin_node_name = UTF8ToUTF16(origin->first.host());
    if (filter.empty() ||
        (origin_node_name.find(filter) != string16::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin->first, container->app_id(),
                                      container->app_name());
      CookieTreeAppCachesNode* appcaches_node =
          origin_node->GetOrCreateAppCachesNode();

      for (std::list<AppCacheInfo>::iterator info = origin->second.begin();
           info != origin->second.end(); ++info) {
        appcaches_node->AddAppCacheNode(
            new CookieTreeAppCacheNode(origin->first, info));
      }
    }
  }
}

void CookiesTreeModel::PopulateCookieInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  notifier->StartBatchUpdate();
  for (CookieList::iterator it = container->cookie_list_.begin();
       it != container->cookie_list_.end(); ++it) {
    std::string source_string = it->Source();
    if (source_string.empty() || !group_by_cookie_source_) {
      std::string domain = it->Domain();
      if (domain.length() > 1 && domain[0] == '.')
        domain = domain.substr(1);

      // We treat secure cookies just the same as normal ones.
      source_string = std::string(chrome::kHttpScheme) +
          content::kStandardSchemeSeparator + domain + "/";
    }

    GURL source(source_string);
    if (!filter.size() ||
        (CookieTreeOriginNode::TitleForUrl(source, container->app_id(),
        container->app_name()).find(filter) != string16::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(source, container->app_id(),
                                      container->app_name());
      CookieTreeCookiesNode* cookies_node =
          origin_node->GetOrCreateCookiesNode();
      CookieTreeCookieNode* new_cookie = new CookieTreeCookieNode(it);
      cookies_node->AddCookieNode(new_cookie);
    }
  }
}

void CookiesTreeModel::PopulateDatabaseInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->database_info_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (DatabaseInfoList::iterator database_info =
           container->database_info_list_.begin();
       database_info != container->database_info_list_.end();
       ++database_info) {
    GURL origin(database_info->origin);

    if (!filter.size() ||
        (CookieTreeOriginNode::TitleForUrl(origin, container->app_id(),
            container->app_name()).find(filter) != string16::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin, container->app_id(),
                                      container->app_name());
      CookieTreeDatabasesNode* databases_node =
          origin_node->GetOrCreateDatabasesNode();
      databases_node->AddDatabaseNode(
          new CookieTreeDatabaseNode(database_info));
    }
  }
}

void CookiesTreeModel::PopulateLocalStorageInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->local_storage_info_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (LocalStorageInfoList::iterator local_storage_info =
           container->local_storage_info_list_.begin();
       local_storage_info != container->local_storage_info_list_.end();
       ++local_storage_info) {
    const GURL& origin(local_storage_info->origin_url);

    if (!filter.size() ||
        (CookieTreeOriginNode::TitleForUrl(origin, container->app_id(),
            container->app_name()).find(filter) != std::string::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin, container->app_id(),
                                      container->app_name());
      CookieTreeLocalStoragesNode* local_storages_node =
          origin_node->GetOrCreateLocalStoragesNode();
      local_storages_node->AddLocalStorageNode(
          new CookieTreeLocalStorageNode(local_storage_info));
    }
  }
}

void CookiesTreeModel::PopulateSessionStorageInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->session_storage_info_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (LocalStorageInfoList::iterator session_storage_info =
           container->session_storage_info_list_.begin();
       session_storage_info != container->session_storage_info_list_.end();
       ++session_storage_info) {
    const GURL& origin = session_storage_info->origin_url;

    if (!filter.size() ||
        (CookieTreeOriginNode::TitleForUrl(origin, container->app_id(),
            container->app_name()).find(filter) != string16::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin, container->app_id(),
                                      container->app_name());
      CookieTreeSessionStoragesNode* session_storages_node =
          origin_node->GetOrCreateSessionStoragesNode();
      session_storages_node->AddSessionStorageNode(
          new CookieTreeSessionStorageNode(session_storage_info));
    }
  }
}

void CookiesTreeModel::PopulateIndexedDBInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->indexed_db_info_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (IndexedDBInfoList::iterator indexed_db_info =
           container->indexed_db_info_list_.begin();
       indexed_db_info != container->indexed_db_info_list_.end();
       ++indexed_db_info) {
    const GURL& origin = indexed_db_info->origin;

    if (!filter.size() ||
        (CookieTreeOriginNode::TitleForUrl(origin, container->app_id(),
            container->app_name()).find(filter) != string16::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin, container->app_id(),
                                      container->app_name());
      CookieTreeIndexedDBsNode* indexed_dbs_node =
          origin_node->GetOrCreateIndexedDBsNode();
      indexed_dbs_node->AddIndexedDBNode(
          new CookieTreeIndexedDBNode(indexed_db_info));
    }
  }
}

void CookiesTreeModel::PopulateServerBoundCertInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->server_bound_cert_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (ServerBoundCertList::iterator cert_info =
           container->server_bound_cert_list_.begin();
       cert_info != container->server_bound_cert_list_.end();
       ++cert_info) {
    GURL origin(cert_info->server_identifier());
    if (!origin.is_valid()) {
      // Domain Bound Cert.  Make a valid URL to satisfy the
      // CookieTreeRootNode::GetOrCreateOriginNode interface.
      origin = GURL(std::string(chrome::kHttpsScheme) +
          content::kStandardSchemeSeparator +
          cert_info->server_identifier() + "/");
    }
    string16 title = CookieTreeOriginNode::TitleForUrl(origin,
        container->app_id(),
        container->app_name());

    if (!filter.size() || title.find(filter) != string16::npos) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin, container->app_id(),
                                      container->app_name());
      CookieTreeServerBoundCertsNode* server_bound_certs_node =
          origin_node->GetOrCreateServerBoundCertsNode();
      server_bound_certs_node->AddServerBoundCertNode(
          new CookieTreeServerBoundCertNode(cert_info));
    }
  }
}

void CookiesTreeModel::PopulateFileSystemInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->file_system_info_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (FileSystemInfoList::iterator file_system_info =
           container->file_system_info_list_.begin();
       file_system_info != container->file_system_info_list_.end();
       ++file_system_info) {
    GURL origin(file_system_info->origin);

    if (!filter.size() ||
        (CookieTreeOriginNode::TitleForUrl(origin, container->app_id(),
        container->app_name()).find(filter) != string16::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(origin, container->app_id(),
                                      container->app_name());
      CookieTreeFileSystemsNode* file_systems_node =
          origin_node->GetOrCreateFileSystemsNode();
      file_systems_node->AddFileSystemNode(
          new CookieTreeFileSystemNode(file_system_info));
    }
  }
}

void CookiesTreeModel::PopulateQuotaInfoWithFilter(
    LocalDataContainer* container,
    ScopedBatchUpdateNotifier* notifier,
    const string16& filter) {
  CookieTreeRootNode* root = static_cast<CookieTreeRootNode*>(GetRoot());

  if (container->quota_info_list_.empty())
    return;

  notifier->StartBatchUpdate();
  for (QuotaInfoList::iterator quota_info = container->quota_info_list_.begin();
       quota_info != container->quota_info_list_.end();
       ++quota_info) {
    if (!filter.size() ||
        (UTF8ToUTF16(quota_info->host).find(filter) != string16::npos)) {
      CookieTreeOriginNode* origin_node =
          root->GetOrCreateOriginNode(GURL("http://" + quota_info->host),
                                      container->app_id(),
                                      container->app_name());
      origin_node->UpdateOrCreateQuotaNode(quota_info);
    }
  }
}

void CookiesTreeModel::NotifyObserverBeginBatch() {
  // Only notify the model once if we're batching in a nested manner.
  if (batch_update_++ == 0) {
    FOR_EACH_OBSERVER(Observer,
                      cookies_observer_list_,
                      TreeModelBeginBatch(this));
  }
}

void CookiesTreeModel::NotifyObserverEndBatch() {
  // Only notify the observers if this is the outermost call to EndBatch() if
  // called in a nested manner.
  if (--batch_update_ == 0) {
    FOR_EACH_OBSERVER(Observer,
                      cookies_observer_list_,
                      TreeModelEndBatch(this));
  }
}

LocalDataContainer* CookiesTreeModel::GetLocalDataContainer(
    const std::string& app_id) {
  LocalDataContainer* container = app_data_map_[app_id];
  return container;
}
