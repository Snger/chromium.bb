// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_UI_H_
#define CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_UI_H_
#pragma once

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/ref_counted_memory.h"
#include "base/time.h"
#include "chrome/browser/printing/print_preview_data_service.h"
#include "chrome/browser/ui/webui/chrome_web_ui.h"

class PrintPreviewDataService;
class PrintPreviewHandler;
struct PrintHostMsg_DidGetPreviewPageCount_Params;

class PrintPreviewUI : public ChromeWebUI {
 public:
  explicit PrintPreviewUI(TabContents* contents);
  virtual ~PrintPreviewUI();

  // Gets the print preview |data|. |index| is zero-based, and can be
  // |printing::COMPLETE_PREVIEW_DOCUMENT_INDEX| to get the entire preview
  // document.
  void GetPrintPreviewDataForIndex(int index,
                                   scoped_refptr<RefCountedBytes>* data);

  // Sets the print preview |data|. |index| is zero-based, and can be
  // |printing::COMPLETE_PREVIEW_DOCUMENT_INDEX| to set the entire preview
  // document.
  void SetPrintPreviewDataForIndex(int index, const RefCountedBytes* data);

  // Clear the existing print preview data.
  void ClearAllPreviewData();

  // Notifies the Web UI that there is a print preview request. There should be
  // a matching call to OnPreviewDataIsAvailable() or OnPrintPreviewFailed().
  void OnPrintPreviewRequest();

  // Notifies the Web UI about the page count of the request preview.
  void OnDidGetPreviewPageCount(
      const PrintHostMsg_DidGetPreviewPageCount_Params& params);

  // Notifies the Web UI that the 0-based page |page_number| has been rendered.
  // |preview_request_id| indicates wich request resulted in this response.
  void OnDidPreviewPage(int page_number, int preview_request_id);

  // Notifies the Web UI renderer that preview data is available.
  // |expected_pages_count| specifies the total number of pages. |job_title| is
  // the title of the page being previewed. |preview_request_id| indicates which
  // request resulted in this response.
  void OnPreviewDataIsAvailable(int expected_pages_count,
                                const string16& job_title,
                                int preview_request_id);

  void OnReusePreviewData(int preview_request_id);

  // Notifies the Web UI that a navigation has occurred in this tab. This is the
  // last chance to communicate with the source tab before the association is
  // erased.
  void OnNavigation();

  // Notifies the Web UI that the print preview failed to render.
  void OnPrintPreviewFailed();

  // Notifies the Web UI that the print preview request has been cancelled.
  void OnPrintPreviewCancelled();

  // Notifies the Web UI that initiator tab is closed, so we can disable all the
  // controls that need the initiator tab for generating the preview data.
  void OnInitiatorTabClosed(const std::string& initiator_url);

  // Notifies the Web UI renderer that file selection has been cancelled.
  void OnFileSelectionCancelled();

  // Returns true if there are pending requests.
  bool HasPendingRequests();

  int document_cookie();

 private:
  // Returns the Singleton instance of the PrintPreviewDataService.
  PrintPreviewDataService* print_preview_data_service();

  void DecrementRequestCount();

  base::TimeTicks initial_preview_start_time_;

  // Store the PrintPreviewUI address string.
  std::string preview_ui_addr_str_;

  // Weak pointer to the WebUI handler.
  PrintPreviewHandler* handler_;

  // The number of print preview requests in flight.
  uint32 request_count_;

  // Document cookie from the initiator renderer.
  int document_cookie_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_PRINT_PREVIEW_UI_H_
