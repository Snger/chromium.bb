/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error This file is not meant for use in the TCB
#endif

#include "native_client/src/trusted/validator_arm/inst_classes_testers.h"

#include "gtest/gtest.h"
#include "native_client/src/trusted/validator_arm/decoder_tester.h"

using nacl_arm_dec::kConditions;
using nacl_arm_dec::kRegisterNone;
using nacl_arm_dec::kRegisterPc;
using nacl_arm_dec::kRegisterStack;
using nacl_arm_dec::Instruction;

namespace nacl_arm_test {

// UnsafeClassDecoderTester
UnsafeCondNopTester::UnsafeCondNopTester(
    const NamedClassDecoder& decoder) : Arm32DecoderTester(decoder) {}

bool UnsafeCondNopTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::UnsafeCondNop expected_decoder(nacl_arm_dec::UNKNOWN);

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Apply ARM restriction -- I.e. we shouldn't be here. This is an
  // UNSAFE instruction.
  NC_EXPECT_FALSE_PRECOND(true);

  // Don't continue, we've already reported the root problem!
  return false;
}

// CondNopTester
CondNopTester::CondNopTester(const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool CondNopTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::CondNop expected_decoder;
  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  return true;
}

// Unary1RegisterImmediateOpTester
Unary1RegisterImmediateOpTester::Unary1RegisterImmediateOpTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Unary1RegisterImmediateOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary1RegisterImmediateOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder.imm4.value(inst), inst.Bits(19, 16));
  EXPECT_EQ(expected_decoder.imm12.value(inst), inst.Bits(11, 0));
  EXPECT_EQ(expected_decoder.ImmediateValue(inst),
            (inst.Bits(19, 16) << 12) | inst.Bits(11, 0));
  EXPECT_LT(expected_decoder.ImmediateValue(inst), (uint32_t) 0x10000);

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Unary1RegisterImmediateOpTesterRegsNotPc
Unary1RegisterImmediateOpTesterRegsNotPc::
Unary1RegisterImmediateOpTesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}

bool Unary1RegisterImmediateOpTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary1RegisterImmediateOp expected_decoder;

  NC_PRECOND(Unary1RegisterImmediateOpTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Unary1RegisterImmediateOpTesterNotRdIsPcAndS
Unary1RegisterImmediateOpTesterNotRdIsPcAndS::
Unary1RegisterImmediateOpTesterNotRdIsPcAndS(
    const NamedClassDecoder& decoder)
    : Unary1RegisterImmediateOpTester(decoder) {}

bool Unary1RegisterImmediateOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary1RegisterImmediateOp expected_decoder;

  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder.d.reg(inst).Equals(kRegisterPc)) &&
      expected_decoder.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Unary1RegisterImmediateOpTester::ApplySanityChecks(inst, decoder);
}

// Unary1RegisterBitRangeTester
Unary1RegisterBitRangeTester::Unary1RegisterBitRangeTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Unary1RegisterBitRangeTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary1RegisterBitRange expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check registers and flags used.
  EXPECT_TRUE(expected_decoder.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_EQ(expected_decoder.lsb.value(inst), inst.Bits(11, 7));
  EXPECT_EQ(expected_decoder.msb.value(inst), inst.Bits(20, 16));
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
        << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Binary2RegisterImmediateOpTester
Binary2RegisterImmediateOpTester::Binary2RegisterImmediateOpTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder), apply_rd_is_pc_check_(true) {}

bool Binary2RegisterImmediateOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary2RegisterImmediateOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_TRUE(expected_decoder.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_TRUE(expected_decoder.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder.imm.value(inst), inst.Bits(11, 0));

  // Other NaCl constraints about this instruction.
  if (apply_rd_is_pc_check_) {
    EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
        << "Expected FORBIDDEN_OPERANDS for " << InstContents();
  }

  return true;
}

// Binary2RegisterImmediateOpTesterNotRdIsPcAndS
Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
Binary2RegisterImmediateOpTesterNotRdIsPcAndS(
    const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTester(decoder) {}

bool Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary2RegisterImmediateOp expected_decoder;

  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder.d.reg(inst).Equals(kRegisterPc)) &&
      expected_decoder.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Binary2RegisterImmediateOpTester::ApplySanityChecks(inst, decoder);
}

// Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS
Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS::
Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS(
    const NamedClassDecoder& decoder)
    : Binary2RegisterImmediateOpTesterNotRdIsPcAndS(decoder) {}

bool Binary2RegisterImmediateOpTesterNeitherRdIsPcAndSNorRnIsPcAndNotS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary2RegisterImmediateOp expected_decoder;

  // Check that we don't parse when Rn=15 and S=0.
  if ((expected_decoder.n.reg(inst).Equals(kRegisterPc)) &&
      !expected_decoder.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Binary2RegisterImmediateOpTesterNotRdIsPcAndS::
      ApplySanityChecks(inst, decoder);
}

// BinaryRegisterImmediateTestTester
BinaryRegisterImmediateTestTester::BinaryRegisterImmediateTestTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool BinaryRegisterImmediateTestTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::BinaryRegisterImmediateTest expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder.imm.value(inst), inst.Bits(11, 0));

  return true;
}

// Unary2RegisterOpTester
Unary2RegisterOpTester::Unary2RegisterOpTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Unary2RegisterOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary2RegisterOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(!Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Unary2RegisterOpTesterNotRdIsPcAndS
Unary2RegisterOpTesterNotRdIsPcAndS::Unary2RegisterOpTesterNotRdIsPcAndS(
    const NamedClassDecoder& decoder)
    : Unary2RegisterOpTester(decoder) {}


bool Unary2RegisterOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary2RegisterOp expected_decoder;

  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder.d.reg(inst).Equals(kRegisterPc)) &&
      expected_decoder.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Unary2RegisterOpTester::ApplySanityChecks(inst, decoder);
}

