// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef BitmapCanvasLayerTextureUpdater_h
#define BitmapCanvasLayerTextureUpdater_h

#include "cc/canvas_layer_texture_updater.h"

class SkCanvas;

namespace cc {

class LayerPainter;

// This class rasterizes the contentRect into a skia bitmap canvas. It then updates
// textures by copying from the canvas into the texture, using MapSubImage if
// possible.
class BitmapCanvasLayerTextureUpdater : public CanvasLayerTextureUpdater {
public:
    class Texture : public LayerTextureUpdater::Texture {
    public:
        Texture(BitmapCanvasLayerTextureUpdater*, scoped_ptr<PrioritizedTexture>);
        virtual ~Texture();

        virtual void update(TextureUpdateQueue&, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate, RenderingStats&) OVERRIDE;

    private:
        BitmapCanvasLayerTextureUpdater* textureUpdater() { return m_textureUpdater; }

        BitmapCanvasLayerTextureUpdater* m_textureUpdater;
    };

    static scoped_refptr<BitmapCanvasLayerTextureUpdater> create(scoped_ptr<LayerPainter>);

    virtual scoped_ptr<LayerTextureUpdater::Texture> createTexture(PrioritizedTextureManager*) OVERRIDE;
    virtual SampledTexelFormat sampledTexelFormat(GLenum textureFormat) OVERRIDE;
    virtual void prepareToUpdate(const IntRect& contentRect, const IntSize& tileSize, float contentsWidthScale, float contentsHeightScale, IntRect& resultingOpaqueRect, RenderingStats&) OVERRIDE;
    void updateTexture(TextureUpdateQueue&, PrioritizedTexture*, const IntRect& sourceRect, const IntSize& destOffset, bool partialUpdate);

    virtual void setOpaque(bool) OVERRIDE;

protected:
    explicit BitmapCanvasLayerTextureUpdater(scoped_ptr<LayerPainter>);
    virtual ~BitmapCanvasLayerTextureUpdater();

    scoped_ptr<SkCanvas> m_canvas;
    IntSize m_canvasSize;
    bool m_opaque;
};

}  // namespace cc

#endif  // BitmapCanvasLayerTextureUpdater_h
