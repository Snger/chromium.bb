/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "core/rendering/style/ContentData.h"

#include "core/layout/LayoutCounter.h"
#include "core/rendering/RenderImage.h"
#include "core/rendering/RenderImageResource.h"
#include "core/rendering/RenderImageResourceStyleImage.h"
#include "core/rendering/RenderQuote.h"
#include "core/rendering/RenderTextFragment.h"
#include "core/rendering/style/RenderStyle.h"

namespace blink {

PassOwnPtr<ContentData> ContentData::create(PassRefPtr<StyleImage> image)
{
    return adoptPtr(new ImageContentData(image));
}

PassOwnPtr<ContentData> ContentData::create(const String& text)
{
    return adoptPtr(new TextContentData(text));
}

PassOwnPtr<ContentData> ContentData::create(PassOwnPtr<CounterContent> counter)
{
    return adoptPtr(new CounterContentData(counter));
}

PassOwnPtr<ContentData> ContentData::create(QuoteType quote)
{
    return adoptPtr(new QuoteContentData(quote));
}

PassOwnPtr<ContentData> ContentData::clone() const
{
    OwnPtr<ContentData> result = cloneInternal();

    ContentData* lastNewData = result.get();
    for (const ContentData* contentData = next(); contentData; contentData = contentData->next()) {
        OwnPtr<ContentData> newData = contentData->cloneInternal();
        lastNewData->setNext(newData.release());
        lastNewData = lastNewData->next();
    }

    return result.release();
}

LayoutObject* ImageContentData::createRenderer(Document& doc, RenderStyle& pseudoStyle) const
{
    RenderImage* image = RenderImage::createAnonymous(&doc);
    image->setPseudoStyle(&pseudoStyle);
    if (m_image)
        image->setImageResource(RenderImageResourceStyleImage::create(m_image.get()));
    else
        image->setImageResource(RenderImageResource::create());
    return image;
}

LayoutObject* TextContentData::createRenderer(Document& doc, RenderStyle& pseudoStyle) const
{
    LayoutObject* renderer = new RenderTextFragment(&doc, m_text.impl());
    renderer->setPseudoStyle(&pseudoStyle);
    return renderer;
}

LayoutObject* CounterContentData::createRenderer(Document& doc, RenderStyle& pseudoStyle) const
{
    LayoutObject* renderer = new LayoutCounter(&doc, *m_counter);
    renderer->setPseudoStyle(&pseudoStyle);
    return renderer;
}

LayoutObject* QuoteContentData::createRenderer(Document& doc, RenderStyle& pseudoStyle) const
{
    LayoutObject* renderer = new RenderQuote(&doc, m_quote);
    renderer->setPseudoStyle(&pseudoStyle);
    return renderer;
}

} // namespace blink
