// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/task_manager_child_process_resource_provider.h"

#include "base/basictypes.h"
#include "base/i18n/rtl.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_process_type.h"
#include "content/public/browser/browser_child_process_host_iterator.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/web_contents.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"

using content::BrowserChildProcessHostIterator;
using content::BrowserThread;
using content::WebContents;

////////////////////////////////////////////////////////////////////////////////
// TaskManagerChildProcessResource class
////////////////////////////////////////////////////////////////////////////////
gfx::ImageSkia* TaskManagerChildProcessResource::default_icon_ = NULL;

TaskManagerChildProcessResource::TaskManagerChildProcessResource(
    int process_type,
    const string16& name,
    base::ProcessHandle handle,
    int unique_process_id)
    : process_type_(process_type),
      name_(name),
      handle_(handle),
      unique_process_id_(unique_process_id),
      network_usage_support_(false) {
  // We cache the process id because it's not cheap to calculate, and it won't
  // be available when we get the plugin disconnected notification.
  pid_ = base::GetProcId(handle);
  if (!default_icon_) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    default_icon_ = rb.GetImageSkiaNamed(IDR_PLUGINS_FAVICON);
    // TODO(jabdelmalek): use different icon for web workers.
  }
}

TaskManagerChildProcessResource::~TaskManagerChildProcessResource() {
}

// TaskManagerResource methods:
string16 TaskManagerChildProcessResource::GetTitle() const {
  if (title_.empty())
    title_ = GetLocalizedTitle();

  return title_;
}

string16 TaskManagerChildProcessResource::GetProfileName() const {
  return string16();
}

gfx::ImageSkia TaskManagerChildProcessResource::GetIcon() const {
  return *default_icon_;
}

base::ProcessHandle TaskManagerChildProcessResource::GetProcess() const {
  return handle_;
}

int TaskManagerChildProcessResource::GetUniqueChildProcessId() const {
  return unique_process_id_;
}

TaskManager::Resource::Type TaskManagerChildProcessResource::GetType() const {
  // Translate types to TaskManager::ResourceType, since ChildProcessData's type
  // is not available for all TaskManager resources.
  switch (process_type_) {
    case content::PROCESS_TYPE_PLUGIN:
    case content::PROCESS_TYPE_PPAPI_PLUGIN:
    case content::PROCESS_TYPE_PPAPI_BROKER:
      return TaskManager::Resource::PLUGIN;
    case content::PROCESS_TYPE_UTILITY:
      return TaskManager::Resource::UTILITY;
    case content::PROCESS_TYPE_ZYGOTE:
      return TaskManager::Resource::ZYGOTE;
    case content::PROCESS_TYPE_SANDBOX_HELPER:
      return TaskManager::Resource::SANDBOX_HELPER;
    case content::PROCESS_TYPE_GPU:
      return TaskManager::Resource::GPU;
    case PROCESS_TYPE_PROFILE_IMPORT:
      return TaskManager::Resource::PROFILE_IMPORT;
    case PROCESS_TYPE_NACL_LOADER:
    case PROCESS_TYPE_NACL_BROKER:
      return TaskManager::Resource::NACL;
    default:
      return TaskManager::Resource::UNKNOWN;
  }
}

bool TaskManagerChildProcessResource::SupportNetworkUsage() const {
  return network_usage_support_;
}

void TaskManagerChildProcessResource::SetSupportNetworkUsage() {
  network_usage_support_ = true;
}

string16 TaskManagerChildProcessResource::GetLocalizedTitle() const {
  string16 title = name_;
  if (title.empty()) {
    switch (process_type_) {
      case content::PROCESS_TYPE_PLUGIN:
      case content::PROCESS_TYPE_PPAPI_PLUGIN:
      case content::PROCESS_TYPE_PPAPI_BROKER:
        title = l10n_util::GetStringUTF16(IDS_TASK_MANAGER_UNKNOWN_PLUGIN_NAME);
        break;
      default:
        // Nothing to do for non-plugin processes.
        break;
    }
  }

  // Explicitly mark name as LTR if there is no strong RTL character,
  // to avoid the wrong concatenation result similar to "!Yahoo Mail: the
  // best web-based Email: NIGULP", in which "NIGULP" stands for the Hebrew
  // or Arabic word for "plugin".
  base::i18n::AdjustStringForLocaleDirection(&title);

  switch (process_type_) {
    case content::PROCESS_TYPE_UTILITY:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_UTILITY_PREFIX);
    case content::PROCESS_TYPE_GPU:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_GPU_PREFIX);
    case content::PROCESS_TYPE_PLUGIN:
    case content::PROCESS_TYPE_PPAPI_PLUGIN:
      return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PLUGIN_PREFIX, title);
    case content::PROCESS_TYPE_PPAPI_BROKER:
      return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PLUGIN_BROKER_PREFIX,
                                        title);
    case PROCESS_TYPE_PROFILE_IMPORT:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_UTILITY_PREFIX);
    case PROCESS_TYPE_NACL_BROKER:
      return l10n_util::GetStringUTF16(IDS_TASK_MANAGER_NACL_BROKER_PREFIX);
    case PROCESS_TYPE_NACL_LOADER:
      return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_NACL_PREFIX, title);
    // These types don't need display names or get them from elsewhere.
    case content::PROCESS_TYPE_BROWSER:
    case content::PROCESS_TYPE_RENDERER:
    case content::PROCESS_TYPE_ZYGOTE:
    case content::PROCESS_TYPE_SANDBOX_HELPER:
    case content::PROCESS_TYPE_MAX:
      NOTREACHED();
      break;

    case content::PROCESS_TYPE_WORKER:
      NOTREACHED() << "Workers are not handled by this provider.";
      break;
    case content::PROCESS_TYPE_UNKNOWN:
      NOTREACHED() << "Need localized name for child process type.";
  }

  return title;
}

