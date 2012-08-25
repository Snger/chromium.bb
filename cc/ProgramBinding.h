// Copyright 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ProgramBinding_h
#define ProgramBinding_h

#if USE(ACCELERATED_COMPOSITING)

#include <wtf/text/WTFString.h>

namespace WebKit {
class WebGraphicsContext3D;
}

namespace WebCore {

class ProgramBindingBase {
public:
    ProgramBindingBase();
    ~ProgramBindingBase();

    void init(WebKit::WebGraphicsContext3D*, const String& vertexShader, const String& fragmentShader);
    void link(WebKit::WebGraphicsContext3D*);
    void cleanup(WebKit::WebGraphicsContext3D*);

    unsigned program() const { ASSERT(m_initialized); return m_program; }
    bool initialized() const { return m_initialized; }

protected:

    unsigned loadShader(WebKit::WebGraphicsContext3D*, unsigned type, const String& shaderSource);
    unsigned createShaderProgram(WebKit::WebGraphicsContext3D*, unsigned vertexShader, unsigned fragmentShader);
    void cleanupShaders(WebKit::WebGraphicsContext3D*);

    unsigned m_program;
    unsigned m_vertexShaderId;
    unsigned m_fragmentShaderId;
    bool m_initialized;
};

template<class VertexShader, class FragmentShader>
class ProgramBinding : public ProgramBindingBase {
public:
    explicit ProgramBinding(WebKit::WebGraphicsContext3D* context)
    {
        ProgramBindingBase::init(context, m_vertexShader.getShaderString(), m_fragmentShader.getShaderString());
    }

    void initialize(WebKit::WebGraphicsContext3D* context, bool usingBindUniform)
    {
        ASSERT(context);
        ASSERT(m_program);
        ASSERT(!m_initialized);

        // Need to bind uniforms before linking
        if (!usingBindUniform)
            link(context);

        int baseUniformIndex = 0;
        m_vertexShader.init(context, m_program, usingBindUniform, &baseUniformIndex);
        m_fragmentShader.init(context, m_program, usingBindUniform, &baseUniformIndex);

        // Link after binding uniforms
        if (usingBindUniform)
            link(context);

        m_initialized = true;
    }

    const VertexShader& vertexShader() const { return m_vertexShader; }
    const FragmentShader& fragmentShader() const { return m_fragmentShader; }

private:

    VertexShader m_vertexShader;
    FragmentShader m_fragmentShader;
};

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)

#endif
