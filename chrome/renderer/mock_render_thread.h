// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_MOCK_RENDER_THREAD_H_
#define CHROME_RENDERER_MOCK_RENDER_THREAD_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/shared_memory.h"
#include "chrome/common/extensions/extension_set.h"
#include "chrome/renderer/mock_printer.h"
#include "content/public/renderer/render_thread.h"
#include "ipc/ipc_test_sink.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebPopupType.h"

namespace IPC {
class MessageReplyDeserializer;
}

namespace base {
class DictionaryValue;
}

struct PrintHostMsg_DidGetPreviewPageCount_Params;
struct PrintHostMsg_DidPreviewPage_Params;
struct PrintHostMsg_ScriptedPrint_Params;
struct PrintMsg_PrintPages_Params;
struct PrintMsg_Print_Params;

// This class is very simple mock of RenderThread. It simulates an IPC channel
// which supports only two messages:
// ViewHostMsg_CreateWidget : sync message sent by the Widget.
// ViewMsg_Close : async, send to the Widget.
class MockRenderThread : public content::RenderThread {
 public:
  MockRenderThread();
  virtual ~MockRenderThread();

  // Provides access to the messages that have been received by this thread.
  IPC::TestSink& sink() { return sink_; }

  // content::RenderThread implementation:
  virtual bool Send(IPC::Message* msg) OVERRIDE;
  virtual MessageLoop* GetMessageLoop() OVERRIDE;
  virtual IPC::SyncChannel* GetChannel() OVERRIDE;
  virtual ResourceDispatcher* GetResourceDispatcher() OVERRIDE;
  virtual std::string GetLocale() OVERRIDE;
  virtual void AddRoute(int32 routing_id,
                        IPC::Channel::Listener* listener) OVERRIDE;
  virtual void RemoveRoute(int32 routing_id) OVERRIDE;
  virtual void AddFilter(IPC::ChannelProxy::MessageFilter* filter) OVERRIDE;
  virtual void RemoveFilter(IPC::ChannelProxy::MessageFilter* filter) OVERRIDE;
  virtual void SetOutgoingMessageFilter(
      IPC::ChannelProxy::OutgoingMessageFilter* filter) OVERRIDE;
  virtual void AddObserver(content::RenderProcessObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      content::RenderProcessObserver* observer) OVERRIDE;
  virtual void WidgetHidden() OVERRIDE;
  virtual void WidgetRestored() OVERRIDE;
  virtual void EnsureWebKitInitialized() OVERRIDE;
  virtual void RecordUserMetrics(const std::string& action) OVERRIDE;
  virtual base::SharedMemoryHandle HostAllocateSharedMemoryBuffer(
      uint32 buffer_size) OVERRIDE;
  virtual void RegisterExtension(v8::Extension* extension) OVERRIDE;
  virtual bool IsRegisteredExtension(
      const std::string& v8_extension_name) const OVERRIDE;
  virtual void ScheduleIdleHandler(double initial_delay_s) OVERRIDE;
  virtual void IdleHandler() OVERRIDE;
  virtual double GetIdleNotificationDelayInS() const OVERRIDE;
  virtual void SetIdleNotificationDelayInS(
      double idle_notification_delay_in_s) OVERRIDE;
#if defined(OS_WIN)
  virtual void PreCacheFont(const LOGFONT& log_font) OVERRIDE;
  virtual void ReleaseCachedFonts() OVERRIDE;
#endif

  //////////////////////////////////////////////////////////////////////////
  // The following functions are called by the test itself.

  void set_routing_id(int32 id) {
    routing_id_ = id;
  }

  int32 opener_id() const {
    return opener_id_;
  }

  bool has_widget() const {
    return widget_ ? true : false;
  }

  // Simulates the Widget receiving a close message. This should result
  // on releasing the internal reference counts and destroying the internal
  // state.
  void SendCloseMessage();

  // Returns the pseudo-printer instance.
  MockPrinter* printer() const { return printer_.get(); }

  // Call with |response| set to true if the user wants to print.
  // False if the user decides to cancel.
  void set_print_dialog_user_response(bool response);

  // Cancel print preview when print preview has |page| remaining pages.
  void set_print_preview_cancel_page_number(int page);

  // Get the number of pages to generate for print preview.
  int print_preview_pages_remaining();

 private:
  // This function operates as a regular IPC listener.
  bool OnMessageReceived(const IPC::Message& msg);

  // The Widget expects to be returned valid route_id.
  void OnMsgCreateWidget(int opener_id,
                         WebKit::WebPopupType popup_type,
                         int* route_id);

  // The callee expects to be returned a valid channel_id.
  void OnMsgOpenChannelToExtension(
      int routing_id, const std::string& extension_id,
      const std::string& source_extension_id,
      const std::string& target_extension_id, int* port_id);

#if defined(OS_WIN)
  void OnDuplicateSection(base::SharedMemoryHandle renderer_handle,
                          base::SharedMemoryHandle* browser_handle);
#endif

#if defined(OS_CHROMEOS)
  void OnAllocateTempFileForPrinting(base::FileDescriptor* renderer_fd,
                                     int* browser_fd);
  void OnTempFileForPrintingWritten(int browser_fd);
#endif

  // PrintWebViewHelper expects default print settings.
  void OnGetDefaultPrintSettings(PrintMsg_Print_Params* setting);

  // PrintWebViewHelper expects final print settings from the user.
  void OnScriptedPrint(const PrintHostMsg_ScriptedPrint_Params& params,
                       PrintMsg_PrintPages_Params* settings);

  void OnDidGetPrintedPagesCount(int cookie, int number_pages);
  void OnDidPrintPage(const PrintHostMsg_DidPrintPage_Params& params);
  void OnDidGetPreviewPageCount(
      const PrintHostMsg_DidGetPreviewPageCount_Params& params);
  void OnDidPreviewPage(const PrintHostMsg_DidPreviewPage_Params& params);
  void OnCheckForCancel(const std::string& preview_ui_addr,
                        int preview_request_id,
                        bool* cancel);


  // For print preview, PrintWebViewHelper will update settings.
  void OnUpdatePrintSettings(int document_cookie,
                             const base::DictionaryValue& job_settings,
                             PrintMsg_PrintPages_Params* params);

  IPC::TestSink sink_;

  // Routing id what will be assigned to the Widget.
  int32 routing_id_;

  // Opener id reported by the Widget.
  int32 opener_id_;

  // We only keep track of one Widget, we learn its pointer when it
  // adds a new route.
  IPC::Channel::Listener* widget_;

  // The last known good deserializer for sync messages.
  scoped_ptr<IPC::MessageReplyDeserializer> reply_deserializer_;

  // A mock printer device used for printing tests.
  scoped_ptr<MockPrinter> printer_;

  // True to simulate user clicking print. False to cancel.
  bool print_dialog_user_response_;

  // Simulates cancelling print preview if |print_preview_pages_remaining_|
  // equals this.
  int print_preview_cancel_page_number_;

  // Number of pages to generate for print preview.
  int print_preview_pages_remaining_;
};

#endif  // CHROME_RENDERER_MOCK_RENDER_THREAD_H_
