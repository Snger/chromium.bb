// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "cc/layer.h"

#include "CCKeyframedAnimationCurve.h"
#include "CCLayerImpl.h"
#include "CCLayerTreeHost.h"
#include "CCSingleThreadProxy.h"
#include "cc/layer_painter.h"
#include "cc/test/fake_layer_tree_host_client.h"
#include "cc/test/geometry_test_utils.h"
#include "cc/test/web_compositor_initializer.h"
#include "cc/test/test_common.h"
#include "cc/settings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include <public/WebTransformationMatrix.h>

using namespace cc;
using namespace WebKitTests;
using WebKit::WebTransformationMatrix;
using ::testing::Mock;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AnyNumber;

#define EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(numTimesExpectedSetNeedsCommit, codeToTest) do { \
        EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times((numTimesExpectedSetNeedsCommit));      \
        codeToTest;                                                                                   \
        Mock::VerifyAndClearExpectations(m_layerTreeHost.get());                                      \
    } while (0)

namespace {

class MockCCLayerTreeHost : public CCLayerTreeHost {
public:
    MockCCLayerTreeHost()
        : CCLayerTreeHost(&m_fakeClient, CCLayerTreeSettings())
    {
        initialize();
    }

    MOCK_METHOD0(setNeedsCommit, void());

private:
    FakeCCLayerTreeHostClient m_fakeClient;
};

class MockLayerPainterChromium : public LayerPainterChromium {
public:
    virtual void paint(SkCanvas*, const IntRect&, FloatRect&) OVERRIDE { }
};


class LayerChromiumTest : public testing::Test {
public:
    LayerChromiumTest()
        : m_compositorInitializer(0)
    {
    }

protected:
    virtual void SetUp()
    {
        m_layerTreeHost = scoped_ptr<MockCCLayerTreeHost>(new MockCCLayerTreeHost);
    }

    virtual void TearDown()
    {
        Mock::VerifyAndClearExpectations(m_layerTreeHost.get());
        EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AnyNumber());
        m_parent = NULL;
        m_child1 = NULL;
        m_child2 = NULL;
        m_child3 = NULL;
        m_grandChild1 = NULL;
        m_grandChild2 = NULL;
        m_grandChild3 = NULL;

        m_layerTreeHost->setRootLayer(0);
        m_layerTreeHost.reset();
    }

    void verifyTestTreeInitialState() const
    {
        ASSERT_EQ(static_cast<size_t>(3), m_parent->children().size());
        EXPECT_EQ(m_child1, m_parent->children()[0]);
        EXPECT_EQ(m_child2, m_parent->children()[1]);
        EXPECT_EQ(m_child3, m_parent->children()[2]);
        EXPECT_EQ(m_parent.get(), m_child1->parent());
        EXPECT_EQ(m_parent.get(), m_child2->parent());
        EXPECT_EQ(m_parent.get(), m_child3->parent());

        ASSERT_EQ(static_cast<size_t>(2), m_child1->children().size());
        EXPECT_EQ(m_grandChild1, m_child1->children()[0]);
        EXPECT_EQ(m_grandChild2, m_child1->children()[1]);
        EXPECT_EQ(m_child1.get(), m_grandChild1->parent());
        EXPECT_EQ(m_child1.get(), m_grandChild2->parent());

        ASSERT_EQ(static_cast<size_t>(1), m_child2->children().size());
        EXPECT_EQ(m_grandChild3, m_child2->children()[0]);
        EXPECT_EQ(m_child2.get(), m_grandChild3->parent());

        ASSERT_EQ(static_cast<size_t>(0), m_child3->children().size());
    }

    void createSimpleTestTree()
    {
        m_parent = LayerChromium::create();
        m_child1 = LayerChromium::create();
        m_child2 = LayerChromium::create();
        m_child3 = LayerChromium::create();
        m_grandChild1 = LayerChromium::create();
        m_grandChild2 = LayerChromium::create();
        m_grandChild3 = LayerChromium::create();

        EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AnyNumber());
        m_layerTreeHost->setRootLayer(m_parent);

        m_parent->addChild(m_child1);
        m_parent->addChild(m_child2);
        m_parent->addChild(m_child3);
        m_child1->addChild(m_grandChild1);
        m_child1->addChild(m_grandChild2);
        m_child2->addChild(m_grandChild3);

        Mock::VerifyAndClearExpectations(m_layerTreeHost.get());

