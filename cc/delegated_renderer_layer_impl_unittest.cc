// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCDelegatedRendererLayerImpl.h"

#include "CCAppendQuadsData.h"
#include "CCGeometryTestUtils.h"
#include "CCLayerTreeHostImpl.h"
#include "CCQuadSink.h"
#include "CCRenderPassDrawQuad.h"
#include "CCRenderPassTestCommon.h"
#include "CCSingleThreadProxy.h"
#include "CCSolidColorDrawQuad.h"
#include "CCSolidColorLayerImpl.h"
#include "FakeWebCompositorOutputSurface.h"
#include "FakeWebGraphicsContext3D.h"
#include "MockCCQuadCuller.h"
#include "cc/scoped_ptr_vector.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <public/WebTransformationMatrix.h>

using WebKit::FakeWebCompositorOutputSurface;
using WebKit::FakeWebGraphicsContext3D;
using WebKit::WebTransformationMatrix;

using namespace cc;
using namespace WebKitTests;

namespace {

class CCDelegatedRendererLayerImplTest : public testing::Test, public CCLayerTreeHostImplClient {
public:
    CCDelegatedRendererLayerImplTest()
    {
        CCLayerTreeSettings settings;
        settings.minimumOcclusionTrackingSize = IntSize();

        m_hostImpl = CCLayerTreeHostImpl::create(settings, this);
        m_hostImpl->initializeRenderer(createContext());
        m_hostImpl->setViewportSize(IntSize(10, 10), IntSize(10, 10));
    }

    // CCLayerTreeHostImplClient implementation.
    virtual void didLoseContextOnImplThread() OVERRIDE { }
    virtual void onSwapBuffersCompleteOnImplThread() OVERRIDE { }
    virtual void onVSyncParametersChanged(double, double) OVERRIDE { }
    virtual void onCanDrawStateChanged(bool) OVERRIDE { }
    virtual void setNeedsRedrawOnImplThread() OVERRIDE { }
    virtual void setNeedsCommitOnImplThread() OVERRIDE { }
    virtual void postAnimationEventsToMainThreadOnImplThread(scoped_ptr<CCAnimationEventsVector>, double wallClockTime) OVERRIDE { }
    virtual void releaseContentsTexturesOnImplThread() OVERRIDE { }

protected:
    scoped_ptr<CCGraphicsContext> createContext()
    {
        return FakeWebCompositorOutputSurface::create(adoptPtr(new FakeWebGraphicsContext3D)).PassAs<CCGraphicsContext>();
    }

    DebugScopedSetImplThread m_alwaysImplThread;
    DebugScopedSetMainThreadBlocked m_alwaysMainThreadBlocked;

