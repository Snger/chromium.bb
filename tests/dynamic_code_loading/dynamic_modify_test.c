/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include <sys/nacl_syscalls.h>

#include "native_client/tests/dynamic_code_loading/templates.h"
#include "native_client/tests/inbrowser_test_runner/test_runner.h"

#if defined(__x86_64__)
#define BUF_SIZE 64
#else
#define BUF_SIZE 32
#endif

#define NACL_BUNDLE_SIZE  32
/*
 * TODO(bsy): get this value from the toolchain.  Get the toolchain
 * team to provide this value.
 */
#define NUM_BUNDLES_FOR_HLT 3

/* TODO(mseaborn): Add a symbol to the linker script for finding the
   end of the static code segment more accurately.  The value below is
   an approximation. */
#define DYNAMIC_CODE_SEGMENT_START 0x80000
/* TODO(mseaborn): Add a symbol to the linker script for finding the
   end of the dynamic code region.  The value below is duplicated in
   nacl.scons, passed via --section-start. */
#define DYNAMIC_CODE_SEGMENT_END 0x1000000

struct code_section {
  char *name;
  char *start;
  char *end;
};

struct code_section illegal_code_sections[] = {
  { "misaligned_replacement",
    &template_func_misaligned_replacement,
    &template_func_misaligned_replacement_end },
  { "illegal_register_replacement",
    &template_func_illegal_register_replacement,
    &template_func_illegal_register_replacement_end },
  { "illegal_guard_replacement",
    &template_func_illegal_guard_replacement,
    &template_func_illegal_guard_replacement_end },
  { "illegal_call_target",
    &template_func_illegal_call_target,
    &template_func_illegal_call_target_end },
  { "illegal_constant_replacement",
    &template_func_illegal_constant_replacement,
    &template_func_illegal_constant_replacement_end },
};

uint8_t *next_addr = (uint8_t *) DYNAMIC_CODE_SEGMENT_START;

uint8_t *allocate_code_space(int pages) {
  uint8_t *addr = next_addr;
  next_addr += 0x10000 * pages;
  assert(next_addr < (uint8_t *) DYNAMIC_CODE_SEGMENT_END);
  return addr;
}

void fill_int32(uint8_t *data, size_t size, int32_t value) {
  int i;
  assert(size % 4 == 0);
  /* All the archs we target supported unaligned word read/write, but
     check that the pointer is aligned anyway. */
  assert(((uintptr_t) data) % 4 == 0);
  for (i = 0; i < size / 4; i++)
    ((uint32_t *) data)[i] = value;
}

void fill_nops(uint8_t *data, size_t size) {
#if defined(__i386__) || defined(__x86_64__)
  memset(data, 0x90, size); /* NOPs */
#elif defined(__arm__)
  fill_int32(data, size, 0xe1a00000); /* NOP (MOV r0, r0) */
#else
# error "Unknown arch"
#endif
}

/* Getting the assembler to pad our code fragments in templates.S is
   awkward because we have to output them in data mode, in which the
   assembler wants to output zeroes instead of NOPs for padding.
   Also, the assembler won't put in a terminating HLT, which we need
   on x86-32.  So we do the padding at run time. */
void copy_and_pad_fragment(void *dest,
                           int dest_size,
                           const char *fragment_start,
                           const char *fragment_end) {
  int fragment_size = fragment_end - fragment_start;
  assert(dest_size % 32 == 0);
  assert(fragment_size <= dest_size);
  fill_nops(dest, dest_size);
  memcpy(dest, fragment_start, fragment_size);
}

/* Check that we can't dynamically rewrite code. */
void test_replacing_code() {
  uint8_t *load_area = allocate_code_space(1);
  uint8_t buf[BUF_SIZE];
  int rc;
  int (*func)();

  copy_and_pad_fragment(buf, sizeof(buf), &template_func, &template_func_end);
  rc = nacl_dyncode_create(load_area, buf, sizeof(buf));
  assert(rc == 0);
  func = (int (*)()) (uintptr_t) load_area;
  rc = func();
  assert(rc == 1234);

  /* write replacement to the same location */
  copy_and_pad_fragment(buf, sizeof(buf), &template_func_replacement,
                                          &template_func_replacement_end);
  rc = nacl_dyncode_modify(load_area, buf, sizeof(buf));
  assert(rc == 0);
  func = (int (*)()) (uintptr_t) load_area;
  rc = func();
  assert(rc == 4321);
}