        verifyTestTreeInitialState();
    }

    scoped_ptr<MockCCLayerTreeHost> m_layerTreeHost;
    scoped_refptr<LayerChromium> m_parent, m_child1, m_child2, m_child3, m_grandChild1, m_grandChild2, m_grandChild3;
    WebCompositorInitializer m_compositorInitializer;
};

TEST_F(LayerChromiumTest, basicCreateAndDestroy)
{
    scoped_refptr<LayerChromium> testLayer = LayerChromium::create();
    ASSERT_TRUE(testLayer);

    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(0);
    testLayer->setLayerTreeHost(m_layerTreeHost.get());
}

TEST_F(LayerChromiumTest, addAndRemoveChild)
{
    scoped_refptr<LayerChromium> parent = LayerChromium::create();
    scoped_refptr<LayerChromium> child = LayerChromium::create();

    // Upon creation, layers should not have children or parent.
    ASSERT_EQ(static_cast<size_t>(0), parent->children().size());
    EXPECT_FALSE(child->parent());

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, m_layerTreeHost->setRootLayer(parent));

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, parent->addChild(child));

    ASSERT_EQ(static_cast<size_t>(1), parent->children().size());
    EXPECT_EQ(child.get(), parent->children()[0]);
    EXPECT_EQ(parent.get(), child->parent());
    EXPECT_EQ(parent.get(), child->rootLayer());

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(AtLeast(1), child->removeFromParent());
}

TEST_F(LayerChromiumTest, insertChild)
{
    scoped_refptr<LayerChromium> parent = LayerChromium::create();
    scoped_refptr<LayerChromium> child1 = LayerChromium::create();
    scoped_refptr<LayerChromium> child2 = LayerChromium::create();
    scoped_refptr<LayerChromium> child3 = LayerChromium::create();
    scoped_refptr<LayerChromium> child4 = LayerChromium::create();

    parent->setLayerTreeHost(m_layerTreeHost.get());

    ASSERT_EQ(static_cast<size_t>(0), parent->children().size());

    // Case 1: inserting to empty list.
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, parent->insertChild(child3, 0));
    ASSERT_EQ(static_cast<size_t>(1), parent->children().size());
    EXPECT_EQ(child3, parent->children()[0]);
    EXPECT_EQ(parent.get(), child3->parent());

    // Case 2: inserting to beginning of list
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, parent->insertChild(child1, 0));
    ASSERT_EQ(static_cast<size_t>(2), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child3, parent->children()[1]);
    EXPECT_EQ(parent.get(), child1->parent());

    // Case 3: inserting to middle of list
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, parent->insertChild(child2, 1));
    ASSERT_EQ(static_cast<size_t>(3), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);
    EXPECT_EQ(child3, parent->children()[2]);
    EXPECT_EQ(parent.get(), child2->parent());

    // Case 4: inserting to end of list
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, parent->insertChild(child4, 3));

    ASSERT_EQ(static_cast<size_t>(4), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);
    EXPECT_EQ(child3, parent->children()[2]);
    EXPECT_EQ(child4, parent->children()[3]);
    EXPECT_EQ(parent.get(), child4->parent());

    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));
}

TEST_F(LayerChromiumTest, insertChildPastEndOfList)
{
    scoped_refptr<LayerChromium> parent = LayerChromium::create();
    scoped_refptr<LayerChromium> child1 = LayerChromium::create();
    scoped_refptr<LayerChromium> child2 = LayerChromium::create();

    ASSERT_EQ(static_cast<size_t>(0), parent->children().size());

    // insert to an out-of-bounds index
    parent->insertChild(child1, 53);

    ASSERT_EQ(static_cast<size_t>(1), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);

    // insert another child to out-of-bounds, when list is not already empty.
    parent->insertChild(child2, 2459);

    ASSERT_EQ(static_cast<size_t>(2), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);
}

TEST_F(LayerChromiumTest, insertSameChildTwice)
{
    scoped_refptr<LayerChromium> parent = LayerChromium::create();
    scoped_refptr<LayerChromium> child1 = LayerChromium::create();
    scoped_refptr<LayerChromium> child2 = LayerChromium::create();

    parent->setLayerTreeHost(m_layerTreeHost.get());

    ASSERT_EQ(static_cast<size_t>(0), parent->children().size());

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, parent->insertChild(child1, 0));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, parent->insertChild(child2, 1));

    ASSERT_EQ(static_cast<size_t>(2), parent->children().size());
    EXPECT_EQ(child1, parent->children()[0]);
    EXPECT_EQ(child2, parent->children()[1]);

    // Inserting the same child again should cause the child to be removed and re-inserted at the new location.
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(AtLeast(1), parent->insertChild(child1, 1));

    // child1 should now be at the end of the list.
    ASSERT_EQ(static_cast<size_t>(2), parent->children().size());
    EXPECT_EQ(child2, parent->children()[0]);
    EXPECT_EQ(child1, parent->children()[1]);

    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));
}

