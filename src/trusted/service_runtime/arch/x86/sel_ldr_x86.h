/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef SERVICE_RUNTIME_ARCH_X86_SEL_LDR_H__
#define SERVICE_RUNTIME_ARCH_X86_SEL_LDR_H__ 1

/* to make LDT_ENTRIES available */
#if NACL_WINDOWS
# define LDT_ENTRIES 8192
#elif NACL_OSX
# define LDT_ENTRIES 8192
#elif NACL_LINUX
# include <asm/ldt.h>
#endif

#include "native_client/src/trusted/service_runtime/arch/x86/nacl_ldt_x86.h"

#define NACL_THREAD_MAX     LDT_ENTRIES

#define NACL_MAX_ADDR_BITS  (8 + 20)
#define NACL_NOOP_OPCODE    0x90
#define NACL_HALT_OPCODE    0xf4
#define NACL_HALT_LEN       1           /* length of halt instruction */

#endif /* SERVICE_RUNTIME_ARCH_X86_SEL_LDR_H__ */
