/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HighlightMarker_h
#define HighlightMarker_h

#include "core/editing/markers/DocumentMarker.h"

namespace blink {

// A subclass of DocumentMarker used to store information specific to Highlight
// markers. We store whether or not the match is active, a LayoutRect used for
// rendering the marker, and whether or not the LayoutRect is currently
// up-to-date.
class CORE_EXPORT HighlightMarker final : public DocumentMarker {
 public:
  HighlightMarker(unsigned start_offset,
                  unsigned end_offset,
                  Color foreground_color,
                  Color background_color,
									bool include_nonselectable_text);

  // DocumentMarker implementations
  MarkerType GetType() const final;

  // HighlightMarker-specific implementations
  Color ForegroundColor() const;
  Color BackgroundColor() const;
  bool IncludeNonSelectableText() const;

 private:
	Color foreground_color;
	Color background_color;
	bool include_nonselectable_text = false;
  DISALLOW_COPY_AND_ASSIGN(HighlightMarker);
};

DEFINE_TYPE_CASTS(HighlightMarker,
                  DocumentMarker,
                  marker,
                  marker->GetType() == DocumentMarker::kHighlight,
                  marker.GetType() == DocumentMarker::kHighlight);

}  // namespace blink

#endif
