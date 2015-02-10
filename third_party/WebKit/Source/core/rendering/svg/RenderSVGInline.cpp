/*
 * Copyright (C) 2006 Oliver Hunt <ojh16@student.canterbury.ac.nz>
 * Copyright (C) 2006 Apple Inc. All rights reserved.
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "core/rendering/svg/RenderSVGInline.h"

#include "core/layout/svg/SVGLayoutSupport.h"
#include "core/layout/svg/SVGResourcesCache.h"
#include "core/layout/svg/line/SVGInlineFlowBox.h"
#include "core/rendering/svg/RenderSVGText.h"
#include "core/svg/SVGAElement.h"

namespace blink {

bool RenderSVGInline::isChildAllowed(LayoutObject* child, const LayoutStyle& style) const
{
    if (child->isText())
        return SVGLayoutSupport::isRenderableTextNode(child);

    if (isSVGAElement(*node())) {
        // Disallow direct descendant 'a'.
        if (isSVGAElement(*child->node()))
            return false;
    }

    if (!child->isSVGInline() && !child->isSVGInlineText())
        return false;

    return RenderInline::isChildAllowed(child, style);
}

RenderSVGInline::RenderSVGInline(Element* element)
    : RenderInline(element)
{
    setAlwaysCreateLineBoxes();
}

InlineFlowBox* RenderSVGInline::createInlineFlowBox()
{
    InlineFlowBox* box = new SVGInlineFlowBox(*this);
    box->setHasVirtualLogicalHeight();
    return box;
}

FloatRect RenderSVGInline::objectBoundingBox() const
{
    if (const LayoutObject* object = RenderSVGText::locateRenderSVGTextAncestor(this))
        return object->objectBoundingBox();

    return FloatRect();
}

FloatRect RenderSVGInline::strokeBoundingBox() const
{
    if (const LayoutObject* object = RenderSVGText::locateRenderSVGTextAncestor(this))
        return object->strokeBoundingBox();

    return FloatRect();
}

FloatRect RenderSVGInline::paintInvalidationRectInLocalCoordinates() const
{
    if (const LayoutObject* object = RenderSVGText::locateRenderSVGTextAncestor(this))
        return object->paintInvalidationRectInLocalCoordinates();

    return FloatRect();
}

LayoutRect RenderSVGInline::clippedOverflowRectForPaintInvalidation(const LayoutLayerModelObject* paintInvalidationContainer, const PaintInvalidationState* paintInvalidationState) const
{
    return SVGLayoutSupport::clippedOverflowRectForPaintInvalidation(this, paintInvalidationContainer, paintInvalidationState);
}

void RenderSVGInline::mapLocalToContainer(const LayoutLayerModelObject* paintInvalidationContainer, TransformState& transformState, MapCoordinatesFlags, bool* wasFixed, const PaintInvalidationState* paintInvalidationState) const
{
    SVGLayoutSupport::mapLocalToContainer(this, paintInvalidationContainer, transformState, wasFixed, paintInvalidationState);
}

const LayoutObject* RenderSVGInline::pushMappingToContainer(const LayoutLayerModelObject* ancestorToStopAt, RenderGeometryMap& geometryMap) const
{
    return SVGLayoutSupport::pushMappingToContainer(this, ancestorToStopAt, geometryMap);
}

void RenderSVGInline::absoluteQuads(Vector<FloatQuad>& quads, bool* wasFixed) const
{
    const LayoutObject* object = RenderSVGText::locateRenderSVGTextAncestor(this);
    if (!object)
        return;

    FloatRect textBoundingBox = object->strokeBoundingBox();
    for (InlineFlowBox* box = firstLineBox(); box; box = box->nextLineBox())
        quads.append(localToAbsoluteQuad(FloatRect(textBoundingBox.x() + box->x(), textBoundingBox.y() + box->y(), box->logicalWidth(), box->logicalHeight()), false, wasFixed));
}

void RenderSVGInline::willBeDestroyed()
{
    SVGResourcesCache::clientDestroyed(this);
    RenderInline::willBeDestroyed();
}

void RenderSVGInline::styleDidChange(StyleDifference diff, const LayoutStyle* oldStyle)
{
    if (diff.needsFullLayout())
        setNeedsBoundariesUpdate();

    RenderInline::styleDidChange(diff, oldStyle);
    SVGResourcesCache::clientStyleChanged(this, diff, styleRef());
}

void RenderSVGInline::addChild(LayoutObject* child, LayoutObject* beforeChild)
{
    RenderInline::addChild(child, beforeChild);
    SVGResourcesCache::clientWasAddedToTree(child, child->styleRef());

    if (RenderSVGText* textRenderer = RenderSVGText::locateRenderSVGTextAncestor(this))
        textRenderer->subtreeChildWasAdded(child);
}

void RenderSVGInline::removeChild(LayoutObject* child)
{
    SVGResourcesCache::clientWillBeRemovedFromTree(child);

    RenderSVGText* textRenderer = RenderSVGText::locateRenderSVGTextAncestor(this);
    if (!textRenderer) {
        RenderInline::removeChild(child);
        return;
    }
    Vector<SVGTextLayoutAttributes*, 2> affectedAttributes;
    textRenderer->subtreeChildWillBeRemoved(child, affectedAttributes);
    RenderInline::removeChild(child);
    textRenderer->subtreeChildWasRemoved(affectedAttributes);
}

}
