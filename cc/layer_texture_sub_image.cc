// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/layer_texture_sub_image.h"

#include "CCRendererGL.h" // For the GLC() macro.
#include "base/debug/trace_event.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include <public/WebGraphicsContext3D.h>

using WebKit::WebGraphicsContext3D;

namespace cc {

LayerTextureSubImage::LayerTextureSubImage(bool useMapTexSubImage)
    : m_useMapTexSubImage(useMapTexSubImage)
    , m_subImageSize(0)
{
}

LayerTextureSubImage::~LayerTextureSubImage()
{
}

void LayerTextureSubImage::upload(const uint8_t* image, const IntRect& imageRect,
                                  const IntRect& sourceRect, const IntSize& destOffset,
                                  GLenum format, WebGraphicsContext3D* context)
{
    if (m_useMapTexSubImage)
        uploadWithMapTexSubImage(image, imageRect, sourceRect, destOffset, format, context);
    else
        uploadWithTexSubImage(image, imageRect, sourceRect, destOffset, format, context);
}

void LayerTextureSubImage::uploadWithTexSubImage(const uint8_t* image, const IntRect& imageRect,
                                                 const IntRect& sourceRect, const IntSize& destOffset,
                                                 GLenum format, WebGraphicsContext3D* context)
{
    TRACE_EVENT0("cc", "LayerTextureSubImage::uploadWithTexSubImage");

    // Offset from image-rect to source-rect.
    IntPoint offset(sourceRect.x() - imageRect.x(), sourceRect.y() - imageRect.y());

    const uint8_t* pixelSource;
    if (imageRect.width() == sourceRect.width() && !offset.x())
        pixelSource = &image[4 * offset.y() * imageRect.width()];
    else {
        size_t neededSize = 4 * sourceRect.width() * sourceRect.height();
        if (m_subImageSize < neededSize) {
          m_subImage.reset(new uint8_t[neededSize]);
          m_subImageSize = neededSize;
        }
        // Strides not equal, so do a row-by-row memcpy from the
        // paint results into a temp buffer for uploading.
        for (int row = 0; row < sourceRect.height(); ++row)
            memcpy(&m_subImage[sourceRect.width() * 4 * row],
                   &image[4 * (offset.x() + (offset.y() + row) * imageRect.width())],
                   sourceRect.width() * 4);

        pixelSource = &m_subImage[0];
    }

    GLC(context, context->texSubImage2D(GL_TEXTURE_2D, 0, destOffset.width(), destOffset.height(), sourceRect.width(), sourceRect.height(), format, GL_UNSIGNED_BYTE, pixelSource));
}

void LayerTextureSubImage::uploadWithMapTexSubImage(const uint8_t* image, const IntRect& imageRect,
                                                    const IntRect& sourceRect, const IntSize& destOffset,
                                                    GLenum format, WebGraphicsContext3D* context)
{
    TRACE_EVENT0("cc", "LayerTextureSubImage::uploadWithMapTexSubImage");
    // Offset from image-rect to source-rect.
    IntPoint offset(sourceRect.x() - imageRect.x(), sourceRect.y() - imageRect.y());

    // Upload tile data via a mapped transfer buffer
    uint8_t* pixelDest = static_cast<uint8_t*>(context->mapTexSubImage2DCHROMIUM(GL_TEXTURE_2D, 0, destOffset.width(), destOffset.height(), sourceRect.width(), sourceRect.height(), format, GL_UNSIGNED_BYTE, GL_WRITE_ONLY));

    if (!pixelDest) {
        uploadWithTexSubImage(image, imageRect, sourceRect, destOffset, format, context);
        return;
    }

    unsigned int componentsPerPixel = 0;
    switch (format) {
    case GL_RGBA:
    case GL_BGRA_EXT:
        componentsPerPixel = 4;
        break;
    case GL_LUMINANCE:
        componentsPerPixel = 1;
        break;
    default:
        NOTREACHED();
    }
    unsigned int bytesPerComponent = 1;

    if (imageRect.width() == sourceRect.width() && !offset.x())
        memcpy(pixelDest, &image[offset.y() * imageRect.width() * componentsPerPixel * bytesPerComponent], imageRect.width() * sourceRect.height() * componentsPerPixel * bytesPerComponent);
    else {
        // Strides not equal, so do a row-by-row memcpy from the
        // paint results into the pixelDest
        for (int row = 0; row < sourceRect.height(); ++row)
            memcpy(&pixelDest[sourceRect.width() * row * componentsPerPixel * bytesPerComponent],
                   &image[4 * (offset.x() + (offset.y() + row) * imageRect.width())],
                   sourceRect.width() * componentsPerPixel * bytesPerComponent);
    }
    GLC(context, context->unmapTexSubImage2DCHROMIUM(pixelDest));
}

} // namespace cc
