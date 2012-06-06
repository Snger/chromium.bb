/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdexcept>
#include <string.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/gdb_rsp/abi.h"
#include "native_client/src/trusted/port/mutex.h"
#include "native_client/src/trusted/port/platform.h"
#include "native_client/src/trusted/port/thread.h"
#include "native_client/src/trusted/service_runtime/nacl_signal.h"

/*
 * Define the OS specific portions of IThread interface.
 */

namespace {

const int kX86TrapFlag = 1 << 8;

}  // namespace

namespace port {

static IThread::CatchFunc_t s_CatchFunc = NULL;
static void* s_CatchCookie = NULL;

static enum NaClSignalResult SignalHandler(int signal, void *ucontext) {
  struct NaClSignalContext context;
  NaClSignalContextFromHandler(&context, ucontext);
  if (NaClSignalContextIsUntrusted(&context)) {
    uint32_t thread_id = IPlatform::GetCurrentThread();
    IThread* thread = IThread::Acquire(thread_id);

    NaClSignalContextFromHandler(thread->GetContext(), ucontext);
    if (s_CatchFunc != NULL)
      s_CatchFunc(thread_id, signal, s_CatchCookie);
    NaClSignalContextToHandler(ucontext, thread->GetContext());

    IThread::Release(thread);
    return NACL_SIGNAL_RETURN;
  } else {
    // Do not attempt to debug crashes in trusted code.
    return NACL_SIGNAL_SEARCH;
  }
}

static IMutex* ThreadGetLock() {
  static IMutex* mutex_ = IMutex::Allocate();
  return mutex_;
}

static IThread::ThreadMap_t *ThreadGetMap() {
  static IThread::ThreadMap_t* map_ = new IThread::ThreadMap_t;
  return map_;
}

class Thread : public IThread {
 public:
  Thread(uint32_t id, struct NaClAppThread *natp)
      : ref_(1), id_(id), natp_(natp) {}
  ~Thread() {}

  uint32_t GetId() {
    return id_;
  }

  virtual bool Suspend() {
    SuspendOneThread(this, natp_);
    return true;
  }

  virtual bool Resume() {
    ResumeOneThread(this, natp_);
    return true;
  }

  // TODO(mseaborn): Similar logic is duplicated in the Windows version.
  virtual bool SetStep(bool on) {
#if NACL_ARCH(NACL_BUILD_ARCH) == NACL_x86
    if (on) {
      context_.flags |= kX86TrapFlag;
    } else {
      context_.flags &= ~kX86TrapFlag;
    }
    return true;
#else
    // TODO(mseaborn): Implement for ARM.
    UNREFERENCED_PARAMETER(on);
    return false;
#endif
  }

  virtual bool GetRegister(uint32_t index, void *dst, uint32_t len) {
    const gdb_rsp::Abi *abi = gdb_rsp::Abi::Get();
    const gdb_rsp::Abi::RegDef *reg = abi->GetRegisterDef(index);
    memcpy(dst, (char *) &context_ + reg->offset_, len);
    return false;
  }

  virtual bool SetRegister(uint32_t index, void* src, uint32_t len) {
    const gdb_rsp::Abi *abi = gdb_rsp::Abi::Get();
    const gdb_rsp::Abi::RegDef *reg = abi->GetRegisterDef(index);
    memcpy((char *) &context_ + reg->offset_, src, len);
    return false;
  }

  virtual struct NaClSignalContext *GetContext() { return &context_; }

 private:
  uint32_t ref_;
  uint32_t id_;
  struct NaClAppThread *natp_;
  struct NaClSignalContext context_;

  friend class IThread;
};

// TODO(eaeltsin): This is duplicated in the Windows version.
IThread* IThread::Create(uint32_t id, struct NaClAppThread* natp) {
  MutexLock lock(ThreadGetLock());
  Thread* thread;
  ThreadMap_t &map = *ThreadGetMap();

  if (map.count(id)) {
    NaClLog(LOG_FATAL, "IThread::Create: thread 0x%x already exists\n", id);
  }

  thread = new Thread(id, natp);
  map[id] = thread;
  return thread;
}

// TODO(eaeltsin): This is duplicated in the Windows version.
IThread* IThread::Acquire(uint32_t id) {
  MutexLock lock(ThreadGetLock());
  Thread* thread;
  ThreadMap_t &map = *ThreadGetMap();

  if (map.count(id) == 0) {
    NaClLog(LOG_FATAL, "IThread::Acquire: thread 0x%x does not exist\n", id);
  }

  thread = static_cast<Thread*>(map[id]);
  thread->ref_++;
  return thread;
}

// TODO(eaeltsin): This is duplicated in the Windows version.
void IThread::Release(IThread *ithread) {
  MutexLock lock(ThreadGetLock());
  Thread* thread = static_cast<Thread*>(ithread);
  thread->ref_--;

  if (thread->ref_ == 0) {
    ThreadGetMap()->erase(thread->id_);
    delete static_cast<IThread*>(thread);
  }
}

void IThread::SuspendOneThread(IThread *, struct NaClAppThread *) {
  // TODO(eaeltsin): implement this.
}

void IThread::ResumeOneThread(IThread *, struct NaClAppThread *) {
  // TODO(eaeltsin): implement this.
}

void IThread::SetExceptionCatch(IThread::CatchFunc_t func, void *cookie) {
  NaClSignalHandlerAdd(SignalHandler);
  s_CatchFunc = func;
  s_CatchCookie = cookie;
}


}  // End of port namespace

