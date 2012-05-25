/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <windows.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


void NaClAppThreadSetSuspendState(struct NaClAppThread *natp,
                                  enum NaClSuspendState old_state,
                                  enum NaClSuspendState new_state) {
  NaClXMutexLock(&natp->mu);
  while ((natp->suspend_state & NACL_APP_THREAD_SUSPENDING) != 0) {
    /*
     * We are being suspended, but SuspendThread() has not taken effect yet.
     */
    NaClXCondVarWait(&natp->cv, &natp->mu);
  }
  DCHECK(natp->suspend_state == old_state);
  natp->suspend_state = new_state;
  NaClXMutexUnlock(&natp->mu);
}

void NaClUntrustedThreadSuspend(struct NaClAppThread *natp,
                                int save_registers) {
  enum NaClSuspendState old_state;

  /*
   * Note that if we are being called from a NaCl syscall (which is
   * likely), natp could be the thread we are running in.  That is
   * fine, because this thread will be in the
   * NACL_APP_THREAD_TRUSTED state, and so we will not call
   * SuspendThread() on it.
   */

  /*
   * We do not want the thread to enter a NaCl syscall and start
   * taking locks when SuspendThread() takes effect, so we ask the
   * thread to suspend even if it currently running untrusted code.
   */
  NaClXMutexLock(&natp->mu);
  old_state = natp->suspend_state;
  natp->suspend_state = old_state | NACL_APP_THREAD_SUSPENDING;
  if (old_state == NACL_APP_THREAD_UNTRUSTED) {
    CONTEXT context;
    if (SuspendThread(natp->thread.tid) == (DWORD) -1) {
      NaClLog(LOG_FATAL, "NaClUntrustedThreadsSuspend: "
              "SuspendThread() call failed\n");
    }
    /*
     * SuspendThread() can return before the thread has been
     * suspended, because internally it only sends a message asking
     * for the thread to be suspended.
     * See http://code.google.com/p/nativeclient/issues/detail?id=2557
     *
     * As a workaround for that, we call GetThreadContext() even when
     * save_registers=0.  GetThreadContext() should only be able to
     * return a snapshot of the register state once the thread has
     * actually suspended.
     *
     * If save_registers=0, the set of registers we request via
     * ContextFlags is unimportant as long as it is non-empty.
     */
    context.ContextFlags = CONTEXT_CONTROL;
    if (save_registers) {
      context.ContextFlags |= CONTEXT_INTEGER;
    }
    if (!GetThreadContext(natp->thread.tid, &context)) {
      NaClLog(LOG_FATAL, "NaClUntrustedThreadsSuspend: "
              "GetThreadContext() failed\n");
    }
    if (save_registers) {
      if (natp->suspended_registers == NULL) {
        natp->suspended_registers = malloc(sizeof(*natp->suspended_registers));
        if (natp->suspended_registers == NULL) {
          NaClLog(LOG_FATAL, "NaClUntrustedThreadSuspend: malloc() failed\n");
        }
      }
      NaClSignalContextFromHandler(natp->suspended_registers, &context);
    }
  }
  NaClXMutexUnlock(&natp->mu);
}

void NaClUntrustedThreadResume(struct NaClAppThread *natp) {
  enum NaClSuspendState old_state;

  NaClXMutexLock(&natp->mu);
  old_state = natp->suspend_state;
  DCHECK((old_state & NACL_APP_THREAD_SUSPENDING) != 0);
  if (old_state == (NACL_APP_THREAD_UNTRUSTED | NACL_APP_THREAD_SUSPENDING)) {
    if (ResumeThread(natp->thread.tid) == (DWORD) -1) {
      NaClLog(LOG_FATAL, "NaClUntrustedThreadsResume: "
              "ResumeThread() call failed\n");
    }
  }
  natp->suspend_state = old_state & ~NACL_APP_THREAD_SUSPENDING;
  NaClXCondVarSignal(&natp->cv);
  NaClXMutexUnlock(&natp->mu);
}

/*
 * NaClUntrustedThreadsSuspend() ensures that any untrusted code is
 * temporarily suspended.
 *
 * If a thread is currently executing a NaCl syscall, we tell the
 * thread not to return to untrusted code yet.  If a thread is
 * currently executing untrusted code, we suspend it.
 *
 * This returns with the lock threads_mu held, because we need to pin
 * the list of threads.  NaClUntrustedThreadsResume() must be called
 * to undo this.
 */
void NaClUntrustedThreadsSuspendAll(struct NaClApp *nap, int save_registers) {
  size_t index;

  NaClXMutexLock(&nap->threads_mu);

  /*
   * TODO(mseaborn): A possible refinement here would be to do
   * SuspendThread() and GetThreadContext() in separate loops
   * across the threads.  This might be faster, since we would not
   * be waiting for each thread to suspend one by one.  It would
   * take advantage of SuspendThread()'s asynchronous nature.
   */
  for (index = 0; index < nap->threads.num_entries; index++) {
    struct NaClAppThread *natp = NaClGetThreadMu(nap, (int) index);
    if (natp != NULL) {
      NaClUntrustedThreadSuspend(natp, save_registers);
    }
  }
}

void NaClUntrustedThreadsResumeAll(struct NaClApp *nap) {
  size_t index;
  for (index = 0; index < nap->threads.num_entries; index++) {
    struct NaClAppThread *natp = NaClGetThreadMu(nap, (int) index);
    if (natp != NULL) {
      NaClUntrustedThreadResume(natp);
    }
  }

  NaClXMutexUnlock(&nap->threads_mu);
}
