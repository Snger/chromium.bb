// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "CCDrawQuad.h"

#include "CCCheckerboardDrawQuad.h"
#include "CCDebugBorderDrawQuad.h"
#include "CCIOSurfaceDrawQuad.h"
#include "CCRenderPassDrawQuad.h"
#include "CCSolidColorDrawQuad.h"
#include "CCStreamVideoDrawQuad.h"
#include "CCTextureDrawQuad.h"
#include "cc/tile_draw_quad.h"
#include "cc/yuv_video_draw_quad.h"
#include "IntRect.h"

namespace cc {

CCDrawQuad::CCDrawQuad(const CCSharedQuadState* sharedQuadState, Material material, const IntRect& quadRect)
    : m_sharedQuadState(sharedQuadState)
    , m_sharedQuadStateId(sharedQuadState->id)
    , m_material(material)
    , m_quadRect(quadRect)
    , m_quadVisibleRect(quadRect)
    , m_quadOpaque(true)
    , m_needsBlending(false)
{
    ASSERT(m_sharedQuadState);
    ASSERT(m_material != Invalid);
}

IntRect CCDrawQuad::opaqueRect() const
{
    if (opacity() != 1)
        return IntRect();
    if (m_sharedQuadState->opaque && m_quadOpaque)
        return m_quadRect;
    return m_opaqueRect;
}

void CCDrawQuad::setQuadVisibleRect(const IntRect& quadVisibleRect)
{
    IntRect intersection = quadVisibleRect;
    intersection.intersect(m_quadRect);
    m_quadVisibleRect = intersection;
}

unsigned CCDrawQuad::size() const
{
    switch (material()) {
    case Checkerboard:
        return sizeof(CCCheckerboardDrawQuad);
    case DebugBorder:
        return sizeof(CCDebugBorderDrawQuad);
    case IOSurfaceContent:
        return sizeof(CCIOSurfaceDrawQuad);
    case TextureContent:
        return sizeof(CCTextureDrawQuad);
    case SolidColor:
        return sizeof(CCSolidColorDrawQuad);
    case TiledContent:
        return sizeof(CCTileDrawQuad);
    case StreamVideoContent:
        return sizeof(CCStreamVideoDrawQuad);
    case RenderPass:
        return sizeof(CCRenderPassDrawQuad);
    case YUVVideoContent:
        return sizeof(CCYUVVideoDrawQuad);
    case Invalid:
        break;
    }

    CRASH();
    return sizeof(CCDrawQuad);
}

scoped_ptr<CCDrawQuad> CCDrawQuad::copy(const CCSharedQuadState* copiedSharedQuadState) const
{
    // RenderPass quads have their own copy() method.
    ASSERT(material() != RenderPass);

    unsigned bytes = size();
    ASSERT(bytes);

    scoped_ptr<CCDrawQuad> copyQuad(reinterpret_cast<CCDrawQuad*>(new char[bytes]));
    memcpy(copyQuad.get(), this, bytes);
    copyQuad->setSharedQuadState(copiedSharedQuadState);

    return copyQuad.Pass();
}

void CCDrawQuad::setSharedQuadState(const CCSharedQuadState* sharedQuadState)
{
    m_sharedQuadState = sharedQuadState;
    m_sharedQuadStateId = sharedQuadState->id;
}

}
