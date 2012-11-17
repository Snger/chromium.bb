// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/stream_video_draw_quad.h"

#include "base/logging.h"

namespace cc {

scoped_ptr<StreamVideoDrawQuad> StreamVideoDrawQuad::create(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, const gfx::Rect& opaqueRect, unsigned textureId, const WebKit::WebTransformationMatrix& matrix)
{
    return make_scoped_ptr(new StreamVideoDrawQuad(sharedQuadState, quadRect, opaqueRect, textureId, matrix));
}

StreamVideoDrawQuad::StreamVideoDrawQuad(const SharedQuadState* sharedQuadState, const gfx::Rect& quadRect, const gfx::Rect& opaqueRect, unsigned textureId, const WebKit::WebTransformationMatrix& matrix)
    : m_textureId(textureId)
    , m_matrix(matrix)
{
    gfx::Rect visibleRect = quadRect;
    bool needsBlending = false;
    DrawQuad::SetAll(sharedQuadState, DrawQuad::STREAM_VIDEO_CONTENT, quadRect, opaqueRect, visibleRect, needsBlending);
}

const StreamVideoDrawQuad* StreamVideoDrawQuad::materialCast(const DrawQuad* quad)
{
    DCHECK(quad->material == DrawQuad::STREAM_VIDEO_CONTENT);
    return static_cast<const StreamVideoDrawQuad*>(quad);
}

}  // namespace cc
