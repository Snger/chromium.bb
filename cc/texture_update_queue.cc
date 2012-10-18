// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "CCTextureUpdateQueue.h"

#include "CCPrioritizedTexture.h"

namespace cc {

CCTextureUpdateQueue::CCTextureUpdateQueue()
{
}

CCTextureUpdateQueue::~CCTextureUpdateQueue()
{
}

void CCTextureUpdateQueue::appendFullUpload(const ResourceUpdate& upload)
{
    m_fullEntries.push_back(upload);
}

void CCTextureUpdateQueue::appendPartialUpload(const ResourceUpdate& upload)
{
    m_partialEntries.push_back(upload);
}

void CCTextureUpdateQueue::appendCopy(TextureCopier::Parameters copy)
{
    m_copyEntries.push_back(copy);
}

void CCTextureUpdateQueue::clearUploadsToEvictedResources()
{
    clearUploadsToEvictedResources(m_fullEntries);
    clearUploadsToEvictedResources(m_partialEntries);
}

void CCTextureUpdateQueue::clearUploadsToEvictedResources(std::deque<ResourceUpdate>& entryQueue)
{
    std::deque<ResourceUpdate> temp;
    entryQueue.swap(temp);
    while (temp.size()) {
        ResourceUpdate upload = temp.front();
        temp.pop_front();
        if (!upload.texture->backingResourceWasEvicted())
            entryQueue.push_back(upload);
    }
}

ResourceUpdate CCTextureUpdateQueue::takeFirstFullUpload()
{
    ResourceUpdate first = m_fullEntries.front();
    m_fullEntries.pop_front();
    return first;
}

ResourceUpdate CCTextureUpdateQueue::takeFirstPartialUpload()
{
    ResourceUpdate first = m_partialEntries.front();
    m_partialEntries.pop_front();
    return first;
}

TextureCopier::Parameters CCTextureUpdateQueue::takeFirstCopy()
{
    TextureCopier::Parameters first = m_copyEntries.front();
    m_copyEntries.pop_front();
    return first;
}

bool CCTextureUpdateQueue::hasMoreUpdates() const
{
    return m_fullEntries.size() || m_partialEntries.size() || m_copyEntries.size();
}

}
