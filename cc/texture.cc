// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/texture.h"

namespace cc {

void Texture::setDimensions(const IntSize& size, GLenum format)
{
    m_size = size;
    m_format = format;
}

size_t Texture::bytes() const
{
    if (m_size.isEmpty())
        return 0u;

    return memorySizeBytes(m_size, m_format);
}

size_t Texture::memorySizeBytes(const IntSize& size, GLenum format)
{
    unsigned int componentsPerPixel = 4;
    unsigned int bytesPerComponent = 1;
    return componentsPerPixel * bytesPerComponent * size.width() * size.height();
}

}
