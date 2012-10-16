// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCScopedTexture.h"

#include "CCRenderer.h"
#include "CCSingleThreadProxy.h" // For DebugScopedSetImplThread
#include "GraphicsContext3D.h"
#include "cc/test/fake_graphics_context.h"
#include "cc/test/tiled_layer_test_common.h"
#include "testing/gtest/include/gtest/gtest.h"

using namespace cc;
using namespace WebKit;
using namespace WebKitTests;

namespace {

TEST(CCScopedTextureTest, NewScopedTexture)
{
    scoped_ptr<CCGraphicsContext> context(createFakeCCGraphicsContext());
    DebugScopedSetImplThread implThread;
    OwnPtr<CCResourceProvider> resourceProvider(CCResourceProvider::create(context.get()));
    scoped_ptr<CCScopedTexture> texture = CCScopedTexture::create(resourceProvider.get());

    // New scoped textures do not hold a texture yet.
    EXPECT_EQ(0u, texture->id());

    // New scoped textures do not have a size yet.
    EXPECT_EQ(IntSize(), texture->size());
    EXPECT_EQ(0u, texture->bytes());
}

TEST(CCScopedTextureTest, CreateScopedTexture)
{
    scoped_ptr<CCGraphicsContext> context(createFakeCCGraphicsContext());
    DebugScopedSetImplThread implThread;
    OwnPtr<CCResourceProvider> resourceProvider(CCResourceProvider::create(context.get()));
    scoped_ptr<CCScopedTexture> texture = CCScopedTexture::create(resourceProvider.get());
    texture->allocate(CCRenderer::ImplPool, IntSize(30, 30), GraphicsContext3D::RGBA, CCResourceProvider::TextureUsageAny);

    // The texture has an allocated byte-size now.
    size_t expectedBytes = 30 * 30 * 4;
    EXPECT_EQ(expectedBytes, texture->bytes());

    EXPECT_LT(0u, texture->id());
    EXPECT_EQ(GraphicsContext3D::RGBA, texture->format());
    EXPECT_EQ(IntSize(30, 30), texture->size());
}

TEST(CCScopedTextureTest, ScopedTextureIsDeleted)
{
    scoped_ptr<CCGraphicsContext> context(createFakeCCGraphicsContext());
    DebugScopedSetImplThread implThread;
    OwnPtr<CCResourceProvider> resourceProvider(CCResourceProvider::create(context.get()));

    {
        scoped_ptr<CCScopedTexture> texture = CCScopedTexture::create(resourceProvider.get());

        EXPECT_EQ(0u, resourceProvider->numResources());
        texture->allocate(CCRenderer::ImplPool, IntSize(30, 30), GraphicsContext3D::RGBA, CCResourceProvider::TextureUsageAny);
        EXPECT_LT(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());
    }

    EXPECT_EQ(0u, resourceProvider->numResources());

    {
        scoped_ptr<CCScopedTexture> texture = CCScopedTexture::create(resourceProvider.get());
        EXPECT_EQ(0u, resourceProvider->numResources());
        texture->allocate(CCRenderer::ImplPool, IntSize(30, 30), GraphicsContext3D::RGBA, CCResourceProvider::TextureUsageAny);
        EXPECT_LT(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());
        texture->free();
        EXPECT_EQ(0u, resourceProvider->numResources());
    }
}

TEST(CCScopedTextureTest, LeakScopedTexture)
{
    scoped_ptr<CCGraphicsContext> context(createFakeCCGraphicsContext());
    DebugScopedSetImplThread implThread;
    OwnPtr<CCResourceProvider> resourceProvider(CCResourceProvider::create(context.get()));

    {
        scoped_ptr<CCScopedTexture> texture = CCScopedTexture::create(resourceProvider.get());

        EXPECT_EQ(0u, resourceProvider->numResources());
        texture->allocate(CCRenderer::ImplPool, IntSize(30, 30), GraphicsContext3D::RGBA, CCResourceProvider::TextureUsageAny);
        EXPECT_LT(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());

        texture->leak();
        EXPECT_EQ(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());

        texture->free();
        EXPECT_EQ(0u, texture->id());
        EXPECT_EQ(1u, resourceProvider->numResources());
    }

    EXPECT_EQ(1u, resourceProvider->numResources());
}

}
