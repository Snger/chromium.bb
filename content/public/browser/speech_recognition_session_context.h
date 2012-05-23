// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONTEXT_H_
#define CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONTEXT_H_
#pragma once

#include "base/string16.h"
#include "content/common/content_export.h"
#include "ui/gfx/rect.h"

namespace content {

// The context information required by clients of the SpeechRecognitionManager
// and its delegates for mapping the recognition session to other browser
// elements involved with it (e.g., the page element that requested the
// recognition). The SpeechRecognitionManager is not aware of the content of
// this struct and does NOT use it for its purposes. However the manager keeps
// this struct "attached" to the recognition session during all the session
// lifetime, making its contents available to clients (In this regard, see
// SpeechRecognitionManager::GetSessionContext and
// SpeechRecognitionManager::LookupSessionByContext methods).
struct CONTENT_EXPORT SpeechRecognitionSessionContext {
  SpeechRecognitionSessionContext();
  ~SpeechRecognitionSessionContext();

  int render_process_id;
  int render_view_id;
  int render_request_id;
  int js_handle_id;

  // Determines whether recognition was requested by a page element (in which
  // case its coordinates are passed in |element_rect|).
  bool requested_by_page_element;

  // The coordinates of the page element for placing the bubble (valid only when
  // |requested_by_page_element| = true).
  gfx::Rect element_rect;

  // Determines whether this is the first time that this context (identified by
  // |context_name|) is requesting a recognition.
  // TODO(primiano) This is really temporary, remove after CL1.12 which will
  // refactor SpeechRecognitionPreferences and move this check entirely whithin
  // chrome, without involving content.
  bool is_first_request_for_context;

  // A texual description of the context (website, extension name) that is
  // requesting recognition, for prompting security notifications to the user.
  string16 context_name;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_SPEECH_RECOGNITION_SESSION_CONTEXT_H_