TEST_F(LayerChromiumTest, replaceChildWithNewChild)
{
    createSimpleTestTree();
    scoped_refptr<LayerChromium> child4 = LayerChromium::create();

    EXPECT_FALSE(child4->parent());

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(AtLeast(1), m_parent->replaceChild(m_child2.get(), child4));

    ASSERT_EQ(static_cast<size_t>(3), m_parent->children().size());
    EXPECT_EQ(m_child1, m_parent->children()[0]);
    EXPECT_EQ(child4, m_parent->children()[1]);
    EXPECT_EQ(m_child3, m_parent->children()[2]);
    EXPECT_EQ(m_parent.get(), child4->parent());

    EXPECT_FALSE(m_child2->parent());
}

TEST_F(LayerChromiumTest, replaceChildWithNewChildThatHasOtherParent)
{
    createSimpleTestTree();

    // create another simple tree with testLayer and child4.
    scoped_refptr<LayerChromium> testLayer = LayerChromium::create();
    scoped_refptr<LayerChromium> child4 = LayerChromium::create();
    testLayer->addChild(child4);
    ASSERT_EQ(static_cast<size_t>(1), testLayer->children().size());
    EXPECT_EQ(child4, testLayer->children()[0]);
    EXPECT_EQ(testLayer.get(), child4->parent());

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(AtLeast(1), m_parent->replaceChild(m_child2.get(), child4));

    ASSERT_EQ(static_cast<size_t>(3), m_parent->children().size());
    EXPECT_EQ(m_child1, m_parent->children()[0]);
    EXPECT_EQ(child4, m_parent->children()[1]);
    EXPECT_EQ(m_child3, m_parent->children()[2]);
    EXPECT_EQ(m_parent.get(), child4->parent());

    // testLayer should no longer have child4,
    // and child2 should no longer have a parent.
    ASSERT_EQ(static_cast<size_t>(0), testLayer->children().size());
    EXPECT_FALSE(m_child2->parent());
}

TEST_F(LayerChromiumTest, replaceChildWithSameChild)
{
    createSimpleTestTree();

    // setNeedsCommit should not be called because its the same child
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, m_parent->replaceChild(m_child2.get(), m_child2));

    verifyTestTreeInitialState();
}

TEST_F(LayerChromiumTest, removeAllChildren)
{
    createSimpleTestTree();

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(AtLeast(3), m_parent->removeAllChildren());

    ASSERT_EQ(static_cast<size_t>(0), m_parent->children().size());
    EXPECT_FALSE(m_child1->parent());
    EXPECT_FALSE(m_child2->parent());
    EXPECT_FALSE(m_child3->parent());
}

TEST_F(LayerChromiumTest, setChildren)
{
    scoped_refptr<LayerChromium> oldParent = LayerChromium::create();
    scoped_refptr<LayerChromium> newParent = LayerChromium::create();

    scoped_refptr<LayerChromium> child1 = LayerChromium::create();
    scoped_refptr<LayerChromium> child2 = LayerChromium::create();

    std::vector<scoped_refptr<LayerChromium> > newChildren;
    newChildren.push_back(child1);
    newChildren.push_back(child2);

    // Set up and verify initial test conditions: child1 has a parent, child2 has no parent.
    oldParent->addChild(child1);
    ASSERT_EQ(static_cast<size_t>(0), newParent->children().size());
    EXPECT_EQ(oldParent.get(), child1->parent());
    EXPECT_FALSE(child2->parent());

    newParent->setLayerTreeHost(m_layerTreeHost.get());

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(AtLeast(1), newParent->setChildren(newChildren));

    ASSERT_EQ(static_cast<size_t>(2), newParent->children().size());
    EXPECT_EQ(newParent.get(), child1->parent());
    EXPECT_EQ(newParent.get(), child2->parent());

    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));
}

