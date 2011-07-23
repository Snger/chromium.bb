// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/print_web_view_helper.h"

#import <AppKit/AppKit.h>

#include "base/logging.h"
#include "base/mac/scoped_nsautorelease_pool.h"
#include "base/metrics/histogram.h"
#include "chrome/common/print_messages.h"
#include "printing/metafile.h"
#include "printing/metafile_impl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFrame.h"

#if defined(USE_SKIA)
#include "printing/metafile_skia_wrapper.h"
#include "skia/ext/vector_canvas.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCanvas.h"
#endif

using WebKit::WebFrame;

void PrintWebViewHelper::PrintPageInternal(
    const PrintMsg_PrintPage_Params& params,
    const gfx::Size& canvas_size,
    WebFrame* frame) {
  printing::NativeMetafile metafile;
  if (!metafile.Init())
    return;

  float scale_factor = frame->getPrintPageShrink(params.page_number);
  int page_number = params.page_number;

  // Render page for printing.
  gfx::Rect content_area(params.params.printable_size);
  RenderPage(params.params.printable_size, content_area, scale_factor,
      page_number, frame, &metafile);
  metafile.FinishDocument();

  PrintHostMsg_DidPrintPage_Params page_params;
  page_params.data_size = metafile.GetDataSize();
  page_params.page_number = page_number;
  page_params.document_cookie = params.params.document_cookie;
  page_params.actual_shrink = scale_factor;
  page_params.page_size = params.params.page_size;
  page_params.content_area = gfx::Rect(params.params.margin_left,
                                       params.params.margin_top,
                                       params.params.printable_size.width(),
                                       params.params.printable_size.height());

  // Ask the browser to create the shared memory for us.
  if (!CopyMetafileDataToSharedMem(&metafile,
                                   &(page_params.metafile_data_handle))) {
    page_params.data_size = 0;
  }

  Send(new PrintHostMsg_DidPrintPage(routing_id(), page_params));
}

void PrintWebViewHelper::RenderPreviewPage(int page_number) {
  float scale_factor = print_preview_context_.frame()->getPrintPageShrink(0);
  PrintMsg_Print_Params printParams = print_preview_context_.print_params();
  gfx::Rect content_area(printParams.margin_left, printParams.margin_top,
                         printParams.printable_size.width(),
                         printParams.printable_size.height());

  base::TimeTicks begin_time = base::TimeTicks::Now();
  RenderPage(printParams.page_size, content_area, scale_factor, page_number,
             print_preview_context_.frame(), print_preview_context_.metafile());
  print_preview_context_.RenderedPreviewPage(
      base::TimeTicks::Now() - begin_time);
  PreviewPageRendered(page_number);
}

void PrintWebViewHelper::RenderPage(
    const gfx::Size& page_size, const gfx::Rect& content_area,
    const float& scale_factor, int page_number, WebFrame* frame,
    printing::Metafile* metafile) {

  {
#if defined(USE_SKIA)
    SkDevice* device = metafile->StartPageForVectorCanvas(
        page_number, page_size, content_area, scale_factor);
    if (!device)
      return;

    SkRefPtr<skia::VectorCanvas> canvas = new skia::VectorCanvas(device);
    canvas->unref();  // SkRefPtr and new both took a reference.
    WebKit::WebCanvas* canvasPtr = canvas.get();
    printing::MetafileSkiaWrapper::SetMetafileOnCanvas(canvasPtr, metafile);
#else
    bool success = metafile->StartPage(page_size, content_area, scale_factor);
    DCHECK(success);
    // printPage can create autoreleased references to |context|. PDF contexts
    // don't write all their data until they are destroyed, so we need to make
    // certain that there are no lingering references.
    base::mac::ScopedNSAutoreleasePool pool;
    CGContextRef cgContext = metafile->context();
    CGContextRef canvasPtr = cgContext;
#endif
    frame->printPage(page_number, canvasPtr);
  }

  // Done printing. Close the device context to retrieve the compiled metafile.
  metafile->FinishPage();
}
