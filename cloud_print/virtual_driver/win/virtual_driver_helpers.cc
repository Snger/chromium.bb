// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cloud_print/virtual_driver/win/virtual_driver_helpers.h"
#include <windows.h>
#include <winspool.h>
#include "base/file_util.h"
#include "cloud_print/virtual_driver/win/virtual_driver_consts.h"

namespace cloud_print {

const size_t kMaxMessageLen = 100;

void DisplayWindowsMessage(HWND hwnd, HRESULT message_id) {
  wchar_t message_text[kMaxMessageLen + 1] = L"";

  ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
                  NULL,
                  message_id,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                  message_text,
                  kMaxMessageLen,
                  NULL);
  ::MessageBox(hwnd, message_text, kVirtualDriverName, MB_OK);
}

HRESULT GetLastHResult() {
  DWORD error_code = GetLastError();
  return HRESULT_FROM_WIN32(error_code);
}

HRESULT GetPrinterDriverDir(FilePath* path) {
  BYTE driver_dir_buffer[MAX_PATH * sizeof(wchar_t)];
  DWORD needed = 0;
  if (!GetPrinterDriverDirectory(NULL,
                                 NULL,
                                 1,
                                 driver_dir_buffer,
                                 MAX_PATH * sizeof(wchar_t),
                                 &needed)) {
    // We could try to allocate a larger buffer if needed > MAX_PATH
    // but that really shouldn't happen.
    return cloud_print::GetLastHResult();
  }
  *path = FilePath(reinterpret_cast<wchar_t*>(driver_dir_buffer));

  // The XPS driver is a "Level 3" driver
  *path = path->Append(L"3");
  return S_OK;
}
}

