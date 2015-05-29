/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "assembler_thumb2.h"

#include "base/stl_util.h"
#include "utils/assembler_test.h"

namespace art {

class AssemblerThumb2Test : public AssemblerTest<arm::Thumb2Assembler,
                                                 arm::Register, arm::SRegister,
                                                 uint32_t> {
 protected:
  std::string GetArchitectureString() OVERRIDE {
    return "arm";
  }

  std::string GetAssemblerParameters() OVERRIDE {
    return " -march=armv7-a -mcpu=cortex-a15 -mfpu=neon -mthumb";
  }

  const char* GetAssemblyHeader() OVERRIDE {
    return kThumb2AssemblyHeader;
  }

  std::string GetDisassembleParameters() OVERRIDE {
    return " -D -bbinary -marm --disassembler-options=force-thumb --no-show-raw-insn";
  }

  void SetUpHelpers() OVERRIDE {
    if (registers_.size() == 0) {
      registers_.insert(end(registers_),
                        {  // NOLINT(whitespace/braces)
                          new arm::Register(arm::R0),
                          new arm::Register(arm::R1),
                          new arm::Register(arm::R2),
                          new arm::Register(arm::R3),
                          new arm::Register(arm::R4),
                          new arm::Register(arm::R5),
                          new arm::Register(arm::R6),
                          new arm::Register(arm::R7),
                          new arm::Register(arm::R8),
                          new arm::Register(arm::R9),
                          new arm::Register(arm::R10),
                          new arm::Register(arm::R11),
                          new arm::Register(arm::R12),
                          new arm::Register(arm::R13),
                          new arm::Register(arm::R14),
                          new arm::Register(arm::R15)
                        });
    }
  }

  void TearDown() OVERRIDE {
    AssemblerTest::TearDown();
    STLDeleteElements(&registers_);
  }

  std::vector<arm::Register*> GetRegisters() OVERRIDE {
    return registers_;
  }

  uint32_t CreateImmediate(int64_t imm_value) OVERRIDE {
    return imm_value;
  }

  std::string RepeatInsn(size_t count, const std::string& insn) {
    std::string result;
    for (; count != 0u; --count) {
      result += insn;
    }
    return result;
  }

 private:
  std::vector<arm::Register*> registers_;

  static constexpr const char* kThumb2AssemblyHeader = ".syntax unified\n.thumb\n";
};

#define REPEAT_INSN_VAR_IMPL(base, line) a##b
#define REPEAT_INSN_VAR(line) REPEAT_INSN_VAR_IMPL(repeat_insn_var_, line)
#define REPEAT_INSN_IMPL(count, expr, var) \
  for (size_t var = 0u; var != count; ++var) { expr; }
#define REPEAT_INSN(count, expr) \
  do { \
    REPEAT_INSN_IMPL(count, expr, REPEAT_INSN_VAR(__LINE__)) \
  } while (false)


TEST_F(AssemblerThumb2Test, Toolchain) {
  EXPECT_TRUE(CheckTools());
}

#define __ GetAssembler()->

TEST_F(AssemblerThumb2Test, Sbfx) {
  __ sbfx(arm::R0, arm::R1, 0, 1);
  __ sbfx(arm::R0, arm::R1, 0, 8);
  __ sbfx(arm::R0, arm::R1, 0, 16);
  __ sbfx(arm::R0, arm::R1, 0, 32);

  __ sbfx(arm::R0, arm::R1, 8, 1);
  __ sbfx(arm::R0, arm::R1, 8, 8);
  __ sbfx(arm::R0, arm::R1, 8, 16);
  __ sbfx(arm::R0, arm::R1, 8, 24);

  __ sbfx(arm::R0, arm::R1, 16, 1);
  __ sbfx(arm::R0, arm::R1, 16, 8);
  __ sbfx(arm::R0, arm::R1, 16, 16);

  __ sbfx(arm::R0, arm::R1, 31, 1);

  const char* expected =
      "sbfx r0, r1, #0, #1\n"
      "sbfx r0, r1, #0, #8\n"
      "sbfx r0, r1, #0, #16\n"
      "sbfx r0, r1, #0, #32\n"

      "sbfx r0, r1, #8, #1\n"
      "sbfx r0, r1, #8, #8\n"
      "sbfx r0, r1, #8, #16\n"
      "sbfx r0, r1, #8, #24\n"

      "sbfx r0, r1, #16, #1\n"
      "sbfx r0, r1, #16, #8\n"
      "sbfx r0, r1, #16, #16\n"

      "sbfx r0, r1, #31, #1\n";
  DriverStr(expected, "sbfx");
}

TEST_F(AssemblerThumb2Test, Ubfx) {
  __ ubfx(arm::R0, arm::R1, 0, 1);
  __ ubfx(arm::R0, arm::R1, 0, 8);
  __ ubfx(arm::R0, arm::R1, 0, 16);
  __ ubfx(arm::R0, arm::R1, 0, 32);

  __ ubfx(arm::R0, arm::R1, 8, 1);
  __ ubfx(arm::R0, arm::R1, 8, 8);
  __ ubfx(arm::R0, arm::R1, 8, 16);
  __ ubfx(arm::R0, arm::R1, 8, 24);

  __ ubfx(arm::R0, arm::R1, 16, 1);
  __ ubfx(arm::R0, arm::R1, 16, 8);
  __ ubfx(arm::R0, arm::R1, 16, 16);

  __ ubfx(arm::R0, arm::R1, 31, 1);

  const char* expected =
      "ubfx r0, r1, #0, #1\n"
      "ubfx r0, r1, #0, #8\n"
      "ubfx r0, r1, #0, #16\n"
      "ubfx r0, r1, #0, #32\n"

      "ubfx r0, r1, #8, #1\n"
      "ubfx r0, r1, #8, #8\n"
      "ubfx r0, r1, #8, #16\n"
      "ubfx r0, r1, #8, #24\n"

      "ubfx r0, r1, #16, #1\n"
      "ubfx r0, r1, #16, #8\n"
      "ubfx r0, r1, #16, #16\n"

      "ubfx r0, r1, #31, #1\n";
  DriverStr(expected, "ubfx");
}

TEST_F(AssemblerThumb2Test, Vmstat) {
  __ vmstat();

  const char* expected = "vmrs APSR_nzcv, FPSCR\n";

  DriverStr(expected, "vmrs");
}

TEST_F(AssemblerThumb2Test, ldrexd) {
  __ ldrexd(arm::R0, arm::R1, arm::R0);
  __ ldrexd(arm::R0, arm::R1, arm::R1);
  __ ldrexd(arm::R0, arm::R1, arm::R2);
  __ ldrexd(arm::R5, arm::R3, arm::R7);

  const char* expected =
      "ldrexd r0, r1, [r0]\n"
      "ldrexd r0, r1, [r1]\n"
      "ldrexd r0, r1, [r2]\n"
      "ldrexd r5, r3, [r7]\n";
  DriverStr(expected, "ldrexd");
}

TEST_F(AssemblerThumb2Test, strexd) {
  __ strexd(arm::R9, arm::R0, arm::R1, arm::R0);
  __ strexd(arm::R9, arm::R0, arm::R1, arm::R1);
  __ strexd(arm::R9, arm::R0, arm::R1, arm::R2);
  __ strexd(arm::R9, arm::R5, arm::R3, arm::R7);

  const char* expected =
      "strexd r9, r0, r1, [r0]\n"
      "strexd r9, r0, r1, [r1]\n"
      "strexd r9, r0, r1, [r2]\n"
      "strexd r9, r5, r3, [r7]\n";
  DriverStr(expected, "strexd");
}

TEST_F(AssemblerThumb2Test, LdrdStrd) {
  __ ldrd(arm::R0, arm::Address(arm::R2, 8));
  __ ldrd(arm::R0, arm::Address(arm::R12));
  __ strd(arm::R0, arm::Address(arm::R2, 8));

  const char* expected =
      "ldrd r0, r1, [r2, #8]\n"
      "ldrd r0, r1, [r12]\n"
      "strd r0, r1, [r2, #8]\n";
  DriverStr(expected, "ldrdstrd");
}

TEST_F(AssemblerThumb2Test, eor) {
  __ eor(arm::R1, arm::R1, arm::ShifterOperand(arm::R0));
  __ eor(arm::R1, arm::R0, arm::ShifterOperand(arm::R1));
  __ eor(arm::R1, arm::R8, arm::ShifterOperand(arm::R0));
  __ eor(arm::R8, arm::R1, arm::ShifterOperand(arm::R0));
  __ eor(arm::R1, arm::R0, arm::ShifterOperand(arm::R8));

  const char* expected =
      "eors r1, r0\n"
      "eor r1, r0, r1\n"
      "eor r1, r8, r0\n"
      "eor r8, r1, r0\n"
      "eor r1, r0, r8\n";
  DriverStr(expected, "abs");
}

TEST_F(AssemblerThumb2Test, sub) {
  __ subs(arm::R1, arm::R0, arm::ShifterOperand(42));
  __ sub(arm::R1, arm::R0, arm::ShifterOperand(42));
  __ subs(arm::R1, arm::R0, arm::ShifterOperand(arm::R2, arm::ASR, 31));
  __ sub(arm::R1, arm::R0, arm::ShifterOperand(arm::R2, arm::ASR, 31));

  const char* expected =
      "subs r1, r0, #42\n"
      "subw r1, r0, #42\n"
      "subs r1, r0, r2, asr #31\n"
      "sub r1, r0, r2, asr #31\n";
  DriverStr(expected, "sub");
}

TEST_F(AssemblerThumb2Test, add) {
  __ adds(arm::R1, arm::R0, arm::ShifterOperand(42));
  __ add(arm::R1, arm::R0, arm::ShifterOperand(42));
  __ adds(arm::R1, arm::R0, arm::ShifterOperand(arm::R2, arm::ASR, 31));
  __ add(arm::R1, arm::R0, arm::ShifterOperand(arm::R2, arm::ASR, 31));

  const char* expected =
      "adds r1, r0, #42\n"
      "addw r1, r0, #42\n"
      "adds r1, r0, r2, asr #31\n"
      "add r1, r0, r2, asr #31\n";
  DriverStr(expected, "add");
}

TEST_F(AssemblerThumb2Test, umull) {
  __ umull(arm::R0, arm::R1, arm::R2, arm::R3);

  const char* expected =
      "umull r0, r1, r2, r3\n";
  DriverStr(expected, "umull");
}

TEST_F(AssemblerThumb2Test, smull) {
  __ smull(arm::R0, arm::R1, arm::R2, arm::R3);

  const char* expected =
      "smull r0, r1, r2, r3\n";
  DriverStr(expected, "smull");
}

TEST_F(AssemblerThumb2Test, StoreWordToThumbOffset) {
  arm::StoreOperandType type = arm::kStoreWord;
  int32_t offset = 4092;
  ASSERT_TRUE(arm::Address::CanHoldStoreOffsetThumb(type, offset));

  __ StoreToOffset(type, arm::R0, arm::SP, offset);
  __ StoreToOffset(type, arm::IP, arm::SP, offset);
  __ StoreToOffset(type, arm::IP, arm::R5, offset);

  const char* expected =
      "str r0, [sp, #4092]\n"
      "str ip, [sp, #4092]\n"
      "str ip, [r5, #4092]\n";
  DriverStr(expected, "StoreWordToThumbOffset");
}

TEST_F(AssemblerThumb2Test, StoreWordToNonThumbOffset) {
  arm::StoreOperandType type = arm::kStoreWord;
  int32_t offset = 4096;
  ASSERT_FALSE(arm::Address::CanHoldStoreOffsetThumb(type, offset));

  __ StoreToOffset(type, arm::R0, arm::SP, offset);
  __ StoreToOffset(type, arm::IP, arm::SP, offset);
  __ StoreToOffset(type, arm::IP, arm::R5, offset);

  const char* expected =
      "mov ip, #4096\n"       // LoadImmediate(ip, 4096)
      "add ip, ip, sp\n"
      "str r0, [ip, #0]\n"

      "str r5, [sp, #-4]!\n"  // Push(r5)
      "movw r5, #4100\n"      // LoadImmediate(r5, 4096 + kRegisterSize)
      "add r5, r5, sp\n"
      "str ip, [r5, #0]\n"
      "ldr r5, [sp], #4\n"    // Pop(r5)

      "str r6, [sp, #-4]!\n"  // Push(r6)
      "mov r6, #4096\n"       // LoadImmediate(r6, 4096)
      "add r6, r6, r5\n"
      "str ip, [r6, #0]\n"
      "ldr r6, [sp], #4\n";   // Pop(r6)
  DriverStr(expected, "StoreWordToNonThumbOffset");
}

TEST_F(AssemblerThumb2Test, StoreWordPairToThumbOffset) {
  arm::StoreOperandType type = arm::kStoreWordPair;
  int32_t offset = 1020;
  ASSERT_TRUE(arm::Address::CanHoldStoreOffsetThumb(type, offset));

  __ StoreToOffset(type, arm::R0, arm::SP, offset);
  // We cannot use IP (i.e. R12) as first source register, as it would
  // force us to use SP (i.e. R13) as second source register, which
  // would have an "unpredictable" effect according to the ARMv7
  // specification (the T1 encoding describes the result as
  // UNPREDICTABLE when of the source registers is R13).
  //
  // So we use (R11, IP) (e.g. (R11, R12)) as source registers in the
  // following instructions.
  __ StoreToOffset(type, arm::R11, arm::SP, offset);
  __ StoreToOffset(type, arm::R11, arm::R5, offset);

  const char* expected =
      "strd r0, r1, [sp, #1020]\n"
      "strd r11, ip, [sp, #1020]\n"
      "strd r11, ip, [r5, #1020]\n";
  DriverStr(expected, "StoreWordPairToThumbOffset");
}

TEST_F(AssemblerThumb2Test, StoreWordPairToNonThumbOffset) {
  arm::StoreOperandType type = arm::kStoreWordPair;
  int32_t offset = 1024;
  ASSERT_FALSE(arm::Address::CanHoldStoreOffsetThumb(type, offset));

  __ StoreToOffset(type, arm::R0, arm::SP, offset);
  // Same comment as in AssemblerThumb2Test.StoreWordPairToThumbOffset
  // regarding the use of (R11, IP) (e.g. (R11, R12)) as source
  // registers in the following instructions.
  __ StoreToOffset(type, arm::R11, arm::SP, offset);
  __ StoreToOffset(type, arm::R11, arm::R5, offset);

  const char* expected =
      "mov ip, #1024\n"           // LoadImmediate(ip, 1024)
      "add ip, ip, sp\n"
      "strd r0, r1, [ip, #0]\n"

      "str r5, [sp, #-4]!\n"      // Push(r5)
      "movw r5, #1028\n"          // LoadImmediate(r5, 1024 + kRegisterSize)
      "add r5, r5, sp\n"
      "strd r11, ip, [r5, #0]\n"
      "ldr r5, [sp], #4\n"        // Pop(r5)

      "str r6, [sp, #-4]!\n"      // Push(r6)
      "mov r6, #1024\n"           // LoadImmediate(r6, 1024)
      "add r6, r6, r5\n"
      "strd r11, ip, [r6, #0]\n"
      "ldr r6, [sp], #4\n";       // Pop(r6)
  DriverStr(expected, "StoreWordPairToNonThumbOffset");
}

TEST_F(AssemblerThumb2Test, TwoCbzMaxOffset) {
  Label label1;
  __ cbz(arm::R0, &label1);
  constexpr size_t kLdrR0R0Count1 = 63;
  REPEAT_INSN(kLdrR0R0Count1, __ ldr(arm::R0, arm::Address(arm::R0)));
  Label label2;
  __ cbz(arm::R0, &label2);
  __ Bind(&label1);
  constexpr size_t kLdrR0R0Count2 = 64;
  REPEAT_INSN(kLdrR0R0Count2, __ ldr(arm::R0, arm::Address(arm::R0)));
  __ Bind(&label2);

  std::string expected =
      "cbz r0, 1f\n" +            // cbz r0, label1
      RepeatInsn(kLdrR0R0Count1, "ldr r0, [r0]\n") +
      "cbz r0, 2f\n"              // cbz r0, label2
      "1:\n" +
      RepeatInsn(kLdrR0R0Count2, "ldr r0, [r0]\n") +
      "2:\n";
  DriverStr(expected, "BranchRelocation1");
}

TEST_F(AssemblerThumb2Test, TwoCbzBeyondMaxOffset) {
  Label label1;
  __ cbz(arm::R0, &label1);
  constexpr size_t kLdrR0R0Count1 = 63;
  REPEAT_INSN(kLdrR0R0Count1, __ ldr(arm::R0, arm::Address(arm::R0)));
  Label label2;
  __ cbz(arm::R0, &label2);
  __ Bind(&label1);
  constexpr size_t kLdrR0R0Count2 = 65;
  REPEAT_INSN(kLdrR0R0Count2, __ ldr(arm::R0, arm::Address(arm::R0)));
  __ Bind(&label2);

  std::string expected =
      "cmp r0, #0\n"              // cbz r0, label1
      "beq.n 1f\n" +
      RepeatInsn(kLdrR0R0Count1, "ldr r0, [r0]\n") +
      "cmp r0, #0\n"              // cbz r0, label2
      "beq.n 2f\n"
      "1:\n" +
      RepeatInsn(kLdrR0R0Count2, "ldr r0, [r0]\n") +
      "2:\n";
  DriverStr(expected, "BranchRelocation1");
}

TEST_F(AssemblerThumb2Test, TwoCbzSecondAtMaxB16Offset) {
  Label label1;
  __ cbz(arm::R0, &label1);
  constexpr size_t kLdrR0R0Count1 = 62;
  REPEAT_INSN(kLdrR0R0Count1, __ ldr(arm::R0, arm::Address(arm::R0)));
  Label label2;
  __ cbz(arm::R0, &label2);
  __ Bind(&label1);
  constexpr size_t kLdrR0R0Count2 = 128;
  REPEAT_INSN(kLdrR0R0Count2, __ ldr(arm::R0, arm::Address(arm::R0)));
  __ Bind(&label2);

  std::string expected =
      "cbz r0, 1f\n" +            // cbz r0, label1
      RepeatInsn(kLdrR0R0Count1, "ldr r0, [r0]\n") +
      "cmp r0, #0\n"              // cbz r0, label2
      "beq.n 2f\n"
      "1:\n" +
      RepeatInsn(kLdrR0R0Count2, "ldr r0, [r0]\n") +
      "2:\n";
  DriverStr(expected, "BranchRelocation1");
}

TEST_F(AssemblerThumb2Test, TwoCbzSecondBeyondMaxB16Offset) {
  Label label1;
  __ cbz(arm::R0, &label1);
  constexpr size_t kLdrR0R0Count1 = 62;
  REPEAT_INSN(kLdrR0R0Count1, __ ldr(arm::R0, arm::Address(arm::R0)));
  Label label2;
  __ cbz(arm::R0, &label2);
  __ Bind(&label1);
  constexpr size_t kLdrR0R0Count2 = 129;
  REPEAT_INSN(kLdrR0R0Count2, __ ldr(arm::R0, arm::Address(arm::R0)));
  __ Bind(&label2);

  std::string expected =
      "cmp r0, #0\n"              // cbz r0, label1
      "beq.n 1f\n" +
      RepeatInsn(kLdrR0R0Count1, "ldr r0, [r0]\n") +
      "cmp r0, #0\n"              // cbz r0, label2
      "beq.w 2f\n"
      "1:\n" +
      RepeatInsn(kLdrR0R0Count2, "ldr r0, [r0]\n") +
      "2:\n";
  DriverStr(expected, "BranchRelocation1");
}

TEST_F(AssemblerThumb2Test, TwoCbzFirstAtMaxB16Offset) {
  Label label1;
  __ cbz(arm::R0, &label1);
  constexpr size_t kLdrR0R0Count1 = 127;
  REPEAT_INSN(kLdrR0R0Count1, __ ldr(arm::R0, arm::Address(arm::R0)));
  Label label2;
  __ cbz(arm::R0, &label2);
  __ Bind(&label1);
  constexpr size_t kLdrR0R0Count2 = 64;
  REPEAT_INSN(kLdrR0R0Count2, __ ldr(arm::R0, arm::Address(arm::R0)));
  __ Bind(&label2);

  std::string expected =
      "cmp r0, #0\n"              // cbz r0, label1
      "beq.n 1f\n" +
      RepeatInsn(kLdrR0R0Count1, "ldr r0, [r0]\n") +
      "cbz r0, 2f\n"              // cbz r0, label2
      "1:\n" +
      RepeatInsn(kLdrR0R0Count2, "ldr r0, [r0]\n") +
      "2:\n";
  DriverStr(expected, "BranchRelocation1");
}

TEST_F(AssemblerThumb2Test, TwoCbzBeyondAtMaxB16Offset) {
  Label label1;
  __ cbz(arm::R0, &label1);
  constexpr size_t kLdrR0R0Count1 = 127;
  REPEAT_INSN(kLdrR0R0Count1, __ ldr(arm::R0, arm::Address(arm::R0)));
  Label label2;
  __ cbz(arm::R0, &label2);
  __ Bind(&label1);
  constexpr size_t kLdrR0R0Count2 = 65;
  REPEAT_INSN(kLdrR0R0Count2, __ ldr(arm::R0, arm::Address(arm::R0)));
  __ Bind(&label2);

  std::string expected =
      "cmp r0, #0\n"              // cbz r0, label1
      "beq.w 1f\n" +
      RepeatInsn(kLdrR0R0Count1, "ldr r0, [r0]\n") +
      "cmp r0, #0\n"              // cbz r0, label2
      "beq.n 2f\n"
      "1:\n" +
      RepeatInsn(kLdrR0R0Count2, "ldr r0, [r0]\n") +
      "2:\n";
  DriverStr(expected, "BranchRelocation1");
}

TEST_F(AssemblerThumb2Test, LoadLiteralMax1KiB) {
  arm::Literal* literal = __ NewLiteral<int32_t>(0x12345678);
  __ LoadLiteral(arm::R0, literal);
  constexpr size_t kLdrR0R0Count = 511;
  REPEAT_INSN(kLdrR0R0Count, __ ldr(arm::R0, arm::Address(arm::R0)));

  std::string expected =
      "1:\n"
      "ldr.n r0, [pc, #((2f - 1b - 2) & ~2)]\n" +
      RepeatInsn(kLdrR0R0Count, "ldr r0, [r0]\n") +
      ".align 2, 0\n"
      "2:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "BranchRelocation1");
}

TEST_F(AssemblerThumb2Test, LoadLiteralBeyondMax1KiB) {
  arm::Literal* literal = __ NewLiteral<int32_t>(0x12345678);
  __ LoadLiteral(arm::R0, literal);
  constexpr size_t kLdrR0R0Count = 512;
  REPEAT_INSN(kLdrR0R0Count, __ ldr(arm::R0, arm::Address(arm::R0)));

  std::string expected =
      "1:\n"
      "ldr.w r0, [pc, #((2f - 1b - 2) & ~2)]\n" +
      RepeatInsn(kLdrR0R0Count, "ldr r0, [r0]\n") +
      ".align 2, 0\n"
      "2:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "BranchRelocation1");
}

TEST_F(AssemblerThumb2Test, LoadLiteralMax4KiB) {
  arm::Literal* literal = __ NewLiteral<int32_t>(0x12345678);
  __ LoadLiteral(arm::R1, literal);
  constexpr size_t kLdrR0R0Count = 2046;
  REPEAT_INSN(kLdrR0R0Count, __ ldr(arm::R0, arm::Address(arm::R0)));

  std::string expected =
      "1:\n"
      "ldr.w r1, [pc, #((2f - 1b - 2) & ~2)]\n" +
      RepeatInsn(kLdrR0R0Count, "ldr r0, [r0]\n") +
      ".align 2, 0\n"
      "2:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "BranchRelocation1");
}

TEST_F(AssemblerThumb2Test, LoadLiteralBeyondMax4KiB) {
  arm::Literal* literal = __ NewLiteral<int32_t>(0x12345678);
  __ LoadLiteral(arm::R1, literal);
  constexpr size_t kLdrR0R0Count = 2047;
  REPEAT_INSN(kLdrR0R0Count, __ ldr(arm::R0, arm::Address(arm::R0)));

  std::string expected =
      "movw r1, #4096\n"  // "as" does not consider (2f - 1f - 4) a constant expression for movw.
      "1:\n"
      "add r1, pc\n"
      "ldr r1, [r1, #0]\n" +
      RepeatInsn(kLdrR0R0Count, "ldr r0, [r0]\n") +
      ".align 2, 0\n"
      "2:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "BranchRelocation1");
}

TEST_F(AssemblerThumb2Test, LoadLiteralMax64KiB) {
  arm::Literal* literal = __ NewLiteral<int32_t>(0x12345678);
  __ LoadLiteral(arm::R1, literal);
  constexpr size_t kLdrR0R0Count = (1u << 15) - 2u;
  REPEAT_INSN(kLdrR0R0Count, __ ldr(arm::R0, arm::Address(arm::R0)));

  std::string expected =
      "movw r1, #0xfffc\n"  // "as" does not consider (2f - 1f - 4) a constant expression for movw.
      "1:\n"
      "add r1, pc\n"
      "ldr r1, [r1, #0]\n" +
      RepeatInsn(kLdrR0R0Count, "ldr r0, [r0]\n") +
      ".align 2, 0\n"
      "2:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "BranchRelocation1");
}

TEST_F(AssemblerThumb2Test, LoadLiteralBeyondMax64KiB) {
  arm::Literal* literal = __ NewLiteral<int32_t>(0x12345678);
  __ LoadLiteral(arm::R1, literal);
  constexpr size_t kLdrR0R0Count = (1u << 15) - 1u;
  REPEAT_INSN(kLdrR0R0Count, __ ldr(arm::R0, arm::Address(arm::R0)));

  std::string expected =
      "mov.w r1, #((2f - 1f - 4) & ~0xfff)\n"
      "1:\n"
      "add r1, pc\n"
      "ldr r1, [r1, #((2f - 1b - 4) & 0xfff)]\n" +
      RepeatInsn(kLdrR0R0Count, "ldr r0, [r0]\n") +
      ".align 2, 0\n"
      "2:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "BranchRelocation1");
}

TEST_F(AssemblerThumb2Test, LoadLiteralMax1MiB) {
  arm::Literal* literal = __ NewLiteral<int32_t>(0x12345678);
  __ LoadLiteral(arm::R1, literal);
  constexpr size_t kLdrR0R0Count = (1u << 19) - 3u;
  REPEAT_INSN(kLdrR0R0Count, __ ldr(arm::R0, arm::Address(arm::R0)));

  std::string expected =
      "mov.w r1, #((2f - 1f - 4) & ~0xfff)\n"
      "1:\n"
      "add r1, pc\n"
      "ldr r1, [r1, #((2f - 1b - 4) & 0xfff)]\n" +
      RepeatInsn(kLdrR0R0Count, "ldr r0, [r0]\n") +
      ".align 2, 0\n"
      "2:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "BranchRelocation1");
}

TEST_F(AssemblerThumb2Test, LoadLiteralBeyondMax1MiB) {
  arm::Literal* literal = __ NewLiteral<int32_t>(0x12345678);
  __ LoadLiteral(arm::R1, literal);
  constexpr size_t kLdrR0R0Count = (1u << 19) - 2u;
  REPEAT_INSN(kLdrR0R0Count, __ ldr(arm::R0, arm::Address(arm::R0)));

  std::string expected =
      // "as" does not consider ((2f - 1f - 4) & 0xffff) a constant expression for movw.
      "movw r1, #(0x100000 & 0xffff)\n"
      // "as" does not consider ((2f - 1f - 4) >> 16) a constant expression for movt.
      "movt r1, #(0x100000 >> 16)\n"
      "1:\n"
      "add r1, pc\n"
      "ldr.w r1, [r1, #((2f - 1b - 4) & 0xfff)]\n" +
      RepeatInsn(kLdrR0R0Count, "ldr r0, [r0]\n") +
      ".align 2, 0\n"
      "2:\n"
      ".word 0x12345678\n";
  DriverStr(expected, "BranchRelocation1");
}

}  // namespace art
