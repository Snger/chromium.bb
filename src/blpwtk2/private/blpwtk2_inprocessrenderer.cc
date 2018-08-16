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

#include <blpwtk2_inprocessrenderer.h>
#include <blpwtk2_rendercompositor.h>

#include <blpwtk2_statics.h>

#include <base/message_loop/message_loop.h>
#include <content/common/in_process_child_thread_params.h>
#include <content/public/renderer/render_thread.h>
#include <content/renderer/render_process_impl.h>
#include <content/child/dwrite_font_proxy/dwrite_font_proxy_init_win.h>
#include <third_party/WebKit/public/platform/WebRuntimeFeatures.h>
#include <third_party/WebKit/public/web/win/WebFontRendering.h>
#include <ui/base/win/scoped_ole_initializer.h>
#include <ui/gfx/win/direct_write.h>
#include <ui/display/screen.h>
#include <ui/display/win/dpi.h>
#include <ui/display/win/screen_win.h>

namespace {

std::unique_ptr<ui::ScopedOleInitializer> g_oleInitializer;
std::unique_ptr<display::win::ScreenWin> g_screen;

}

namespace blpwtk2 {

static void InitDirectWrite()
{
    // This code is adapted from RendererMainPlatformDelegate::PlatformInitialize,
    // which is used for out-of-process renderers, but is not used for in-process
    // renderers.  So, we need to do it here.

    content::InitializeDWriteFontProxy();
    blink::WebFontRendering::SetDeviceScaleFactor(display::win::GetDPIScale());
}

                        // =============================
                        // class InProcessRendererThread
                        // =============================

class InProcessRendererThread : public base::Thread {
    // DATA
    scoped_refptr<base::SingleThreadTaskRunner> d_browserIOTaskRunner;
    mojo::edk::OutgoingBrokerClientInvitation* d_broker_client_invitation;
    std::string d_serviceToken;
    int d_mojoHandle;

    // Called just prior to starting the message loop
    void Init() override;

    // Called just after the message loop ends
    void CleanUp() override;

    DISALLOW_COPY_AND_ASSIGN(InProcessRendererThread);

public:
    InProcessRendererThread(
            const scoped_refptr<base::SingleThreadTaskRunner>& browserIOTaskRunner,
            mojo::edk::OutgoingBrokerClientInvitation*         broker_client_invitation,
            const std::string&                                 serviceToken,
            int                                                mojoHandle);
    ~InProcessRendererThread();
};

                        // -----------------------------
                        // class InProcessRendererThread
                        // -----------------------------

void InProcessRendererThread::Init()
{
    Statics::rendererMessageLoop = message_loop();
    InitDirectWrite();
    content::RenderThread::InitInProcessRenderer(
        content::InProcessChildThreadParams(d_browserIOTaskRunner,
                                            d_broker_client_invitation,
                                            d_serviceToken,
                                            d_mojoHandle));

    // No longer need to hold on to this reference.
    d_browserIOTaskRunner = nullptr;
}


void InProcessRendererThread::CleanUp()
{
    RenderCompositorContext::Terminate();

    content::RenderThread::CleanUpInProcessRenderer();
    Statics::rendererMessageLoop = 0;
}


InProcessRendererThread::InProcessRendererThread(
        const scoped_refptr<base::SingleThreadTaskRunner>& browserIOTaskRunner,
        mojo::edk::OutgoingBrokerClientInvitation*         broker_client_invitation,
        const std::string&                                 serviceToken,
        int                                                mojoHandle)
: base::Thread("BlpInProcRenderer")
, d_browserIOTaskRunner(browserIOTaskRunner)
, d_broker_client_invitation(broker_client_invitation)
, d_serviceToken(serviceToken)
, d_mojoHandle(mojoHandle)
{
    base::Thread::Options options;
    options.message_loop_type = base::MessageLoop::TYPE_UI;
    StartWithOptions(options);
}

InProcessRendererThread::~InProcessRendererThread()
{
    Stop();
}

                        // ------------------------
                        // struct InProcessRenderer
                        // ------------------------

static InProcessRendererThread *g_inProcessRendererThread;

// static
void InProcessRenderer::init(
        const scoped_refptr<base::SingleThreadTaskRunner>& browserIOTaskRunner,
        mojo::edk::OutgoingBrokerClientInvitation* broker_client_invitation,
        const std::string&                                 serviceToken,
        int                                                mojoHandle)
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(!g_inProcessRendererThread);
    DCHECK(!Statics::rendererMessageLoop);

    if (Statics::isOriginalThreadMode()) {
        DCHECK(Statics::isOriginalThreadMode());
        g_inProcessRendererThread =
            new InProcessRendererThread(browserIOTaskRunner,
                                        broker_client_invitation,
                                        serviceToken,
                                        mojoHandle);
    }
    else {
        Statics::rendererMessageLoop = base::MessageLoop::current();
        InitDirectWrite();
        content::RenderThread::InitInProcessRenderer(
            content::InProcessChildThreadParams(browserIOTaskRunner,
                                                broker_client_invitation,
                                                serviceToken,
                                                mojoHandle));

        DCHECK(Statics::rendererMessageLoop);

        g_oleInitializer.reset(new ui::ScopedOleInitializer());

        if (!display::Screen::GetScreen()) {
            g_screen.reset(new display::win::ScreenWin());
            display::Screen::SetScreenInstance(g_screen.get());
        }
    }
}

// static
void InProcessRenderer::cleanup()
{
    DCHECK(Statics::isInApplicationMainThread());

    if (Statics::isOriginalThreadMode()) {
        DCHECK(g_inProcessRendererThread);
        delete g_inProcessRendererThread;
        g_inProcessRendererThread = 0;
    }
    else {
        RenderCompositorContext::Terminate();

        g_oleInitializer.reset();

        if (g_screen) {
            display::Screen::SetScreenInstance(nullptr);
            g_screen.reset();
        }

        DCHECK(Statics::rendererMessageLoop);
        DCHECK(!g_inProcessRendererThread);
        content::RenderThread::CleanUpInProcessRenderer();
        Statics::rendererMessageLoop = 0;
    }
}

// static
scoped_refptr<base::SingleThreadTaskRunner> InProcessRenderer::ioTaskRunner()
{
    DCHECK(Statics::isInApplicationMainThread());
    DCHECK(Statics::isRendererMainThreadMode());
    return content::RenderThread::IOTaskRunner();
}

}  // close namespace blpwtk2

// vim: ts=4 et

