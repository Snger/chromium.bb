// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ThrottledTextureUploader_h
#define ThrottledTextureUploader_h

#include "TextureUploader.h"

#include "base/basictypes.h"
#include <deque>
#include <wtf/Deque.h>

namespace WebKit {
class WebGraphicsContext3D;
}

namespace cc {

class ThrottledTextureUploader : public TextureUploader {
public:
    static PassOwnPtr<ThrottledTextureUploader> create(WebKit::WebGraphicsContext3D* context)
    {
        return adoptPtr(new ThrottledTextureUploader(context));
    }
    virtual ~ThrottledTextureUploader();

    virtual size_t numPendingUploads() OVERRIDE;
    virtual double estimatedTexturesPerSecond() OVERRIDE;
    virtual void beginUploads() OVERRIDE;
    virtual void endUploads() OVERRIDE;
    virtual void uploadTexture(CCResourceProvider*, Parameters) OVERRIDE;

private:
    class Query {
    public:
        static PassOwnPtr<Query> create(WebKit::WebGraphicsContext3D* context) { return adoptPtr(new Query(context)); }

        virtual ~Query();

        void begin();
        void end(size_t texturesUploaded);
        bool isPending();
        void wait();
        unsigned value();
        size_t texturesUploaded();

    private:
        explicit Query(WebKit::WebGraphicsContext3D*);

        WebKit::WebGraphicsContext3D* m_context;
        unsigned m_queryId;
        unsigned m_value;
        bool m_hasValue;
        size_t m_texturesUploaded;
    };

    ThrottledTextureUploader(WebKit::WebGraphicsContext3D*);

    void processQueries();

    WebKit::WebGraphicsContext3D* m_context;
    Deque<OwnPtr<Query> > m_pendingQueries;
    Deque<OwnPtr<Query> > m_availableQueries;
    std::deque<double> m_texturesPerSecondHistory;
    size_t m_texturesUploaded;
    size_t m_numPendingTextureUploads;

    DISALLOW_COPY_AND_ASSIGN(ThrottledTextureUploader);
};

}

#endif
