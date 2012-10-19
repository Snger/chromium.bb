// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HeadsUpDisplayLayerChromium_h
#define HeadsUpDisplayLayerChromium_h

#include "CCFontAtlas.h"
#include "IntSize.h"
#include "base/memory/scoped_ptr.h"
#include "cc/layer.h"

namespace cc {

class HeadsUpDisplayLayerChromium : public LayerChromium {
public:
    static scoped_refptr<HeadsUpDisplayLayerChromium> create();

    virtual void update(CCTextureUpdateQueue&, const CCOcclusionTracker*, CCRenderingStats&) OVERRIDE;
    virtual bool drawsContent() const OVERRIDE;

    void setFontAtlas(scoped_ptr<CCFontAtlas>);

    virtual scoped_ptr<CCLayerImpl> createCCLayerImpl() OVERRIDE;
    virtual void pushPropertiesTo(CCLayerImpl*) OVERRIDE;

protected:
    HeadsUpDisplayLayerChromium();

private:
    virtual ~HeadsUpDisplayLayerChromium();

    scoped_ptr<CCFontAtlas> m_fontAtlas;
};

}  // namespace cc

#endif