TEST_F(LayerChromiumTest, getRootLayerAfterTreeManipulations)
{
    createSimpleTestTree();

    // For this test we don't care about setNeedsCommit calls.
    EXPECT_CALL(*m_layerTreeHost, setNeedsCommit()).Times(AtLeast(1));

    scoped_refptr<LayerChromium> child4 = LayerChromium::create();

    EXPECT_EQ(m_parent.get(), m_parent->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child3->rootLayer());
    EXPECT_EQ(child4.get(),   child4->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild3->rootLayer());

    m_child1->removeFromParent();

    // child1 and its children, grandChild1 and grandChild2 are now on a separate subtree.
    EXPECT_EQ(m_parent.get(), m_parent->rootLayer());
    EXPECT_EQ(m_child1.get(), m_child1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child3->rootLayer());
    EXPECT_EQ(child4.get(), child4->rootLayer());
    EXPECT_EQ(m_child1.get(), m_grandChild1->rootLayer());
    EXPECT_EQ(m_child1.get(), m_grandChild2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild3->rootLayer());

    m_grandChild3->addChild(child4);

    EXPECT_EQ(m_parent.get(), m_parent->rootLayer());
    EXPECT_EQ(m_child1.get(), m_child1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child3->rootLayer());
    EXPECT_EQ(m_parent.get(), child4->rootLayer());
    EXPECT_EQ(m_child1.get(), m_grandChild1->rootLayer());
    EXPECT_EQ(m_child1.get(), m_grandChild2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild3->rootLayer());

    m_child2->replaceChild(m_grandChild3.get(), m_child1);

    // grandChild3 gets orphaned and the child1 subtree gets planted back into the tree under child2.
    EXPECT_EQ(m_parent.get(), m_parent->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child2->rootLayer());
    EXPECT_EQ(m_parent.get(), m_child3->rootLayer());
    EXPECT_EQ(m_grandChild3.get(), child4->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild1->rootLayer());
    EXPECT_EQ(m_parent.get(), m_grandChild2->rootLayer());
    EXPECT_EQ(m_grandChild3.get(), m_grandChild3->rootLayer());
}

TEST_F(LayerChromiumTest, checkSetNeedsDisplayCausesCorrectBehavior)
{
    // The semantics for setNeedsDisplay which are tested here:
    //   1. sets needsDisplay flag appropriately.
    //   2. indirectly calls setNeedsCommit, exactly once for each call to setNeedsDisplay.

    scoped_refptr<LayerChromium> testLayer = LayerChromium::create();
    testLayer->setLayerTreeHost(m_layerTreeHost.get());

    IntSize testBounds = IntSize(501, 508);

    FloatRect dirty1 = FloatRect(10, 15, 1, 2);
    FloatRect dirty2 = FloatRect(20, 25, 3, 4);
    FloatRect emptyDirtyRect = FloatRect(40, 45, 0, 0);
    FloatRect outOfBoundsDirtyRect = FloatRect(400, 405, 500, 502);

    // Before anything, testLayer should not be dirty.
    EXPECT_FALSE(testLayer->needsDisplay());

    // This is just initialization, but setNeedsCommit behavior is verified anyway to avoid warnings.
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setBounds(testBounds));
    testLayer = LayerChromium::create();
    testLayer->setLayerTreeHost(m_layerTreeHost.get());
    EXPECT_FALSE(testLayer->needsDisplay());

    // The real test begins here.

    // Case 1: needsDisplay flag should not change because of an empty dirty rect.
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setNeedsDisplayRect(emptyDirtyRect));
    EXPECT_FALSE(testLayer->needsDisplay());

    // Case 2: basic.
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setNeedsDisplayRect(dirty1));
    EXPECT_TRUE(testLayer->needsDisplay());

    // Case 3: a second dirty rect.
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setNeedsDisplayRect(dirty2));
    EXPECT_TRUE(testLayer->needsDisplay());

    // Case 4: LayerChromium should accept dirty rects that go beyond its bounds.
    testLayer = LayerChromium::create();
    testLayer->setLayerTreeHost(m_layerTreeHost.get());
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setBounds(testBounds));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setNeedsDisplayRect(outOfBoundsDirtyRect));
    EXPECT_TRUE(testLayer->needsDisplay());

    // Case 5: setNeedsDisplay() without the dirty rect arg.
    testLayer = LayerChromium::create();
    testLayer->setLayerTreeHost(m_layerTreeHost.get());
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setBounds(testBounds));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setNeedsDisplay());
    EXPECT_TRUE(testLayer->needsDisplay());
}

