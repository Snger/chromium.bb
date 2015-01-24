// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RoundedInnerRectClipper_h
#define RoundedInnerRectClipper_h

#include "core/rendering/PaintPhase.h"

namespace blink {

class FloatRoundedRect;
class LayoutRect;
class RenderObject;
struct PaintInfo;

enum RoundedInnerRectClipperBehavior {
    ApplyToDisplayListIfEnabled,
    ApplyToContext
};

class RoundedInnerRectClipper {
public:
    RoundedInnerRectClipper(RenderObject&, const PaintInfo&, const LayoutRect&, const FloatRoundedRect& clipRect, RoundedInnerRectClipperBehavior);
    ~RoundedInnerRectClipper();

private:
    RenderObject& m_renderer;
    const PaintInfo& m_paintInfo;
    bool m_useDisplayItemList;
};

} // namespace blink

#endif // RoundedInnerRectClipper_h