// Binary3RegisterOpTester
Binary3RegisterOpTester::Binary3RegisterOpTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Binary3RegisterOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary3RegisterOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder.m.reg(inst).Equals(inst.Reg(11, 8)));
  EXPECT_TRUE(expected_decoder.n.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Binary3RegisterOpTesterRegsNotPc
Binary3RegisterOpTesterRegsNotPc::Binary3RegisterOpTesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Binary3RegisterOpTester(decoder) {}

bool Binary3RegisterOpTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary3RegisterOp expected_decoder;

  NC_PRECOND(Binary3RegisterOpTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.n.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Binary3RegisterOpTesterAltA
Binary3RegisterOpAltATester::Binary3RegisterOpAltATester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Binary3RegisterOpAltATester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary3RegisterOpAltA expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder.d.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_TRUE(expected_decoder.m.reg(inst).Equals(inst.Reg(11, 8)));
  EXPECT_TRUE(expected_decoder.n.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_TRUE(expected_decoder.conditions.conds_if_updated(inst).
                Equals(kConditions));
  } else {
    EXPECT_TRUE(expected_decoder.conditions.conds_if_updated(inst).
                Equals(kRegisterNone));
  }

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Binary3RegisterOpAltATesterRegsNotPc
Binary3RegisterOpAltATesterRegsNotPc::Binary3RegisterOpAltATesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltATester(decoder) {}

bool Binary3RegisterOpAltATesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary3RegisterOpAltA expected_decoder;

  NC_PRECOND(Binary3RegisterOpAltATester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.n.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Binary3RegisterOpTesterAltB
Binary3RegisterOpAltBTester::Binary3RegisterOpAltBTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder), test_conditions_(true) {}

bool Binary3RegisterOpAltBTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary3RegisterOpAltB expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_TRUE(expected_decoder.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.Bit(20));
  if (test_conditions_) {
    if (expected_decoder.conditions.is_updated(inst)) {
      EXPECT_TRUE(expected_decoder.conditions.conds_if_updated(inst).
                  Equals(kConditions));
    } else {
      EXPECT_TRUE(expected_decoder.conditions.conds_if_updated(inst).
                  Equals(kRegisterNone));
    }
  }

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Binary3RegisterOpAltBTesterRegsNotPc
Binary3RegisterOpAltBTesterRegsNotPc::Binary3RegisterOpAltBTesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTester(decoder) {}

bool Binary3RegisterOpAltBTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary3RegisterOpAltB expected_decoder;

  NC_PRECOND(Binary3RegisterOpAltBTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.n.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Binary3RegisterOpAltBNoCondUpdatesTester
Binary3RegisterOpAltBNoCondUpdatesTester::
Binary3RegisterOpAltBNoCondUpdatesTester(
    const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBTester(decoder) {
  test_conditions_ = false;
}

// Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc
Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Binary3RegisterOpAltBNoCondUpdatesTester(decoder) {
}

bool Binary3RegisterOpAltBNoCondUpdatesTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary3RegisterOpAltBNoCondUpdates expected_decoder;

  NC_PRECOND(Binary3RegisterOpAltBNoCondUpdatesTester::
             ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.n.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Binary4RegisterDualOpTester
Binary4RegisterDualOpTester::Binary4RegisterDualOpTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Binary4RegisterDualOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary4RegisterDualOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder.d.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_TRUE(expected_decoder.a.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder.m.reg(inst).Equals(inst.Reg(11, 8)));
  EXPECT_TRUE(expected_decoder.n.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_TRUE(expected_decoder.conditions.conds_if_updated(inst).
                Equals(kConditions));
  } else {
    EXPECT_TRUE(expected_decoder.conditions.conds_if_updated(inst).
                Equals(kRegisterNone));
  }

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Binary4RegisterDualOpTesterRegsNotPc
Binary4RegisterDualOpTesterRegsNotPc::Binary4RegisterDualOpTesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Binary4RegisterDualOpTester(decoder) {}

bool Binary4RegisterDualOpTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary4RegisterDualOp expected_decoder;
  NC_PRECOND(Binary4RegisterDualOpTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.a.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.n.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();

  return true;
}
// Binary4RegisterDualResultTester
Binary4RegisterDualResultTester::Binary4RegisterDualResultTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Binary4RegisterDualResultTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary4RegisterDualResult expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder.d_hi.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_TRUE(expected_decoder.d_lo.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder.m.reg(inst).Equals(inst.Reg(11, 8)));
  EXPECT_TRUE(expected_decoder.n.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_TRUE(expected_decoder.conditions.conds_if_updated(inst).
                Equals(kConditions));
  } else {
    EXPECT_TRUE(expected_decoder.conditions.conds_if_updated(inst).
                Equals(kRegisterNone));
  }

  // Arm constraint between RdHi and RdLo.
  EXPECT_FALSE(expected_decoder.d_hi.reg(inst).
               Equals(expected_decoder.d_lo.reg(inst)))
      << "Expected UNPREDICTABLE for " << InstContents();

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder.d_lo.reg(inst).Equals(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();
  EXPECT_FALSE(expected_decoder.d_hi.reg(inst).Equals(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Binary4RegisterDualResultTesterRegsNotPc
Binary4RegisterDualResultTesterRegsNotPc::
Binary4RegisterDualResultTesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Binary4RegisterDualResultTester(decoder) {}

bool Binary4RegisterDualResultTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary4RegisterDualResult expected_decoder;

  NC_PRECOND(Binary4RegisterDualResultTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder.d_hi.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.d_lo.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.n.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// LoadStore2RegisterImmediateOpTester
LoadStore2RegisterImmediateOpTester::LoadStore2RegisterImmediateOpTester(
    const NamedClassDecoder& decoder) : Arm32DecoderTester(decoder) {}

bool LoadStore2RegisterImmediateOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::LoadStore2RegisterImmediateOp expected_decoder;
  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Should not parse if P=0 && W=1.
  if (expected_decoder.indexing.IsPostIndexing(inst) &&
      expected_decoder.writes.IsDefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_TRUE(expected_decoder.t.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_EQ(expected_decoder.writes.IsDefined(inst), inst.Bit(21));
  EXPECT_EQ(expected_decoder.direction.IsAdd(inst), inst.Bit(23));
  EXPECT_EQ(expected_decoder.indexing.IsPreIndexing(inst), inst.Bit(24));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder.t.reg(inst).Equals(kRegisterPc))
      << "Expected UNPREDICTABLE for " << InstContents();

  EXPECT_FALSE(expected_decoder.HasWriteBack(inst) &&
               (expected_decoder.n.reg(inst).Equals(kRegisterPc) ||
                expected_decoder.n.reg(inst).Equals(
                    expected_decoder.t.reg(inst))))
      << "Expected UNPREDICTABLE for " << InstContents();

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(ExpectedDecoder().defs(inst).Contains(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// LoadStore2RegisterImmediateOpTesterNotRnIsPc
LoadStore2RegisterImmediateOpTesterNotRnIsPc::
LoadStore2RegisterImmediateOpTesterNotRnIsPc(
    const NamedClassDecoder& decoder)
    : LoadStore2RegisterImmediateOpTester(decoder) {}

bool LoadStore2RegisterImmediateOpTesterNotRnIsPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::LoadStore2RegisterImmediateOp expected_decoder;

  // Check that we don't parse when Rn=15.
  if (expected_decoder.n.reg(inst).Equals(kRegisterPc)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return LoadStore2RegisterImmediateOpTester::ApplySanityChecks(inst, decoder);
}

// LoadStore2RegisterImmediateDoubleOpTester
LoadStore2RegisterImmediateDoubleOpTester::
LoadStore2RegisterImmediateDoubleOpTester(const NamedClassDecoder& decoder)
    : LoadStore2RegisterImmediateOpTester(decoder) {}

bool LoadStore2RegisterImmediateDoubleOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore2RegisterImmediateOpTester::
             ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  nacl_arm_dec::LoadStore2RegisterImmediateDoubleOp expected_decoder;
  EXPECT_EQ(expected_decoder.t.number(inst) + 1,
            expected_decoder.t2.number(inst));

  // Other ARM constraints about this instruction.
  EXPECT_TRUE(expected_decoder.t.IsEven(inst));
  EXPECT_NE(expected_decoder.t2.number(inst), static_cast<uint32_t>(15))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder.HasWriteBack(inst) &&
               expected_decoder.n.reg(inst).Equals(
                   expected_decoder.t2.reg(inst)))
      << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// LoadStore2RegisterImmediateDoubleOpTesterNotRnIsPc
LoadStore2RegisterImmediateDoubleOpTesterNotRnIsPc::
LoadStore2RegisterImmediateDoubleOpTesterNotRnIsPc(
    const NamedClassDecoder& decoder)
    : LoadStore2RegisterImmediateDoubleOpTester(decoder) {}

bool LoadStore2RegisterImmediateDoubleOpTesterNotRnIsPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::LoadStore2RegisterImmediateDoubleOp expected_decoder;

  // Check that we don't parse when Rn=15.
  if (expected_decoder.n.reg(inst).Equals(kRegisterPc)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return LoadStore2RegisterImmediateDoubleOpTester::
      ApplySanityChecks(inst, decoder);
}

// LoadStore3RegisterOpTester
LoadStore3RegisterOpTester::LoadStore3RegisterOpTester(
    const NamedClassDecoder& decoder) : Arm32DecoderTester(decoder) {}

bool LoadStore3RegisterOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::LoadStore3RegisterOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Should not parse if P=0 && W=1.
  if (expected_decoder.indexing.IsPostIndexing(inst) &&
      expected_decoder.writes.IsDefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  EXPECT_TRUE(expected_decoder.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_TRUE(expected_decoder.t.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_EQ(expected_decoder.writes.IsDefined(inst), inst.Bit(21));
  EXPECT_EQ(expected_decoder.direction.IsAdd(inst), inst.Bit(23));
  EXPECT_EQ(expected_decoder.indexing.IsPreIndexing(inst), inst.Bit(24));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder.n.reg(inst).Equals(kRegisterPc))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder.t.reg(inst).Equals(kRegisterPc))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder.HasWriteBack(inst) &&
               (expected_decoder.n.reg(inst).Equals(kRegisterPc) ||
                expected_decoder.n.reg(inst).Equals(
                    expected_decoder.t.reg(inst))))
      << "Expected UNPREDICTABLE for " << InstContents();

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder.indexing.IsPreIndexing(inst))
      << "Expected FORBIDDEN for " << InstContents();

  EXPECT_FALSE(ExpectedDecoder().defs(inst).Contains(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// LoadStore3RegisterDoubleOpTester
LoadStore3RegisterDoubleOpTester::
LoadStore3RegisterDoubleOpTester(const NamedClassDecoder& decoder)
    : LoadStore3RegisterOpTester(decoder) {
}

bool LoadStore3RegisterDoubleOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  NC_PRECOND(LoadStore3RegisterOpTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used.
  nacl_arm_dec::LoadStore3RegisterDoubleOp expected_decoder;
  EXPECT_EQ(expected_decoder.t.number(inst) + 1,
            expected_decoder.t2.number(inst));

  // Other ARM constraints about this instruction.
  EXPECT_TRUE(expected_decoder.t.IsEven(inst));
  EXPECT_NE(expected_decoder.t2.number(inst), static_cast<uint32_t>(15))
      << "Expected UNPREDICTABLE for " << InstContents();
  EXPECT_FALSE(expected_decoder.HasWriteBack(inst) &&
               expected_decoder.n.reg(inst).Equals(
                   expected_decoder.t2.reg(inst)))
      << "Expected UNPREDICTABLE for " << InstContents();

  return true;
}

// Unary2RegisterImmedShiftedOpTester
Unary2RegisterImmedShiftedOpTester::Unary2RegisterImmedShiftedOpTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Unary2RegisterImmedShiftedOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary2RegisterImmedShiftedOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder.imm.value(inst), inst.Bits(11, 7));
  EXPECT_EQ(expected_decoder.shift_type.value(inst), inst.Bits(6, 5));

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Unary2RegisterImmedShiftedOpTesterImm5NotZero
Unary2RegisterImmedShiftedOpTesterImm5NotZero::
Unary2RegisterImmedShiftedOpTesterImm5NotZero(
    const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}

bool Unary2RegisterImmedShiftedOpTesterImm5NotZero::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary2RegisterImmedShiftedOp expected_decoder;

  // Check that we don't parse when imm5=0.
  if (0 == expected_decoder.imm.value(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Unary2RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder);
}

// Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS
Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS::
Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS(
    const NamedClassDecoder& decoder)
    : Unary2RegisterImmedShiftedOpTester(decoder) {}

bool Unary2RegisterImmedShiftedOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary2RegisterImmedShiftedOp expected_decoder;

  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder.d.reg(inst).Equals(kRegisterPc)) &&
      expected_decoder.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Unary2RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder);
}

// Unary3RegisterShiftedOpTester
Unary3RegisterShiftedOpTester::Unary3RegisterShiftedOpTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Unary3RegisterShiftedOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary3RegisterShiftedOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder.s.reg(inst).Equals(inst.Reg(11, 8)));
  EXPECT_TRUE(expected_decoder.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Check the shift type.
  EXPECT_EQ(expected_decoder.shift_type.value(inst), inst.Bits(6, 5));

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Unary3RegisterShiftedOpTesterRegsNotPc
Unary3RegisterShiftedOpTesterRegsNotPc::Unary3RegisterShiftedOpTesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Unary3RegisterShiftedOpTester(decoder) {}

bool Unary3RegisterShiftedOpTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Unary3RegisterShiftedOp expected_decoder;

  NC_PRECOND(Unary3RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.s.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Binary3RegisterImmedShiftedOpTester
Binary3RegisterImmedShiftedOpTester::Binary3RegisterImmedShiftedOpTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Binary3RegisterImmedShiftedOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary3RegisterImmedShiftedOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_TRUE(expected_decoder.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder.imm.value(inst), inst.Bits(11, 7));
  EXPECT_EQ(expected_decoder.shift_type.value(inst), inst.Bits(6, 5));

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS
Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS(
    const NamedClassDecoder& decoder)
    : Binary3RegisterImmedShiftedOpTester(decoder) {}

bool Binary3RegisterImmedShiftedOpTesterNotRdIsPcAndS::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary3RegisterImmedShiftedOp expected_decoder;

  // Check that we don't parse when Rd=15 and S=1.
  if ((expected_decoder.d.reg(inst).Equals(kRegisterPc)) &&
      expected_decoder.conditions.is_updated(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  return Binary3RegisterImmedShiftedOpTester::ApplySanityChecks(inst, decoder);
}

// Binary4RegisterShiftedOpTester
Binary4RegisterShiftedOpTester::Binary4RegisterShiftedOpTester(
    const NamedClassDecoder& decoder)
      : Arm32DecoderTester(decoder) {}

bool Binary4RegisterShiftedOpTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary4RegisterShiftedOp expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_TRUE(expected_decoder.d.reg(inst).Equals(inst.Reg(15, 12)));
  EXPECT_TRUE(expected_decoder.s.reg(inst).Equals(inst.Reg(11, 8)));
  EXPECT_TRUE(expected_decoder.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Other NaCl constraints about this instruction.
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected FORBIDDEN_OPERANDS for " << InstContents();

  return true;
}

// Binary4RegisterShiftedOpTesterRegsNotPc
Binary4RegisterShiftedOpTesterRegsNotPc::
Binary4RegisterShiftedOpTesterRegsNotPc(
    const NamedClassDecoder& decoder)
      : Binary4RegisterShiftedOpTester(decoder) {}

bool Binary4RegisterShiftedOpTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary4RegisterShiftedOp expected_decoder;

  NC_PRECOND(Binary4RegisterShiftedOpTester::ApplySanityChecks(inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder.n.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.d.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.s.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

// Binary2RegisterImmedShiftedTestTester
Binary2RegisterImmedShiftedTestTester::Binary2RegisterImmedShiftedTestTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Binary2RegisterImmedShiftedTestTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary2RegisterImmedShiftedTest expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_TRUE(expected_decoder.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }

  // Check that immediate value is computed correctly.
  EXPECT_EQ(expected_decoder.imm.value(inst), inst.Bits(11, 7));
  EXPECT_EQ(expected_decoder.shift_type.value(inst), inst.Bits(6, 5));

  return true;
}

// Binary3RegisterShiftedTestTester
Binary3RegisterShiftedTestTester::Binary3RegisterShiftedTestTester(
    const NamedClassDecoder& decoder)
    : Arm32DecoderTester(decoder) {}

bool Binary3RegisterShiftedTestTester::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary3RegisterShiftedTest expected_decoder;

  // Check that condition is defined correctly.
  EXPECT_EQ(expected_decoder.cond.value(inst), inst.Bits(31, 28));

  // Didn't parse undefined conditional.
  if (expected_decoder.cond.undefined(inst)) {
    NC_EXPECT_NE_PRECOND(&ExpectedDecoder(), &decoder);
  }

  // Check if expected class name found.
  NC_PRECOND(Arm32DecoderTester::ApplySanityChecks(inst, decoder));

  // Check Registers and flags used in DataProc.
  EXPECT_TRUE(expected_decoder.n.reg(inst).Equals(inst.Reg(19, 16)));
  EXPECT_TRUE(expected_decoder.s.reg(inst).Equals(inst.Reg(11, 8)));
  EXPECT_TRUE(expected_decoder.m.reg(inst).Equals(inst.Reg(3, 0)));
  EXPECT_EQ(expected_decoder.conditions.is_updated(inst), inst.Bit(20));
  if (expected_decoder.conditions.is_updated(inst)) {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).Equals(kConditions));
  } else {
    EXPECT_TRUE(
        expected_decoder.conditions.conds_if_updated(inst).
        Equals(kRegisterNone));
  }
  EXPECT_EQ(expected_decoder.shift_type.value(inst), inst.Bits(6, 5));

  return true;
}

// Binary3RegisterShiftedTestTesterRegsNotPc
Binary3RegisterShiftedTestTesterRegsNotPc::
Binary3RegisterShiftedTestTesterRegsNotPc(
    const NamedClassDecoder& decoder)
    : Binary3RegisterShiftedTestTester(decoder) {}

bool Binary3RegisterShiftedTestTesterRegsNotPc::
ApplySanityChecks(Instruction inst,
                  const NamedClassDecoder& decoder) {
  nacl_arm_dec::Binary3RegisterShiftedTest expected_decoder;

  NC_PRECOND(Binary3RegisterShiftedTestTester::ApplySanityChecks(
      inst, decoder));

  // Other ARM constraints about this instruction.
  EXPECT_FALSE(expected_decoder.n.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.s.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();
  EXPECT_FALSE(expected_decoder.m.reg(inst).Equals(kRegisterPc))
      << "Expected Unpredictable for " << InstContents();

  return true;
}

}  // namespace
