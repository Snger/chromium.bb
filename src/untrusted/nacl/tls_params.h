/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_NACL_TLS_PARAMS_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_NACL_TLS_PARAMS_H_ 1

/*
 * Native Client support for thread local storage
 *
 * Support for TLS in NaCl depends upon the cooperation of the
 * compiler's code generator, the linker (and linker script), the
 * startup code (_start), and the CPU-specific routines defined here.
 *
 * Each thread is associated with both a TLS region, comprising the .tdata
 * and .tbss (zero-initialized) sections of the ELF file, and a thread
 * descriptor block (TDB), a structure containing information about the
 * thread that is mostly private to the thread library implementation.
 * The details of the TLS and TDB regions vary by platform; this module
 * provides a generic abstraction which may be supported by any platform.
 *
 * The "combined area" is an opaque region of memory associated with a
 * thread, containing its TDB and TLS and sufficient internal padding so
 * that it can be allocated anywhere without regard to alignment.  We
 * provide here the CPU-specific parametrization routines that control how
 * that should be layed out.  The src/untrusted/nacl/tls.c module
 * provides some routines for setting up the combined area, which use
 * these parameters.
 *
 * Each time a thread is created (including the main thread via
 * _start), a combined area is allocated and initialized for it.
 *
 * Once the main thread's TLS area is initialized, a nacl_tls_init() call
 * is made to save its location in the "thread register" (aka $tp).  How
 * this is stored in the machine state varies by platform.  Additional
 * threads set up $tp in the thread creation call and don't need to use
 * nacl_tls_init() explicitly.
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * Example 1: x86-32.  This diagram shows the combined area:
 *
 *  +-----+-----------------+------+-----+
 *  | pad |  TLS data, bss  | TDB  | pad |
 *  +-----+-----------------+------+-----+
 *                          ^
 *                          |
 *                          +--- %gs
 *
 * The first word of the TDB is its own address, relative to the default
 * segment register (DS).  Negative offsets from %gs will not work since
 * NaCl enforces segmentation violations, so TLS references explicitly
 * retrieve this TLS base pointer and then indirect relative to it (using
 * DS).  This first word the only part of the TDB that is part of any
 * public ABI; the rest is private to the thread library's implementation.
 *
 * The TLS section is aligned as needed by the program's particular TLS
 * data.  Since $tp (%gs) points right after it, the TDB also gets the
 * same alignment, though the TDB itself needs only word-alignment.
 *
 *      mov     %gs:0, %eax              ; load TDB's (DS-)address from TDB.
 *      mov     -0x20(%eax), %ebx        ; load TLS object into ebx
 *
 * It's also possible to use direct %gs:OFFSET accesses (with positive
 * OFFSETS only) to refer to the TDB, though we do not make use of that.
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * Example 2: x86-64.
 *
 * The layout of the combined area is the same as for x86-32; the TDB
 * address is accessed via an intrinsic, __nacl_read_tp().
 *
 *      callq  __nacl_read_tp            ; load TDB address into eax.
 *      mov    -0x20(%r15,%rax,1),%eax   ; sandboxed load from r15 "segment".
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * Example 3: ARM.  The positions of TDB and TLS are reversed.
 *
 *  +-----------+--------+----------------+
 *  |   TDB     | header | TLS data, bss  |
 *  +-----------+--------+----------------+
 *              ^        ^
 *              |        |
 *              |        +--- __aeabi_read_tp()+8
 *              +--- __aeabi_read_tp()
 *
 * The header is fixed at 8 bytes by the ARM ELF TLS ABI.  The linker
 * automatically lays out TLS symbols starting at offset 8.  (We do not
 * actually make use of the header space in this implementation, though
 * other, more thorough TLS implementations do.)  The size of the TDB is
 * not part of the ABI, and is private to the thread library.  Code
 * supported by this runtime uses only the non-PIC style, known as the
 * "local exec" TLS model ("initial exec" is similar).  The generated
 * code calls __aeabi_read_tp() to locate the TLS area, then uses
 * positive offsets from this address.  Our implementation of this
 * function computes r9[32:12]; the low bits are reserved for the
 * trusted runtime's use.  The padding is distributed so that the header
 * falls on a 4KB page boundary, since with any finer alignment, there
 * aren't enough low bits in r9 to avoid collisions.
 *
 *      mov     r1, #192          @ offset of symbol from $tp, i.e. var(tpoff)
 *      bl      __aeabi_read_tp
 *      ldr     r2, [r0, r1]
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * The trivial inline functions below indicate the TLS layout parameters
 * specific to the machine.  The rest of the runtime code is generic to
 * any machine, by using the values of these functions.
 */

#include <stddef.h>

/*
 * TODO(mcgrathr):
 * This is conditionalized only for the purpose of compiling tls_params.c
 * for pnacl's benefit.  That should go away once the compiler intrinsics
 * are fixed properly, see TODO comment below.
 */
#ifndef NACL_UNTRUSTED_INLINE
#define NACL_UNTRUSTED_INLINE static inline __attribute__((__unused__))
#endif

#if defined(__pnacl__)

