// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_PEPPER_V8_VAR_CONVERTER_H
#define CONTENT_RENDERER_PEPPER_V8_VAR_CONVERTER_H


#include "base/basictypes.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/message_loop/message_loop_proxy.h"
#include "ppapi/c/pp_var.h"
#include "v8/include/v8.h"
#include "content/common/content_export.h"

namespace ppapi {
class ScopedPPVar;
}

namespace content {
namespace V8VarConverter {

// Converts the given PP_Var to a v8::Value. True is returned upon success.
bool CONTENT_EXPORT ToV8Value(const PP_Var& var,
                              v8::Handle<v8::Context> context,
                              v8::Handle<v8::Value>* result);

// A version of FromV8Value that accepts the message loop to run the callback
// from for the purposes of testing. The default is to use the current message
// loop. See the description of FromV8Value below.
void CONTENT_EXPORT FromV8Value(
    v8::Handle<v8::Value> val,
    v8::Handle<v8::Context> context,
    const base::Callback<void(const ppapi::ScopedPPVar&, bool)>& callback,
    const scoped_refptr<base::MessageLoopProxy>& message_loop_proxy);

// Converts the given v8::Value to a PP_Var. Every PP_Var in the reference graph
// in the result will have a refcount equal to the number of references to it in
// the graph. The root of the result will have one additional reference. The
// callback is run when conversion is complete with the resulting var and a bool
// indicating success or failure. Conversion is asynchronous because converting
// some resources may result in communication across IPC. |context| is
// guaranteed to only be used synchronously.
void CONTENT_EXPORT FromV8Value(
    v8::Handle<v8::Value> val,
    v8::Handle<v8::Context> context,
    const base::Callback<void(const ppapi::ScopedPPVar&, bool)>& callback);

}  // namespace V8VarConverter
}  // namespace content

#endif  // CONTENT_RENDERER_PEPPER_V8_VAR_CONVERTER_H
