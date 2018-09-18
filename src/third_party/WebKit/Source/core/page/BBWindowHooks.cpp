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
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/editing/EditingUtilities.h"
#include "core/editing/Editor.h"
#include "core/editing/EphemeralRange.h"
#include "core/editing/iterators/SearchBuffer.h"
#include "core/editing/markers/DocumentMarkerController.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/frame/Settings.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

BBWindowHooks::BBWindowHooks(LocalFrame* frame)
    : DOMWindowClient(frame)
{
}

bool BBWindowHooks::matchSelector(Node *node, const String& selector)
{
    if (node->IsElementNode() && !selector.IsEmpty()) {
        Element *e = ToElement(node);
        return e->matches(AtomicString(selector), IGNORE_EXCEPTION_FOR_TESTING);
    }
    return false;
}

void BBWindowHooks::appendTextContent(Node *node, StringBuilder& content,
                                      const String& excluder, const String& mask)
{
    if (matchSelector(node, excluder)) {
        content.Append(mask);
    } else if (node->getNodeType() == Node::kTextNode) {
        content.Append((static_cast<CharacterData*>(node))->data());
    } else {
        if (node->HasTagName(HTMLNames::brTag)) {
            content.Append('\n');
        } else {
            for (Node* child = node->firstChild(); child;
                child = child->nextSibling()) {
                    appendTextContent(child, content, excluder, mask);
                    if (!matchSelector(child, excluder) && isBlock(child) &&
                        !(child->HasTagName(HTMLNames::tdTag) ||
                        child->HasTagName(HTMLNames::thTag))) {
                            content.Append('\n');
                    } else if (child->nextSibling()) {
                        if (!matchSelector(child->nextSibling(), excluder)) {
                            if (child->HasTagName(HTMLNames::tdTag) ||
                                child->HasTagName(HTMLNames::thTag)) {
                                    content.Append('\t');
                            } else if (child->HasTagName(HTMLNames::trTag)
                                || isBlock(child->nextSibling())) {
                                    content.Append('\n');
                            }
                        }
                    }
            }
        }
    }
}

bool BBWindowHooks::isBlock(Node* node)
{
    return blink::IsEnclosingBlock(node);
}

String BBWindowHooks::getPlainText(Node* node, const String& excluder, const String& mask)
{
    StringBuilder content;
    appendTextContent(node, content, excluder, mask);
    return content.ToString();
}

void BBWindowHooks::setTextMatchMarkerVisibility(Document* document, bool highlight)
{
    LocalFrame *frame = document->GetFrame();
    frame->GetEditor().SetMarkedTextMatchesAreHighlighted(highlight);
}

bool BBWindowHooks::checkSpellingForRange(Range* range)
{
    Node* ancestor = range->commonAncestorContainer();

    if (!ancestor) {
        return false;
    }

    LocalFrame *frame = range->OwnerDocument().GetFrame();
    // VisibleSelection s(range);
    frame->GetSpellChecker().ReplaceMisspelledRange(range->toString());
    // frame->GetSpellChecker().MarkMisspellingsAndBadGrammar(s, false, s);
    return true;
}

void BBWindowHooks::removeMarker(Range* range, long mask)
{
    range->OwnerDocument().Markers().RemoveMarkersInRange(
        EphemeralRange(range),
        DocumentMarker::MarkerTypes(mask));
}

void BBWindowHooks::addHighlightMarker(Range* range, long foregroundColor, long backgroundColor, bool includeNonSelectableText)
{
    range->OwnerDocument().Markers().AddHighlightMarker(
        EphemeralRange(range),
        Color(foregroundColor),
        Color(backgroundColor).BlendWithWhite(),
        includeNonSelectableText);
}

void BBWindowHooks::removeHighlightMarker(Range *range)
{
    removeMarker(range, DocumentMarker::kHighlight);
}

Range* BBWindowHooks::findPlainText(Range* range, const String& target, long options)
{
    EphemeralRange result = blink::FindPlainText(EphemeralRange(range), target, options);
    return Range::Create(result.GetDocument(), result.StartPosition(), result.EndPosition());
}

bool BBWindowHooks::checkSpellingForNode(Node* node)
{
    if (!node->IsElementNode()) {
        return false;
    }

    Element* e = ToElement(node);

    if (e && e->IsSpellCheckingEnabled()) {
        LocalFrame *frame = e->GetDocument().GetFrame();
        if (frame) {
            // VisibleSelection s(
            //     FirstPositionInOrBeforeNode(e),
            //     LastPositionInOrAfterNode(e));
            frame->GetSpellChecker().ReplaceMisspelledRange(Range::Create(e->GetDocument())->toString());
            // frame->GetSpellChecker().MarkMisspellingsAndBadGrammar(s, false, s);
        }
        return true;
    }
    return false;
}

DOMRectReadOnly* BBWindowHooks::getAbsoluteCaretRectAtOffset(Node* node, long offset)
{
    VisiblePosition visiblePos = CreateVisiblePosition(Position(node, offset));
    IntRect rc = AbsoluteCaretBoundsOf(visiblePos);
    return DOMRectReadOnly::FromIntRect(rc);
}

bool BBWindowHooks::isOverwriteModeEnabled(Document* document)
{
    LocalFrame *frame = document->GetFrame();
    return frame->GetEditor().IsOverwriteModeEnabled();
}

void BBWindowHooks::toggleOverwriteMode(Document* document)
{
    document->GetFrame()->GetEditor().ToggleOverwriteModeEnabled();
}

DEFINE_TRACE(BBWindowHooks) {
  DOMWindowClient::Trace(visitor);
  Supplementable<BBWindowHooks>::Trace(visitor);
}

} // namespace blink
