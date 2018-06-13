/*
 * Copyright (C) 2013 Bloomberg L.P. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/editing/commands/IndentBlockCommand.h"

#include "core/dom/Document.h"
#include "core/editing/EditingUtilities.h"
#include "core/html/HTMLElement.h"
#include "core/HTMLNames.h"

namespace blink {

using namespace HTMLNames;

static const HTMLQualifiedName& getBlockQuoteName(const Node* parent)
{
    if (isHTMLUListElement(parent))
        return ulTag;
    else if (isHTMLOListElement(parent))
        return olTag;
    else
        return blockquoteTag;
}

IndentBlockCommand::IndentBlockCommand(Document& document)
    : BlockCommand(document)
{
}

Element* IndentBlockCommand::createIndentBlock(const QualifiedName& tagName) const
{
    Element* element = createHTMLElement(document(), tagName);
    if (tagName.matches(blockquoteTag))
        element->setAttribute(styleAttr, "margin: 0 0 0 40px; border: none; padding: 0px;");
    return element;
}

void IndentBlockCommand::indentSiblings(Node* prpFirstSibling, Node* prpLastSibling, Node* lastNode, EditingState *editingState)
{
    Node* firstSibling = prpFirstSibling;
    Node* lastSibling = prpLastSibling;

    Element* blockForIndent = nullptr;
    Node* refChild = nullptr;
    bool needToMergeNextSibling = false;

    const blink::HTMLQualifiedName blockQName
        = getBlockQuoteName(firstSibling->parentNode());

    Node* previousSibling = previousRenderedSiblingExcludingWhitespace(firstSibling);
    if (previousSibling && previousSibling->isElementNode()
            && toElement(previousSibling)->hasTagName(blockQName)) {
        blockForIndent = toElement(previousSibling);
        firstSibling = previousSibling->nextSibling();
    }

    Node* nextSibling = nextRenderedSiblingExcludingWhitespace(lastSibling);
    if (nextSibling && nextSibling->hasTagName(blockQName) && !lastNode->isDescendantOf(nextSibling)) {
        if (!blockForIndent) {
            blockForIndent = toElement(nextSibling);
            refChild = nextSibling->firstChild();
        }
        else if (nextSibling->firstChild())
            needToMergeNextSibling = true;
        lastSibling = nextSibling->previousSibling();
    }

    if (!blockForIndent) {
        blockForIndent = createIndentBlock(blockQName);
        insertNodeBefore(blockForIndent, firstSibling, editingState);
    }

    moveRemainingSiblingsToNewParent(firstSibling, lastSibling->nextSibling(), blockForIndent, editingState, refChild);
    if (needToMergeNextSibling) {
        moveRemainingSiblingsToNewParent(nextSibling->firstChild(), nextSibling->lastChild()->nextSibling(), blockForIndent, editingState);
        removeNode(nextSibling, editingState);
    }
}

}
