/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * cpuid.c
 * This module provides a simple abstraction for using the CPUID
 * instruction to determine instruction set extensions supported by
 * the current processor.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NACL_CPUID_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NACL_CPUID_H_

#include "native_client/src/include/portability.h"

typedef unsigned char bool;

/* Features needed to show that the architecture is supported. */
typedef struct nacl_arch_features {
  bool f_cpuid_supported;  /* CPUID is defined for the hardward. */
  bool f_cpu_supported;    /* CPU is one we support. */
} nacl_arch_features;

/* Features we can get about the x86 hardware. */
typedef struct cpu_feature_struct {
  nacl_arch_features arch_features;
  bool f_x87;
  bool f_MMX;
  bool f_SSE;
  bool f_SSE2;
  bool f_SSE3;
  bool f_SSSE3;
  bool f_SSE41;
  bool f_SSE42;
  bool f_MOVBE;
  bool f_POPCNT;
  bool f_CX8;
  bool f_CX16;
  bool f_CMOV;
  bool f_MON;
  bool f_FXSR;
  bool f_CLFLUSH;
  bool f_TSC;
  /* These instructions are illegal but included for completeness */
  bool f_MSR;
  bool f_VME;
  bool f_PSN;
  bool f_VMX;
  /* AMD-specific features */
  bool f_3DNOW;
  bool f_EMMX;
  bool f_E3DNOW;
  bool f_LZCNT;
  bool f_SSE4A;
  bool f_LM;
  bool f_SVM;
} CPUFeatures;

/* Define the maximum length of a CPUID string.
 *
 * Note: If you change this length, fix the static initialization of wlid
 * in nacl_cpuid.c to be initialized with an appropriate string.
 */
#define /* static const int */ kCPUIDStringLength 21

/* Defines the maximum number of feature registers used to hold CPUID.
 * Note: This value corresponds to the number of enumerated elements in
 * enum CPUFeatureReg defined in nacl_cpuid.c.
 */
#define kMaxCPUFeatureReg 8

/* Define a cache for collected CPU runtime information, from which
 * queries can answer questions.
 */
typedef struct NaClCPUData {
  /* The following is used to cache whether CPUID is defined for the
   * architecture the code is running on.
   */
  int _has_CPUID;
  /* Version ID words used by CPUVersionID. */
  uint32_t _vidwords[4];
  /* Define the set of CPUID feature register values for the architecture.
   * Note: We have two sets (of 4 registers) so that AMD specific flags can be
   * picked up.
   */
  uint32_t _featurev[kMaxCPUFeatureReg];
  /* Define a string to hold and cache the CPUID. In such cases, such races
   * will at worst cause the CPUID to not be recognized.
   */
  char _wlid[kCPUIDStringLength];
} NaClCPUData;

/* Collect CPU data about this CPU, and put into the given data structure.
 */
void NaClCPUDataGet(NaClCPUData* data);

/* Set cpu check state fields to all true. */
void NaClSetAllCPUFeatures(CPUFeatures *features);

/* Clear cpu check state fields (i.e. set all fields to false). */
void NaClClearCPUFeatures(CPUFeatures *features);

/* Copy a set of cpu features. */
void NaClCopyCPUFeatures(CPUFeatures* target, const CPUFeatures* source);

/* Fills in cpuf with feature vector for this CPU. */
extern void GetCPUFeatures(NaClCPUData* data, CPUFeatures *cpuf);

/* GetCPUIDString creates an ASCII string that identfies this CPU's
 * vendor ID, family, model, and stepping, as per the CPUID instruction
 */
extern char *GetCPUIDString(NaClCPUData* data);

/* Returns true if CPUID is defined, and the CPU is supported. */
extern bool NaClArchSupported(NaClCPUData* data);

#endif /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NACL_CPUID_H_ */
