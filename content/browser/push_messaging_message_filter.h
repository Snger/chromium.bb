// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_PUSH_MESSAGING_MESSAGE_FILTER_H_
#define CONTENT_BROWSER_PUSH_MESSAGING_MESSAGE_FILTER_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "content/public/browser/browser_message_filter.h"
#include "url/gurl.h"

namespace content {

class PushMessagingService;

class PushMessagingMessageFilter : public BrowserMessageFilter {
 public:
  explicit PushMessagingMessageFilter(int render_process_id);

 private:
  virtual ~PushMessagingMessageFilter();

  // BrowserMessageFilter implementation.
  virtual bool OnMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnRegister(int render_frame_id,
                  int callbacks_id,
                  const std::string& sender_id,
                  bool user_gesture);

  void DoRegister(int render_frame_id,
                  int callbacks_id,
                  const std::string& sender_id,
                  bool user_gesture);

  void DidRegister(int render_frame_id,
                   int callbacks_id,
                   const GURL& endpoint,
                   const std::string& registration_id,
                   bool success);

  PushMessagingService* service();

  int render_process_id_;

  // Owned by the content embedder's browsing context.
  PushMessagingService* service_;

  base::WeakPtrFactory<PushMessagingMessageFilter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PushMessagingMessageFilter);
};

}  // namespace content

#endif  // CONTENT_BROWSER_PUSH_MESSAGING_MESSAGE_FILTER_H_
