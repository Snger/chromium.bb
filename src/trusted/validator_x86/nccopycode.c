/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * nccopycode.c
 * Copies two code streams in a thread-safe way
 *
 */

#include "native_client/src/include/portability.h"

#if NACL_WINDOWS == 1
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include "native_client/src/shared/platform/nacl_check.h"
#if NACL_TARGET_SUBARCH == 32
#include "native_client/src/trusted/validator_x86/ncdecode.h"
#include "native_client/src/trusted/validator_x86/ncvalidate.h"
#elif NACL_TARGET_SUBARCH == 64
#include "native_client/src/trusted/validator_x86/nc_inst_iter.h"
#include "native_client/src/trusted/validator_x86/nc_segment.h"
#include "native_client/src/trusted/validator_x86/nc_inst_state_internal.h"
#else
#error "Unknown Platform"
#endif

/* x86 HALT opcode */
static const uint8_t kNaClFullStop = 0xf4;

/*
 * Max size of aligned writes we may issue to code without syncing.
 * 8 is a safe value according to:
 *   [1] Advance Micro Devices Inc. AMD64 Architecture Program-
 *   mers Manual Volume 1: Application Programming, 2009.
 *   [2] Intel Inc. Intel 64 and IA-32 Architectures Software Developers
 *   Manual Volume 3A: System Programming Guide, Part 1, 2010.
 *   [3] Vijay Sundaresan, Daryl Maier, Pramod Ramarao, and Mark
 *   Stoodley. Experiences with multi-threading and dynamic class
 *   loading in a java just-in-time compiler. Code Generation and
 *   Optimization, IEEE/ACM International Symposium on, 0:87–
 *   97, 2006.
 */
static const int kTrustAligned = 8;

/*
 * Max size of unaligned writes we may issue to code.
 * Empirically this may be larger, however no docs to support it.
 * 1 means disabled.
 */
static const int kTrustUnaligned = 1;

/*
 * Boundary no write may ever cross.
 * On AMD machines must be leq 8.  Intel machines leq cache line.
 */
static const int kInstructionFetchSize = 8;

/* defined in nccopycode_stores.S */
void _cdecl onestore_memmove4(uint8_t* dst, uint8_t* src);

/* defined in nccopycode_stores.S */
void _cdecl onestore_memmove8(uint8_t* dst, uint8_t* src);

static INLINE int IsAligned(uint8_t *dst, int size, int align) {
  uintptr_t startaligned = ((uintptr_t)dst)        & ~(align-1);
  uintptr_t stopaligned  = ((uintptr_t)dst+size-1) & ~(align-1);
  return startaligned == stopaligned;
}

/*
 * Test if it is safe to issue a unsynced change to dst/size using a
 * writesize write.  Outputs the offset to start the write at.
 * 1 if it is ok, 0 if it is not ok.
 */
static int IsTrustedWrite(uint8_t *dst,
                          int size,
                          int writesize,
                          intptr_t* offset) {
  if (size > writesize) {
    return 0;
  }
  if (!IsAligned(dst, size, kInstructionFetchSize)) {
    return 0;
  }
  if (writesize <= kTrustAligned && IsAligned(dst, size, writesize)) {
    /* aligned write is trusted */
    *offset = (intptr_t) dst & (writesize - 1);
    return 1;
  }
  if (writesize <= kTrustUnaligned) {
    /* unaligned write is trusted */
    *offset = 0;
    return 1;
  }
  return 0;
}

#if NACL_WINDOWS == 1
static void* valloc(size_t s) {
  /* allocate twice as much then round up to nearest s */
  uintptr_t m = (uintptr_t) malloc(2 * s);
  /* check for power of 2: */
  CHECK(0 == (s & (s - 1)));
  m = (m + s) & ~(s - 1);
  return (void*) m;
}
#endif

/* this is global to prevent a (very smart) compiler from optimizing it out */
void* g_squashybuffer = NULL;

static void SerializeAllProcessors() {
  /*
   * We rely on the OS mprotect() call to issue interprocessor interrupts,
   * which will cause other processors to execute an IRET, which is
   * serializing.
   */
#if NACL_WINDOWS == 1
  static const DWORD prot_a = PAGE_EXECUTE_READWRITE;
  static const DWORD prot_b = PAGE_NOACCESS;
  static DWORD prot = PAGE_NOACCESS;
  static DWORD size = 0;
  BOOL rv;
  DWORD oldprot;
  if (NULL == g_squashybuffer) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    size = si.dwPageSize;
    g_squashybuffer = valloc(size);
  }
  CHECK(0 != size);
  CHECK(NULL != g_squashybuffer);
  prot = (prot == prot_a ? prot_b : prot_a);
  rv = VirtualProtect(g_squashybuffer, size, prot, &oldprot);
  CHECK(rv);
#else
  static const int prot_a = PROT_READ|PROT_WRITE|PROT_EXEC;
  static const int prot_b = PROT_NONE;
  static int prot = PROT_NONE;
  static int size = 0;
  int rv;
  if (NULL == g_squashybuffer) {
    size = sysconf(_SC_PAGE_SIZE);
    g_squashybuffer = valloc(size);
  }
  CHECK(0 != size);
  CHECK(NULL != g_squashybuffer);
  prot = (prot == prot_a ? prot_b : prot_a);
  rv = mprotect(g_squashybuffer, size, prot);
  CHECK(rv == 0);