/*
 * These are compiler intrinsics in NaCl's variant of LLVM.
 *
 * See:
 *      hg/llvm-gcc/llvm-gcc-4.2/gcc/builtins.def
 *      hg/llvm-gcc/llvm-gcc-4.2/gcc/builtin-types.def
 *      hg/llvm/llvm-trunk/include/llvm/Intrinsics.td
 *      hg/llvm/llvm-trunk/lib/Target/X86/X86InstrNaCl.td
 *      hg/llvm/llvm-trunk/lib/Target/ARM/ARMInstrInfo.td
 *
 * TODO(mcgrathr): Actually, they're not.  The llvm-gcc frontend defines
 * built-in functions called __builtin_nacl_*, but we don't get those
 * unless we use the __builtin_nacl_* names rather than __nacl_* names.
 * The intent is that those be defined such that the LLVM backend replaces
 * them with their trivial expansions appropriate for each machine.
 * However, the way that was implemented in NaCl's LLVM fork is wrong in
 * two ways, and we don't actually use it.  Those were implemented as
 * target-specific intrinsics that generate explicit assembly code.
 * This means two bad things:
 * 1. They actually are not implemented when LLVM is directly generating
 *    machine code (.o) rather than assembly code (.s)--in fact, LLVM
 *    happily just omits the code, so the functions have the effect of
 *    returning whatever garbage value was in the register chosen as
 *    the output register.
 * 2. They are implemented too low in the compilation to be optimized.
 *    This is quite pessimal for such trivial functions that each either
 *    return a constant or perform one arithmetic operation on an argument.
 *    What would be actually useful is to have them converted to those
 *    constants/arithmetic higher up in the compiler, where their effects
 *    on surrounding code could then be folded in during code generation.
 * Pending that LLVM work being done, we're actually just emitting calls to
 * these as external functions and they are defined by ../stubs/tls_params.c
 * via #include of this file after #define'ing away NACL_UNTRUSTED_INLINE.
 * We hope this is a temporary situation.
 */

/*
 * Minimum alignment that the thread pointer address ($tp) must have.
 * This is independent of any alignment requirement for actual TLS data,
 * which might be more or less than this.
 */
size_t __nacl_tp_alignment(void);

/*
 * Signed offset from $tp to the beginning of TLS data.
 * This is where the actual TLS for a thread is found.
 * The address ($tp + __nacl_tp_tls_offset(tls_size))
 * is what gets initialized with the .tdata image.
 */
ptrdiff_t __nacl_tp_tls_offset(size_t tls_size);

/*
 * Signed offset from $tp to the thread library's private thread data block.
 * This is where implementation-private data for the thread library (if any)
 * is stored.  On some machines it's required that the first word of this
 * be a pointer with value $tp.
 */
ptrdiff_t __nacl_tp_tdb_offset(size_t tdb_size);


/*
 * This actually has nothing whatsoever to do with TLS and doesn't really
 * belong here.  But it's treated the same way, so this is the convenient
 * place for it.
 *
 * Byte size of alignment padding required at the top of stack for a new
 * thread.  (On machines such as x86, this is the place where the return
 * address of the thread function would go.)
 */
size_t __nacl_thread_stack_padding(void);

#elif defined(__i386__) || defined(__x86_64__)

/*
 *  +-----------------+------+
 *  |  TLS data, bss  | TDB  |
 *  +-----------------+------+
 *                    ^
 *                    |
 *                    +--- $tp points here
 *                    |
 *                    +--- first word's value is $tp address
 *
 * In x86-32, %gs segment prefix gets the $tp address, as does fetching %gs:0.
 * In x86-64, __nacl_read_tp() must be called; it returns the $tp address.
 */

NACL_UNTRUSTED_INLINE
ptrdiff_t __nacl_tp_tls_offset(size_t tls_size) {
  return -(ptrdiff_t) tls_size;
}

NACL_UNTRUSTED_INLINE
ptrdiff_t __nacl_tp_tdb_offset(size_t tdb_size) {
  return 0;
}

/*
 * No particular alignment is required by the ABI.  But some x86 chips
 * behave poorly if the segment is not aligned to a cache line.  Those
 * chips have 64-byte cache lines (Atom).
 */
NACL_UNTRUSTED_INLINE
size_t __nacl_tp_alignment(void) {
  return 64;
}

NACL_UNTRUSTED_INLINE
size_t __nacl_thread_stack_padding(void) {
#ifdef __x86_64__
  return 8;
#else
  return 4;
#endif
}

#elif defined(__arm__)

/*
 *  +-----------+--------+----------------+
 *  |   TDB     | header | TLS data, bss  |
 *  +-----------+--------+----------------+
 *              ^        ^
 *              |        |
 *              |        +--- $tp+8 points here
 *              |
 *              +--- $tp points here
 *
 * In ARM EABI, __aeabi_read_tp() gets $tp address.
 * In NaCl, this is defined as register r9 with the low 12 bits masked off.
 */

NACL_UNTRUSTED_INLINE
ptrdiff_t __nacl_tp_tls_offset(size_t tls_size) {
  return 8;
}

NACL_UNTRUSTED_INLINE
ptrdiff_t __nacl_tp_tdb_offset(size_t tdb_size) {
  return -(ptrdiff_t) tdb_size;
}

/*
 * The $tp address must be aligned to reserve the low 12 bits of the
 * register for trusted code's use.
 */
NACL_UNTRUSTED_INLINE
size_t __nacl_tp_alignment(void) {
  return 1 << 12;
}

NACL_UNTRUSTED_INLINE
size_t __nacl_thread_stack_padding(void) {
  return 4;
}

#else

#error "unknown platform"

#endif

#endif /* NATIVE_CLIENT_SRC_UNTRUSTED_NACL_TLS_PARAMS_H_ */
