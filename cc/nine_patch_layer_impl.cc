// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "nine_patch_layer_impl.h"

#include "base/stringprintf.h"
#include "cc/quad_sink.h"
#include "cc/texture_draw_quad.h"
#include "ui/gfx/rect_f.h"

namespace cc {

NinePatchLayerImpl::NinePatchLayerImpl(int id)
    : LayerImpl(id)
    , m_resourceId(0)
{
}

NinePatchLayerImpl::~NinePatchLayerImpl()
{
}

ResourceProvider::ResourceId NinePatchLayerImpl::contentsResourceId() const
{
    return 0;
}

void NinePatchLayerImpl::willDraw(ResourceProvider* resourceProvider)
{
}

static gfx::RectF normalizedRect(float x, float y, float width, float height, float totalWidth, float totalHeight)
{
    return gfx::RectF(x / totalWidth, y / totalHeight, width / totalWidth, height / totalHeight);
}

void NinePatchLayerImpl::setLayout(const gfx::Size& imageBounds, const gfx::Rect& aperture)
{
    m_imageBounds = imageBounds;
    m_imageAperture = aperture;
}

void NinePatchLayerImpl::appendQuads(QuadSink& quadSink, AppendQuadsData& appendQuadsData)
{
    if (!m_resourceId)
        return;

    SharedQuadState* sharedQuadState = quadSink.useSharedQuadState(createSharedQuadState());
    appendDebugBorderQuad(quadSink, sharedQuadState, appendQuadsData);

    static const bool flipped = false;
    static const bool premultipliedAlpha = true;

    DCHECK(!bounds().IsEmpty());

    // NinePatch border widths in bitmap pixel space
    int leftWidth = m_imageAperture.x();
    int topHeight = m_imageAperture.y();
    int rightWidth = m_imageBounds.width() - m_imageAperture.right();
    int bottomHeight = m_imageBounds.height() - m_imageAperture.bottom();

    // Patch positions in layer space
    gfx::Rect topLeft(0, 0, leftWidth, topHeight);
    gfx::Rect topRight(bounds().width() - rightWidth, 0, rightWidth, topHeight);
    gfx::Rect bottomLeft(0, bounds().height() - bottomHeight, leftWidth, bottomHeight);
    gfx::Rect bottomRight(topRight.x(), bottomLeft.y(), rightWidth, bottomHeight);
    gfx::Rect top(topLeft.right(), 0, bounds().width() - leftWidth - rightWidth, topHeight);
    gfx::Rect left(0, topLeft.bottom(), leftWidth, bounds().height() - topHeight - bottomHeight);
    gfx::Rect right(topRight.x(), topRight.bottom(), rightWidth, left.height());
    gfx::Rect bottom(top.x(), bottomLeft.y(), top.width(), bottomHeight);

    float imgWidth = m_imageBounds.width();
    float imgHeight = m_imageBounds.height();

    // Patch positions in bitmap UV space (from zero to one)
    gfx::RectF uvTopLeft = normalizedRect(0, 0, leftWidth, topHeight, imgWidth, imgHeight);
    gfx::RectF uvTopRight = normalizedRect(imgWidth - rightWidth, 0, rightWidth, topHeight, imgWidth, imgHeight);
    gfx::RectF uvBottomLeft = normalizedRect(0, imgHeight - bottomHeight, leftWidth, bottomHeight, imgWidth, imgHeight);
    gfx::RectF uvBottomRight = normalizedRect(imgWidth - rightWidth, imgHeight - bottomHeight, rightWidth, bottomHeight, imgWidth, imgHeight);
    gfx::RectF uvTop(uvTopLeft.right(), 0, (imgWidth - leftWidth - rightWidth) / imgWidth, (topHeight) / imgHeight);
    gfx::RectF uvLeft(0, uvTopLeft.bottom(), leftWidth / imgWidth, (imgHeight - topHeight - bottomHeight) / imgHeight);
    gfx::RectF uvRight(uvTopRight.x(), uvTopRight.bottom(), rightWidth / imgWidth, uvLeft.height());
    gfx::RectF uvBottom(uvTop.x(), uvBottomLeft.y(), uvTop.width(), bottomHeight / imgHeight);

    quadSink.append(TextureDrawQuad::create(sharedQuadState, topLeft, m_resourceId, premultipliedAlpha, uvTopLeft, flipped).PassAs<DrawQuad>(), appendQuadsData);
    quadSink.append(TextureDrawQuad::create(sharedQuadState, topRight, m_resourceId, premultipliedAlpha, uvTopRight, flipped).PassAs<DrawQuad>(), appendQuadsData);
    quadSink.append(TextureDrawQuad::create(sharedQuadState, bottomLeft, m_resourceId, premultipliedAlpha, uvBottomLeft, flipped).PassAs<DrawQuad>(), appendQuadsData);
    quadSink.append(TextureDrawQuad::create(sharedQuadState, bottomRight, m_resourceId, premultipliedAlpha, uvBottomRight, flipped).PassAs<DrawQuad>(), appendQuadsData);
    quadSink.append(TextureDrawQuad::create(sharedQuadState, top, m_resourceId, premultipliedAlpha, uvTop, flipped).PassAs<DrawQuad>(), appendQuadsData);
    quadSink.append(TextureDrawQuad::create(sharedQuadState, left, m_resourceId, premultipliedAlpha, uvLeft, flipped).PassAs<DrawQuad>(), appendQuadsData);
    quadSink.append(TextureDrawQuad::create(sharedQuadState, right, m_resourceId, premultipliedAlpha, uvRight, flipped).PassAs<DrawQuad>(), appendQuadsData);
    quadSink.append(TextureDrawQuad::create(sharedQuadState, bottom, m_resourceId, premultipliedAlpha, uvBottom, flipped).PassAs<DrawQuad>(), appendQuadsData);
}

void NinePatchLayerImpl::didDraw(ResourceProvider* resourceProvider)
{
}

void NinePatchLayerImpl::didLoseContext()
{
    m_resourceId = 0;
}

const char* NinePatchLayerImpl::layerTypeAsString() const
{
    return "NinePatchLayer";
}

void NinePatchLayerImpl::dumpLayerProperties(std::string* str, int indent) const
{
    str->append(indentString(indent));
    base::StringAppendF(str, "imageAperture: %s\n", m_imageAperture.ToString().c_str());
    LayerImpl::dumpLayerProperties(str, indent);
}

}
