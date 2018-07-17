// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/editing/markers/HighlightMarkerListImpl.h"

#include "core/editing/markers/UnsortedDocumentMarkerListEditor.h"

namespace blink {

DocumentMarker::MarkerType HighlightMarkerListImpl::MarkerType() const {
  return DocumentMarker::kHighlight;
}

bool HighlightMarkerListImpl::IsEmpty() const {
  return markers_.IsEmpty();
}

void HighlightMarkerListImpl::Add(DocumentMarker* marker) {
  DCHECK_EQ(DocumentMarker::kHighlight, marker->GetType());
  markers_.push_back(marker);
}

void HighlightMarkerListImpl::Clear() {
  markers_.clear();
}

const HeapVector<Member<DocumentMarker>>&
HighlightMarkerListImpl::GetMarkers() const {
  return markers_;
}

DocumentMarker* HighlightMarkerListImpl::FirstMarkerIntersectingRange(
    unsigned start_offset,
    unsigned end_offset) const {
  return UnsortedDocumentMarkerListEditor::FirstMarkerIntersectingRange(
      markers_, start_offset, end_offset);
}

HeapVector<Member<DocumentMarker>>
HighlightMarkerListImpl::MarkersIntersectingRange(unsigned start_offset,
                                                    unsigned end_offset) const {
  return UnsortedDocumentMarkerListEditor::MarkersIntersectingRange(
      markers_, start_offset, end_offset);
}

bool HighlightMarkerListImpl::MoveMarkers(int length,
                                            DocumentMarkerList* dst_markers_) {
  return UnsortedDocumentMarkerListEditor::MoveMarkers(&markers_, length,
                                                       dst_markers_);
}

bool HighlightMarkerListImpl::RemoveMarkers(unsigned start_offset,
                                              int length) {
  return UnsortedDocumentMarkerListEditor::RemoveMarkers(&markers_,
                                                         start_offset, length);
}

bool HighlightMarkerListImpl::ShiftMarkers(const String&,
                                             unsigned offset,
                                             unsigned old_length,
                                             unsigned new_length) {
  return UnsortedDocumentMarkerListEditor::ShiftMarkersContentIndependent(
      &markers_, offset, old_length, new_length);
}

DEFINE_TRACE(HighlightMarkerListImpl) {
  visitor->Trace(markers_);
  DocumentMarkerList::Trace(visitor);
}

}  // namespace blink
