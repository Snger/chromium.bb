// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "TreeSynchronizer.h"

#include "CCAnimationTestCommon.h"
#include "CCLayerAnimationController.h"
#include "CCLayerImpl.h"
#include "CCProxy.h"
#include "CCSingleThreadProxy.h"
#include "LayerChromium.h"
#include "Region.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace cc;
using namespace WebKitTests;

namespace {

class MockCCLayerImpl : public CCLayerImpl {
public:
    static scoped_ptr<MockCCLayerImpl> create(int layerId)
    {
        return make_scoped_ptr(new MockCCLayerImpl(layerId));
    }
    virtual ~MockCCLayerImpl()
    {
        if (m_ccLayerDestructionList)
            m_ccLayerDestructionList->append(id());
    }

    void setCCLayerDestructionList(Vector<int>* list) { m_ccLayerDestructionList = list; }

private:
    MockCCLayerImpl(int layerId)
        : CCLayerImpl(layerId)
        , m_ccLayerDestructionList(0)
    {
    }

    Vector<int>* m_ccLayerDestructionList;
};

class MockLayerChromium : public LayerChromium {
public:
    static scoped_refptr<MockLayerChromium> create(Vector<int>* ccLayerDestructionList)
    {
        return make_scoped_refptr(new MockLayerChromium(ccLayerDestructionList));
    }

    virtual scoped_ptr<CCLayerImpl> createCCLayerImpl() OVERRIDE
    {
        return MockCCLayerImpl::create(m_layerId).PassAs<CCLayerImpl>();
    }

    virtual void pushPropertiesTo(CCLayerImpl* ccLayer) OVERRIDE
    {
        LayerChromium::pushPropertiesTo(ccLayer);

        MockCCLayerImpl* mockCCLayer = static_cast<MockCCLayerImpl*>(ccLayer);
        mockCCLayer->setCCLayerDestructionList(m_ccLayerDestructionList);
    }

private:
    MockLayerChromium(Vector<int>* ccLayerDestructionList)
        : LayerChromium()
        , m_ccLayerDestructionList(ccLayerDestructionList)
    {
    }
    virtual ~MockLayerChromium() { }

    Vector<int>* m_ccLayerDestructionList;
};

class FakeLayerAnimationController : public CCLayerAnimationController {
public:
    static scoped_ptr<FakeLayerAnimationController> create(CCLayerAnimationControllerClient* client)
    {
        return make_scoped_ptr(new FakeLayerAnimationController(client));
    }

    bool synchronizedAnimations() const { return m_synchronizedAnimations; }

private:
    explicit FakeLayerAnimationController(CCLayerAnimationControllerClient* client)
        : CCLayerAnimationController(client)
        , m_synchronizedAnimations(false)
    {
    }

    virtual void pushAnimationUpdatesTo(CCLayerAnimationController* controllerImpl)
    {
        CCLayerAnimationController::pushAnimationUpdatesTo(controllerImpl);
        m_synchronizedAnimations = true;
    }

    bool m_synchronizedAnimations;
};

void expectTreesAreIdentical(LayerChromium* layer, CCLayerImpl* ccLayer, CCLayerTreeHostImpl* hostImpl)
{
    ASSERT_TRUE(layer);
    ASSERT_TRUE(ccLayer);

    EXPECT_EQ(layer->id(), ccLayer->id());
    EXPECT_EQ(ccLayer->layerTreeHostImpl(), hostImpl);

    EXPECT_EQ(layer->nonFastScrollableRegion(), ccLayer->nonFastScrollableRegion());

    ASSERT_EQ(!!layer->maskLayer(), !!ccLayer->maskLayer());
    if (layer->maskLayer())
        expectTreesAreIdentical(layer->maskLayer(), ccLayer->maskLayer(), hostImpl);

    ASSERT_EQ(!!layer->replicaLayer(), !!ccLayer->replicaLayer());
    if (layer->replicaLayer())
        expectTreesAreIdentical(layer->replicaLayer(), ccLayer->replicaLayer(), hostImpl);

    const std::vector<scoped_refptr<LayerChromium> >& layerChildren = layer->children();
    const ScopedPtrVector<CCLayerImpl>& ccLayerChildren = ccLayer->children();

    ASSERT_EQ(layerChildren.size(), ccLayerChildren.size());

    for (size_t i = 0; i < layerChildren.size(); ++i)
        expectTreesAreIdentical(layerChildren[i].get(), ccLayerChildren[i], hostImpl);
}

// Attempts to synchronizes a null tree. This should not crash, and should
// return a null tree.
TEST(TreeSynchronizerTest, syncNullTree)
{
    DebugScopedSetImplThread impl;

    scoped_ptr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(0, scoped_ptr<CCLayerImpl>(), 0);

    EXPECT_TRUE(!ccLayerTreeRoot.get());
}

// Constructs a very simple tree and synchronizes it without trying to reuse any preexisting layers.
TEST(TreeSynchronizerTest, syncSimpleTreeFromEmpty)
{
    DebugScopedSetImplThread impl;

    CCLayerTreeSettings settings;
    scoped_ptr<CCLayerTreeHostImpl> hostImpl = CCLayerTreeHostImpl::create(settings, 0);

    scoped_refptr<LayerChromium> layerTreeRoot = LayerChromium::create();
    layerTreeRoot->addChild(LayerChromium::create());
    layerTreeRoot->addChild(LayerChromium::create());

    scoped_ptr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<CCLayerImpl>(), hostImpl.get());

    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get(), hostImpl.get());
}