/* Check that we can dynamically rewrite code. */
void test_replacing_code_unaligned() {
  uint8_t *load_area = allocate_code_space(1);
  uint8_t buf[BUF_SIZE];
  int first_diff = 0;
  int rc;
  int (*func)();

  copy_and_pad_fragment(buf, sizeof(buf), &template_func, &template_func_end);
  rc = nacl_dyncode_create(load_area, buf, sizeof(buf));
  assert(rc == 0);
  func = (int (*)()) (uintptr_t) load_area;
  rc = func();
  assert(rc == 1234);

  /* write replacement to the same location, unaligned */
  copy_and_pad_fragment(buf, sizeof(buf), &template_func_replacement,
                                          &template_func_replacement_end);
  while (buf[first_diff] == load_area[first_diff] && first_diff < sizeof buf) {
    first_diff++;
  }
  assert(first_diff > 0 && first_diff <= sizeof(buf));
  rc = nacl_dyncode_modify(load_area+first_diff, buf+first_diff,
                           sizeof(buf)-first_diff);
  assert(rc == 0);
  func = (int (*)()) (uintptr_t) load_area;
  rc = func();
  assert(rc == 4321);
}

/* Check that we can dynamically delete code. */
void test_deleting_code() {
  uint8_t *load_area = allocate_code_space(1);
  uint8_t buf[BUF_SIZE];
  int rc;
  int (*func)();

  copy_and_pad_fragment(buf, sizeof(buf), &template_func, &template_func_end);
  rc = nacl_dyncode_create(load_area, buf, sizeof(buf));
  assert(rc == 0);
  func = (int (*)()) (uintptr_t) load_area;
  rc = func();
  assert(rc == 1234);

  rc = nacl_dyncode_delete(load_area, sizeof buf);
  assert(rc == 0);
  assert(load_area[0] != buf[0]);
}

/* Check code replacement constraints */
void test_illegal_code_replacment() {
  uint8_t *load_area = allocate_code_space(1);
  uint8_t buf[BUF_SIZE];
  int rc;
  int i;
  int (*func)();

  copy_and_pad_fragment(buf, sizeof(buf), &template_func, &template_func_end);
  rc = nacl_dyncode_create(load_area, buf, sizeof(buf));
  assert(rc == 0);
  func = (int (*)()) (uintptr_t) load_area;
  rc = func();
  assert(rc == 1234);

  for (i = 0;
       i < (sizeof(illegal_code_sections) / sizeof(struct code_section));
       i++) {
    printf("\t%s\n", illegal_code_sections[i].name);

    /* write illegal replacement to the same location */
    copy_and_pad_fragment(buf, sizeof(buf), illegal_code_sections[i].start,
                                            illegal_code_sections[i].end);
    rc = nacl_dyncode_modify(load_area, buf, sizeof(buf));
    assert(rc != 0);
    func = (int (*)()) (uintptr_t) load_area;
    rc = func();
    assert(rc == 1234);
  }
}

void test_external_jump_target_replacement() {
  uint8_t *load_area = allocate_code_space(1);
  /* BUF_SIZE * 2 because this function necessarily has an extra bundle. */
  uint8_t buf[BUF_SIZE * 2];
  int rc;
  int (*func)();
  const int kNaClBundleSize = NACL_BUNDLE_SIZE;

  copy_and_pad_fragment(buf, sizeof(buf),
                        &template_func_external_jump_target,
                        &template_func_external_jump_target_end);

  rc = nacl_dyncode_create(load_area, buf, sizeof(buf));
  assert(rc == 0);
  func = (int (*)()) (uintptr_t) load_area;
  rc = func();
  assert(rc == 1234);

  copy_and_pad_fragment(buf, sizeof(buf),
                        &template_func_external_jump_target_replace,
                        &template_func_external_jump_target_replace_end);
  /* Only copy one bundle so we can test an unaligned external jump target */
  rc = nacl_dyncode_modify(load_area, buf, kNaClBundleSize);
  assert(rc == 0);
  func = (int (*)()) (uintptr_t) load_area;
  rc = func();
  assert(rc == 4321);
}

