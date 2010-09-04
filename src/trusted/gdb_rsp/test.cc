/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "native_client/src/trusted/gdb_rsp/test.h"
#include "native_client/src/trusted/port/platform.h"

//  Mock portability objects
namespace port {
void IPlatform::Relinquish(uint32_t msec) {
  (void) msec;
  return;
}

void IPlatform::LogInfo(const char *fmt, ...) {
  va_list argptr;
  va_start(argptr, fmt);

  vprintf(fmt, argptr);
}

void IPlatform::LogWarning(const char *fmt, ...) {
  va_list argptr;
  va_start(argptr, fmt);

  vprintf(fmt, argptr);
}

void IPlatform::LogError(const char *fmt, ...) {
  va_list argptr;
  va_start(argptr, fmt);

  vprintf(fmt, argptr);
}

//  The unit tests are singly threaded, so we just do nothing
//  for synchronization
class Mutex : public IMutex {
  void Lock() {}
  void Unlock() {}
  bool Try() { return true; }
};

IMutex* IMutex::Allocate() { return new Mutex; }
void IMutex::Free(IMutex* mtx) { delete static_cast<Mutex*>(mtx); }

}  // End of namespace port

int main(int argc, const char *argv[]) {
  int errs = 0;

  (void) argc;
  (void) argv;

  printf("Testing Utils.\n");
  errs += TestUtil();

  printf("Testing ABI.\n");
  errs += TestAbi();

  printf("Testing Packets.\n");
  errs += TestPacket();

  printf("Testing Session.\n");
  errs += TestSession();

  if (errs) printf("FAILED with %d errors.\n", errs);
  return errs;
}