#endif
}

/*
 * Copy a single instruction, avoiding the possibility of other threads
 * executing a partially changed instruction.
 */
void CopyInstructionInternal(uint8_t *dst,
                             uint8_t *src,
                             uint8_t sz) {
  intptr_t offset = 0;
  uint8_t *firstbyte_p = dst;

  while (sz > 0 && dst[0] == src[0]) {
    /* scroll to first changed byte */
    dst++, src++, sz--;
  }

  if (sz == 0) {
    /* instructions are identical, we are done */
    return;
  }

  while (sz > 0 && dst[sz-1] == src[sz-1]) {
    /* trim identical bytes at end */
    sz--;
  }

  if (sz == 1) {
    /* we assume a 1-byte change is atomic */
    *dst = *src;
  } else if (IsTrustedWrite(dst, sz, 4, &offset)) {
    uint8_t tmp[4];
    memcpy(tmp, dst-offset, sizeof tmp);
    memcpy(tmp+offset, src, sz);
    onestore_memmove4(dst-offset, tmp);
  } else if (IsTrustedWrite(dst, sz, 8, &offset)) {
    uint8_t tmp[8];
    memcpy(tmp, dst-offset, sizeof tmp);
    memcpy(tmp+offset, src, sz);
    onestore_memmove8(dst-offset, tmp);
  } else {
    /* the slow path, first flip first byte to halt*/
    uint8_t firstbyte = firstbyte_p[0];
    firstbyte_p[0] = kNaClFullStop;

    SerializeAllProcessors();

    /* copy the rest of instruction */
    if (dst == firstbyte_p) {
      /* but not the first byte! */
      firstbyte = *src;
      dst++, src++, sz--;
    }
    memcpy(dst, src, sz);

    SerializeAllProcessors();

    /* flip first byte back */
    firstbyte_p[0] = firstbyte;
  }
}

#if NACL_TARGET_SUBARCH == 32

/*
 * Copy a single instruction, avoiding the possibility of other threads
 * executing a partially changed instruction.
 */
void CopyInstruction(const NCDecoderInst *dinst_old,
                     const NCDecoderInst *dinst_new) {
  NCRemainingMemory* mem_old = &dinst_old->dstate->memory;
  NCRemainingMemory* mem_new = &dinst_new->dstate->memory;
  CHECK(mem_old->read_length == mem_new->read_length);

  CopyInstructionInternal(mem_old->mpc,
                          mem_new->mpc,
                          mem_old->read_length);
}

int NCCopyCode(uint8_t *dst, uint8_t *src, NaClPcAddress vbase,
               size_t sz, int bundle_size) {
  /* TODO(karl): The current implementation dies with runtime
   * errors if something goes wrong. Fix so that NCDecodeSegmentPair
   * returns a status value, so that the proper value can be
   * returned by this routine.
   */
  NCDecodeSegmentPair(dst, src, vbase, sz, CopyInstruction);
  return 1;
}

#elif NACL_TARGET_SUBARCH == 64

int NaClCopyCodeIter(uint8_t *dst, uint8_t *src,
                     NaClPcAddress vbase, size_t size) {
  NaClSegment segment_old, segment_new;
  NaClInstIter *iter_old, *iter_new;
  NaClInstState *istate_old, *istate_new;

  NaClSegmentInitialize(dst, vbase, size, &segment_old);
  NaClSegmentInitialize(src, vbase, size, &segment_new);

  iter_old = NaClInstIterCreate(&segment_old);
  iter_new = NaClInstIterCreate(&segment_new);
  while (NaClInstIterHasNext(iter_old) &&
         NaClInstIterHasNext(iter_new)) {
    /* March over every instruction, which means NaCl pseudo-instructions are
     * treated as multiple instructions.  Checks in NaClValidateCodeReplacement
     * guarantee that only valid replacements will happen, and no pseudo-
     * instructions should be touched.
     */
    istate_old = NaClInstIterGetState(iter_old);
    istate_new = NaClInstIterGetState(iter_new);
    if (istate_old->bytes.length != istate_new->bytes.length ||
        iter_old->memory.read_length != iter_new->memory.read_length ||
        istate_new->vpc != istate_old->vpc) {
      /* Sanity check: this should never happen based on checks in
       * NaClValidateInstReplacement.
       */
      NaClLog(LOG_FATAL,
              "Segment replacement: copied instructions misaligned\n");
      return 0;
    }
    /* Replacing all modified instructions at once could yield a speedup here
     * as every time we modify instructions we must serialize all processors
     * twice.  Re-evaluate if code modification performance is an issue.
     */
    CopyInstructionInternal(iter_old->memory.mpc,
                            iter_new->memory.mpc,
                            iter_old->memory.read_length);
    NaClInstIterAdvance(iter_old);
    NaClInstIterAdvance(iter_new);
  }

  CHECK(!NaClInstIterHasNext(iter_old) && !NaClInstIterHasNext(iter_new));

  NaClInstIterDestroy(iter_old);
  NaClInstIterDestroy(iter_new);
  return 1;
}

#else
#error "Unknown Platform"
#endif
