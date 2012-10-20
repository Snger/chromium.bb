// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCCheckerboardDrawQuad_h
#define CCCheckerboardDrawQuad_h

#include "CCDrawQuad.h"
#include "base/memory/scoped_ptr.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {

#pragma pack(push, 4)

class CCCheckerboardDrawQuad : public CCDrawQuad {
public:
    static scoped_ptr<CCCheckerboardDrawQuad> create(const CCSharedQuadState*, const gfx::Rect&, SkColor);

    SkColor color() const { return m_color; };

    static const CCCheckerboardDrawQuad* materialCast(const CCDrawQuad*);
private:
    CCCheckerboardDrawQuad(const CCSharedQuadState*, const gfx::Rect&, SkColor);

    SkColor m_color;
};

#pragma pack(pop)

}

#endif