TEST_F(LayerChromiumTest, checkPropertyChangeCausesCorrectBehavior)
{
    scoped_refptr<LayerChromium> testLayer = LayerChromium::create();
    testLayer->setLayerTreeHost(m_layerTreeHost.get());

    scoped_refptr<LayerChromium> dummyLayer = LayerChromium::create(); // just a dummy layer for this test case.

    // sanity check of initial test condition
    EXPECT_FALSE(testLayer->needsDisplay());

    // Test properties that should not call needsDisplay and needsCommit when changed.
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, testLayer->setVisibleContentRect(IntRect(0, 0, 40, 50)));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, testLayer->setUseLCDText(true));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, testLayer->setDrawOpacity(0.5));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, testLayer->setRenderTarget(0));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, testLayer->setDrawTransform(WebTransformationMatrix()));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, testLayer->setScreenSpaceTransform(WebTransformationMatrix()));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(0, testLayer->setDrawableContentRect(IntRect(4, 5, 6, 7)));
    EXPECT_FALSE(testLayer->needsDisplay());

    // Next, test properties that should call setNeedsCommit (but not setNeedsDisplay)
    // All properties need to be set to new values in order for setNeedsCommit to be called.
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setAnchorPoint(FloatPoint(1.23f, 4.56f)));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setAnchorPointZ(0.7f));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setBackgroundColor(SK_ColorLTGRAY));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setMasksToBounds(true));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setMaskLayer(dummyLayer.get()));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setOpacity(0.5));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setContentsOpaque(true));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setPosition(FloatPoint(4, 9)));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setReplicaLayer(dummyLayer.get()));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setSublayerTransform(WebTransformationMatrix(0, 0, 0, 0, 0, 0)));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setScrollable(true));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setShouldScrollOnMainThread(true));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setNonFastScrollableRegion(Region(IntRect(1, 1, 2, 2))));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setHaveWheelEventHandlers(true));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setScrollPosition(IntPoint(10, 10)));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setTransform(WebTransformationMatrix(0, 0, 0, 0, 0, 0)));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setDoubleSided(false));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setDebugName("Test Layer"));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setDrawCheckerboardForMissingTiles(!testLayer->drawCheckerboardForMissingTiles()));
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setForceRenderSurface(true));

    // The above tests should not have caused a change to the needsDisplay flag.
    EXPECT_FALSE(testLayer->needsDisplay());

    // Test properties that should call setNeedsDisplay and setNeedsCommit
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setBounds(IntSize(5, 10)));
    EXPECT_TRUE(testLayer->needsDisplay());
}

TEST_F(LayerChromiumTest, verifyPushPropertiesAccumulatesUpdateRect)
{
    DebugScopedSetImplThread setImplThread;

    scoped_refptr<LayerChromium> testLayer = LayerChromium::create();
    scoped_ptr<CCLayerImpl> implLayer = CCLayerImpl::create(1);

    testLayer->setNeedsDisplayRect(FloatRect(FloatPoint::zero(), FloatSize(5, 5)));
    testLayer->pushPropertiesTo(implLayer.get());
    EXPECT_FLOAT_RECT_EQ(FloatRect(FloatPoint::zero(), FloatSize(5, 5)), implLayer->updateRect());

    // The CCLayerImpl's updateRect should be accumulated here, since we did not do anything to clear it.
    testLayer->setNeedsDisplayRect(FloatRect(FloatPoint(10, 10), FloatSize(5, 5)));
    testLayer->pushPropertiesTo(implLayer.get());
    EXPECT_FLOAT_RECT_EQ(FloatRect(FloatPoint::zero(), FloatSize(15, 15)), implLayer->updateRect());

    // If we do clear the CCLayerImpl side, then the next updateRect should be fresh without accumulation.
    implLayer->resetAllChangeTrackingForSubtree();
    testLayer->setNeedsDisplayRect(FloatRect(FloatPoint(10, 10), FloatSize(5, 5)));
    testLayer->pushPropertiesTo(implLayer.get());
    EXPECT_FLOAT_RECT_EQ(FloatRect(FloatPoint(10, 10), FloatSize(5, 5)), implLayer->updateRect());
}

class LayerChromiumWithContentScaling : public LayerChromium {
public:
    explicit LayerChromiumWithContentScaling()
        : LayerChromium()
    {
    }

    virtual bool needsContentsScale() const OVERRIDE
    {
        return true;
    }

