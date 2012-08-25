// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CCTextureUpdateController_h
#define CCTextureUpdateController_h

#include "CCTextureUpdateQueue.h"
#include "CCTimer.h"
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

class TextureCopier;
class TextureUploader;

class CCTextureUpdateController : public CCTimerClient {
    WTF_MAKE_NONCOPYABLE(CCTextureUpdateController);
public:
    static PassOwnPtr<CCTextureUpdateController> create(CCThread* thread, PassOwnPtr<CCTextureUpdateQueue> queue, CCResourceProvider* resourceProvider, TextureCopier* copier, TextureUploader* uploader)
    {
        return adoptPtr(new CCTextureUpdateController(thread, queue, resourceProvider, copier, uploader));
    }
    static size_t maxPartialTextureUpdates();
    static void updateTextures(CCResourceProvider*, TextureCopier*, TextureUploader*, CCTextureUpdateQueue*, size_t count);

    virtual ~CCTextureUpdateController();

    bool hasMoreUpdates() const;
    void updateMoreTextures(double monotonicTimeLimit);

    // CCTimerClient implementation.
    virtual void onTimerFired() OVERRIDE;

    // Virtual for testing.
    virtual double monotonicTimeNow() const;
    virtual double updateMoreTexturesTime() const;
    virtual size_t updateMoreTexturesSize() const;

protected:
    CCTextureUpdateController(CCThread*, PassOwnPtr<CCTextureUpdateQueue>, CCResourceProvider*, TextureCopier*, TextureUploader*);

    void updateMoreTexturesIfEnoughTimeRemaining();
    void updateMoreTexturesNow();

    OwnPtr<CCTimer> m_timer;
    OwnPtr<CCTextureUpdateQueue> m_queue;
    bool m_contentsTexturesPurged;
    CCResourceProvider* m_resourceProvider;
    TextureCopier* m_copier;
    TextureUploader* m_uploader;
    double m_monotonicTimeLimit;
    bool m_firstUpdateAttempt;
};

}

#endif // CCTextureUpdateController_h