// Constructs a very simple tree and synchronizes it attempting to reuse some layers
TEST(TreeSynchronizerTest, syncSimpleTreeReusingLayers)
{
    DebugScopedSetImplThread impl;
    Vector<int> ccLayerDestructionList;

    CCLayerTreeSettings settings;
    scoped_ptr<CCLayerTreeHostImpl> hostImpl = CCLayerTreeHostImpl::create(settings, 0);

    scoped_refptr<LayerChromium> layerTreeRoot = MockLayerChromium::create(&ccLayerDestructionList);
    layerTreeRoot->addChild(MockLayerChromium::create(&ccLayerDestructionList));
    layerTreeRoot->addChild(MockLayerChromium::create(&ccLayerDestructionList));

    scoped_ptr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<CCLayerImpl>(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get(), hostImpl.get());

    // Add a new layer to the LayerChromium side
    layerTreeRoot->children()[0]->addChild(MockLayerChromium::create(&ccLayerDestructionList));
    // Remove one.
    layerTreeRoot->children()[1]->removeFromParent();
    int secondCCLayerId = ccLayerTreeRoot->children()[1]->id();

    // Synchronize again. After the sync the trees should be equivalent and we should have created and destroyed one CCLayerImpl.
    ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), ccLayerTreeRoot.Pass(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get(), hostImpl.get());

    ASSERT_EQ(1u, ccLayerDestructionList.size());
    EXPECT_EQ(secondCCLayerId, ccLayerDestructionList[0]);
}

// Constructs a very simple tree and checks that a stacking-order change is tracked properly.
TEST(TreeSynchronizerTest, syncSimpleTreeAndTrackStackingOrderChange)
{
    DebugScopedSetImplThread impl;
    Vector<int> ccLayerDestructionList;

    CCLayerTreeSettings settings;
    scoped_ptr<CCLayerTreeHostImpl> hostImpl = CCLayerTreeHostImpl::create(settings, 0);

    // Set up the tree and sync once. child2 needs to be synced here, too, even though we
    // remove it to set up the intended scenario.
    scoped_refptr<LayerChromium> layerTreeRoot = MockLayerChromium::create(&ccLayerDestructionList);
    scoped_refptr<LayerChromium> child2 = MockLayerChromium::create(&ccLayerDestructionList);
    layerTreeRoot->addChild(MockLayerChromium::create(&ccLayerDestructionList));
    layerTreeRoot->addChild(child2);
    scoped_ptr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<CCLayerImpl>(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get(), hostImpl.get());
    ccLayerTreeRoot->resetAllChangeTrackingForSubtree();

    // re-insert the layer and sync again.
    child2->removeFromParent();
    layerTreeRoot->addChild(child2);
    ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), ccLayerTreeRoot.Pass(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get(), hostImpl.get());

    // Check that the impl thread properly tracked the change.
    EXPECT_FALSE(ccLayerTreeRoot->layerPropertyChanged());
    EXPECT_FALSE(ccLayerTreeRoot->children()[0]->layerPropertyChanged());
    EXPECT_TRUE(ccLayerTreeRoot->children()[1]->layerPropertyChanged());
}

