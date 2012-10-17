// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCRendererSoftware.h"

#include "CCQuadSink.h"
#include "CCRenderPass.h"
#include "CCRenderPassDrawQuad.h"
#include "CCSettings.h"
#include "CCSingleThreadProxy.h" // For DebugScopedSetImplThread
#include "CCSolidColorDrawQuad.h"
#include "CCTileDrawQuad.h"
#include "cc/test/animation_test_common.h"
#include "cc/test/fake_web_compositor_output_surface.h"
#include "cc/test/fake_web_compositor_software_output_device.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/render_pass_test_common.h"
#include "cc/test/test_common.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace cc;
using namespace WebKit;
using namespace WebKitTests;

namespace {

class CCRendererSoftwareTest : public testing::Test, public CCRendererClient {
public:
    void initializeRenderer() {
        m_outputSurface = FakeWebCompositorOutputSurface::createSoftware(adoptPtr(new FakeWebCompositorSoftwareOutputDevice));
        m_resourceProvider = CCResourceProvider::create(m_outputSurface.get());
        m_renderer = CCRendererSoftware::create(this, resourceProvider(), softwareDevice());
    }

    WebCompositorSoftwareOutputDevice* softwareDevice() const { return m_outputSurface->softwareDevice(); }
    FakeWebCompositorOutputSurface* outputSurface() const { return m_outputSurface.get(); }
    CCResourceProvider* resourceProvider() const { return m_resourceProvider.get(); }
    CCRendererSoftware* renderer() const { return m_renderer.get(); }
    void setViewportSize(IntSize viewportSize) { m_viewportSize = viewportSize; }

    // CCRendererClient implementation.
    virtual const IntSize& deviceViewportSize() const OVERRIDE { return m_viewportSize; }
    virtual const CCLayerTreeSettings& settings() const OVERRIDE { return m_settings; }
    virtual void didLoseContext() OVERRIDE { }
    virtual void onSwapBuffersComplete() OVERRIDE { }
    virtual void setFullRootLayerDamage() OVERRIDE { }
    virtual void setMemoryAllocationLimitBytes(size_t) OVERRIDE { }

protected:
    DebugScopedSetImplThread m_alwaysImplThread;

