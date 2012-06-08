// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/file_system/file_system_api.h"

#include "base/file_path.h"
#include "chrome/common/extensions/api/file_system.h"
#include "content/public/browser/child_process_security_policy.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_process_host.h"
#include "webkit/fileapi/file_system_util.h"
#include "webkit/fileapi/isolated_context.h"

namespace GetDisplayPath = extensions::api::file_system::GetDisplayPath;

namespace extensions {

const char kInvalidParameters[] = "Invalid parameters";
const char kSecurityError[] = "Security error";

bool FileSystemGetDisplayPathFunction::RunImpl() {
  scoped_ptr<GetDisplayPath::Params> params(GetDisplayPath::Params::Create(
          *args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  std::string filesystem_id;
  if (!fileapi::CrackIsolatedFileSystemName(params->fsname, &filesystem_id)) {
    error_ = kInvalidParameters;
    return false;
  }

  fileapi::IsolatedContext* context = fileapi::IsolatedContext::GetInstance();
  FilePath relative_path = FilePath::FromUTF8Unsafe(params->fspath);
  FilePath virtual_path = context->CreateVirtualPath(filesystem_id,
                                                     relative_path);
  FilePath file_path;
  if (!context->CrackIsolatedPath(virtual_path,
                                  &filesystem_id,
                                  NULL,
                                  &file_path)) {
    error_ = kInvalidParameters;
    return false;
  }

  // Only return the display path if the process has read access to the
  // filesystem.
  content::ChildProcessSecurityPolicy* policy =
      content::ChildProcessSecurityPolicy::GetInstance();
  if (!policy->CanReadFileSystem(render_view_host_->GetProcess()->GetID(),
                                 filesystem_id)) {
    error_ = kSecurityError;
    return false;
  }

  result_.reset(base::Value::CreateStringValue(file_path.value()));
  return true;
}

}  // namespace extensions
