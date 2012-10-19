// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/shared_quad_state.h"

#include "FloatQuad.h"

namespace cc {

scoped_ptr<CCSharedQuadState> CCSharedQuadState::create(const WebKit::WebTransformationMatrix& quadTransform, const IntRect& visibleContentRect, const IntRect& clippedRectInTarget, float opacity, bool opaque)
{
    return make_scoped_ptr(new CCSharedQuadState(quadTransform, visibleContentRect, clippedRectInTarget, opacity, opaque));
}

CCSharedQuadState::CCSharedQuadState(const WebKit::WebTransformationMatrix& quadTransform, const IntRect& visibleContentRect, const IntRect& clippedRectInTarget, float opacity, bool opaque)
    : id(-1)
    , quadTransform(quadTransform)
    , visibleContentRect(visibleContentRect)
    , clippedRectInTarget(clippedRectInTarget)
    , opacity(opacity)
    , opaque(opaque)
{
}

scoped_ptr<CCSharedQuadState> CCSharedQuadState::copy() const
{
    scoped_ptr<CCSharedQuadState> copiedState(create(quadTransform, visibleContentRect, clippedRectInTarget, opacity, opaque));
    copiedState->id = id;
    return copiedState.Pass();
}

}  // namespace cc