    scoped_ptr<FakeWebCompositorOutputSurface> m_outputSurface;
    scoped_ptr<CCResourceProvider> m_resourceProvider;
    scoped_ptr<CCRendererSoftware> m_renderer;
    IntSize m_viewportSize;
    CCLayerTreeSettings m_settings;
};

TEST_F(CCRendererSoftwareTest, solidColorQuad)
{
    IntSize outerSize(100, 100);
    int outerPixels = outerSize.width() * outerSize.height();
    IntSize innerSize(98, 98);
    IntRect outerRect(IntPoint(), outerSize);
    IntRect innerRect(IntPoint(1, 1), innerSize);
    setViewportSize(outerSize);

    initializeRenderer();

    scoped_ptr<CCSharedQuadState> sharedQuadState = CCSharedQuadState::create(WebTransformationMatrix(), outerRect, outerRect, 1.0, true);
    CCRenderPass::Id rootRenderPassId = CCRenderPass::Id(1, 1);
    scoped_ptr<CCRenderPass> rootRenderPass = CCTestRenderPass::create(rootRenderPassId, outerRect, WebTransformationMatrix());
    CCTestRenderPass* testRenderPass = static_cast<CCTestRenderPass*>(rootRenderPass.get());
    scoped_ptr<CCDrawQuad> outerQuad = CCSolidColorDrawQuad::create(sharedQuadState.get(), outerRect, SK_ColorYELLOW).PassAs<CCDrawQuad>();
    scoped_ptr<CCDrawQuad> innerQuad = CCSolidColorDrawQuad::create(sharedQuadState.get(), innerRect, SK_ColorCYAN).PassAs<CCDrawQuad>();
    testRenderPass->appendQuad(innerQuad.Pass());
    testRenderPass->appendQuad(outerQuad.Pass());

    CCRenderPassList list;
    CCRenderPassIdHashMap hashmap;
    list.push_back(rootRenderPass.get());
    hashmap.add(rootRenderPassId, rootRenderPass.Pass());
    renderer()->drawFrame(list, hashmap);

    scoped_array<SkColor> pixels(new SkColor[deviceViewportSize().width() * deviceViewportSize().height()]);
    renderer()->getFramebufferPixels(pixels.get(), outerRect);

// FIXME: This fails on Android. Endianness maybe?
// Yellow: expects 0xFFFFFF00, was 0xFF00FFFF on android.
// Cyan:   expects 0xFF00FFFF, was 0xFFFFFF00 on android.
// http://crbug.com/154528
#ifndef OS_ANDROID
    EXPECT_EQ(SK_ColorYELLOW, pixels[0]);
    EXPECT_EQ(SK_ColorYELLOW, pixels[outerPixels - 1]);
    EXPECT_EQ(SK_ColorCYAN, pixels[outerSize.width() + 1]);
    EXPECT_EQ(SK_ColorCYAN, pixels[outerPixels - outerSize.width() - 2]);
#endif
}

TEST_F(CCRendererSoftwareTest, tileQuad)
{
    IntSize outerSize(100, 100);
    int outerPixels = outerSize.width() * outerSize.height();
    IntSize innerSize(98, 98);
    int innerPixels = innerSize.width() * innerSize.height();
    IntRect outerRect(IntPoint(), outerSize);
    IntRect innerRect(IntPoint(1, 1), innerSize);
    setViewportSize(outerSize);
    initializeRenderer();

    CCResourceProvider::ResourceId resourceYellow = resourceProvider()->createResource(1, outerSize, GraphicsContext3D::RGBA, CCResourceProvider::TextureUsageAny);
    CCResourceProvider::ResourceId resourceCyan = resourceProvider()->createResource(1, innerSize, GraphicsContext3D::RGBA, CCResourceProvider::TextureUsageAny);

    SkColor yellow = SK_ColorYELLOW;
    SkColor cyan = SK_ColorCYAN;
    scoped_array<SkColor> yellowPixels(new SkColor[outerPixels]);
    scoped_array<SkColor> cyanPixels(new SkColor[innerPixels]);
    for (int i = 0; i < outerPixels; i++)
      yellowPixels[i] = yellow;
    for (int i = 0; i < innerPixels; i++)
      cyanPixels[i] = cyan;

    resourceProvider()->upload(resourceYellow, reinterpret_cast<uint8_t*>(yellowPixels.get()), IntRect(IntPoint(), outerSize), IntRect(IntPoint(), outerSize), IntSize());
    resourceProvider()->upload(resourceCyan, reinterpret_cast<uint8_t*>(cyanPixels.get()), IntRect(IntPoint(), innerSize), IntRect(IntPoint(), innerSize), IntSize());

    IntRect rect = IntRect(IntPoint(), deviceViewportSize());

    scoped_ptr<CCSharedQuadState> sharedQuadState = CCSharedQuadState::create(WebTransformationMatrix(), outerRect, outerRect, 1.0, true);
    CCRenderPass::Id rootRenderPassId = CCRenderPass::Id(1, 1);
    scoped_ptr<CCRenderPass> rootRenderPass = CCTestRenderPass::create(rootRenderPassId, IntRect(IntPoint(), deviceViewportSize()), WebTransformationMatrix());
    CCTestRenderPass* testRenderPass = static_cast<CCTestRenderPass*>(rootRenderPass.get());
    scoped_ptr<CCDrawQuad> outerQuad = CCTileDrawQuad::create(sharedQuadState.get(), outerRect, outerRect, resourceYellow, IntPoint(), outerSize, 0, false, false, false, false, false).PassAs<CCDrawQuad>();
    scoped_ptr<CCDrawQuad> innerQuad = CCTileDrawQuad::create(sharedQuadState.get(), innerRect, innerRect, resourceCyan, IntPoint(), innerSize, 0, false, false, false, false, false).PassAs<CCDrawQuad>();
    testRenderPass->appendQuad(innerQuad.Pass());
    testRenderPass->appendQuad(outerQuad.Pass());

    CCRenderPassList list;
    CCRenderPassIdHashMap hashmap;
    list.push_back(rootRenderPass.get());
    hashmap.add(rootRenderPassId, rootRenderPass.Pass());
    renderer()->drawFrame(list, hashmap);

    scoped_array<SkColor> pixels(new SkColor[deviceViewportSize().width() * deviceViewportSize().height()]);
    renderer()->getFramebufferPixels(pixels.get(), outerRect);

    EXPECT_EQ(SK_ColorYELLOW, pixels[0]);
    EXPECT_EQ(SK_ColorYELLOW, pixels[outerPixels - 1]);
    EXPECT_EQ(SK_ColorCYAN, pixels[outerSize.width() + 1]);
    EXPECT_EQ(SK_ColorCYAN, pixels[outerPixels - outerSize.width() - 2]);
}

} // namespace