    scoped_ptr<CCLayerTreeHostImpl> m_hostImpl;
};

static CCTestRenderPass* addRenderPass(ScopedPtrVector<CCRenderPass>& passList, CCRenderPass::Id id, IntRect outputRect, WebTransformationMatrix rootTransform)
{
    scoped_ptr<CCRenderPass> pass(CCRenderPass::create(id, outputRect, rootTransform));
    CCTestRenderPass* testPass = static_cast<CCTestRenderPass*>(pass.get());
    passList.append(pass.Pass());
    return testPass;
}

static CCSolidColorDrawQuad* addQuad(CCTestRenderPass* pass, IntRect rect, SkColor color)
{
    MockCCQuadCuller quadSink(pass->quadList(), pass->sharedQuadStateList());
    CCAppendQuadsData data(pass->id());
    CCSharedQuadState* sharedState = quadSink.useSharedQuadState(CCSharedQuadState::create(WebTransformationMatrix(), rect, rect, 1, false));
    scoped_ptr<CCSolidColorDrawQuad> quad = CCSolidColorDrawQuad::create(sharedState, rect, color);
    CCSolidColorDrawQuad* quadPtr = quad.get();
    quadSink.append(quad.PassAs<CCDrawQuad>(), data);
    return quadPtr;
}

static void addRenderPassQuad(CCTestRenderPass* toPass, CCTestRenderPass* contributingPass)
{
    MockCCQuadCuller quadSink(toPass->quadList(), toPass->sharedQuadStateList());
    CCAppendQuadsData data(toPass->id());
    IntRect outputRect = contributingPass->outputRect();
    CCSharedQuadState* sharedState = quadSink.useSharedQuadState(CCSharedQuadState::create(WebTransformationMatrix(), outputRect, outputRect, 1, false));
    scoped_ptr<CCRenderPassDrawQuad> quad = CCRenderPassDrawQuad::create(sharedState, outputRect, contributingPass->id(), false, 0, outputRect, 0, 0, 0, 0);
    quadSink.append(quad.PassAs<CCDrawQuad>(), data);
}

class CCDelegatedRendererLayerImplTestSimple : public CCDelegatedRendererLayerImplTest {
public:
    CCDelegatedRendererLayerImplTestSimple()
        : CCDelegatedRendererLayerImplTest()
    {
        scoped_ptr<CCLayerImpl> rootLayer = CCSolidColorLayerImpl::create(1).PassAs<CCLayerImpl>();
        scoped_ptr<CCLayerImpl> layerBefore = CCSolidColorLayerImpl::create(2).PassAs<CCLayerImpl>();
        scoped_ptr<CCLayerImpl> layerAfter = CCSolidColorLayerImpl::create(3).PassAs<CCLayerImpl>();
        scoped_ptr<CCDelegatedRendererLayerImpl> delegatedRendererLayer = CCDelegatedRendererLayerImpl::create(4);

        m_hostImpl->setViewportSize(IntSize(100, 100), IntSize(100, 100));
        rootLayer->setBounds(IntSize(100, 100));

        layerBefore->setPosition(IntPoint(20, 20));
        layerBefore->setBounds(IntSize(14, 14));
        layerBefore->setContentBounds(IntSize(14, 14));
        layerBefore->setDrawsContent(true);
        layerBefore->setForceRenderSurface(true);

        layerAfter->setPosition(IntPoint(5, 5));
        layerAfter->setBounds(IntSize(15, 15));
        layerAfter->setContentBounds(IntSize(15, 15));
        layerAfter->setDrawsContent(true);
        layerAfter->setForceRenderSurface(true);

        delegatedRendererLayer->setPosition(IntPoint(3, 3));
        delegatedRendererLayer->setBounds(IntSize(10, 10));
        delegatedRendererLayer->setContentBounds(IntSize(10, 10));
        delegatedRendererLayer->setDrawsContent(true);
        WebTransformationMatrix transform;
        transform.translate(1, 1);
        delegatedRendererLayer->setTransform(transform);

        ScopedPtrVector<CCRenderPass> delegatedRenderPasses;
        CCTestRenderPass* pass1 = addRenderPass(delegatedRenderPasses, CCRenderPass::Id(9, 6), IntRect(6, 6, 6, 6), WebTransformationMatrix());
        addQuad(pass1, IntRect(0, 0, 6, 6), 33u);
        CCTestRenderPass* pass2 = addRenderPass(delegatedRenderPasses, CCRenderPass::Id(9, 7), IntRect(7, 7, 7, 7), WebTransformationMatrix());
        addQuad(pass2, IntRect(0, 0, 7, 7), 22u);
        addRenderPassQuad(pass2, pass1);
        CCTestRenderPass* pass3 = addRenderPass(delegatedRenderPasses, CCRenderPass::Id(9, 8), IntRect(8, 8, 8, 8), WebTransformationMatrix());
        addRenderPassQuad(pass3, pass2);
        delegatedRendererLayer->setRenderPasses(delegatedRenderPasses);

        // The RenderPasses should be taken by the layer.
        EXPECT_EQ(0u, delegatedRenderPasses.size());

        m_rootLayerPtr = rootLayer.get();
        m_layerBeforePtr = layerBefore.get();
        m_layerAfterPtr = layerAfter.get();
        m_delegatedRendererLayerPtr = delegatedRendererLayer.get();

        // Force the delegated RenderPasses to come before the RenderPass from layerAfter.
        layerAfter->addChild(delegatedRendererLayer.PassAs<CCLayerImpl>());
        rootLayer->addChild(layerAfter.Pass());

        // Get the RenderPass generated by layerBefore to come before the delegated RenderPasses.
        rootLayer->addChild(layerBefore.Pass());

        m_hostImpl->setRootLayer(rootLayer.Pass());
    }

protected:
    CCLayerImpl* m_rootLayerPtr;
    CCLayerImpl* m_layerBeforePtr;
    CCLayerImpl* m_layerAfterPtr;
    CCDelegatedRendererLayerImpl* m_delegatedRendererLayerPtr;
};

TEST_F(CCDelegatedRendererLayerImplTestSimple, AddsContributingRenderPasses)
{
    CCLayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);