    virtual void setNeedsDisplayRect(const FloatRect& dirtyRect) OVERRIDE
    {
        m_lastNeedsDisplayRect = dirtyRect;
        LayerChromium::setNeedsDisplayRect(dirtyRect);
    }

    void resetNeedsDisplay()
    {
        m_needsDisplay = false;
    }

    const FloatRect& lastNeedsDisplayRect() const { return m_lastNeedsDisplayRect; }

private:
    virtual ~LayerChromiumWithContentScaling()
    {
    }

    FloatRect m_lastNeedsDisplayRect;
};

TEST_F(LayerChromiumTest, checkContentsScaleChangeTriggersNeedsDisplay)
{
    scoped_refptr<LayerChromiumWithContentScaling> testLayer = make_scoped_refptr(new LayerChromiumWithContentScaling());
    testLayer->setLayerTreeHost(m_layerTreeHost.get());

    IntSize testBounds = IntSize(320, 240);
    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setBounds(testBounds));

    testLayer->resetNeedsDisplay();
    EXPECT_FALSE(testLayer->needsDisplay());

    EXECUTE_AND_VERIFY_SET_NEEDS_COMMIT_BEHAVIOR(1, testLayer->setContentsScale(testLayer->contentsScale() + 1.f));
    EXPECT_TRUE(testLayer->needsDisplay());
    EXPECT_FLOAT_RECT_EQ(FloatRect(0, 0, 320, 240), testLayer->lastNeedsDisplayRect());
}

class FakeCCLayerTreeHost : public CCLayerTreeHost {
public:
    static scoped_ptr<FakeCCLayerTreeHost> create()
    {
        scoped_ptr<FakeCCLayerTreeHost> host(new FakeCCLayerTreeHost);
        // The initialize call will fail, since our client doesn't provide a valid GraphicsContext3D, but it doesn't matter in the tests that use this fake so ignore the return value.
        host->initialize();
        return host.Pass();
    }

private:
    FakeCCLayerTreeHost()
        : CCLayerTreeHost(&m_client, CCLayerTreeSettings())
    {
    }

    FakeCCLayerTreeHostClient m_client;
};

void assertLayerTreeHostMatchesForSubtree(LayerChromium* layer, CCLayerTreeHost* host)
{
    EXPECT_EQ(host, layer->layerTreeHost());

    for (size_t i = 0; i < layer->children().size(); ++i)
        assertLayerTreeHostMatchesForSubtree(layer->children()[i].get(), host);

    if (layer->maskLayer())
        assertLayerTreeHostMatchesForSubtree(layer->maskLayer(), host);

    if (layer->replicaLayer())
        assertLayerTreeHostMatchesForSubtree(layer->replicaLayer(), host);
}


TEST(LayerChromiumLayerTreeHostTest, enteringTree)
{
    WebCompositorInitializer compositorInitializer(0);
    scoped_refptr<LayerChromium> parent = LayerChromium::create();
    scoped_refptr<LayerChromium> child = LayerChromium::create();
    scoped_refptr<LayerChromium> mask = LayerChromium::create();
    scoped_refptr<LayerChromium> replica = LayerChromium::create();
    scoped_refptr<LayerChromium> replicaMask = LayerChromium::create();

    // Set up a detached tree of layers. The host pointer should be nil for these layers.
    parent->addChild(child);
    child->setMaskLayer(mask.get());
    child->setReplicaLayer(replica.get());
    replica->setMaskLayer(mask.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), 0);

    scoped_ptr<FakeCCLayerTreeHost> layerTreeHost(FakeCCLayerTreeHost::create());
    // Setting the root layer should set the host pointer for all layers in the tree.
    layerTreeHost->setRootLayer(parent.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), layerTreeHost.get());

    // Clearing the root layer should also clear out the host pointers for all layers in the tree.
    layerTreeHost->setRootLayer(0);

    assertLayerTreeHostMatchesForSubtree(parent.get(), 0);
}

