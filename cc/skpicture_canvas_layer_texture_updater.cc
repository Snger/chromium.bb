// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/skpicture_canvas_layer_texture_updater.h"

#include "base/debug/trace_event.h"
#include "cc/layer_painter.h"
#include "cc/texture_update_queue.h"
#include "third_party/skia/include/core/SkCanvas.h"

namespace cc {

SkPictureCanvasLayerTextureUpdater::Texture::Texture(SkPictureCanvasLayerTextureUpdater* textureUpdater, scoped_ptr<PrioritizedTexture> texture)
    : LayerTextureUpdater::Texture(texture.Pass())
    , m_textureUpdater(textureUpdater)
{
}

SkPictureCanvasLayerTextureUpdater::Texture::~Texture()
{
}

void SkPictureCanvasLayerTextureUpdater::Texture::update(TextureUpdateQueue& queue, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, RenderingStats&)
{
    textureUpdater()->updateTexture(queue, texture(), sourceRect, destOffset, partialUpdate);
}

SkPictureCanvasLayerTextureUpdater::SkPictureCanvasLayerTextureUpdater(scoped_ptr<LayerPainter> painter)
    : CanvasLayerTextureUpdater(painter.Pass())
    , m_layerIsOpaque(false)
{
}

SkPictureCanvasLayerTextureUpdater::~SkPictureCanvasLayerTextureUpdater()
{
}

scoped_refptr<SkPictureCanvasLayerTextureUpdater> SkPictureCanvasLayerTextureUpdater::create(scoped_ptr<LayerPainter> painter)
{
    return make_scoped_refptr(new SkPictureCanvasLayerTextureUpdater(painter.Pass()));
}

scoped_ptr<LayerTextureUpdater::Texture> SkPictureCanvasLayerTextureUpdater::createTexture(PrioritizedTextureManager* manager)
{
    return scoped_ptr<LayerTextureUpdater::Texture>(new Texture(this, PrioritizedTexture::create(manager)));
}

void SkPictureCanvasLayerTextureUpdater::prepareToUpdate(const IntRect& contentRect, const IntSize&, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, RenderingStats& stats)
{
    SkCanvas* canvas = m_picture.beginRecording(contentRect.width(), contentRect.height());
    paintContents(canvas, contentRect, contentsWidthScale, contentsHeightScale, resultingOpaqueRect, stats);
    m_picture.endRecording();
}

void SkPictureCanvasLayerTextureUpdater::drawPicture(SkCanvas* canvas)
{
    TRACE_EVENT0("cc", "SkPictureCanvasLayerTextureUpdater::drawPicture");
    canvas->drawPicture(m_picture);
}

void SkPictureCanvasLayerTextureUpdater::updateTexture(TextureUpdateQueue& queue, PrioritizedTexture* texture, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate)
{
    ResourceUpdate upload = ResourceUpdate::CreateFromPicture(
        texture, &m_picture, contentRect(), sourceRect, destOffset);
    if (partialUpdate)
        queue.appendPartialUpload(upload);
    else
        queue.appendFullUpload(upload);
}

void SkPictureCanvasLayerTextureUpdater::setOpaque(bool opaque)
{
    m_layerIsOpaque = opaque;
}

} // namespace cc