    // Each non-DelegatedRendererLayer added one RenderPass. The DelegatedRendererLayer added two contributing passes.
    ASSERT_EQ(5u, frame.renderPasses.size());

    // The DelegatedRendererLayer should have added its contributing RenderPasses to the frame.
    EXPECT_EQ(4, frame.renderPasses[1]->id().layerId);
    EXPECT_EQ(1, frame.renderPasses[1]->id().index);
    EXPECT_EQ(4, frame.renderPasses[2]->id().layerId);
    EXPECT_EQ(2, frame.renderPasses[2]->id().index);
    // And all other RenderPasses should be non-delegated.
    EXPECT_NE(4, frame.renderPasses[0]->id().layerId);
    EXPECT_EQ(0, frame.renderPasses[0]->id().index);
    EXPECT_NE(4, frame.renderPasses[3]->id().layerId);
    EXPECT_EQ(0, frame.renderPasses[3]->id().index);
    EXPECT_NE(4, frame.renderPasses[4]->id().layerId);
    EXPECT_EQ(0, frame.renderPasses[4]->id().index);

    // The DelegatedRendererLayer should have added its RenderPasses to the frame in order.
    EXPECT_RECT_EQ(IntRect(6, 6, 6, 6), frame.renderPasses[1]->outputRect());
    EXPECT_RECT_EQ(IntRect(7, 7, 7, 7), frame.renderPasses[2]->outputRect());
}

TEST_F(CCDelegatedRendererLayerImplTestSimple, AddsQuadsToContributingRenderPasses)
{
    CCLayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);

    // Each non-DelegatedRendererLayer added one RenderPass. The DelegatedRendererLayer added two contributing passes.
    ASSERT_EQ(5u, frame.renderPasses.size());

    // The DelegatedRendererLayer should have added its contributing RenderPasses to the frame.
    EXPECT_EQ(4, frame.renderPasses[1]->id().layerId);
    EXPECT_EQ(1, frame.renderPasses[1]->id().index);
    EXPECT_EQ(4, frame.renderPasses[2]->id().layerId);
    EXPECT_EQ(2, frame.renderPasses[2]->id().index);

    // The DelegatedRendererLayer should have added copies of its quads to contributing RenderPasses.
    ASSERT_EQ(1u, frame.renderPasses[1]->quadList().size());
    EXPECT_RECT_EQ(IntRect(0, 0, 6, 6), frame.renderPasses[1]->quadList()[0]->quadRect());

    // Verify it added the right quads.
    ASSERT_EQ(2u, frame.renderPasses[2]->quadList().size());
    EXPECT_RECT_EQ(IntRect(0, 0, 7, 7), frame.renderPasses[2]->quadList()[0]->quadRect());
    EXPECT_RECT_EQ(IntRect(6, 6, 6, 6), frame.renderPasses[2]->quadList()[1]->quadRect());
    ASSERT_EQ(1u, frame.renderPasses[1]->quadList().size());
    EXPECT_RECT_EQ(IntRect(0, 0, 6, 6), frame.renderPasses[1]->quadList()[0]->quadRect());
}

