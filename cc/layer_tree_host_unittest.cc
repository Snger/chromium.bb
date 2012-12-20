// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/layer_tree_host.h"

#include "base/synchronization/lock.h"
#include "cc/content_layer.h"
#include "cc/content_layer_client.h"
#include "cc/layer_impl.h"
#include "cc/layer_tree_host_impl.h"
#include "cc/layer_tree_impl.h"
#include "cc/output_surface.h"
#include "cc/single_thread_proxy.h"
#include "cc/test/fake_content_layer_client.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/fake_output_surface.h"
#include "cc/test/fake_proxy.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/layer_tree_test_common.h"
#include "cc/resource_update_queue.h"
#include "cc/test/occlusion_tracker_test_common.h"
#include "cc/timing_function.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebLayerScrollClient.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebSize.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/point_conversions.h"
#include "ui/gfx/size_conversions.h"
#include "ui/gfx/vector2d_conversions.h"

namespace cc {
namespace {

class LayerTreeHostTest : public ThreadedTest { };

// Shortlived layerTreeHosts shouldn't die.
class LayerTreeHostTestShortlived1 : public LayerTreeHostTest {
public:
    LayerTreeHostTestShortlived1() { }

    virtual void beginTest() OVERRIDE
    {
        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.reset();

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

// Shortlived layerTreeHosts shouldn't die with a commit in flight.
class LayerTreeHostTestShortlived2 : public LayerTreeHostTest {
public:
    LayerTreeHostTestShortlived2() { }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.reset();

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestShortlived2)

// Shortlived layerTreeHosts shouldn't die with a redraw in flight.
class LayerTreeHostTestShortlived3 : public LayerTreeHostTest {
public:
    LayerTreeHostTestShortlived3() { }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsRedrawToMainThread();

        // Kill the layerTreeHost immediately.
        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.reset();

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestShortlived3)

// Test interleaving of redraws and commits
class LayerTreeHostTestCommitingWithContinuousRedraw : public LayerTreeHostTest {
public:
    LayerTreeHostTestCommitingWithContinuousRedraw()
        : m_numCompleteCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        m_numCompleteCommits++;
        if (m_numCompleteCommits == 2)
            endTest();
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        if (m_numDraws == 1)
          postSetNeedsCommitToMainThread();
        m_numDraws++;
        postSetNeedsRedrawToMainThread();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numCompleteCommits;
    int m_numDraws;
};

TEST_F(LayerTreeHostTestCommitingWithContinuousRedraw, runMultiThread)
{
    runTest(true);
}

// Two setNeedsCommits in a row should lead to at least 1 commit and at least 1
// draw with frame 0.
class LayerTreeHostTestSetNeedsCommit1 : public LayerTreeHostTest {
public:
    LayerTreeHostTestSetNeedsCommit1()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        m_numDraws++;
        if (!impl->activeTree()->source_frame_number())
            endTest();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        m_numCommits++;
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_GE(1, m_numCommits);
        EXPECT_GE(1, m_numDraws);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

TEST_F(LayerTreeHostTestSetNeedsCommit1, DISABLED_runMultiThread)
{
    runTest(true);
}

// A setNeedsCommit should lead to 1 commit. Issuing a second commit after that
// first committed frame draws should lead to another commit.
class LayerTreeHostTestSetNeedsCommit2 : public LayerTreeHostTest {
public:
    LayerTreeHostTestSetNeedsCommit2()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        if (impl->activeTree()->source_frame_number() == 0)
            postSetNeedsCommitToMainThread();
        else if (impl->activeTree()->source_frame_number() == 1)
            endTest();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        m_numCommits++;
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(2, m_numCommits);
        EXPECT_GE(2, m_numDraws);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

TEST_F(LayerTreeHostTestSetNeedsCommit2, runMultiThread)
{
    runTest(true);
}

// 1 setNeedsRedraw after the first commit has completed should lead to 1
// additional draw.
class LayerTreeHostTestSetNeedsRedraw : public LayerTreeHostTest {
public:
    LayerTreeHostTestSetNeedsRedraw()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        EXPECT_EQ(0, impl->activeTree()->source_frame_number());
        if (!m_numDraws)
            postSetNeedsRedrawToMainThread(); // Redraw again to verify that the second redraw doesn't commit.
        else
            endTest();
        m_numDraws++;
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        EXPECT_EQ(0, m_numDraws);
        m_numCommits++;
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_GE(2, m_numDraws);
        EXPECT_EQ(1, m_numCommits);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

TEST_F(LayerTreeHostTestSetNeedsRedraw, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestCompositeAndReadback : public LayerTreeHostTest {
public:
    LayerTreeHostTestCompositeAndReadback()
        : m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommit() OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            char pixels[4];
            m_layerTreeHost->compositeAndReadback(static_cast<void*>(&pixels), gfx::Rect(0, 0, 1, 1));
        } else if (m_numCommits == 2) {
            // This is inside the readback. We should get another commit after it.
        } else if (m_numCommits == 3) {
            endTest();
        } else {
          NOTREACHED();
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
};

TEST_F(LayerTreeHostTestCompositeAndReadback, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestCompositeAndReadbackBeforePreviousCommitDraws : public LayerTreeHostTest {
public:
    LayerTreeHostTestCompositeAndReadbackBeforePreviousCommitDraws()
        : m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommit() OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            m_layerTreeHost->setNeedsCommit();
        } else if (m_numCommits == 2) {
            char pixels[4];
            m_layerTreeHost->compositeAndReadback(static_cast<void*>(&pixels), gfx::Rect(0, 0, 1, 1));
        } else if (m_numCommits == 3) {
            // This is inside the readback. We should get another commit after it.
        } else if (m_numCommits == 4) {
            endTest();
        } else {
          NOTREACHED();
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
};

TEST_F(LayerTreeHostTestCompositeAndReadbackBeforePreviousCommitDraws, runMultiThread)
{
    runTest(true);
}

// If the layerTreeHost says it can't draw, then we should not try to draw.
class LayerTreeHostTestCanDrawBlocksDrawing : public LayerTreeHostTest {
public:
    LayerTreeHostTestCanDrawBlocksDrawing()
        : m_numCommits(0)
        , m_done(false)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        if (m_done)
            return;
        // Only the initial draw should bring us here.
        EXPECT_TRUE(impl->canDraw());
        EXPECT_EQ(0, impl->activeTree()->source_frame_number());
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        if (m_done)
            return;
        if (m_numCommits >= 1) {
            // After the first commit, we should not be able to draw.
            EXPECT_FALSE(impl->canDraw());
        }
    }

    virtual void didCommit() OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            // Make the viewport empty so the host says it can't draw.
            m_layerTreeHost->setViewportSize(gfx::Size(0, 0), gfx::Size(0, 0));
        } else if (m_numCommits == 2) {
            char pixels[4];
            m_layerTreeHost->compositeAndReadback(&pixels, gfx::Rect(0, 0, 1, 1));
        } else if (m_numCommits == 3) {
            // Let it draw so we go idle and end the test.
            m_layerTreeHost->setViewportSize(gfx::Size(1, 1), gfx::Size(1, 1));
            m_done = true;
            endTest();
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
    bool m_done;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestCanDrawBlocksDrawing)

// beginLayerWrite should prevent draws from executing until a commit occurs
class LayerTreeHostTestWriteLayersRedraw : public LayerTreeHostTest {
public:
    LayerTreeHostTestWriteLayersRedraw()
        : m_numCommits(0)
        , m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postAcquireLayerTextures();
        postSetNeedsRedrawToMainThread(); // should be inhibited without blocking
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        m_numDraws++;
        EXPECT_EQ(m_numDraws, m_numCommits);
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        m_numCommits++;
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(1, m_numCommits);
    }

private:
    int m_numCommits;
    int m_numDraws;
};

TEST_F(LayerTreeHostTestWriteLayersRedraw, runMultiThread)
{
    runTest(true);
}

// Verify that when resuming visibility, requesting layer write permission
// will not deadlock the main thread even though there are not yet any
// scheduled redraws. This behavior is critical for reliably surviving tab
// switching. There are no failure conditions to this test, it just passes
// by not timing out.
class LayerTreeHostTestWriteLayersAfterVisible : public LayerTreeHostTest {
public:
    LayerTreeHostTestWriteLayersAfterVisible()
        : m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 2)
            endTest();
        else if (m_numCommits < 2) {
            postSetVisibleToMainThread(false);
            postSetVisibleToMainThread(true);
            postAcquireLayerTextures();
            postSetNeedsCommitToMainThread();
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
};

TEST_F(LayerTreeHostTestWriteLayersAfterVisible, runMultiThread)
{
    runTest(true);
}

// A compositeAndReadback while invisible should force a normal commit without assertion.
class LayerTreeHostTestCompositeAndReadbackWhileInvisible : public LayerTreeHostTest {
public:
    LayerTreeHostTestCompositeAndReadbackWhileInvisible()
        : m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        m_numCommits++;
        if (m_numCommits == 1) {
            m_layerTreeHost->setVisible(false);
            m_layerTreeHost->setNeedsCommit();
            m_layerTreeHost->setNeedsCommit();
            char pixels[4];
            m_layerTreeHost->compositeAndReadback(static_cast<void*>(&pixels), gfx::Rect(0, 0, 1, 1));
        } else
            endTest();

    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    int m_numCommits;
};

TEST_F(LayerTreeHostTestCompositeAndReadbackWhileInvisible, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestAbortFrameWhenInvisible : public LayerTreeHostTest {
public:
    LayerTreeHostTestAbortFrameWhenInvisible()
    {
    }

    virtual void beginTest() OVERRIDE
    {
        // Request a commit (from the main thread), which will trigger the commit flow from the impl side.
        m_layerTreeHost->setNeedsCommit();
        // Then mark ourselves as not visible before processing any more messages on the main thread.
        m_layerTreeHost->setVisible(false);
        // If we make it without kicking a frame, we pass!
        endTestAfterDelay(1);
    }

    virtual void layout() OVERRIDE
    {
        ASSERT_FALSE(true);
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
};

TEST_F(LayerTreeHostTestAbortFrameWhenInvisible, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestScrollSimple : public LayerTreeHostTest {
public:
    LayerTreeHostTestScrollSimple()
        : m_initialScroll(10, 20)
        , m_secondScroll(40, 5)
        , m_scrollAmount(2, -1)
        , m_scrolls(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->rootLayer()->setScrollable(true);
        m_layerTreeHost->rootLayer()->setScrollOffset(m_initialScroll);
        postSetNeedsCommitToMainThread();
    }

    virtual void layout() OVERRIDE
    {
        Layer* root = m_layerTreeHost->rootLayer();
        if (!m_layerTreeHost->commitNumber())
            EXPECT_VECTOR_EQ(root->scrollOffset(), m_initialScroll);
        else {
            EXPECT_VECTOR_EQ(root->scrollOffset(), m_initialScroll + m_scrollAmount);

            // Pretend like Javascript updated the scroll position itself.
            root->setScrollOffset(m_secondScroll);
        }
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        LayerImpl* root = impl->rootLayer();
        EXPECT_VECTOR_EQ(root->scrollDelta(), gfx::Vector2d());

        root->setScrollable(true);
        root->setMaxScrollOffset(gfx::Vector2d(100, 100));
        root->scrollBy(m_scrollAmount);

        if (!impl->activeTree()->source_frame_number()) {
            EXPECT_VECTOR_EQ(root->scrollOffset(), m_initialScroll);
            EXPECT_VECTOR_EQ(root->scrollDelta(), m_scrollAmount);
            postSetNeedsCommitToMainThread();
        } else if (impl->activeTree()->source_frame_number() == 1) {
            EXPECT_VECTOR_EQ(root->scrollOffset(), m_secondScroll);
            EXPECT_VECTOR_EQ(root->scrollDelta(), m_scrollAmount);
            endTest();
        }
    }

    virtual void applyScrollAndScale(gfx::Vector2d scrollDelta, float scale) OVERRIDE
    {
        gfx::Vector2d offset = m_layerTreeHost->rootLayer()->scrollOffset();
        m_layerTreeHost->rootLayer()->setScrollOffset(offset + scrollDelta);
        m_scrolls++;
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(1, m_scrolls);
    }
private:
    gfx::Vector2d m_initialScroll;
    gfx::Vector2d m_secondScroll;
    gfx::Vector2d m_scrollAmount;
    int m_scrolls;
};

TEST_F(LayerTreeHostTestScrollSimple, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestScrollMultipleRedraw : public LayerTreeHostTest {
public:
    LayerTreeHostTestScrollMultipleRedraw()
        : m_initialScroll(40, 10)
        , m_scrollAmount(-3, 17)
        , m_scrolls(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->rootLayer()->setScrollable(true);
        m_layerTreeHost->rootLayer()->setScrollOffset(m_initialScroll);
        postSetNeedsCommitToMainThread();
    }

    virtual void beginCommitOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        Layer* root = m_layerTreeHost->rootLayer();
        if (!m_layerTreeHost->commitNumber())
            EXPECT_VECTOR_EQ(root->scrollOffset(), m_initialScroll);
        else if (m_layerTreeHost->commitNumber() == 1)
            EXPECT_VECTOR_EQ(root->scrollOffset(), m_initialScroll + m_scrollAmount + m_scrollAmount);
        else if (m_layerTreeHost->commitNumber() == 2)
            EXPECT_VECTOR_EQ(root->scrollOffset(), m_initialScroll + m_scrollAmount + m_scrollAmount);
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        LayerImpl* root = impl->rootLayer();
        root->setScrollable(true);
        root->setMaxScrollOffset(gfx::Vector2d(100, 100));

        if (!impl->activeTree()->source_frame_number() && impl->sourceAnimationFrameNumber() == 1) {
            // First draw after first commit.
            EXPECT_VECTOR_EQ(root->scrollDelta(), gfx::Vector2d());
            root->scrollBy(m_scrollAmount);
            EXPECT_VECTOR_EQ(root->scrollDelta(), m_scrollAmount);

            EXPECT_VECTOR_EQ(root->scrollOffset(), m_initialScroll);
            postSetNeedsRedrawToMainThread();
        } else if (!impl->activeTree()->source_frame_number() && impl->sourceAnimationFrameNumber() == 2) {
            // Second draw after first commit.
            EXPECT_EQ(root->scrollDelta(), m_scrollAmount);
            root->scrollBy(m_scrollAmount);
            EXPECT_VECTOR_EQ(root->scrollDelta(), m_scrollAmount + m_scrollAmount);

            EXPECT_VECTOR_EQ(root->scrollOffset(), m_initialScroll);
            postSetNeedsCommitToMainThread();
        } else if (impl->activeTree()->source_frame_number() == 1) {
            // Third or later draw after second commit.
            EXPECT_GE(impl->sourceAnimationFrameNumber(), 3);
            EXPECT_VECTOR_EQ(root->scrollDelta(), gfx::Vector2d());
            EXPECT_VECTOR_EQ(root->scrollOffset(), m_initialScroll + m_scrollAmount + m_scrollAmount);
            endTest();
        }
    }

    virtual void applyScrollAndScale(gfx::Vector2d scrollDelta, float scale) OVERRIDE
    {
        gfx::Vector2d offset = m_layerTreeHost->rootLayer()->scrollOffset();
        m_layerTreeHost->rootLayer()->setScrollOffset(offset + scrollDelta);
        m_scrolls++;
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(1, m_scrolls);
    }
private:
    gfx::Vector2d m_initialScroll;
    gfx::Vector2d m_scrollAmount;
    int m_scrolls;
};

TEST_F(LayerTreeHostTestScrollMultipleRedraw, runMultiThread)
{
    runTest(true);
}

// This test verifies that properties on the layer tree host are commited to the impl side.
class LayerTreeHostTestCommit : public LayerTreeHostTest {
public:

    LayerTreeHostTestCommit() { }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(gfx::Size(20, 20), gfx::Size(20, 20));
        m_layerTreeHost->setBackgroundColor(SK_ColorGRAY);
        m_layerTreeHost->setPageScaleFactorAndLimits(5, 5, 5);

        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        EXPECT_EQ(gfx::Size(20, 20), impl->layoutViewportSize());
        EXPECT_EQ(SK_ColorGRAY, impl->backgroundColor());
        EXPECT_EQ(5, impl->pageScaleFactor());

        endTest();
    }

    virtual void afterTest() OVERRIDE { }
};

TEST_F(LayerTreeHostTestCommit, runTest)
{
    runTest(true);
}

// Verifies that startPageScaleAnimation events propagate correctly from LayerTreeHost to
// LayerTreeHostImpl in the MT compositor.
class LayerTreeHostTestStartPageScaleAnimation : public LayerTreeHostTest {
public:

    LayerTreeHostTestStartPageScaleAnimation()
        : m_animationRequested(false)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->rootLayer()->setScrollable(true);
        m_layerTreeHost->rootLayer()->setScrollOffset(gfx::Vector2d());
        postSetNeedsCommitToMainThread();
        postSetNeedsRedrawToMainThread();
    }

    void requestStartPageScaleAnimation()
    {
        layerTreeHost()->startPageScaleAnimation(gfx::Vector2d(), false, 1.25, base::TimeDelta());
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        impl->rootLayer()->setScrollable(true);
        impl->rootLayer()->setScrollOffset(gfx::Vector2d());
        impl->setPageScaleFactorAndLimits(impl->pageScaleFactor(), 0.5, 2);

        // We request animation only once.
        if (!m_animationRequested) {
            m_mainThreadProxy->postTask(FROM_HERE, base::Bind(&LayerTreeHostTestStartPageScaleAnimation::requestStartPageScaleAnimation, base::Unretained(this)));
            m_animationRequested = true;
        }
    }

    virtual void applyScrollAndScale(gfx::Vector2d scrollDelta, float scale) OVERRIDE
    {
        gfx::Vector2d offset = m_layerTreeHost->rootLayer()->scrollOffset();
        m_layerTreeHost->rootLayer()->setScrollOffset(offset + scrollDelta);
        m_layerTreeHost->setPageScaleFactorAndLimits(scale, 0.5, 2);
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        impl->processScrollDeltas();
        // We get one commit before the first draw, and the animation doesn't happen until the second draw.
        if (impl->activeTree()->source_frame_number() == 1) {
            EXPECT_EQ(1.25, impl->pageScaleFactor());
            endTest();
        } else
            postSetNeedsRedrawToMainThread();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    bool m_animationRequested;
};

TEST_F(LayerTreeHostTestStartPageScaleAnimation, runTest)
{
    runTest(true);
}

class LayerTreeHostTestSetVisible : public LayerTreeHostTest {
public:

    LayerTreeHostTestSetVisible()
        : m_numDraws(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
        postSetVisibleToMainThread(false);
        postSetNeedsRedrawToMainThread(); // This is suppressed while we're invisible.
        postSetVisibleToMainThread(true); // Triggers the redraw.
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        EXPECT_TRUE(impl->visible());
        ++m_numDraws;
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(1, m_numDraws);
    }

private:
    int m_numDraws;
};

TEST_F(LayerTreeHostTestSetVisible, runMultiThread)
{
    runTest(true);
}

class TestOpacityChangeLayerDelegate : public ContentLayerClient {
public:
    TestOpacityChangeLayerDelegate()
        : m_testLayer(0)
    {
    }

    void setTestLayer(Layer* testLayer)
    {
        m_testLayer = testLayer;
    }

    virtual void paintContents(SkCanvas*, const gfx::Rect&, gfx::RectF&) OVERRIDE
    {
        // Set layer opacity to 0.
        if (m_testLayer)
            m_testLayer->setOpacity(0);
    }

private:
    Layer* m_testLayer;
};

class ContentLayerWithUpdateTracking : public ContentLayer {
public:
    static scoped_refptr<ContentLayerWithUpdateTracking> create(ContentLayerClient* client) { return make_scoped_refptr(new ContentLayerWithUpdateTracking(client)); }

    int paintContentsCount() { return m_paintContentsCount; }
    void resetPaintContentsCount() { m_paintContentsCount = 0; }

    virtual void update(ResourceUpdateQueue& queue, const OcclusionTracker* occlusion, RenderingStats& stats) OVERRIDE
    {
        ContentLayer::update(queue, occlusion, stats);
        m_paintContentsCount++;
    }

private:
    explicit ContentLayerWithUpdateTracking(ContentLayerClient* client)
        : ContentLayer(client)
        , m_paintContentsCount(0)
    {
        setAnchorPoint(gfx::PointF(0, 0));
        setBounds(gfx::Size(10, 10));
        setIsDrawable(true);
    }
    virtual ~ContentLayerWithUpdateTracking()
    {
    }

    int m_paintContentsCount;
};

// Layer opacity change during paint should not prevent compositor resources from being updated during commit.
class LayerTreeHostTestOpacityChange : public LayerTreeHostTest {
public:
    LayerTreeHostTestOpacityChange()
        : m_testOpacityChangeDelegate()
        , m_updateCheckLayer(ContentLayerWithUpdateTracking::create(&m_testOpacityChangeDelegate))
    {
        m_testOpacityChangeDelegate.setTestLayer(m_updateCheckLayer.get());
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
        m_layerTreeHost->rootLayer()->addChild(m_updateCheckLayer);

        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        // update() should have been called once.
        EXPECT_EQ(1, m_updateCheckLayer->paintContentsCount());

        // clear m_updateCheckLayer so LayerTreeHost dies.
        m_updateCheckLayer = NULL;
    }

private:
    TestOpacityChangeLayerDelegate m_testOpacityChangeDelegate;
    scoped_refptr<ContentLayerWithUpdateTracking> m_updateCheckLayer;
};

TEST_F(LayerTreeHostTestOpacityChange, runMultiThread)
{
    runTest(true);
}

class NoScaleContentLayer : public ContentLayer {
public:
    static scoped_refptr<NoScaleContentLayer> create(ContentLayerClient* client) { return make_scoped_refptr(new NoScaleContentLayer(client)); }

    virtual void calculateContentsScale(
        float idealContentsScale,
        float* contentsScaleX,
        float* contentsScaleY,
        gfx::Size* contentBounds) OVERRIDE
    {
        Layer::calculateContentsScale(
             idealContentsScale,
             contentsScaleX,
             contentsScaleY,
             contentBounds);
    }

private:
    explicit NoScaleContentLayer(ContentLayerClient* client)
        : ContentLayer(client) { }
    virtual ~NoScaleContentLayer() { }
};

class LayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers : public LayerTreeHostTest {
public:

    LayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers()
        : m_rootLayer(NoScaleContentLayer::create(&m_client))
        , m_childLayer(ContentLayer::create(&m_client))
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(gfx::Size(40, 40), gfx::Size(60, 60));
        m_layerTreeHost->setDeviceScaleFactor(1.5);
        EXPECT_EQ(gfx::Size(40, 40), m_layerTreeHost->layoutViewportSize());
        EXPECT_EQ(gfx::Size(60, 60), m_layerTreeHost->deviceViewportSize());

        m_rootLayer->addChild(m_childLayer);

        m_rootLayer->setIsDrawable(true);
        m_rootLayer->setBounds(gfx::Size(30, 30));
        m_rootLayer->setAnchorPoint(gfx::PointF(0, 0));

        m_childLayer->setIsDrawable(true);
        m_childLayer->setPosition(gfx::Point(2, 2));
        m_childLayer->setBounds(gfx::Size(10, 10));
        m_childLayer->setAnchorPoint(gfx::PointF(0, 0));

        m_layerTreeHost->setRootLayer(m_rootLayer);

        ASSERT_TRUE(m_layerTreeHost->initializeRendererIfNeeded());
        ResourceUpdateQueue queue;
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        // Get access to protected methods.
        MockLayerTreeHostImpl* mockImpl = static_cast<MockLayerTreeHostImpl*>(impl);

        // Should only do one commit.
        EXPECT_EQ(0, impl->activeTree()->source_frame_number());
        // Device scale factor should come over to impl.
        EXPECT_NEAR(impl->deviceScaleFactor(), 1.5, 0.00001);

        // Both layers are on impl.
        ASSERT_EQ(1u, impl->rootLayer()->children().size());

        // Device viewport is scaled.
        EXPECT_EQ(gfx::Size(40, 40), impl->layoutViewportSize());
        EXPECT_EQ(gfx::Size(60, 60), impl->deviceViewportSize());

        LayerImpl* root = impl->rootLayer();
        LayerImpl* child = impl->rootLayer()->children()[0];

        // Positions remain in layout pixels.
        EXPECT_EQ(gfx::Point(0, 0), root->position());
        EXPECT_EQ(gfx::Point(2, 2), child->position());

        // Compute all the layer transforms for the frame.
        LayerTreeHostImpl::FrameData frameData;
        mockImpl->prepareToDraw(frameData);
        mockImpl->didDrawAllLayers(frameData);

        const MockLayerTreeHostImpl::LayerList& renderSurfaceLayerList =
          *frameData.renderSurfaceLayerList;

        // Both layers should be drawing into the root render surface.
        ASSERT_EQ(1u, renderSurfaceLayerList.size());
        ASSERT_EQ(root->renderSurface(), renderSurfaceLayerList[0]->renderSurface());
        ASSERT_EQ(2u, root->renderSurface()->layerList().size());

        // The root render surface is the size of the viewport.
        EXPECT_RECT_EQ(gfx::Rect(0, 0, 60, 60), root->renderSurface()->contentRect());

        // The content bounds of the child should be scaled.
        gfx::Size childBoundsScaled = gfx::ToCeiledSize(gfx::ScaleSize(child->bounds(), 1.5));
        EXPECT_EQ(childBoundsScaled, child->contentBounds());

        gfx::Transform scaleTransform;
        scaleTransform.Scale(impl->deviceScaleFactor(), impl->deviceScaleFactor());

        // The root layer is scaled by 2x.
        gfx::Transform rootScreenSpaceTransform = scaleTransform;
        gfx::Transform rootDrawTransform = scaleTransform;

        EXPECT_EQ(rootDrawTransform, root->drawTransform());
        EXPECT_EQ(rootScreenSpaceTransform, root->screenSpaceTransform());

        // The child is at position 2,2, which is transformed to 3,3 after the scale
        gfx::Transform childScreenSpaceTransform;
        childScreenSpaceTransform.Translate(3, 3);
        gfx::Transform childDrawTransform = childScreenSpaceTransform;

        EXPECT_TRANSFORMATION_MATRIX_EQ(childDrawTransform, child->drawTransform());
        EXPECT_TRANSFORMATION_MATRIX_EQ(childScreenSpaceTransform, child->screenSpaceTransform());

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        m_rootLayer = NULL;
        m_childLayer = NULL;
    }

private:
    FakeContentLayerClient m_client;
    scoped_refptr<NoScaleContentLayer> m_rootLayer;
    scoped_refptr<ContentLayer> m_childLayer;
};

TEST_F(LayerTreeHostTestDeviceScaleFactorScalesViewportAndLayers, runMultiThread)
{
    runTest(true);
}

// Verify atomicity of commits and reuse of textures.
class LayerTreeHostTestAtomicCommit : public LayerTreeHostTest {
public:
    LayerTreeHostTestAtomicCommit()
        : m_layer(ContentLayerWithUpdateTracking::create(&m_client))
    {
        // Make sure partial texture updates are turned off.
        m_settings.maxPartialTextureUpdates = 0;
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setRootLayer(m_layer);
        m_layerTreeHost->setViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));

        ASSERT_TRUE(m_layerTreeHost->initializeRendererIfNeeded());
        ResourceUpdateQueue queue;
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        FakeWebGraphicsContext3D* context = static_cast<FakeWebGraphicsContext3D*>(impl->outputSurface()->Context3D());

        switch (impl->activeTree()->source_frame_number()) {
        case 0:
            // Number of textures should be one.
            ASSERT_EQ(1, context->NumTextures());
            // Number of textures used for commit should be one.
            EXPECT_EQ(1, context->NumUsedTextures());
            // Verify that used texture is correct.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(0)));

            context->ResetUsedTextures();
            postSetNeedsCommitToMainThread();
            break;
        case 1:
            // Number of textures should be two as the first texture
            // is used by impl thread and cannot by used for update.
            ASSERT_EQ(2, context->NumTextures());
            // Number of textures used for commit should still be one.
            EXPECT_EQ(1, context->NumUsedTextures());
            // First texture should not have been used.
            EXPECT_FALSE(context->UsedTexture(context->TextureAt(0)));
            // New texture should have been used.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(1)));

            context->ResetUsedTextures();
            postSetNeedsCommitToMainThread();
            break;
        case 2:
            endTest();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        FakeWebGraphicsContext3D* context = static_cast<FakeWebGraphicsContext3D*>(impl->outputSurface()->Context3D());

        // Number of textures used for draw should always be one.
        EXPECT_EQ(1, context->NumUsedTextures());
        context->ResetUsedTextures();
    }

    virtual void layout() OVERRIDE
    {
        m_layer->setNeedsDisplay();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    FakeContentLayerClient m_client;
    scoped_refptr<ContentLayerWithUpdateTracking> m_layer;
};

TEST_F(LayerTreeHostTestAtomicCommit, runMultiThread)
{
    runTest(true);
}

static void setLayerPropertiesForTesting(Layer* layer, Layer* parent, const gfx::Transform& transform, const gfx::PointF& anchor, const gfx::PointF& position, const gfx::Size& bounds, bool opaque)
{
    layer->removeAllChildren();
    if (parent)
        parent->addChild(layer);
    layer->setTransform(transform);
    layer->setAnchorPoint(anchor);
    layer->setPosition(position);
    layer->setBounds(bounds);
    layer->setContentsOpaque(opaque);
}

class LayerTreeHostTestAtomicCommitWithPartialUpdate : public LayerTreeHostTest {
public:
    LayerTreeHostTestAtomicCommitWithPartialUpdate()
        : m_parent(ContentLayerWithUpdateTracking::create(&m_client))
        , m_child(ContentLayerWithUpdateTracking::create(&m_client))
        , m_numCommits(0)
    {
        // Allow one partial texture update.
        m_settings.maxPartialTextureUpdates = 1;
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setRootLayer(m_parent);
        m_layerTreeHost->setViewportSize(gfx::Size(10, 20), gfx::Size(10, 20));

        gfx::Transform identityMatrix;
        setLayerPropertiesForTesting(m_parent.get(), 0, identityMatrix, gfx::PointF(0, 0), gfx::PointF(0, 0), gfx::Size(10, 20), true);
        setLayerPropertiesForTesting(m_child.get(), m_parent.get(), identityMatrix, gfx::PointF(0, 0), gfx::PointF(0, 10), gfx::Size(10, 10), false);

        ASSERT_TRUE(m_layerTreeHost->initializeRendererIfNeeded());
        ResourceUpdateQueue queue;
        m_layerTreeHost->updateLayers(queue, std::numeric_limits<size_t>::max());
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        FakeWebGraphicsContext3D* context = static_cast<FakeWebGraphicsContext3D*>(impl->outputSurface()->Context3D());

        switch (impl->activeTree()->source_frame_number()) {
        case 0:
            // Number of textures should be two.
            ASSERT_EQ(2, context->NumTextures());
            // Number of textures used for commit should be two.
            EXPECT_EQ(2, context->NumUsedTextures());
            // Verify that used textures are correct.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(0)));
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(1)));

            context->ResetUsedTextures();
            postSetNeedsCommitToMainThread();
            break;
        case 1:
            // Number of textures used for commit should still be two.
            EXPECT_EQ(2, context->NumUsedTextures());
            // First two textures should not have been used.
            EXPECT_FALSE(context->UsedTexture(context->TextureAt(0)));
            EXPECT_FALSE(context->UsedTexture(context->TextureAt(1)));
            // New textures should have been used.
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(2)));
            EXPECT_TRUE(context->UsedTexture(context->TextureAt(3)));

            context->ResetUsedTextures();
            postSetNeedsCommitToMainThread();
            break;
        case 2:
            // Number of textures used for commit should still be two.
            EXPECT_EQ(2, context->NumUsedTextures());

            context->ResetUsedTextures();
            postSetNeedsCommitToMainThread();
            break;
        case 3:
            // No textures should be used for commit.
            EXPECT_EQ(0, context->NumUsedTextures());

            context->ResetUsedTextures();
            postSetNeedsCommitToMainThread();
            break;
        case 4:
            // Number of textures used for commit should be one.
            EXPECT_EQ(1, context->NumUsedTextures());

            context->ResetUsedTextures();
            postSetNeedsCommitToMainThread();
            break;
        case 5:
            endTest();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        FakeWebGraphicsContext3D* context = static_cast<FakeWebGraphicsContext3D*>(impl->outputSurface()->Context3D());

        // Number of textures used for drawing should two except for frame 4
        // where the viewport only contains one layer.
        if (impl->activeTree()->source_frame_number() == 3)
            EXPECT_EQ(1, context->NumUsedTextures());
        else
            EXPECT_EQ(2, context->NumUsedTextures());

        context->ResetUsedTextures();
    }

    virtual void layout() OVERRIDE
    {
        switch (m_numCommits++) {
        case 0:
        case 1:
            m_parent->setNeedsDisplay();
            m_child->setNeedsDisplay();
            break;
        case 2:
            // Damage part of layers.
            m_parent->setNeedsDisplayRect(gfx::RectF(0, 0, 5, 5));
            m_child->setNeedsDisplayRect(gfx::RectF(0, 0, 5, 5));
            break;
        case 3:
            m_child->setNeedsDisplay();
            m_layerTreeHost->setViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
            break;
        case 4:
            m_layerTreeHost->setViewportSize(gfx::Size(10, 20), gfx::Size(10, 20));
            break;
        case 5:
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    FakeContentLayerClient m_client;
    scoped_refptr<ContentLayerWithUpdateTracking> m_parent;
    scoped_refptr<ContentLayerWithUpdateTracking> m_child;
    int m_numCommits;
};

TEST_F(LayerTreeHostTestAtomicCommitWithPartialUpdate, runMultiThread)
{
    runTest(true);
}

// A loseOutputSurface(1) should lead to a didRecreateOutputSurface(true)
class LayerTreeHostTestSetSingleLostContext : public LayerTreeHostTest {
public:
    LayerTreeHostTestSetSingleLostContext()
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        m_layerTreeHost->loseOutputSurface(1);
    }

    virtual void didRecreateOutputSurface(bool succeeded) OVERRIDE
    {
        EXPECT_TRUE(succeeded);
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

TEST_F(LayerTreeHostTestSetSingleLostContext, runMultiThread)
{
    runTest(true);
}

// A loseOutputSurface(10) should lead to a didRecreateOutputSurface(false), and
// a finishAllRendering() should not hang.
class LayerTreeHostTestSetRepeatedLostContext : public LayerTreeHostTest {
public:
    LayerTreeHostTestSetRepeatedLostContext()
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        m_layerTreeHost->loseOutputSurface(10);
    }

    virtual void didRecreateOutputSurface(bool succeeded) OVERRIDE
    {
        EXPECT_FALSE(succeeded);
        m_layerTreeHost->finishAllRendering();
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

TEST_F(LayerTreeHostTestSetRepeatedLostContext, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestFractionalScroll : public LayerTreeHostTest {
public:
    LayerTreeHostTestFractionalScroll()
        : m_scrollAmount(1.75, 0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->rootLayer()->setScrollable(true);
        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        LayerImpl* root = impl->rootLayer();
        root->setMaxScrollOffset(gfx::Vector2d(100, 100));

        // Check that a fractional scroll delta is correctly accumulated over multiple commits.
        if (!impl->activeTree()->source_frame_number()) {
            EXPECT_VECTOR_EQ(root->scrollOffset(), gfx::Vector2d(0, 0));
            EXPECT_VECTOR_EQ(root->scrollDelta(), gfx::Vector2d(0, 0));
            postSetNeedsCommitToMainThread();
        } else if (impl->activeTree()->source_frame_number() == 1) {
            EXPECT_VECTOR_EQ(root->scrollOffset(), gfx::ToFlooredVector2d(m_scrollAmount));
            EXPECT_VECTOR_EQ(root->scrollDelta(), gfx::Vector2dF(fmod(m_scrollAmount.x(), 1), 0));
            postSetNeedsCommitToMainThread();
        } else if (impl->activeTree()->source_frame_number() == 2) {
            EXPECT_VECTOR_EQ(root->scrollOffset(), gfx::ToFlooredVector2d(m_scrollAmount + m_scrollAmount));
            EXPECT_VECTOR_EQ(root->scrollDelta(), gfx::Vector2dF(fmod(2 * m_scrollAmount.x(), 1), 0));
            endTest();
        }
        root->scrollBy(m_scrollAmount);
    }

    virtual void applyScrollAndScale(gfx::Vector2d scrollDelta, float scale) OVERRIDE
    {
        gfx::Vector2d offset = m_layerTreeHost->rootLayer()->scrollOffset();
        m_layerTreeHost->rootLayer()->setScrollOffset(offset + scrollDelta);
    }

    virtual void afterTest() OVERRIDE
    {
    }
private:
    gfx::Vector2dF m_scrollAmount;
};

TEST_F(LayerTreeHostTestFractionalScroll, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestFinishAllRendering : public LayerTreeHostTest {
public:
    LayerTreeHostTestFinishAllRendering()
        : m_once(false)
        , m_drawCount(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setNeedsRedraw();
        postSetNeedsCommitToMainThread();
    }

    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        if (m_once)
            return;
        m_once = true;
        m_layerTreeHost->setNeedsRedraw();
        m_layerTreeHost->acquireLayerTextures();
        {
            base::AutoLock lock(m_lock);
            m_drawCount = 0;
        }
        m_layerTreeHost->finishAllRendering();
        {
            base::AutoLock lock(m_lock);
            EXPECT_EQ(0, m_drawCount);
        }
        endTest();
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        base::AutoLock lock(m_lock);
        ++m_drawCount;
    }

    virtual void afterTest() OVERRIDE
    {
    }
private:

    bool m_once;
    base::Lock m_lock;
    int m_drawCount;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestFinishAllRendering)

class LayerTreeHostTestScrollChildLayer : public LayerTreeHostTest, public WebKit::WebLayerScrollClient {
public:
    LayerTreeHostTestScrollChildLayer(float deviceScaleFactor)
        : m_deviceScaleFactor(deviceScaleFactor)
        , m_initialScroll(10, 20)
        , m_secondScroll(40, 5)
        , m_scrollAmount(2, -1)
        , m_rootScrolls(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        gfx::Size viewportSize(10, 10);
        gfx::Size deviceViewportSize = gfx::ToCeiledSize(gfx::ScaleSize(viewportSize, m_deviceScaleFactor));
        m_layerTreeHost->setViewportSize(viewportSize, deviceViewportSize);

        m_layerTreeHost->setDeviceScaleFactor(m_deviceScaleFactor);

        m_rootScrollLayer = ContentLayer::create(&m_fakeDelegate);
        m_rootScrollLayer->setBounds(gfx::Size(110, 110));

        m_rootScrollLayer->setPosition(gfx::PointF(0, 0));
        m_rootScrollLayer->setAnchorPoint(gfx::PointF(0, 0));

        m_rootScrollLayer->setIsDrawable(true);
        m_rootScrollLayer->setScrollable(true);
        m_rootScrollLayer->setMaxScrollOffset(gfx::Vector2d(100, 100));
        m_layerTreeHost->rootLayer()->addChild(m_rootScrollLayer);

        m_childLayer = ContentLayer::create(&m_fakeDelegate);
        m_childLayer->setLayerScrollClient(this);
        m_childLayer->setBounds(gfx::Size(110, 110));

        // The scrolls will happen at 5, 5. If they are treated like device pixels, then
        // they will be at 2.5, 2.5 in logical pixels, and will miss this layer.
        m_childLayer->setPosition(gfx::PointF(5, 5));
        m_childLayer->setAnchorPoint(gfx::PointF(0, 0));

        m_childLayer->setIsDrawable(true);
        m_childLayer->setScrollable(true);
        m_childLayer->setMaxScrollOffset(gfx::Vector2d(100, 100));
        m_rootScrollLayer->addChild(m_childLayer);

        m_childLayer->setScrollOffset(m_initialScroll);

        postSetNeedsCommitToMainThread();
    }

    virtual void didScroll() OVERRIDE
    {
        m_finalScrollOffset = m_childLayer->scrollOffset();
    }

    virtual void applyScrollAndScale(gfx::Vector2d scrollDelta, float scale) OVERRIDE
    {
        gfx::Vector2d offset = m_rootScrollLayer->scrollOffset();
        m_rootScrollLayer->setScrollOffset(offset + scrollDelta);
        m_rootScrolls++;
    }

    virtual void layout() OVERRIDE
    {
        EXPECT_VECTOR_EQ(gfx::Vector2d(), m_rootScrollLayer->scrollOffset());

        switch (m_layerTreeHost->commitNumber()) {
        case 0:
            EXPECT_VECTOR_EQ(m_initialScroll, m_childLayer->scrollOffset());
            break;
        case 1:
            EXPECT_VECTOR_EQ(m_initialScroll + m_scrollAmount, m_childLayer->scrollOffset());

            // Pretend like Javascript updated the scroll position itself.
            m_childLayer->setScrollOffset(m_secondScroll);
            break;
        case 2:
            EXPECT_VECTOR_EQ(m_secondScroll + m_scrollAmount, m_childLayer->scrollOffset());
            break;
        }
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        LayerImpl* root = impl->rootLayer();
        LayerImpl* rootScrollLayer = root->children()[0];
        LayerImpl* childLayer = rootScrollLayer->children()[0];

        EXPECT_VECTOR_EQ(root->scrollDelta(), gfx::Vector2d());
        EXPECT_VECTOR_EQ(rootScrollLayer->scrollDelta(), gfx::Vector2d());
        EXPECT_EQ(rootScrollLayer->bounds().width() * m_deviceScaleFactor, rootScrollLayer->contentBounds().width());
        EXPECT_EQ(rootScrollLayer->bounds().height() * m_deviceScaleFactor, rootScrollLayer->contentBounds().height());
        EXPECT_EQ(childLayer->bounds().width() * m_deviceScaleFactor, childLayer->contentBounds().width());
        EXPECT_EQ(childLayer->bounds().height() * m_deviceScaleFactor, childLayer->contentBounds().height());

        switch (impl->activeTree()->source_frame_number()) {
        case 0:
            // Gesture scroll on impl thread.
            EXPECT_EQ(impl->scrollBegin(gfx::Point(5, 5), InputHandlerClient::Gesture), InputHandlerClient::ScrollStarted);
            impl->scrollBy(gfx::Point(), m_scrollAmount);
            impl->scrollEnd();

            EXPECT_VECTOR_EQ(m_initialScroll, childLayer->scrollOffset());
            EXPECT_VECTOR_EQ(m_scrollAmount, childLayer->scrollDelta());
            break;
        case 1:
            // Wheel scroll on impl thread.
            EXPECT_EQ(impl->scrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
            impl->scrollBy(gfx::Point(), m_scrollAmount);
            impl->scrollEnd();

            EXPECT_VECTOR_EQ(m_secondScroll, childLayer->scrollOffset());
            EXPECT_VECTOR_EQ(m_scrollAmount, childLayer->scrollDelta());
            break;
        case 2:
            EXPECT_VECTOR_EQ(m_secondScroll + m_scrollAmount, childLayer->scrollOffset());
            EXPECT_VECTOR_EQ(gfx::Vector2d(0, 0), childLayer->scrollDelta());

            endTest();
        }
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(0, m_rootScrolls);
        EXPECT_VECTOR_EQ(m_secondScroll + m_scrollAmount, m_finalScrollOffset);
    }

private:
    float m_deviceScaleFactor;
    gfx::Vector2d m_initialScroll;
    gfx::Vector2d m_secondScroll;
    gfx::Vector2d m_scrollAmount;
    int m_rootScrolls;
    gfx::Vector2d m_finalScrollOffset;

    FakeContentLayerClient m_fakeDelegate;
    scoped_refptr<Layer> m_rootScrollLayer;
    scoped_refptr<Layer> m_childLayer;
};

class LayerTreeHostTestScrollChildLayerNormalDpi : public LayerTreeHostTestScrollChildLayer {
public:
    LayerTreeHostTestScrollChildLayerNormalDpi() : LayerTreeHostTestScrollChildLayer(1) { }
};

TEST_F(LayerTreeHostTestScrollChildLayerNormalDpi, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestScrollChildLayerHighDpi : public LayerTreeHostTestScrollChildLayer {
public:
    LayerTreeHostTestScrollChildLayerHighDpi() : LayerTreeHostTestScrollChildLayer(2) { }
};

TEST_F(LayerTreeHostTestScrollChildLayerHighDpi, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestScrollRootScrollLayer : public LayerTreeHostTest {
public:
    LayerTreeHostTestScrollRootScrollLayer(float deviceScaleFactor)
        : m_deviceScaleFactor(deviceScaleFactor)
        , m_initialScroll(10, 20)
        , m_secondScroll(40, 5)
        , m_scrollAmount(2, -1)
        , m_rootScrolls(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        gfx::Size viewportSize(10, 10);
        gfx::Size deviceViewportSize = gfx::ToCeiledSize(gfx::ScaleSize(viewportSize, m_deviceScaleFactor));
        m_layerTreeHost->setViewportSize(viewportSize, deviceViewportSize);

        m_layerTreeHost->setDeviceScaleFactor(m_deviceScaleFactor);

        m_rootScrollLayer = ContentLayer::create(&m_fakeDelegate);
        m_rootScrollLayer->setBounds(gfx::Size(110, 110));

        m_rootScrollLayer->setPosition(gfx::PointF(0, 0));
        m_rootScrollLayer->setAnchorPoint(gfx::PointF(0, 0));

        m_rootScrollLayer->setIsDrawable(true);
        m_rootScrollLayer->setScrollable(true);
        m_rootScrollLayer->setMaxScrollOffset(gfx::Vector2d(100, 100));
        m_layerTreeHost->rootLayer()->addChild(m_rootScrollLayer);

        m_rootScrollLayer->setScrollOffset(m_initialScroll);

        postSetNeedsCommitToMainThread();
    }

    virtual void applyScrollAndScale(gfx::Vector2d scrollDelta, float scale) OVERRIDE
    {
        gfx::Vector2d offset = m_rootScrollLayer->scrollOffset();
        m_rootScrollLayer->setScrollOffset(offset + scrollDelta);
        m_rootScrolls++;
    }

    virtual void layout() OVERRIDE
    {
        switch (m_layerTreeHost->commitNumber()) {
        case 0:
            EXPECT_VECTOR_EQ(m_initialScroll, m_rootScrollLayer->scrollOffset());
            break;
        case 1:
            EXPECT_VECTOR_EQ(m_initialScroll + m_scrollAmount, m_rootScrollLayer->scrollOffset());

            // Pretend like Javascript updated the scroll position itself.
            m_rootScrollLayer->setScrollOffset(m_secondScroll);
            break;
        case 2:
            EXPECT_VECTOR_EQ(m_secondScroll + m_scrollAmount, m_rootScrollLayer->scrollOffset());
            break;
        }
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        LayerImpl* root = impl->rootLayer();
        LayerImpl* rootScrollLayer = root->children()[0];

        EXPECT_VECTOR_EQ(root->scrollDelta(), gfx::Vector2d());
        EXPECT_EQ(rootScrollLayer->bounds().width() * m_deviceScaleFactor, rootScrollLayer->contentBounds().width());
        EXPECT_EQ(rootScrollLayer->bounds().height() * m_deviceScaleFactor, rootScrollLayer->contentBounds().height());

        switch (impl->activeTree()->source_frame_number()) {
        case 0:
            // Gesture scroll on impl thread.
            EXPECT_EQ(impl->scrollBegin(gfx::Point(5, 5), InputHandlerClient::Gesture), InputHandlerClient::ScrollStarted);
            impl->scrollBy(gfx::Point(), m_scrollAmount);
            impl->scrollEnd();

            EXPECT_VECTOR_EQ(m_initialScroll, rootScrollLayer->scrollOffset());
            EXPECT_VECTOR_EQ(m_scrollAmount, rootScrollLayer->scrollDelta());
            break;
        case 1:
            // Wheel scroll on impl thread.
            EXPECT_EQ(impl->scrollBegin(gfx::Point(5, 5), InputHandlerClient::Wheel), InputHandlerClient::ScrollStarted);
            impl->scrollBy(gfx::Point(), m_scrollAmount);
            impl->scrollEnd();

            EXPECT_VECTOR_EQ(m_secondScroll, rootScrollLayer->scrollOffset());
            EXPECT_VECTOR_EQ(m_scrollAmount, rootScrollLayer->scrollDelta());
            break;
        case 2:
            EXPECT_VECTOR_EQ(m_secondScroll + m_scrollAmount, rootScrollLayer->scrollOffset());
            EXPECT_VECTOR_EQ(gfx::Vector2d(0, 0), rootScrollLayer->scrollDelta());

            endTest();
        }
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(2, m_rootScrolls);
    }

private:
    float m_deviceScaleFactor;
    gfx::Vector2d m_initialScroll;
    gfx::Vector2d m_secondScroll;
    gfx::Vector2d m_scrollAmount;
    int m_rootScrolls;

    FakeContentLayerClient m_fakeDelegate;
    scoped_refptr<Layer> m_rootScrollLayer;
};

class LayerTreeHostTestScrollRootScrollLayerNormalDpi : public LayerTreeHostTestScrollRootScrollLayer {
public:
    LayerTreeHostTestScrollRootScrollLayerNormalDpi() : LayerTreeHostTestScrollRootScrollLayer(1) { }
};

TEST_F(LayerTreeHostTestScrollRootScrollLayerNormalDpi, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestScrollRootScrollLayerHighDpi : public LayerTreeHostTestScrollRootScrollLayer {
public:
    LayerTreeHostTestScrollRootScrollLayerHighDpi() : LayerTreeHostTestScrollRootScrollLayer(2) { }
};

TEST_F(LayerTreeHostTestScrollRootScrollLayerHighDpi, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestCompositeAndReadbackCleanup : public LayerTreeHostTest {
public:
    LayerTreeHostTestCompositeAndReadbackCleanup() { }

    virtual void beginTest() OVERRIDE
    {
        Layer* rootLayer = m_layerTreeHost->rootLayer();

        char pixels[4];
        m_layerTreeHost->compositeAndReadback(static_cast<void*>(&pixels), gfx::Rect(0, 0, 1, 1));
        EXPECT_FALSE(rootLayer->renderSurface());

        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestCompositeAndReadbackCleanup)

class LayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit : public LayerTreeHostTest {
public:
    LayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit()
        : m_rootLayer(ContentLayerWithUpdateTracking::create(&m_fakeDelegate))
        , m_surfaceLayer1(ContentLayerWithUpdateTracking::create(&m_fakeDelegate))
        , m_replicaLayer1(ContentLayerWithUpdateTracking::create(&m_fakeDelegate))
        , m_surfaceLayer2(ContentLayerWithUpdateTracking::create(&m_fakeDelegate))
        , m_replicaLayer2(ContentLayerWithUpdateTracking::create(&m_fakeDelegate))
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(gfx::Size(100, 100), gfx::Size(100, 100));

        m_rootLayer->setBounds(gfx::Size(100, 100));
        m_surfaceLayer1->setBounds(gfx::Size(100, 100));
        m_surfaceLayer1->setForceRenderSurface(true);
        m_surfaceLayer1->setOpacity(0.5);
        m_surfaceLayer2->setBounds(gfx::Size(100, 100));
        m_surfaceLayer2->setForceRenderSurface(true);
        m_surfaceLayer2->setOpacity(0.5);

        m_surfaceLayer1->setReplicaLayer(m_replicaLayer1.get());
        m_surfaceLayer2->setReplicaLayer(m_replicaLayer2.get());

        m_rootLayer->addChild(m_surfaceLayer1);
        m_surfaceLayer1->addChild(m_surfaceLayer2);
        m_layerTreeHost->setRootLayer(m_rootLayer);

        postSetNeedsCommitToMainThread();
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* hostImpl) OVERRIDE
    {
        Renderer* renderer = hostImpl->renderer();
        RenderPass::Id surface1RenderPassId = hostImpl->rootLayer()->children()[0]->renderSurface()->renderPassId();
        RenderPass::Id surface2RenderPassId = hostImpl->rootLayer()->children()[0]->children()[0]->renderSurface()->renderPassId();

        switch (hostImpl->activeTree()->source_frame_number()) {
        case 0:
            EXPECT_TRUE(renderer->haveCachedResourcesForRenderPassId(surface1RenderPassId));
            EXPECT_TRUE(renderer->haveCachedResourcesForRenderPassId(surface2RenderPassId));

            // Reduce the memory limit to only fit the root layer and one render surface. This
            // prevents any contents drawing into surfaces from being allocated.
            hostImpl->setManagedMemoryPolicy(ManagedMemoryPolicy(100 * 100 * 4 * 2));
            break;
        case 1:
            EXPECT_FALSE(renderer->haveCachedResourcesForRenderPassId(surface1RenderPassId));
            EXPECT_FALSE(renderer->haveCachedResourcesForRenderPassId(surface2RenderPassId));

            endTest();
            break;
        }
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(2, m_rootLayer->paintContentsCount());
        EXPECT_EQ(2, m_surfaceLayer1->paintContentsCount());
        EXPECT_EQ(2, m_surfaceLayer2->paintContentsCount());

        // Clear layer references so LayerTreeHost dies.
        m_rootLayer = NULL;
        m_surfaceLayer1 = NULL;
        m_replicaLayer1 = NULL;
        m_surfaceLayer2 = NULL;
        m_replicaLayer2 = NULL;
    }

private:
    FakeContentLayerClient m_fakeDelegate;
    scoped_refptr<ContentLayerWithUpdateTracking> m_rootLayer;
    scoped_refptr<ContentLayerWithUpdateTracking> m_surfaceLayer1;
    scoped_refptr<ContentLayerWithUpdateTracking> m_replicaLayer1;
    scoped_refptr<ContentLayerWithUpdateTracking> m_surfaceLayer2;
    scoped_refptr<ContentLayerWithUpdateTracking> m_replicaLayer2;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestSurfaceNotAllocatedForLayersOutsideMemoryLimit)

class EvictionTestLayer : public Layer {
public:
    static scoped_refptr<EvictionTestLayer> create() { return make_scoped_refptr(new EvictionTestLayer()); }

    virtual void update(ResourceUpdateQueue&, const OcclusionTracker*, RenderingStats&) OVERRIDE;
    virtual bool drawsContent() const OVERRIDE { return true; }

    virtual scoped_ptr<LayerImpl> createLayerImpl(LayerTreeImpl* treeImpl) OVERRIDE;
    virtual void pushPropertiesTo(LayerImpl*) OVERRIDE;
    virtual void setTexturePriorities(const PriorityCalculator&) OVERRIDE;

    bool haveBackingTexture() const { return m_texture.get() ? m_texture->haveBackingTexture() : false; }

private:
    EvictionTestLayer() : Layer() { }
    virtual ~EvictionTestLayer() { }

    void createTextureIfNeeded()
    {
        if (m_texture.get())
            return;
        m_texture = PrioritizedResource::create(layerTreeHost()->contentsTextureManager());
        m_texture->setDimensions(gfx::Size(10, 10), GL_RGBA);
        m_bitmap.setConfig(SkBitmap::kARGB_8888_Config, 10, 10);
    }

    scoped_ptr<PrioritizedResource> m_texture;
    SkBitmap m_bitmap;
};

class EvictionTestLayerImpl : public LayerImpl {
public:
    static scoped_ptr<EvictionTestLayerImpl> create(LayerTreeImpl* treeImpl, int id)
    {
        return make_scoped_ptr(new EvictionTestLayerImpl(treeImpl, id));
    }
    virtual ~EvictionTestLayerImpl() { }

    virtual void appendQuads(QuadSink& quadSink, AppendQuadsData&) OVERRIDE
    {
        ASSERT_TRUE(m_hasTexture);
        ASSERT_NE(0u, layerTreeImpl()->resource_provider()->numResources());
    }

    void setHasTexture(bool hasTexture) { m_hasTexture = hasTexture; }

private:
    EvictionTestLayerImpl(LayerTreeImpl* treeImpl, int id)
        : LayerImpl(treeImpl, id)
        , m_hasTexture(false) { }

    bool m_hasTexture;
};

void EvictionTestLayer::setTexturePriorities(const PriorityCalculator&)
{
    createTextureIfNeeded();
    if (!m_texture.get())
        return;
    m_texture->setRequestPriority(PriorityCalculator::uiPriority(true));
}

void EvictionTestLayer::update(ResourceUpdateQueue& queue, const OcclusionTracker*, RenderingStats&)
{
    createTextureIfNeeded();
    if (!m_texture.get())
        return;

    gfx::Rect fullRect(0, 0, 10, 10);
    ResourceUpdate upload = ResourceUpdate::Create(
        m_texture.get(), &m_bitmap, fullRect, fullRect, gfx::Vector2d());
    queue.appendFullUpload(upload);
}

scoped_ptr<LayerImpl> EvictionTestLayer::createLayerImpl(LayerTreeImpl* treeImpl)
{
    return EvictionTestLayerImpl::create(treeImpl, m_layerId).PassAs<LayerImpl>();
}

void EvictionTestLayer::pushPropertiesTo(LayerImpl* layerImpl)
{
    Layer::pushPropertiesTo(layerImpl);

    EvictionTestLayerImpl* testLayerImpl = static_cast<EvictionTestLayerImpl*>(layerImpl);
    testLayerImpl->setHasTexture(m_texture->haveBackingTexture());
}

class LayerTreeHostTestEvictTextures : public LayerTreeHostTest {
public:
    LayerTreeHostTestEvictTextures()
        : m_layer(EvictionTestLayer::create())
        , m_implForEvictTextures(0)
        , m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setRootLayer(m_layer);
        m_layerTreeHost->setViewportSize(gfx::Size(10, 20), gfx::Size(10, 20));

        gfx::Transform identityMatrix;
        setLayerPropertiesForTesting(m_layer.get(), 0, identityMatrix, gfx::PointF(0, 0), gfx::PointF(0, 0), gfx::Size(10, 20), true);

        postSetNeedsCommitToMainThread();
    }

    void postEvictTextures()
    {
        DCHECK(implThread());
        implThread()->postTask(base::Bind(&LayerTreeHostTestEvictTextures::evictTexturesOnImplThread,
                               base::Unretained(this)));
    }

    void evictTexturesOnImplThread()
    {
        DCHECK(m_implForEvictTextures);
        m_implForEvictTextures->enforceManagedMemoryPolicy(ManagedMemoryPolicy(0));
    }

    // Commit 1: Just commit and draw normally, then post an eviction at the end
    // that will trigger a commit.
    // Commit 2: Triggered by the eviction, let it go through and then set
    // needsCommit.
    // Commit 3: Triggered by the setNeedsCommit. In layout(), post an eviction
    // task, which will be handled before the commit. Don't set needsCommit, it
    // should have been posted. A frame should not be drawn (note,
    // didCommitAndDrawFrame may be called anyway).
    // Commit 4: Triggered by the eviction, let it go through and then set
    // needsCommit.
    // Commit 5: Triggered by the setNeedsCommit, post an eviction task in
    // layout(), a frame should not be drawn but a commit will be posted.
    // Commit 6: Triggered by the eviction, post an eviction task in
    // layout(), which will be a noop, letting the commit (which recreates the
    // textures) go through and draw a frame, then end the test.
    //
    // Commits 1+2 test the eviction recovery path where eviction happens outside
    // of the beginFrame/commit pair.
    // Commits 3+4 test the eviction recovery path where eviction happens inside
    // the beginFrame/commit pair.
    // Commits 5+6 test the path where an eviction happens during the eviction
    // recovery path.
    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        switch (m_numCommits) {
        case 1:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            postEvictTextures();
            break;
        case 2:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            m_layerTreeHost->setNeedsCommit();
            break;
        case 3:
            break;
        case 4:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            m_layerTreeHost->setNeedsCommit();
            break;
        case 5:
            break;
        case 6:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            endTest();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        m_implForEvictTextures = impl;
    }

    virtual void layout() OVERRIDE
    {
        ++m_numCommits;
        switch (m_numCommits) {
        case 1:
        case 2:
            break;
        case 3:
            postEvictTextures();
            break;
        case 4:
            // We couldn't check in didCommitAndDrawFrame on commit 3, so check here.
            EXPECT_FALSE(m_layer->haveBackingTexture());
            break;
        case 5:
            postEvictTextures();
            break;
        case 6:
            // We couldn't check in didCommitAndDrawFrame on commit 5, so check here.
            EXPECT_FALSE(m_layer->haveBackingTexture());
            postEvictTextures();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    FakeContentLayerClient m_client;
    scoped_refptr<EvictionTestLayer> m_layer;
    LayerTreeHostImpl* m_implForEvictTextures;
    int m_numCommits;
};

TEST_F(LayerTreeHostTestEvictTextures, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestLostContextAfterEvictTextures : public LayerTreeHostTest {
public:
    LayerTreeHostTestLostContextAfterEvictTextures()
        : m_layer(EvictionTestLayer::create())
        , m_implForEvictTextures(0)
        , m_numCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setRootLayer(m_layer);
        m_layerTreeHost->setViewportSize(gfx::Size(10, 20), gfx::Size(10, 20));

        gfx::Transform identityMatrix;
        setLayerPropertiesForTesting(m_layer.get(), 0, identityMatrix, gfx::PointF(0, 0), gfx::PointF(0, 0), gfx::Size(10, 20), true);

        postSetNeedsCommitToMainThread();
    }

    void postEvictTextures()
    {
        if (implThread()) {
            implThread()->postTask(base::Bind(&LayerTreeHostTestLostContextAfterEvictTextures::evictTexturesOnImplThread,
                                              base::Unretained(this)));
        } else {
            DebugScopedSetImplThread impl(proxy());
            evictTexturesOnImplThread();
        }
    }

    void evictTexturesOnImplThread()
    {
        DCHECK(m_implForEvictTextures);
        m_implForEvictTextures->enforceManagedMemoryPolicy(ManagedMemoryPolicy(0));
    }

    // Commit 1: Just commit and draw normally, then at the end, set ourselves
    // invisible (to prevent a commit that would recreate textures after
    // eviction, before the context recovery), and post a task that will evict
    // textures, then cause the context to be lost, and then set ourselves
    // visible again (to allow commits, since that's what causes context
    // recovery in single thread).
    virtual void didCommitAndDrawFrame() OVERRIDE
    {
        ++m_numCommits;
        switch (m_numCommits) {
        case 1:
            EXPECT_TRUE(m_layer->haveBackingTexture());
            m_layerTreeHost->setVisible(false);
            postEvictTextures();
            m_layerTreeHost->loseOutputSurface(1);
            m_layerTreeHost->setVisible(true);
            break;
        default:
            break;
        }
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        m_implForEvictTextures = impl;
    }

    virtual void didRecreateOutputSurface(bool succeeded) OVERRIDE
    {
        EXPECT_TRUE(succeeded);
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }

private:
    FakeContentLayerClient m_client;
    scoped_refptr<EvictionTestLayer> m_layer;
    LayerTreeHostImpl* m_implForEvictTextures;
    int m_numCommits;
};

SINGLE_AND_MULTI_THREAD_TEST_F(LayerTreeHostTestLostContextAfterEvictTextures)

class FakeWebGraphicsContext3DWithEndQueryCausingLostContext : public FakeWebGraphicsContext3D {
public:
    static scoped_ptr<FakeWebGraphicsContext3DWithEndQueryCausingLostContext> create(Attributes attrs)
    {
        return make_scoped_ptr(new FakeWebGraphicsContext3DWithEndQueryCausingLostContext(attrs));
    }

    virtual void setContextLostCallback(WebGraphicsContextLostCallback* callback) { m_contextLostCallback = callback; }
    virtual bool isContextLost() { return m_isContextLost; }

    virtual void beginQueryEXT(WebKit::WGC3Denum, WebKit::WebGLId) { }
    virtual void endQueryEXT(WebKit::WGC3Denum)
    {
        // Lose context.
        if (!m_isContextLost) {
            m_contextLostCallback->onContextLost();
            m_isContextLost = true;
        }
    }
    virtual void getQueryObjectuivEXT(WebKit::WebGLId, WebKit::WGC3Denum pname, WebKit::WGC3Duint* params)
    {
        // Context is lost. We need to behave as if result is available.
        if (pname == GL_QUERY_RESULT_AVAILABLE_EXT)
            *params = 1;
    }

private:
    explicit FakeWebGraphicsContext3DWithEndQueryCausingLostContext(Attributes attrs)
        : FakeWebGraphicsContext3D(attrs)
        , m_contextLostCallback(0)
        , m_isContextLost(false) { }

    WebGraphicsContextLostCallback* m_contextLostCallback;
    bool m_isContextLost;
};

class LayerTreeHostTestLostContextWhileUpdatingResources : public LayerTreeHostTest {
public:
    LayerTreeHostTestLostContextWhileUpdatingResources()
        : m_parent(ContentLayerWithUpdateTracking::create(&m_client))
        , m_numChildren(50)
    {
        for (int i = 0; i < m_numChildren; i++)
            m_children.push_back(ContentLayerWithUpdateTracking::create(&m_client));
    }

    virtual void beginTest()
    {
        m_layerTreeHost->setRootLayer(m_parent);
        m_layerTreeHost->setViewportSize(gfx::Size(m_numChildren, 1), gfx::Size(m_numChildren, 1));

        gfx::Transform identityMatrix;
        setLayerPropertiesForTesting(m_parent.get(), 0, identityMatrix, gfx::PointF(0, 0), gfx::PointF(0, 0), gfx::Size(m_numChildren, 1), true);
        for (int i = 0; i < m_numChildren; i++)
            setLayerPropertiesForTesting(m_children[i].get(), m_parent.get(), identityMatrix, gfx::PointF(0, 0), gfx::PointF(i, 0), gfx::Size(1, 1), false);

        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl* impl)
    {
        endTest();
    }

    virtual void layout()
    {
        m_parent->setNeedsDisplay();
        for (int i = 0; i < m_numChildren; i++)
            m_children[i]->setNeedsDisplay();
    }

    virtual void afterTest()
    {
    }

private:
    FakeContentLayerClient m_client;
    scoped_refptr<ContentLayerWithUpdateTracking> m_parent;
    int m_numChildren;
    std::vector<scoped_refptr<ContentLayerWithUpdateTracking> > m_children;
};

TEST_F(LayerTreeHostTestLostContextWhileUpdatingResources, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestContinuousCommit : public LayerTreeHostTest {
public:
    LayerTreeHostTestContinuousCommit()
        : m_numCommitComplete(0)
        , m_numDrawLayers(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
        m_layerTreeHost->rootLayer()->setBounds(gfx::Size(10, 10));

        postSetNeedsCommitToMainThread();
    }

    virtual void didCommit() OVERRIDE
    {
        if (m_numDrawLayers == 2)
            return;
        postSetNeedsCommitToMainThread();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        if (m_numDrawLayers == 1)
            m_numCommitComplete++;
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        m_numDrawLayers++;
        if (m_numDrawLayers == 2)
            endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        // Check that we didn't commit twice between first and second draw.
        EXPECT_EQ(1, m_numCommitComplete);
    }

private:
    int m_numCommitComplete;
    int m_numDrawLayers;
};

TEST_F(LayerTreeHostTestContinuousCommit, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestContinuousInvalidate : public LayerTreeHostTest {
public:
    LayerTreeHostTestContinuousInvalidate()
        : m_numCommitComplete(0)
        , m_numDrawLayers(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        m_layerTreeHost->setViewportSize(gfx::Size(10, 10), gfx::Size(10, 10));
        m_layerTreeHost->rootLayer()->setBounds(gfx::Size(10, 10));

        m_contentLayer = ContentLayer::create(&m_fakeDelegate);
        m_contentLayer->setBounds(gfx::Size(10, 10));
        m_contentLayer->setPosition(gfx::PointF(0, 0));
        m_contentLayer->setAnchorPoint(gfx::PointF(0, 0));
        m_contentLayer->setIsDrawable(true);
        m_layerTreeHost->rootLayer()->addChild(m_contentLayer);

        postSetNeedsCommitToMainThread();
    }

    virtual void didCommit() OVERRIDE
    {
        if (m_numDrawLayers == 2)
            return;
        m_contentLayer->setNeedsDisplay();
    }

    virtual void commitCompleteOnThread(LayerTreeHostImpl*) OVERRIDE
    {
        if (m_numDrawLayers == 1)
            m_numCommitComplete++;
    }

    virtual void drawLayersOnThread(LayerTreeHostImpl* impl) OVERRIDE
    {
        m_numDrawLayers++;
        if (m_numDrawLayers == 2)
            endTest();
    }

    virtual void afterTest() OVERRIDE
    {
        // Check that we didn't commit twice between first and second draw.
        EXPECT_EQ(1, m_numCommitComplete);

        // Clear layer references so LayerTreeHost dies.
        m_contentLayer = NULL;
    }

private:
    FakeContentLayerClient m_fakeDelegate;
    scoped_refptr<Layer> m_contentLayer;
    int m_numCommitComplete;
    int m_numDrawLayers;
};

TEST_F(LayerTreeHostTestContinuousInvalidate, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestAdjustPointForZoom : public LayerTreeHostTest {
public:
    LayerTreeHostTestAdjustPointForZoom()
    {
    }

    virtual void beginTest() OVERRIDE
    {
        gfx::Transform m;
        m.Translate(250, 360);
        m.Scale(2, 2);

        gfx::Point point(400, 550);
        gfx::Point transformedPoint;

        // Unit transform, no change expected.
        m_layerTreeHost->setImplTransform(gfx::Transform());
        transformedPoint = gfx::ToRoundedPoint(m_layerTreeHost->adjustEventPointForPinchZoom(point));
        EXPECT_EQ(point.x(), transformedPoint.x());
        EXPECT_EQ(point.y(), transformedPoint.y());

        m_layerTreeHost->setImplTransform(m);

        // Apply m^(-1): 75 = (400 - 250) / 2; 95 = (550 - 360) / 2.
        transformedPoint = gfx::ToRoundedPoint(m_layerTreeHost->adjustEventPointForPinchZoom(point));
        EXPECT_EQ(75, transformedPoint.x());
        EXPECT_EQ(95, transformedPoint.y());
        endTest();
    }

    virtual void afterTest() OVERRIDE
    {
    }
};

TEST_F(LayerTreeHostTestAdjustPointForZoom, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostTestDeferCommits : public LayerTreeHostTest {
public:
    LayerTreeHostTestDeferCommits()
        : m_numCommitsDeferred(0)
        , m_numCompleteCommits(0)
    {
    }

    virtual void beginTest() OVERRIDE
    {
        postSetNeedsCommitToMainThread();
    }

    virtual void didDeferCommit() OVERRIDE
    {
        m_numCommitsDeferred++;
        m_layerTreeHost->setDeferCommits(false);
    }

    virtual void didCommit() OVERRIDE
    {
        m_numCompleteCommits++;
        switch (m_numCompleteCommits) {
        case 1:
            EXPECT_EQ(0, m_numCommitsDeferred);
            m_layerTreeHost->setDeferCommits(true);
            postSetNeedsCommitToMainThread();
            break;
        case 2:
            endTest();
            break;
        default:
            NOTREACHED();
            break;
        }
    }

    virtual void afterTest() OVERRIDE
    {
        EXPECT_EQ(1, m_numCommitsDeferred);
        EXPECT_EQ(2, m_numCompleteCommits);
    }

private:
    int m_numCommitsDeferred;
    int m_numCompleteCommits;
};

TEST_F(LayerTreeHostTestDeferCommits, runMultiThread)
{
    runTest(true);
}

class LayerTreeHostWithProxy : public LayerTreeHost {
public:
    LayerTreeHostWithProxy(FakeLayerImplTreeHostClient* client, const LayerTreeSettings& settings, scoped_ptr<Proxy> proxy)
            : LayerTreeHost(client, settings)
    {
        EXPECT_TRUE(initializeForTesting(proxy.Pass()));
    }

private:
    FakeLayerImplTreeHostClient m_client;
};

TEST(LayerTreeHostTest, LimitPartialUpdates)
{
    // When partial updates are not allowed, max updates should be 0.
    {
        FakeLayerImplTreeHostClient client;

        scoped_ptr<FakeProxy> proxy = make_scoped_ptr(new FakeProxy(scoped_ptr<Thread>()));
        proxy->rendererCapabilities().allowPartialTextureUpdates = false;
        proxy->setMaxPartialTextureUpdates(5);

        LayerTreeSettings settings;
        settings.maxPartialTextureUpdates = 10;

        LayerTreeHostWithProxy host(&client, settings, proxy.PassAs<Proxy>());
        EXPECT_TRUE(host.initializeRendererIfNeeded());

        EXPECT_EQ(0u, host.settings().maxPartialTextureUpdates);
    }

    // When partial updates are allowed, max updates should be limited by the proxy.
    {
        FakeLayerImplTreeHostClient client;

        scoped_ptr<FakeProxy> proxy = make_scoped_ptr(new FakeProxy(scoped_ptr<Thread>()));
        proxy->rendererCapabilities().allowPartialTextureUpdates = true;
        proxy->setMaxPartialTextureUpdates(5);

        LayerTreeSettings settings;
        settings.maxPartialTextureUpdates = 10;

        LayerTreeHostWithProxy host(&client, settings, proxy.PassAs<Proxy>());
        EXPECT_TRUE(host.initializeRendererIfNeeded());

        EXPECT_EQ(5u, host.settings().maxPartialTextureUpdates);
    }

    // When partial updates are allowed, max updates should also be limited by the settings.
    {
        FakeLayerImplTreeHostClient client;

        scoped_ptr<FakeProxy> proxy = make_scoped_ptr(new FakeProxy(scoped_ptr<Thread>()));
        proxy->rendererCapabilities().allowPartialTextureUpdates = true;
        proxy->setMaxPartialTextureUpdates(20);

        LayerTreeSettings settings;
        settings.maxPartialTextureUpdates = 10;

        LayerTreeHostWithProxy host(&client, settings, proxy.PassAs<Proxy>());
        EXPECT_TRUE(host.initializeRendererIfNeeded());

        EXPECT_EQ(10u, host.settings().maxPartialTextureUpdates);
    }
}

TEST(LayerTreeHostTest, PartialUpdatesWithGLRenderer)
{
    bool useSoftwareRendering = false;
    bool useDelegatingRenderer = false;
    FakeLayerImplTreeHostClient client(useSoftwareRendering, useDelegatingRenderer);

    LayerTreeSettings settings;
    settings.maxPartialTextureUpdates = 4;

    scoped_ptr<LayerTreeHost> host = LayerTreeHost::create(&client, settings, scoped_ptr<Thread>());
    EXPECT_TRUE(host->initializeRendererIfNeeded());
    EXPECT_EQ(4u, host->settings().maxPartialTextureUpdates);
}

TEST(LayerTreeHostTest, PartialUpdatesWithSoftwareRenderer)
{
    bool useSoftwareRendering = true;
    bool useDelegatingRenderer = false;
    FakeLayerImplTreeHostClient client(useSoftwareRendering, useDelegatingRenderer);

    LayerTreeSettings settings;
    settings.maxPartialTextureUpdates = 4;

    scoped_ptr<LayerTreeHost> host = LayerTreeHost::create(&client, settings, scoped_ptr<Thread>());
    EXPECT_TRUE(host->initializeRendererIfNeeded());
    EXPECT_EQ(4u, host->settings().maxPartialTextureUpdates);
}

TEST(LayerTreeHostTest, PartialUpdatesWithDelegatingRendererAndGLContent)
{
    bool useSoftwareRendering = false;
    bool useDelegatingRenderer = true;
    FakeLayerImplTreeHostClient client(useSoftwareRendering, useDelegatingRenderer);

    LayerTreeSettings settings;
    settings.maxPartialTextureUpdates = 4;

    scoped_ptr<LayerTreeHost> host = LayerTreeHost::create(&client, settings, scoped_ptr<Thread>());
    EXPECT_TRUE(host->initializeRendererIfNeeded());
    EXPECT_EQ(0u, host->settings().maxPartialTextureUpdates);
}

TEST(LayerTreeHostTest, PartialUpdatesWithDelegatingRendererAndSoftwareContent)
{
    bool useSoftwareRendering = true;
    bool useDelegatingRenderer = true;
    FakeLayerImplTreeHostClient client(useSoftwareRendering, useDelegatingRenderer);

    LayerTreeSettings settings;
    settings.maxPartialTextureUpdates = 4;

    scoped_ptr<LayerTreeHost> host = LayerTreeHost::create(&client, settings, scoped_ptr<Thread>());
    EXPECT_TRUE(host->initializeRendererIfNeeded());
    EXPECT_EQ(0u, host->settings().maxPartialTextureUpdates);
}

}  // namespace
}  // namespace cc
