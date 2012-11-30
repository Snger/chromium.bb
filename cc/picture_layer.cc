// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/picture_layer.h"
#include "cc/picture_layer_impl.h"
#include "ui/gfx/rect_conversions.h"

namespace cc {

scoped_refptr<PictureLayer> PictureLayer::create(ContentLayerClient* client) {
  return make_scoped_refptr(new PictureLayer(client));
}

PictureLayer::PictureLayer(ContentLayerClient* client) :
  client_(client) {
}

PictureLayer::~PictureLayer() {
}

bool PictureLayer::drawsContent() const {
  return Layer::drawsContent() && client_;
}

scoped_ptr<LayerImpl> PictureLayer::createLayerImpl() {
  return PictureLayerImpl::create(id()).PassAs<LayerImpl>();
}

void PictureLayer::pushPropertiesTo(LayerImpl* base_layer) {
  Layer::pushPropertiesTo(base_layer);
  PictureLayerImpl* layer_impl = static_cast<PictureLayerImpl*>(base_layer);
  pile_.PushPropertiesTo(layer_impl->pile_);

  // TODO(enne): Once we have two trees on the impl side, we need to
  // sync the active layer's tiles prior to this Invalidate call since it
  // will make new tiles for anything intersecting the invalidation.
  layer_impl->tilings_.Invalidate(pile_invalidation_);
  pile_invalidation_.Clear();
}

void PictureLayer::setNeedsDisplayRect(const gfx::RectF& layer_rect) {
  gfx::Rect rect = gfx::ToEnclosedRect(layer_rect);
  pending_invalidation_.Union(rect);
  Layer::setNeedsDisplayRect(layer_rect);
}

void PictureLayer::update(ResourceUpdateQueue&, const OcclusionTracker*,
                    RenderingStats& stats) {
  if (pile_.size() == bounds() && pending_invalidation_.IsEmpty())
    return;

  pile_.Resize(bounds());

  // Calling paint in WebKit can sometimes cause invalidations, so save
  // off the invalidation prior to calling update.
  pile_invalidation_.Swap(pending_invalidation_);
  pending_invalidation_.Clear();

  pile_.Update(client_, pile_invalidation_, stats);
}

}  // namespace cc