TEST_F(CCDelegatedRendererLayerImplTestSimple, AddsQuadsToTargetRenderPass)
{
    CCLayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);

    // Each non-DelegatedRendererLayer added one RenderPass. The DelegatedRendererLayer added two contributing passes.
    ASSERT_EQ(5u, frame.renderPasses.size());

    // The layer's target is the RenderPass from m_layerAfter.
    EXPECT_EQ(CCRenderPass::Id(3, 0), frame.renderPasses[3]->id());

    // The DelegatedRendererLayer should have added copies of quads in its root RenderPass to its target RenderPass.
    // The m_layerAfter also adds one quad.
    ASSERT_EQ(2u, frame.renderPasses[3]->quadList().size());

    // Verify it added the right quads.
    EXPECT_RECT_EQ(IntRect(7, 7, 7, 7), frame.renderPasses[3]->quadList()[0]->quadRect());

    // Its target layer should have a quad as well.
    EXPECT_RECT_EQ(IntRect(0, 0, 15, 15), frame.renderPasses[3]->quadList()[1]->quadRect());
}

TEST_F(CCDelegatedRendererLayerImplTestSimple, QuadsFromRootRenderPassAreModifiedForTheTarget)
{
    CCLayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);

    // Each non-DelegatedRendererLayer added one RenderPass. The DelegatedRendererLayer added two contributing passes.
    ASSERT_EQ(5u, frame.renderPasses.size());

    // The DelegatedRendererLayer is at position 3,3 compared to its target, and has a translation transform of 1,1.
    // So its root RenderPass' quads should all be transformed by that combined amount.
    WebTransformationMatrix transform;
    transform.translate(4, 4);
    EXPECT_TRANSFORMATION_MATRIX_EQ(transform, frame.renderPasses[3]->quadList()[0]->quadTransform());

    // Quads from non-root RenderPasses should not be shifted though.
    ASSERT_EQ(2u, frame.renderPasses[2]->quadList().size());
    EXPECT_TRANSFORMATION_MATRIX_EQ(WebTransformationMatrix(), frame.renderPasses[2]->quadList()[0]->quadTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(WebTransformationMatrix(), frame.renderPasses[2]->quadList()[1]->quadTransform());
    ASSERT_EQ(1u, frame.renderPasses[1]->quadList().size());
    EXPECT_TRANSFORMATION_MATRIX_EQ(WebTransformationMatrix(), frame.renderPasses[1]->quadList()[0]->quadTransform());
}

class CCDelegatedRendererLayerImplTestOwnSurface : public CCDelegatedRendererLayerImplTestSimple {
public:
    CCDelegatedRendererLayerImplTestOwnSurface()
        : CCDelegatedRendererLayerImplTestSimple()
    {
        m_delegatedRendererLayerPtr->setForceRenderSurface(true);
    }
};

TEST_F(CCDelegatedRendererLayerImplTestOwnSurface, AddsRenderPasses)
{
    CCLayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);

    // Each non-DelegatedRendererLayer added one RenderPass. The DelegatedRendererLayer added two contributing passes and its owned surface added one pass.
    ASSERT_EQ(6u, frame.renderPasses.size());

    // The DelegatedRendererLayer should have added its contributing RenderPasses to the frame.
    EXPECT_EQ(4, frame.renderPasses[1]->id().layerId);
    EXPECT_EQ(1, frame.renderPasses[1]->id().index);
    EXPECT_EQ(4, frame.renderPasses[2]->id().layerId);
    EXPECT_EQ(2, frame.renderPasses[2]->id().index);
    // The DelegatedRendererLayer should have added a RenderPass for its surface to the frame.
    EXPECT_EQ(4, frame.renderPasses[1]->id().layerId);
    EXPECT_EQ(0, frame.renderPasses[3]->id().index);
    // And all other RenderPasses should be non-delegated.
    EXPECT_NE(4, frame.renderPasses[0]->id().layerId);
    EXPECT_EQ(0, frame.renderPasses[0]->id().index);
    EXPECT_NE(4, frame.renderPasses[4]->id().layerId);
    EXPECT_EQ(0, frame.renderPasses[4]->id().index);
    EXPECT_NE(4, frame.renderPasses[5]->id().layerId);
    EXPECT_EQ(0, frame.renderPasses[5]->id().index);

    // The DelegatedRendererLayer should have added its RenderPasses to the frame in order.
    EXPECT_RECT_EQ(IntRect(6, 6, 6, 6), frame.renderPasses[1]->outputRect());
    EXPECT_RECT_EQ(IntRect(7, 7, 7, 7), frame.renderPasses[2]->outputRect());
}

