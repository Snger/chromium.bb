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

#ifndef INCLUDED_BLPWTK2_CONTENTRENDERERCLIENTIMPL_H
#define INCLUDED_BLPWTK2_CONTENTRENDERERCLIENTIMPL_H

#include <blpwtk2_config.h>

#include <content/public/renderer/content_renderer_client.h>
#include <content/public/renderer/render_thread_observer.h>

namespace blpwtk2 {

                        // ===============================
                        // class ContentRendererClientImpl
                        // ===============================

// This interface allows us to add hooks to the "renderer" portion of the
// content module.  This is created during the startup process.
class ContentRendererClientImpl : public content::ContentRendererClient
{
    DISALLOW_COPY_AND_ASSIGN(ContentRendererClientImpl);

  public:
    ContentRendererClientImpl();
    ~ContentRendererClientImpl() final;

    void RenderViewCreated(content::RenderView *render_view) override;
        // Notifies that a new RenderView has been created.

    void PrepareErrorPage(
        content::RenderFrame        *render_frame,
        const blink::WebURLRequest&  failed_request,
        const blink::WebURLError&    error,
        std::string                 *error_html,
        base::string16              *error_description) override;
        // Returns the information to display when a navigation error occurs.
        // If |error_html| is not null then it may be set to a HTML page
        // containing the details of the error and maybe links to more info.
        // If |error_description| is not null it may be set to contain a brief
        // message describing the error that has occurred.
        // Either of the out parameters may be not written to in certain cases
        // (lack of information on the error code) so the caller should take
        // care to initialize the string values with safe defaults before the
        // call.

    content::ResourceLoaderBridge* OverrideResourceLoaderBridge(
        const network::ResourceRequest *request) override;
        // Allows the embedder to override the ResourceLoaderBridge used.
        // If it returns NULL, the content layer will provide a bridge.

    bool OverrideCreatePlugin(
        content::RenderFrame           *render_frame,
        const blink::WebPluginParams&   params,
        blink::WebPlugin              **plugin) override;
        // Allows the embedder to override creating a plugin. If it returns
        // true, then |plugin| will contain the created plugin, although it
        // could be NULL. If it returns false, the content layer will create
        // the plugin.

    bool Dispatch(IPC::Message *msg) override;
};

}  // close namespace blpwtk2

#endif  // INCLUDED_BLPWTK2_CONTENTRENDERERCLIENTIMPL_H

// vim: ts=4 et

