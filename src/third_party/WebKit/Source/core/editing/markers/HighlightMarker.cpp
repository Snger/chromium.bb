// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/HighlightMarker.h"

namespace blink {

HighlightMarker::HighlightMarker(unsigned start_offset,
                                 unsigned end_offset,
                                 Color foreground_color,
                                 Color background_color,
                                 bool include_nonselectable_text)
    : DocumentMarker(start_offset, end_offset),
    	foreground_color(foreground_color),
    	background_color(background_color),
    	include_nonselectable_text(include_nonselectable_text) {}

DocumentMarker::MarkerType HighlightMarker::GetType() const {
  return DocumentMarker::kHighlight;
}

Color HighlightMarker::ForegroundColor() const {
  return foreground_color;
}

Color HighlightMarker::BackgroundColor() const {
  return background_color;
}

bool HighlightMarker::IncludeNonSelectableText() const {
  return include_nonselectable_text;
}

}  // namespace blink
