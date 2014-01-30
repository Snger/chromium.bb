// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_DRAW_SWAP_READBACK_RESULT_H_
#define CC_SCHEDULER_DRAW_SWAP_READBACK_RESULT_H_

namespace cc {

struct DrawSwapReadbackResult {
  enum DrawResult {
    INVALID_RESULT,
    DRAW_SUCCESS,
    DRAW_ABORTED_CHECKERBOARD_ANIMATIONS,
  };

  DrawSwapReadbackResult()
      : draw_result(INVALID_RESULT), did_swap(false), did_readback(false) {}
  DrawSwapReadbackResult(DrawResult draw_result,
                         bool did_swap,
                         bool did_readback)
      : draw_result(draw_result),
        did_swap(did_swap),
        did_readback(did_readback) {}
  DrawResult draw_result;
  bool did_swap;
  bool did_readback;
};

}  // namespace cc

#endif  // CC_SCHEDULER_DRAW_SWAP_READBACK_RESULT_H_
