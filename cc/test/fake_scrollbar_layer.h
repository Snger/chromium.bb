// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_SCROLLBAR_LAYER_H_
#define CC_TEST_FAKE_SCROLLBAR_LAYER_H_

#include "base/memory/scoped_ptr.h"
#include "cc/scrollbar_layer.h"

namespace cc {

class FakeScrollbarLayer : public ScrollbarLayer {
public:
  static scoped_refptr<FakeScrollbarLayer> Create(
      bool paint_during_update, int scrolling_layer_id) {
    return make_scoped_refptr(new FakeScrollbarLayer(
        paint_during_update, scrolling_layer_id)); 
  }

  int update_count() { return update_count_; }
  void reset_update_count() { update_count_ = 0; }

  virtual void update(
      ResourceUpdateQueue& queue,
      const OcclusionTracker* occlusion,
      RenderingStats& stats) OVERRIDE;

private:
  FakeScrollbarLayer(bool paint_during_update, int scrolling_layer_id);
  virtual ~FakeScrollbarLayer();

  int update_count_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_SCROLLBAR_LAYER_H_