TEST(TreeSynchronizerTest, syncSimpleTreeAndProperties)
{
    DebugScopedSetImplThread impl;

    CCLayerTreeSettings settings;
    scoped_ptr<CCLayerTreeHostImpl> hostImpl = CCLayerTreeHostImpl::create(settings, 0);

    scoped_refptr<LayerChromium> layerTreeRoot = LayerChromium::create();
    layerTreeRoot->addChild(LayerChromium::create());
    layerTreeRoot->addChild(LayerChromium::create());

    // Pick some random properties to set. The values are not important, we're just testing that at least some properties are making it through.
    FloatPoint rootPosition = FloatPoint(2.3f, 7.4f);
    layerTreeRoot->setPosition(rootPosition);

    float firstChildOpacity = 0.25f;
    layerTreeRoot->children()[0]->setOpacity(firstChildOpacity);

    IntSize secondChildBounds = IntSize(25, 53);
    layerTreeRoot->children()[1]->setBounds(secondChildBounds);

    scoped_ptr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<CCLayerImpl>(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get(), hostImpl.get());

    // Check that the property values we set on the LayerChromium tree are reflected in the CCLayerImpl tree.
    FloatPoint rootCCLayerPosition = ccLayerTreeRoot->position();
    EXPECT_EQ(rootPosition.x(), rootCCLayerPosition.x());
    EXPECT_EQ(rootPosition.y(), rootCCLayerPosition.y());

    EXPECT_EQ(firstChildOpacity, ccLayerTreeRoot->children()[0]->opacity());

    IntSize secondCCLayerChildBounds = ccLayerTreeRoot->children()[1]->bounds();
    EXPECT_EQ(secondChildBounds.width(), secondCCLayerChildBounds.width());
    EXPECT_EQ(secondChildBounds.height(), secondCCLayerChildBounds.height());
}

TEST(TreeSynchronizerTest, reuseCCLayersAfterStructuralChange)
{
    DebugScopedSetImplThread impl;
    Vector<int> ccLayerDestructionList;

    CCLayerTreeSettings settings;
    scoped_ptr<CCLayerTreeHostImpl> hostImpl = CCLayerTreeHostImpl::create(settings, 0);

    // Set up a tree with this sort of structure:
    // root --- A --- B ---+--- C
    //                     |
    //                     +--- D
    scoped_refptr<LayerChromium> layerTreeRoot = MockLayerChromium::create(&ccLayerDestructionList);
    layerTreeRoot->addChild(MockLayerChromium::create(&ccLayerDestructionList));

    scoped_refptr<LayerChromium> layerA = layerTreeRoot->children()[0].get();
    layerA->addChild(MockLayerChromium::create(&ccLayerDestructionList));

    scoped_refptr<LayerChromium> layerB = layerA->children()[0].get();
    layerB->addChild(MockLayerChromium::create(&ccLayerDestructionList));

    scoped_refptr<LayerChromium> layerC = layerB->children()[0].get();
    layerB->addChild(MockLayerChromium::create(&ccLayerDestructionList));
    scoped_refptr<LayerChromium> layerD = layerB->children()[1].get();

    scoped_ptr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<CCLayerImpl>(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get(), hostImpl.get());

    // Now restructure the tree to look like this:
    // root --- D ---+--- A
    //               |
    //               +--- C --- B
    layerTreeRoot->removeAllChildren();
    layerD->removeAllChildren();
    layerTreeRoot->addChild(layerD);
    layerA->removeAllChildren();
    layerD->addChild(layerA);
    layerC->removeAllChildren();
    layerD->addChild(layerC);
    layerB->removeAllChildren();
    layerC->addChild(layerB);

    // After another synchronize our trees should match and we should not have destroyed any CCLayerImpls
    ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), ccLayerTreeRoot.Pass(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get(), hostImpl.get());

    EXPECT_EQ(0u, ccLayerDestructionList.size());
}

