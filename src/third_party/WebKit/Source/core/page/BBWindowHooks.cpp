/*
 * Copyright (C) 2013 Bloomberg Finance L.P.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS," WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "core/page/BBWindowHooks.h"

#include "core/dom/CharacterData.h"
#include "core/dom/ClientRect.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/iterators/SearchBuffer.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/frame/Settings.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

BBWindowHooks::BBWindowHooks(LocalFrame *frame)
    : DOMWindowProperty(frame)
{
}

DEFINE_TRACE(BBWindowHooks)
{
    DOMWindowProperty::trace(visitor);
}

bool BBWindowHooks::matchSelector(Node *node, const String& selector)
{
    if (node->isElementNode() && !selector.isEmpty()) {
        Element *e = toElement(node);
        return e->matches(selector, IGNORE_EXCEPTION);
    }
    return false;
}

void BBWindowHooks::appendTextContent(Node *node, StringBuilder& content,
                                      const String& excluder, const String& mask)
{
    if (matchSelector(node, excluder)) {
        content.append(mask);
    } else if (node->getNodeType() == Node::TEXT_NODE) {
        content.append((static_cast<CharacterData*>(node))->data());
    } else {
        if (node->hasTagName(HTMLNames::brTag)) {
            content.append('\n');
        } else {
            for (Node* child = node->firstChild(); child;
                child = child->nextSibling()) {
                    appendTextContent(child, content, excluder, mask);
                    if (!matchSelector(child, excluder) && isBlock(child) &&
                        !(child->hasTagName(HTMLNames::tdTag) ||
                        child->hasTagName(HTMLNames::thTag))) {
                            content.append('\n');
                    } else if (child->nextSibling()) {
                        if (!matchSelector(child->nextSibling(), excluder)) {
                            if (child->hasTagName(HTMLNames::tdTag) ||
                                child->hasTagName(HTMLNames::thTag)) {
                                    content.append('\t');
                            } else if (child->hasTagName(HTMLNames::trTag)
                                || isBlock(child->nextSibling())) {
                                    content.append('\n');
                            }
                        }
                    }
            }
        }
    }
}

bool BBWindowHooks::isBlock(Node* node)
{
    return blink::isEnclosingBlock(node);
}

String BBWindowHooks::getPlainText(Node* node, const String& excluder, const String& mask)
{
    StringBuilder content;
    appendTextContent(node, content, excluder, mask);
    return content.toString();
}

void BBWindowHooks::setTextMatchMarkerVisibility(Document* document, bool highlight)
{
    LocalFrame *frame = document->frame();
    frame->editor().setMarkedTextMatchesAreHighlighted(highlight);
}

bool BBWindowHooks::checkSpellingForRange(Range* range)
{
    Node* ancestor = range->commonAncestorContainer();

    if (!ancestor) {
        return false;
    }

    LocalFrame *frame = range->ownerDocument().frame();
    VisibleSelection s(range->startPosition(), range->endPosition());
    frame->spellChecker().clearMisspellingsAndBadGrammar(s);
    frame->spellChecker().markMisspellingsAndBadGrammar(s, false, s);
    return true;
}

void BBWindowHooks::removeMarker(Range* range, long mask, long removeMarkerFlag)
{
    range->ownerDocument().markers().removeMarkers(
        EphemeralRange(range),
        DocumentMarker::MarkerTypes(mask),
        static_cast<DocumentMarkerController::RemovePartiallyOverlappingMarkerOrNot>(removeMarkerFlag));
}

void BBWindowHooks::addMarker(Range* range, long markerType)
{
    range->ownerDocument().markers().addMarker(
        range->startPosition(),
        range->endPosition(),
        static_cast<DocumentMarker::MarkerType>(markerType));

}

void BBWindowHooks::addHighlightMarker(Range* range, long foregroundColor, long backgroundColor)
{
    range->ownerDocument().markers().addHighlightMarker(
        range->startPosition(),
        range->endPosition(),
        Color(foregroundColor),
        Color(backgroundColor).blendWithWhite());
}

Range* BBWindowHooks::findPlainText(Range* range, const String& target, long options)
{
    EphemeralRange result = blink::findPlainText(EphemeralRange(range), target, options);
    return Range::create(result.document(), result.startPosition(), result.endPosition());
}

bool BBWindowHooks::checkSpellingForNode(Node* node)
{
    if (!node->isElementNode()) {
        return false;
    }

    Element* e = toElement(node);

    if (e && e->isSpellCheckingEnabled()) {
        LocalFrame *frame = e->document().frame();
        if (frame) {
            VisibleSelection s(
                firstPositionInOrBeforeNode(e),
                lastPositionInOrAfterNode(e));
            if (frame->settings() && !frame->settings()->asynchronousSpellCheckingEnabled()) {
                frame->spellChecker().clearMisspellingsAndBadGrammar(s);
            }
            frame->spellChecker().markMisspellingsAndBadGrammar(s, false, s);
        }
        return true;
    }
    return false;
}

ClientRect* BBWindowHooks::getAbsoluteCaretRectAtOffset(Node* node, long offset)
{
    VisiblePosition visiblePos = createVisiblePosition(Position(node, offset));
    IntRect rc = absoluteCaretBoundsOf(visiblePos);
    return ClientRect::create(rc);
}

} // namespace blink
