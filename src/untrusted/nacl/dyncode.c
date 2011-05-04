/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <errno.h>
#include <sys/nacl_syscalls.h>

#include "native_client/src/untrusted/irt/irt_interfaces.h"
#include "native_client/src/untrusted/nacl/nacl_irt.h"

/*
 * ABI table for underyling NaCl dyncode interfaces.
 * We set this up in a constructor run implicitly at initialization time.
 */
static struct nacl_irt_dyncode irt_dyncode;

static void __attribute__((constructor)) setup_irt_dyncode(void) {
  if (NULL == __nacl_irt_query ||
      __nacl_irt_query(NACL_IRT_DYNCODE_v0_1, &irt_dyncode,
                       sizeof(irt_dyncode)) != sizeof(irt_dyncode))
    irt_dyncode = nacl_irt_dyncode;
}

int nacl_dyncode_create(void *dest, const void *src, size_t size) {
  int error = irt_dyncode.dyncode_create(dest, src, size);
  if (error) {
    errno = error;
    return -1;
  }
  return 0;
}

int nacl_dyncode_modify(void *dest, const void *src, size_t size) {
  int error = irt_dyncode.dyncode_modify(dest, src, size);
  if (error) {
    errno = error;
    return -1;
  }
  return 0;
}

int nacl_dyncode_delete(void *dest, size_t size) {
  int error = irt_dyncode.dyncode_delete(dest, size);
  if (error) {
    errno = error;
    return -1;
  }
  return 0;
}
