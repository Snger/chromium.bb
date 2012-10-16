// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "ContentLayerChromium.h"

#include "BitmapCanvasLayerTextureUpdater.h"
#include "CCRenderingStats.h"
#include "ContentLayerChromiumClient.h"
#include "cc/test/geometry_test_utils.h"
#include "skia/ext/platform_canvas.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <public/WebFloatRect.h>
#include <public/WebRect.h>

using namespace cc;
using namespace WebKit;

namespace {

class MockContentLayerChromiumClient : public ContentLayerChromiumClient {
public:
    explicit MockContentLayerChromiumClient(IntRect opaqueLayerRect)
        : m_opaqueLayerRect(opaqueLayerRect)
    {
    }

    virtual void paintContents(SkCanvas*, const IntRect&, FloatRect& opaque) OVERRIDE
    {
        opaque = FloatRect(m_opaqueLayerRect);
    }

private:
    IntRect m_opaqueLayerRect;
};

TEST(ContentLayerChromiumTest, ContentLayerPainterWithDeviceScale)
{
    float contentsScale = 2;
    IntRect contentRect(10, 10, 100, 100);
    IntRect opaqueRectInLayerSpace(5, 5, 20, 20);
    IntRect opaqueRectInContentSpace = opaqueRectInLayerSpace;
    opaqueRectInContentSpace.scale(contentsScale);
    MockContentLayerChromiumClient client(opaqueRectInLayerSpace);
    RefPtr<BitmapCanvasLayerTextureUpdater> updater = BitmapCanvasLayerTextureUpdater::create(ContentLayerPainter::create(&client).PassAs<LayerPainterChromium>());

    IntRect resultingOpaqueRect;
    CCRenderingStats stats;
    updater->prepareToUpdate(contentRect, IntSize(256, 256), contentsScale, contentsScale, resultingOpaqueRect, stats);

    EXPECT_RECT_EQ(opaqueRectInContentSpace, resultingOpaqueRect);
}

} // namespace