TEST_F(CCDelegatedRendererLayerImplTestOwnSurface, AddsQuadsToContributingRenderPasses)
{
    CCLayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);

    // Each non-DelegatedRendererLayer added one RenderPass. The DelegatedRendererLayer added two contributing passes and its owned surface added one pass.
    ASSERT_EQ(6u, frame.renderPasses.size());

    // The DelegatedRendererLayer should have added its contributing RenderPasses to the frame.
    EXPECT_EQ(4, frame.renderPasses[1]->id().layerId);
    EXPECT_EQ(1, frame.renderPasses[1]->id().index);
    EXPECT_EQ(4, frame.renderPasses[2]->id().layerId);
    EXPECT_EQ(2, frame.renderPasses[2]->id().index);

    // The DelegatedRendererLayer should have added copies of its quads to contributing RenderPasses.
    ASSERT_EQ(1u, frame.renderPasses[1]->quadList().size());
    EXPECT_RECT_EQ(IntRect(0, 0, 6, 6), frame.renderPasses[1]->quadList()[0]->quadRect());

    // Verify it added the right quads.
    ASSERT_EQ(2u, frame.renderPasses[2]->quadList().size());
    EXPECT_RECT_EQ(IntRect(0, 0, 7, 7), frame.renderPasses[2]->quadList()[0]->quadRect());
    EXPECT_RECT_EQ(IntRect(6, 6, 6, 6), frame.renderPasses[2]->quadList()[1]->quadRect());
    ASSERT_EQ(1u, frame.renderPasses[1]->quadList().size());
    EXPECT_RECT_EQ(IntRect(0, 0, 6, 6), frame.renderPasses[1]->quadList()[0]->quadRect());
}

TEST_F(CCDelegatedRendererLayerImplTestOwnSurface, AddsQuadsToTargetRenderPass)
{
    CCLayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);

    // Each non-DelegatedRendererLayer added one RenderPass. The DelegatedRendererLayer added two contributing passes and its owned surface added one pass.
    ASSERT_EQ(6u, frame.renderPasses.size());

    // The layer's target is the RenderPass owned by itself.
    EXPECT_EQ(CCRenderPass::Id(4, 0), frame.renderPasses[3]->id());

    // The DelegatedRendererLayer should have added copies of quads in its root RenderPass to its target RenderPass.
    // The m_layerAfter also adds one quad.
    ASSERT_EQ(1u, frame.renderPasses[3]->quadList().size());

    // Verify it added the right quads.
    EXPECT_RECT_EQ(IntRect(7, 7, 7, 7), frame.renderPasses[3]->quadList()[0]->quadRect());
}

TEST_F(CCDelegatedRendererLayerImplTestOwnSurface, QuadsFromRootRenderPassAreNotModifiedForTheTarget)
{
    CCLayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);

    // Each non-DelegatedRendererLayer added one RenderPass. The DelegatedRendererLayer added two contributing passes and its owned surface added one pass.
    ASSERT_EQ(6u, frame.renderPasses.size());

    // Because the DelegatedRendererLayer owns a RenderSurface, its root RenderPass' quads do not need to be
    // modified at all.
    EXPECT_TRANSFORMATION_MATRIX_EQ(WebTransformationMatrix(), frame.renderPasses[3]->quadList()[0]->quadTransform());

    // Quads from non-root RenderPasses should not be shifted though.
    ASSERT_EQ(2u, frame.renderPasses[2]->quadList().size());
    EXPECT_TRANSFORMATION_MATRIX_EQ(WebTransformationMatrix(), frame.renderPasses[2]->quadList()[0]->quadTransform());
    EXPECT_TRANSFORMATION_MATRIX_EQ(WebTransformationMatrix(), frame.renderPasses[2]->quadList()[1]->quadTransform());
    ASSERT_EQ(1u, frame.renderPasses[1]->quadList().size());
    EXPECT_TRANSFORMATION_MATRIX_EQ(WebTransformationMatrix(), frame.renderPasses[1]->quadList()[0]->quadTransform());
}

