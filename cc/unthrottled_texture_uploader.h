// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UnthrottledTextureUploader_h
#define UnthrottledTextureUploader_h

#include "CCResourceProvider.h"
#include "base/basictypes.h"
#include "cc/texture_uploader.h"

namespace cc {

class UnthrottledTextureUploader : public TextureUploader {
public:
    static scoped_ptr<UnthrottledTextureUploader> create()
    {
        return make_scoped_ptr(new UnthrottledTextureUploader());
    }
    virtual ~UnthrottledTextureUploader() { }

    virtual size_t numBlockingUploads() OVERRIDE;
    virtual void markPendingUploadsAsNonBlocking() OVERRIDE;
    virtual double estimatedTexturesPerSecond() OVERRIDE;
    virtual void uploadTexture(CCResourceProvider*,
                               CCPrioritizedTexture*,
                               const SkBitmap*,
                               IntRect content_rect,
                               IntRect source_rect,
                               IntSize dest_offset) OVERRIDE;

protected:
    UnthrottledTextureUploader() { }

private:
    DISALLOW_COPY_AND_ASSIGN(UnthrottledTextureUploader);
};

}

#endif