TEST(LayerChromiumLayerTreeHostTest, addingLayerSubtree)
{
    WebCompositorInitializer compositorInitializer(0);
    scoped_refptr<LayerChromium> parent = LayerChromium::create();
    scoped_ptr<FakeCCLayerTreeHost> layerTreeHost(FakeCCLayerTreeHost::create());

    layerTreeHost->setRootLayer(parent.get());

    EXPECT_EQ(parent->layerTreeHost(), layerTreeHost.get());

    // Adding a subtree to a layer already associated with a host should set the host pointer on all layers in that subtree.
    scoped_refptr<LayerChromium> child = LayerChromium::create();
    scoped_refptr<LayerChromium> grandChild = LayerChromium::create();
    child->addChild(grandChild);

    // Masks, replicas, and replica masks should pick up the new host too.
    scoped_refptr<LayerChromium> childMask = LayerChromium::create();
    child->setMaskLayer(childMask.get());
    scoped_refptr<LayerChromium> childReplica = LayerChromium::create();
    child->setReplicaLayer(childReplica.get());
    scoped_refptr<LayerChromium> childReplicaMask = LayerChromium::create();
    childReplica->setMaskLayer(childReplicaMask.get());

    parent->addChild(child);
    assertLayerTreeHostMatchesForSubtree(parent.get(), layerTreeHost.get());

    layerTreeHost->setRootLayer(0);
}

TEST(LayerChromiumLayerTreeHostTest, changeHost)
{
    WebCompositorInitializer compositorInitializer(0);
    scoped_refptr<LayerChromium> parent = LayerChromium::create();
    scoped_refptr<LayerChromium> child = LayerChromium::create();
    scoped_refptr<LayerChromium> mask = LayerChromium::create();
    scoped_refptr<LayerChromium> replica = LayerChromium::create();
    scoped_refptr<LayerChromium> replicaMask = LayerChromium::create();

    // Same setup as the previous test.
    parent->addChild(child);
    child->setMaskLayer(mask.get());
    child->setReplicaLayer(replica.get());
    replica->setMaskLayer(mask.get());

    scoped_ptr<FakeCCLayerTreeHost> firstLayerTreeHost(FakeCCLayerTreeHost::create());
    firstLayerTreeHost->setRootLayer(parent.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), firstLayerTreeHost.get());

    // Now re-root the tree to a new host (simulating what we do on a context lost event).
    // This should update the host pointers for all layers in the tree.
    scoped_ptr<FakeCCLayerTreeHost> secondLayerTreeHost(FakeCCLayerTreeHost::create());
    secondLayerTreeHost->setRootLayer(parent.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), secondLayerTreeHost.get());

    secondLayerTreeHost->setRootLayer(0);
}

TEST(LayerChromiumLayerTreeHostTest, changeHostInSubtree)
{
    WebCompositorInitializer compositorInitializer(0);
    scoped_refptr<LayerChromium> firstParent = LayerChromium::create();
    scoped_refptr<LayerChromium> firstChild = LayerChromium::create();
    scoped_refptr<LayerChromium> secondParent = LayerChromium::create();
    scoped_refptr<LayerChromium> secondChild = LayerChromium::create();
    scoped_refptr<LayerChromium> secondGrandChild = LayerChromium::create();

    // First put all children under the first parent and set the first host.
    firstParent->addChild(firstChild);
    secondChild->addChild(secondGrandChild);
    firstParent->addChild(secondChild);

    scoped_ptr<FakeCCLayerTreeHost> firstLayerTreeHost(FakeCCLayerTreeHost::create());
    firstLayerTreeHost->setRootLayer(firstParent.get());

    assertLayerTreeHostMatchesForSubtree(firstParent.get(), firstLayerTreeHost.get());

    // Now reparent the subtree starting at secondChild to a layer in a different tree.
    scoped_ptr<FakeCCLayerTreeHost> secondLayerTreeHost(FakeCCLayerTreeHost::create());
    secondLayerTreeHost->setRootLayer(secondParent.get());

    secondParent->addChild(secondChild);

    // The moved layer and its children should point to the new host.
    EXPECT_EQ(secondLayerTreeHost.get(), secondChild->layerTreeHost());
    EXPECT_EQ(secondLayerTreeHost.get(), secondGrandChild->layerTreeHost());

    // Test over, cleanup time.
    firstLayerTreeHost->setRootLayer(0);
    secondLayerTreeHost->setRootLayer(0);
}