/* Check that we can't dynamically rewrite code. */
void test_replacing_code_disabled() {
  uint8_t *load_area = allocate_code_space(1);
  uint8_t buf[BUF_SIZE];
  int rc;
  int (*func)();

  copy_and_pad_fragment(buf, sizeof(buf), &template_func, &template_func_end);
  rc = nacl_dyncode_create(load_area, buf, sizeof(buf));
  assert(rc == 0);
  func = (int (*)()) (uintptr_t) load_area;
  rc = func();
  assert(rc == 1234);

  /* write replacement to the same location */
  copy_and_pad_fragment(buf, sizeof(buf), &template_func_replacement,
                                          &template_func_replacement_end);
  rc = nacl_dyncode_modify(load_area, buf, sizeof(buf));
  assert(rc != 0);
  func = (int (*)()) (uintptr_t) load_area;
  rc = func();
  assert(rc == 1234);
}

/* Check that we can dynamically rewrite code. */
void test_replacing_code_unaligned_disabled() {
  uint8_t *load_area = allocate_code_space(1);
  uint8_t buf[BUF_SIZE];
  int first_diff = 0;
  int rc;
  int (*func)();

  copy_and_pad_fragment(buf, sizeof(buf), &template_func, &template_func_end);
  rc = nacl_dyncode_create(load_area, buf, sizeof(buf));
  assert(rc == 0);
  func = (int (*)()) (uintptr_t) load_area;
  rc = func();
  assert(rc == 1234);

  /* write replacement to the same location, unaligned */
  copy_and_pad_fragment(buf, sizeof(buf), &template_func_replacement,
                                          &template_func_replacement_end);
  while (buf[first_diff] == load_area[first_diff] && first_diff < sizeof buf) {
    first_diff++;
  }
  rc = nacl_dyncode_modify(load_area+first_diff, buf+first_diff,
                           sizeof(buf)-first_diff);
  assert(rc != 0);
  func = (int (*)()) (uintptr_t) load_area;
  rc = func();
  assert(rc == 1234);
}


/* Check that we can't delete code */
void test_deleting_code_disabled() {
  uint8_t *load_area = allocate_code_space(1);
  uint8_t buf[BUF_SIZE];
  int rc;
  int (*func)();

  copy_and_pad_fragment(buf, sizeof(buf), &template_func, &template_func_end);
  rc = nacl_dyncode_create(load_area, buf, sizeof(buf));
  assert(rc == 0);
  func = (int (*)()) (uintptr_t) load_area;
  rc = func();
  assert(rc == 1234);

  rc = nacl_dyncode_delete(load_area, sizeof buf);
  assert(rc != 0);
  assert(load_area[0] == buf[0]);
}

void run_test(const char *test_name, void (*test_func)(void)) {
  printf("Running %s...\n", test_name);
  test_func();
}

int is_replacement_enabled() {
  char trash;
  return (0 == nacl_dyncode_modify(allocate_code_space(1), &trash, 0));
}

#define RUN_TEST(test_func) (run_test(#test_func, test_func))

int TestMain() {
  /* Turn off stdout buffering to aid debugging in case of a crash. */
  setvbuf(stdout, NULL, _IONBF, 0);

  if (is_replacement_enabled()) {
    printf("Code replacement ENABLED\n");
    RUN_TEST(test_replacing_code);
    RUN_TEST(test_replacing_code_unaligned);
    RUN_TEST(test_deleting_code);
    RUN_TEST(test_illegal_code_replacment);
    RUN_TEST(test_external_jump_target_replacement);
  } else {
    printf("Code replacement DISABLED\n");
    RUN_TEST(test_replacing_code_disabled);
    RUN_TEST(test_replacing_code_unaligned_disabled);
    RUN_TEST(test_deleting_code_disabled);
  }

  return 0;
}

int main() {
  return RunTests(TestMain);
}