////////////////////////////////////////////////////////////////////////////////
// TaskManagerChildProcessResourceProvider class
////////////////////////////////////////////////////////////////////////////////

TaskManagerChildProcessResourceProvider::
    TaskManagerChildProcessResourceProvider(TaskManager* task_manager)
    : task_manager_(task_manager),
      updating_(false) {
}

TaskManagerChildProcessResourceProvider::
    ~TaskManagerChildProcessResourceProvider() {
}

TaskManager::Resource* TaskManagerChildProcessResourceProvider::GetResource(
    int origin_pid,
    int render_process_host_id,
    int routing_id) {
  PidResourceMap::iterator iter = pid_to_resources_.find(origin_pid);
  if (iter != pid_to_resources_.end())
    return iter->second;
  else
    return NULL;
}

void TaskManagerChildProcessResourceProvider::StartUpdating() {
  DCHECK(!updating_);
  updating_ = true;

  // Get the existing child processes.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(
          &TaskManagerChildProcessResourceProvider::RetrieveChildProcessData,
          this));

  BrowserChildProcessObserver::Add(this);
}

void TaskManagerChildProcessResourceProvider::StopUpdating() {
  DCHECK(updating_);
  updating_ = false;

  // Delete all the resources.
  STLDeleteContainerPairSecondPointers(resources_.begin(), resources_.end());

  resources_.clear();
  pid_to_resources_.clear();

  BrowserChildProcessObserver::Remove(this);
}

void TaskManagerChildProcessResourceProvider::BrowserChildProcessHostConnected(
    const content::ChildProcessData& data) {
  DCHECK(updating_);

  // Workers are handled by TaskManagerWorkerResourceProvider.
  if (data.process_type == content::PROCESS_TYPE_WORKER)
    return;
  if (resources_.count(data.handle)) {
    // The case may happen that we have added a child_process_info as part of
    // the iteration performed during StartUpdating() call but the notification
    // that it has connected was not fired yet. So when the notification
    // happens, we already know about this plugin and just ignore it.
    return;
  }
  AddToTaskManager(data);
}

void TaskManagerChildProcessResourceProvider::
BrowserChildProcessHostDisconnected(const content::ChildProcessData& data) {
  DCHECK(updating_);

  if (data.process_type == content::PROCESS_TYPE_WORKER)
    return;
  ChildProcessMap::iterator iter = resources_.find(data.handle);
  if (iter == resources_.end()) {
    // ChildProcessData disconnection notifications are asynchronous, so we
    // might be notified for a plugin we don't know anything about (if it was
    // closed before the task manager was shown and destroyed after that).
    return;
  }
  // Remove the resource from the Task Manager.
  TaskManagerChildProcessResource* resource = iter->second;
  task_manager_->RemoveResource(resource);
  // Remove it from the provider.
  resources_.erase(iter);
  // Remove it from our pid map.
  PidResourceMap::iterator pid_iter =
      pid_to_resources_.find(resource->process_id());
  DCHECK(pid_iter != pid_to_resources_.end());
  if (pid_iter != pid_to_resources_.end())
    pid_to_resources_.erase(pid_iter);

  // Finally, delete the resource.
  delete resource;
}

void TaskManagerChildProcessResourceProvider::AddToTaskManager(
    const content::ChildProcessData& child_process_data) {
  TaskManagerChildProcessResource* resource =
      new TaskManagerChildProcessResource(
          child_process_data.process_type,
          child_process_data.name,
          child_process_data.handle,
          child_process_data.id);
  resources_[child_process_data.handle] = resource;
  pid_to_resources_[resource->process_id()] = resource;
  task_manager_->AddResource(resource);
}

// The ChildProcessData::Iterator has to be used from the IO thread.
void TaskManagerChildProcessResourceProvider::RetrieveChildProcessData() {
  std::vector<content::ChildProcessData> child_processes;
  for (BrowserChildProcessHostIterator iter; !iter.Done(); ++iter) {
    // Only add processes which are already started, since we need their handle.
    if (iter.GetData().handle == base::kNullProcessHandle)
      continue;
    if (iter.GetData().process_type == content::PROCESS_TYPE_WORKER)
      continue;
    child_processes.push_back(iter.GetData());
  }
  // Now notify the UI thread that we have retrieved information about child
  // processes.
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(
          &TaskManagerChildProcessResourceProvider::ChildProcessDataRetreived,
          this, child_processes));
}

// This is called on the UI thread.
void TaskManagerChildProcessResourceProvider::ChildProcessDataRetreived(
    const std::vector<content::ChildProcessData>& child_processes) {
  for (size_t i = 0; i < child_processes.size(); ++i)
    AddToTaskManager(child_processes[i]);

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TASK_MANAGER_CHILD_PROCESSES_DATA_READY,
      content::Source<TaskManagerChildProcessResourceProvider>(this),
      content::NotificationService::NoDetails());
}
