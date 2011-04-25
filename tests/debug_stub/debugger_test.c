/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

void set_registers_and_stop() {
  /* Set most registers to fixed values before faulting, so that we
     can test that the debug stub successfully returns the same
     values. */
#if defined(__x86_64__)
  /* Note that we cannot assign arbitrary test values to %r15, %rsp
     and %rbp in the x86-64 sandbox. */
  __asm__("mov $0xbbb0000000000ccc, %rax\n"
          "push %rax\n"
          "mov $0x1100000000000022, %rax\n"
          "mov $0x2200000000000033, %rbx\n"
          "mov $0x3300000000000044, %rcx\n"
          "mov $0x4400000000000055, %rdx\n"
          "mov $0x5500000000000066, %rsi\n"
          "mov $0x6600000000000077, %rdi\n"
          "mov $0x7700000000000088, %r8\n"
          "mov $0x8800000000000099, %r9\n"
          "mov $0x99000000000000aa, %r10\n"
          "mov $0xaa000000000000bb, %r11\n"
          "mov $0xbb000000000000cc, %r12\n"
          "mov $0xcc000000000000dd, %r13\n"
          "mov $0xdd000000000000ee, %r14\n"
          "hlt\n");
#else
# error Update this test for other architectures
#endif
}

int main(int argc, char **argv) {
  /*
   * This will crash if the entry-point breakpoint has been mishandled such
   * that our argc and argv values are bogus.  This should catch any
   * regression of http://code.google.com/p/nativeclient/issues/detail?id=1730.
   */
  argv[argc] = 0;

  set_registers_and_stop();
  return 1;
}
