// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "WebContentLayerImpl.h"

#include "ContentLayerChromium.h"
#include "SkMatrix44.h"
#include <public/WebContentLayerClient.h>
#include <public/WebFloatPoint.h>
#include <public/WebFloatRect.h>
#include <public/WebRect.h>
#include <public/WebSize.h>

using namespace WebCore;

namespace WebKit {

WebContentLayer* WebContentLayer::create(WebContentLayerClient* client)
{
    return new WebContentLayerImpl(client);
}

WebContentLayerImpl::WebContentLayerImpl(WebContentLayerClient* client)
    : m_layer(adoptPtr(new WebLayerImpl(ContentLayerChromium::create(this))))
    , m_client(client)
{
    m_layer->layer()->setIsDrawable(true);
}

WebContentLayerImpl::~WebContentLayerImpl()
{
    static_cast<ContentLayerChromium*>(m_layer->layer())->clearClient();
}

WebLayer* WebContentLayerImpl::layer()
{
    return m_layer.get();
}

void WebContentLayerImpl::setDoubleSided(bool doubleSided)
{
    m_layer->layer()->setDoubleSided(doubleSided);
}

void WebContentLayerImpl::setContentsScale(float scale)
{
    m_layer->layer()->setContentsScale(scale);
}

void WebContentLayerImpl::setUseLCDText(bool enable)
{
    m_layer->layer()->setUseLCDText(enable);
}

void WebContentLayerImpl::setDrawCheckerboardForMissingTiles(bool enable)
{
    m_layer->layer()->setDrawCheckerboardForMissingTiles(enable);
}


void WebContentLayerImpl::paintContents(SkCanvas* canvas, const IntRect& clip, FloatRect& opaque)
{
    if (!m_client)
        return;
    WebFloatRect webOpaque;
    m_client->paintContents(canvas, WebRect(clip), webOpaque);
    opaque = webOpaque;
}

} // namespace WebKit