TEST(LayerChromiumLayerTreeHostTest, replaceMaskAndReplicaLayer)
{
    WebCompositorInitializer compositorInitializer(0);
    scoped_refptr<LayerChromium> parent = LayerChromium::create();
    scoped_refptr<LayerChromium> mask = LayerChromium::create();
    scoped_refptr<LayerChromium> replica = LayerChromium::create();
    scoped_refptr<LayerChromium> maskChild = LayerChromium::create();
    scoped_refptr<LayerChromium> replicaChild = LayerChromium::create();
    scoped_refptr<LayerChromium> maskReplacement = LayerChromium::create();
    scoped_refptr<LayerChromium> replicaReplacement = LayerChromium::create();

    parent->setMaskLayer(mask.get());
    parent->setReplicaLayer(replica.get());
    mask->addChild(maskChild);
    replica->addChild(replicaChild);

    scoped_ptr<FakeCCLayerTreeHost> layerTreeHost(FakeCCLayerTreeHost::create());
    layerTreeHost->setRootLayer(parent.get());

    assertLayerTreeHostMatchesForSubtree(parent.get(), layerTreeHost.get());

    // Replacing the mask should clear out the old mask's subtree's host pointers.
    parent->setMaskLayer(maskReplacement.get());
    EXPECT_EQ(0, mask->layerTreeHost());
    EXPECT_EQ(0, maskChild->layerTreeHost());

    // Same for replacing a replica layer.
    parent->setReplicaLayer(replicaReplacement.get());
    EXPECT_EQ(0, replica->layerTreeHost());
    EXPECT_EQ(0, replicaChild->layerTreeHost());

    // Test over, cleanup time.
    layerTreeHost->setRootLayer(0);
}

TEST(LayerChromiumLayerTreeHostTest, destroyHostWithNonNullRootLayer)
{
    WebCompositorInitializer compositorInitializer(0);
    scoped_refptr<LayerChromium> root = LayerChromium::create();
    scoped_refptr<LayerChromium> child = LayerChromium::create();
    root->addChild(child);
    scoped_ptr<FakeCCLayerTreeHost> layerTreeHost(FakeCCLayerTreeHost::create());
    layerTreeHost->setRootLayer(root);
}

static bool addTestAnimation(LayerChromium* layer)
{
    scoped_ptr<CCKeyframedFloatAnimationCurve> curve(CCKeyframedFloatAnimationCurve::create());
    curve->addKeyframe(CCFloatKeyframe::create(0, 0.3f, scoped_ptr<CCTimingFunction>()));
    curve->addKeyframe(CCFloatKeyframe::create(1, 0.7f, scoped_ptr<CCTimingFunction>()));
    scoped_ptr<CCActiveAnimation> animation(CCActiveAnimation::create(curve.PassAs<CCAnimationCurve>(), 0, 0, CCActiveAnimation::Opacity));

    return layer->addAnimation(animation.Pass());
}

TEST(LayerChromiumLayerTreeHostTest, shouldNotAddAnimationWithoutLayerTreeHost)
{
    // Currently, WebCore assumes that animations will be started immediately / very soon
    // if a composited layer's addAnimation() returns true. However, without a layerTreeHost,
    // layers cannot actually animate yet. So, to prevent violating this WebCore assumption,
    // the animation should not be accepted if the layer doesn't already have a layerTreeHost.

    CCScopedSettings scopedSettings;
    Settings::setAcceleratedAnimationEnabled(true);

    WebCompositorInitializer compositorInitializer(0);
    scoped_refptr<LayerChromium> layer = LayerChromium::create();

    // Case 1: without a layerTreeHost, the animation should not be accepted.
    EXPECT_FALSE(addTestAnimation(layer.get()));

    scoped_ptr<FakeCCLayerTreeHost> layerTreeHost(FakeCCLayerTreeHost::create());
    layerTreeHost->setRootLayer(layer.get());
    layer->setLayerTreeHost(layerTreeHost.get());
    assertLayerTreeHostMatchesForSubtree(layer.get(), layerTreeHost.get());

    // Case 2: with a layerTreeHost, the animation should be accepted.
    EXPECT_TRUE(addTestAnimation(layer.get()));
}

class MockLayerChromium : public LayerChromium {
public:
    bool needsDisplay() const { return m_needsDisplay; }

private:
    virtual ~MockLayerChromium()
    {
    }
};

TEST(LayerChromiumTestWithoutFixture, setBoundsTriggersSetNeedsRedrawAfterGettingNonEmptyBounds)
{
    scoped_refptr<MockLayerChromium> layer(new MockLayerChromium);
    EXPECT_FALSE(layer->needsDisplay());
    layer->setBounds(IntSize(0, 10));
    EXPECT_FALSE(layer->needsDisplay());
    layer->setBounds(IntSize(10, 10));
    EXPECT_TRUE(layer->needsDisplay());
}


} // namespace
