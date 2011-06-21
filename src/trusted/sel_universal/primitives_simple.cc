/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Simple implementation of IMultimedia interface
 */

#include <assert.h>
#include <string.h>

#include <functional>
#include <queue>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_semaphore.h"
#include "native_client/src/shared/platform/nacl_sync.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/sel_universal/primitives.h"

#include "ppapi/c/pp_input_event.h"

// Standard helper class to tie mutex lock/unlock to a scope.
class ScopedMutexLock {
 public:
  explicit ScopedMutexLock(NaClMutex* m)
    : mutex_(m) {
    // work around compiler warning, cast to void does not work!
    // More instances of this hack below
    NaClXMutexLock(mutex_);
  }

  ~ScopedMutexLock() {
    NaClXMutexUnlock(mutex_);
  }

 private:
  NaClMutex* mutex_;
};


class EmuPrimitivesSimple : public IMultimedia {
 public:
  EmuPrimitivesSimple(int width, int heigth, const char* title) {
    UNREFERENCED_PARAMETER(width);
    UNREFERENCED_PARAMETER(heigth);
    UNREFERENCED_PARAMETER(title);
    NaClLog(2, "PrimitivesSimpleL::Constructor\n");
    NaClXMutexCtor(&mutex_);
    NaClSemCtor(&sem_, 0);
  }

  virtual ~EmuPrimitivesSimple() {
  }

  virtual int VideoBufferSize() {
    NaClLog(LOG_FATAL, "VideoBufferSize() not supported\n");
    return -1;
  }

  virtual void VideoUpdate(const void* data) {
    UNREFERENCED_PARAMETER(data);
    NaClLog(LOG_FATAL, "VideoUpdate() not supported\n");
  }

  virtual void PushUserEvent(PP_InputEvent* event) {
    ScopedMutexLock lock(&mutex_);
    queue_.push(*event);
    NaClSemPost(&sem_);
  }

  virtual void PushDelayedUserEvent(int delay, PP_InputEvent* event) {
    // for now ignore the delay
    UNREFERENCED_PARAMETER(delay);
    PushUserEvent(event);
  }

  virtual void EventPoll(PP_InputEvent* event) {
    ScopedMutexLock lock(&mutex_);
    if (queue_.size() > 0) {
      // This should always go through without delay
      NaClSemWait(&sem_);
      // copy
      *event = queue_.front();
      queue_.pop();
    } else {
      MakeInvalidEvent(event);
    }
  }

  virtual void EventGet(PP_InputEvent* event) {
    NaClSemWait(&sem_);
    ScopedMutexLock lock(&mutex_);
    assert(queue_.size() > 0);
    // copy
    *event = queue_.front();
    queue_.pop();
  }

  virtual void AudioInit16Bit(int frequency,
                              int channels,
                              int frame_size,
                              AUDIO_CALLBACK cb) {
    UNREFERENCED_PARAMETER(frequency);
    UNREFERENCED_PARAMETER(channels);
    UNREFERENCED_PARAMETER(frame_size);
    UNREFERENCED_PARAMETER(cb);
    NaClLog(LOG_FATAL, "AudioInit16Bit() not supported\n");
  }

  virtual void AudioStart() {
    NaClLog(LOG_FATAL, "AudioStart() not supported\n");
  }

  virtual void AudioStop() {
    NaClLog(LOG_FATAL, "AudioStop() not supported\n");
  }

 private:
  NaClMutex mutex_;
  NaClSemaphore sem_;
  std::queue<PP_InputEvent> queue_;
};

// Factor, so we can hide class MultimediaSDL from the outside world
IMultimedia* MakeEmuPrimitives(int width, int heigth, const char* title) {
  return new EmuPrimitivesSimple(width, heigth, title);
}