class CCDelegatedRendererLayerImplTestSharedData : public CCDelegatedRendererLayerImplTest {
public:
    CCDelegatedRendererLayerImplTestSharedData()
        : CCDelegatedRendererLayerImplTest()
    {
        scoped_ptr<CCLayerImpl> rootLayer = CCLayerImpl::create(1);
        scoped_ptr<CCDelegatedRendererLayerImpl> delegatedRendererLayer = CCDelegatedRendererLayerImpl::create(2);

        m_hostImpl->setViewportSize(IntSize(100, 100), IntSize(100, 100));
        rootLayer->setBounds(IntSize(100, 100));

        delegatedRendererLayer->setPosition(IntPoint(20, 20));
        delegatedRendererLayer->setBounds(IntSize(20, 20));
        delegatedRendererLayer->setContentBounds(IntSize(20, 20));
        delegatedRendererLayer->setDrawsContent(true);
        WebTransformationMatrix transform;
        transform.translate(10, 10);
        delegatedRendererLayer->setTransform(transform);

        ScopedPtrVector<CCRenderPass> delegatedRenderPasses;
        IntRect passRect(0, 0, 50, 50);
        CCTestRenderPass* pass = addRenderPass(delegatedRenderPasses, CCRenderPass::Id(9, 6), passRect, WebTransformationMatrix());
        MockCCQuadCuller quadSink(pass->quadList(), pass->sharedQuadStateList());
        CCAppendQuadsData data(pass->id());
        CCSharedQuadState* sharedState = quadSink.useSharedQuadState(CCSharedQuadState::create(WebTransformationMatrix(), passRect, passRect, 1, false));
        quadSink.append(CCSolidColorDrawQuad::create(sharedState, IntRect(0, 0, 10, 10), 1u).PassAs<CCDrawQuad>(), data);
        quadSink.append(CCSolidColorDrawQuad::create(sharedState, IntRect(0, 10, 10, 10), 2u).PassAs<CCDrawQuad>(), data);
        quadSink.append(CCSolidColorDrawQuad::create(sharedState, IntRect(10, 0, 10, 10), 3u).PassAs<CCDrawQuad>(), data);
        quadSink.append(CCSolidColorDrawQuad::create(sharedState, IntRect(10, 10, 10, 10), 4u).PassAs<CCDrawQuad>(), data);
        delegatedRendererLayer->setRenderPasses(delegatedRenderPasses);

        // The RenderPasses should be taken by the layer.
        EXPECT_EQ(0u, delegatedRenderPasses.size());

        m_rootLayerPtr = rootLayer.get();
        m_delegatedRendererLayerPtr = delegatedRendererLayer.get();

        rootLayer->addChild(delegatedRendererLayer.PassAs<CCLayerImpl>());

        m_hostImpl->setRootLayer(rootLayer.Pass());
    }

protected:
    CCLayerImpl* m_rootLayerPtr;
    CCDelegatedRendererLayerImpl* m_delegatedRendererLayerPtr;
};

TEST_F(CCDelegatedRendererLayerImplTestSharedData, SharedData)
{
    CCLayerTreeHostImpl::FrameData frame;
    EXPECT_TRUE(m_hostImpl->prepareToDraw(frame));
    m_hostImpl->drawLayers(frame);
    m_hostImpl->didDrawAllLayers(frame);

    ASSERT_EQ(1u, frame.renderPasses.size());
    EXPECT_EQ(1, frame.renderPasses[0]->id().layerId);
    EXPECT_EQ(0, frame.renderPasses[0]->id().index);

    const CCQuadList& quadList = frame.renderPasses[0]->quadList();
    ASSERT_EQ(4u, quadList.size());

    // All quads should share the same state.
    const CCSharedQuadState* sharedState = quadList[0]->sharedQuadState();
    EXPECT_EQ(sharedState, quadList[1]->sharedQuadState());
    EXPECT_EQ(sharedState, quadList[2]->sharedQuadState());
    EXPECT_EQ(sharedState, quadList[3]->sharedQuadState());

    // The state should be transformed only once.
    EXPECT_RECT_EQ(IntRect(30, 30, 50, 50), sharedState->clippedRectInTarget);
    WebTransformationMatrix expected;
    expected.translate(30, 30);
    EXPECT_TRANSFORMATION_MATRIX_EQ(expected, sharedState->quadTransform);
}

} // namespace
