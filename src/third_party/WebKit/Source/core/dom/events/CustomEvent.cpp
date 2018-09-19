/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/dom/events/CustomEvent.h"

#include "bindings/core/v8/serialization/SerializedScriptValue.h"
#include "bindings/core/v8/serialization/SerializedScriptValueFactory.h"

namespace blink {

CustomEvent::CustomEvent() {}

CustomEvent::CustomEvent(ScriptState* script_state,
                         const AtomicString& type,
                         const CustomEventInit& initializer)
    : Event(type, initializer) {
  world_ = RefPtr<DOMWrapperWorld>(script_state->World());
  if (initializer.hasDetail()) {
    v8::Isolate *isolate = initializer.detail().GetIsolate();
    detail_.Reset(isolate, initializer.detail().V8Value());
  }
}

CustomEvent::~CustomEvent() {
  detail_.Reset();
}

void CustomEvent::initCustomEvent(ScriptState* script_state,
                                  const AtomicString& type,
                                  bool can_bubble,
                                  bool cancelable,
                                  const ScriptValue& script_value) {
  initEvent(type, can_bubble, cancelable);
  world_ = RefPtr<DOMWrapperWorld>(script_state->World());
  if (!IsBeingDispatched() && !script_value.IsEmpty())
    detail_.Reset(script_value.GetIsolate(), script_value.V8Value());
}

ScriptValue CustomEvent::detail(ScriptState* script_state) const {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (detail_.IsEmpty())
    return ScriptValue(script_state, v8::Null(isolate));
  // Returns a clone of |detail_| if the world is different.
  if (!world_ || world_->GetWorldId() != script_state->World().GetWorldId()) {
    v8::Local<v8::Value> value = v8::Local<v8::Value>::New(isolate, detail_);
    RefPtr<SerializedScriptValue> serialized =
        SerializedScriptValue::SerializeAndSwallowExceptions(isolate, value);
    return ScriptValue(script_state, serialized->Deserialize(isolate));
  }
  return ScriptValue(script_state, v8::Local<v8::Value>::New(isolate, detail_));
}

const AtomicString& CustomEvent::InterfaceName() const {
  return EventNames::CustomEvent;
}

DEFINE_TRACE(CustomEvent) {
  Event::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(CustomEvent) {
  Event::TraceWrappers(visitor);
}

}  // namespace blink
