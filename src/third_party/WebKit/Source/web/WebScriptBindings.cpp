/*
 * Copyright (C) 2017 Bloomberg Finance L.P.
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

#include "public/web/WebScriptBindings.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/V8Binding.h"
#include "public/platform/WebString.h"

#include <v8.h>

namespace blink {

v8::Local<v8::Context> WebScriptBindings::createWebScriptContext()
{
    v8::Isolate* isolate = v8::Isolate::GetCurrent();
    v8::EscapableHandleScope hs(isolate);
    v8::Local<v8::Context> context = v8::Context::New(isolate);

    PassRefPtr<ScriptState> scriptState = ScriptState::create(context, &DOMWrapperWorld::mainWorld());

    return hs.Escape(context);
}

} // namespace blink