// Constructs a very simple tree, synchronizes it, then synchronizes to a totally new tree. All layers from the old tree should be deleted.
TEST(TreeSynchronizerTest, syncSimpleTreeThenDestroy)
{
    DebugScopedSetImplThread impl;
    Vector<int> ccLayerDestructionList;

    CCLayerTreeSettings settings;
    scoped_ptr<CCLayerTreeHostImpl> hostImpl = CCLayerTreeHostImpl::create(settings, 0);

    scoped_refptr<LayerChromium> oldLayerTreeRoot = MockLayerChromium::create(&ccLayerDestructionList);
    oldLayerTreeRoot->addChild(MockLayerChromium::create(&ccLayerDestructionList));
    oldLayerTreeRoot->addChild(MockLayerChromium::create(&ccLayerDestructionList));

    int oldTreeRootLayerId = oldLayerTreeRoot->id();
    int oldTreeFirstChildLayerId = oldLayerTreeRoot->children()[0]->id();
    int oldTreeSecondChildLayerId = oldLayerTreeRoot->children()[1]->id();

    scoped_ptr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(oldLayerTreeRoot.get(), scoped_ptr<CCLayerImpl>(), hostImpl.get());
    expectTreesAreIdentical(oldLayerTreeRoot.get(), ccLayerTreeRoot.get(), hostImpl.get());

    // Remove all children on the LayerChromium side.
    oldLayerTreeRoot->removeAllChildren();

    // Synchronize again. After the sync all CCLayerImpls from the old tree should be deleted.
    scoped_refptr<LayerChromium> newLayerTreeRoot = LayerChromium::create();
    ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(newLayerTreeRoot.get(), ccLayerTreeRoot.Pass(), hostImpl.get());
    expectTreesAreIdentical(newLayerTreeRoot.get(), ccLayerTreeRoot.get(), hostImpl.get());

    ASSERT_EQ(3u, ccLayerDestructionList.size());
    EXPECT_TRUE(ccLayerDestructionList.contains(oldTreeRootLayerId));
    EXPECT_TRUE(ccLayerDestructionList.contains(oldTreeFirstChildLayerId));
    EXPECT_TRUE(ccLayerDestructionList.contains(oldTreeSecondChildLayerId));
}

// Constructs+syncs a tree with mask, replica, and replica mask layers.
TEST(TreeSynchronizerTest, syncMaskReplicaAndReplicaMaskLayers)
{
    DebugScopedSetImplThread impl;

    CCLayerTreeSettings settings;
    scoped_ptr<CCLayerTreeHostImpl> hostImpl = CCLayerTreeHostImpl::create(settings, 0);

    scoped_refptr<LayerChromium> layerTreeRoot = LayerChromium::create();
    layerTreeRoot->addChild(LayerChromium::create());
    layerTreeRoot->addChild(LayerChromium::create());
    layerTreeRoot->addChild(LayerChromium::create());

    // First child gets a mask layer.
    scoped_refptr<LayerChromium> maskLayer = LayerChromium::create();
    layerTreeRoot->children()[0]->setMaskLayer(maskLayer.get());

    // Second child gets a replica layer.
    scoped_refptr<LayerChromium> replicaLayer = LayerChromium::create();
    layerTreeRoot->children()[1]->setReplicaLayer(replicaLayer.get());

    // Third child gets a replica layer with a mask layer.
    scoped_refptr<LayerChromium> replicaLayerWithMask = LayerChromium::create();
    scoped_refptr<LayerChromium> replicaMaskLayer = LayerChromium::create();
    replicaLayerWithMask->setMaskLayer(replicaMaskLayer.get());
    layerTreeRoot->children()[2]->setReplicaLayer(replicaLayerWithMask.get());

    scoped_ptr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<CCLayerImpl>(), hostImpl.get());

    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get(), hostImpl.get());

    // Remove the mask layer.
    layerTreeRoot->children()[0]->setMaskLayer(0);
    ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), ccLayerTreeRoot.Pass(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get(), hostImpl.get());

    // Remove the replica layer.
    layerTreeRoot->children()[1]->setReplicaLayer(0);
    ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), ccLayerTreeRoot.Pass(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get(), hostImpl.get());

    // Remove the replica mask.
    replicaLayerWithMask->setMaskLayer(0);
    ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), ccLayerTreeRoot.Pass(), hostImpl.get());
    expectTreesAreIdentical(layerTreeRoot.get(), ccLayerTreeRoot.get(), hostImpl.get());
}

TEST(TreeSynchronizerTest, synchronizeAnimations)
{
    DebugScopedSetImplThread impl;

    CCLayerTreeSettings settings;
    scoped_ptr<CCLayerTreeHostImpl> hostImpl = CCLayerTreeHostImpl::create(settings, 0);

    scoped_refptr<LayerChromium> layerTreeRoot = LayerChromium::create();

    FakeLayerAnimationControllerClient dummy;
    layerTreeRoot->setLayerAnimationController(FakeLayerAnimationController::create(&dummy).PassAs<CCLayerAnimationController>());

    EXPECT_FALSE(static_cast<FakeLayerAnimationController*>(layerTreeRoot->layerAnimationController())->synchronizedAnimations());

    scoped_ptr<CCLayerImpl> ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), scoped_ptr<CCLayerImpl>(), hostImpl.get());
    ccLayerTreeRoot = TreeSynchronizer::synchronizeTrees(layerTreeRoot.get(), ccLayerTreeRoot.Pass(), hostImpl.get());

    EXPECT_TRUE(static_cast<FakeLayerAnimationController*>(layerTreeRoot->layerAnimationController())->synchronizedAnimations());
}

} // namespace
