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

#include <functional>
#include <type_traits>

#include "arch/instruction_set.h"
#include "arch/arm/instruction_set_features_arm.h"
#include "base/macros.h"
#include "builder.h"
#include "code_generator_arm.h"
#include "code_generator_arm64.h"
#include "code_generator_x86.h"
#include "code_generator_x86_64.h"
#include "common_compiler_test.h"
#include "dex_file.h"
#include "dex_instruction.h"
#include "driver/compiler_options.h"
#include "nodes.h"
#include "optimizing_unit_test.h"
#include "prepare_for_register_allocation.h"
#include "register_allocator.h"
#include "ssa_liveness_analysis.h"
#include "utils.h"

#include "gtest/gtest.h"

namespace art {

// Provide our own codegen, that ensures the C calling conventions
// are preserved. Currently, ART and C do not match as R4 is caller-save
// in ART, and callee-save in C. Alternatively, we could use or write
// the stub that saves and restores all registers, but it is easier
// to just overwrite the code generator.
class TestCodeGeneratorARM : public arm::CodeGeneratorARM {
 public:
  TestCodeGeneratorARM(HGraph* graph,
                       const ArmInstructionSetFeatures& isa_features,
                       const CompilerOptions& compiler_options)
      : arm::CodeGeneratorARM(graph, isa_features, compiler_options) {
    AddAllocatedRegister(Location::RegisterLocation(6));
    AddAllocatedRegister(Location::RegisterLocation(7));
  }

  void SetupBlockedRegisters(bool is_baseline) const OVERRIDE {
    arm::CodeGeneratorARM::SetupBlockedRegisters(is_baseline);
    blocked_core_registers_[4] = true;
    blocked_core_registers_[6] = false;
    blocked_core_registers_[7] = false;
    // Makes pair R6-R7 available.
    blocked_register_pairs_[6 >> 1] = false;
  }
};

class InternalCodeAllocator : public CodeAllocator {
 public:
  InternalCodeAllocator() : size_(0) { }

  virtual uint8_t* Allocate(size_t size) {
    size_ = size;
    memory_.reset(new uint8_t[size]);
    return memory_.get();
  }

  size_t GetSize() const { return size_; }
  uint8_t* GetMemory() const { return memory_.get(); }

 private:
  size_t size_;
  std::unique_ptr<uint8_t[]> memory_;

  DISALLOW_COPY_AND_ASSIGN(InternalCodeAllocator);
};

// Run a piece of code. As this is a transition from native to managed, we will have to save and
// restore callee-save registers. As inline-assembly constraints are hard to satisfy apparently,
// do it by hand.

static uint32_t Run32(uint32_t (*fptr)()) {
#ifdef __i386__
  uint32_t result;
  __asm__ __volatile__(
      "push %%ebx\n\t"
      "push %%edi\n\t"
      "push %%esi\n\t"
      "push %%ebp\n\t"
      "push %%ecx\n\t"                // Caller-save...
      "push %%edx\n\t"                // Caller-save...
      "call *%%eax\n\t"               // Call the code
      "pop %%edx\n\t"
      "pop %%ecx\n\t"
      "pop %%ebp\n\t"
      "pop %%esi\n\t"
      "pop %%edi\n\t"
      "pop %%ebx"
      : "=a" (result) // Use the result from eax
      : "a"(fptr) // This places code into eax.
      : "memory");
  return result;
#elif defined(__x86_64__)
  uint32_t result;
  __asm__ __volatile__(
      "push %%rbx\n\t"
      "push %%rdi\n\t"
      "push %%rsi\n\t"
      "push %%rbp\n\t"
      "push %%rcx\n\t"
      "push %%rdx\n\t"
      "push %%r8\n\t"
      "push %%r9\n\t"
      "push %%r10\n\t"
      "push %%r11\n\t"
      "push %%r12\n\t"
      "push %%r13\n\t"
      "push %%r14\n\t"
      "push %%r15\n\t"                // 14 * 8B => no padding for 16B alignment necessary
      "call *%%rax\n\t"               // Call the code
      "pop %%r15\n\t"
      "pop %%r14\n\t"
      "pop %%r13\n\t"
      "pop %%r12\n\t"
      "pop %%r11\n\t"
      "pop %%r10\n\t"
      "pop %%r9\n\t"
      "pop %%r8\n\t"
      "pop %%rdx\n\t"
      "pop %%rcx\n\t"
      "pop %%rbp\n\t"
      "pop %%rsi\n\t"
      "pop %%rdi\n\t"
      "pop %%rbx"
      : "=a" (result) // Use the result from eax
      : "a"(fptr) // This places code into rax.
      : "memory");
  return result;
#elif defined(__arm__)
  uint32_t result;
  __asm__ __volatile__(
      "push {r0-r12, lr}\n\t"         // Save state, 14*4B = 56B, need 8B padding.
      "sub sp, sp, #8\n\t"
      "blx %[code]\n\t"
      "add sp, sp, #8\n\t"
      "mov lr, r0\n\t"
      "pop {r0-r12}\n\t"
      "mov %[result], lr\n\t"
      "pop {lr}\n\t"                  // Really, really hope lr is not out
      : [result] "=r" (result)
      : [code] "r" (fptr)
      : "memory");
  return result;
#elif defined(__aarch64__)
  uint32_t result;
  __asm__ __volatile__(
      "str x30, [sp, #-16]!\n\t"
      "stp x28, x29, [sp, #-16]!\n\t"
      "stp x26, x27, [sp, #-16]!\n\t"
      "stp x24, x25, [sp, #-16]!\n\t"
      "stp x22, x23, [sp, #-16]!\n\t"
      "stp x20, x21, [sp, #-16]!\n\t"
      "stp x18, x19, [sp, #-16]!\n\t"
      "stp x16, x17, [sp, #-16]!\n\t"
      "stp x14, x15, [sp, #-16]!\n\t"
      "stp x12, x13, [sp, #-16]!\n\t"
      "stp x10, x11, [sp, #-16]!\n\t"
      "stp x8, x9, [sp, #-16]!\n\t"
      "stp x6, x7, [sp, #-16]!\n\t"
      "stp x4, x5, [sp, #-16]!\n\t"
      "stp x2, x3, [sp, #-16]!\n\t"
      "stp x0, x1, [sp, #-16]!\n\t"
      "blr %[code]\n\t"
      "mov x30, x0\n\t"
      "ldp x0, x1, [sp], #16\n\t"
      "ldp x2, x3, [sp], #16\n\t"
      "ldp x4, x5, [sp], #16\n\t"
      "ldp x6, x7, [sp], #16\n\t"
      "ldp x8, x9, [sp], #16\n\t"
      "ldp x10, x11, [sp], #16\n\t"
      "ldp x12, x13, [sp], #16\n\t"
      "ldp x14, x15, [sp], #16\n\t"
      "ldp x16, x17, [sp], #16\n\t"
      "ldp x18, x19, [sp], #16\n\t"
      "ldp x20, x21, [sp], #16\n\t"
      "ldp x22, x23, [sp], #16\n\t"
      "ldp x24, x25, [sp], #16\n\t"
      "ldp x26, x27, [sp], #16\n\t"
      "ldp x28, x29, [sp], #16\n\t"
      "mov %[result], x30\n\t"
      "ldr x30, [sp], #16\n\t"        // Really, really hope lr is not out
      : [result] "=r" (result)
      : [code] "r" (fptr)
      : "memory");
  return result;
#else
  return fptr();
#endif
}

static uint64_t Run64(uint64_t (*fptr)()) {
#ifdef __i386__
  uint64_t result;
  __asm__ __volatile__(
      "push %%ebx\n\t"
      "push %%edi\n\t"
      "push %%esi\n\t"
      "push %%ebp\n\t"
      "push %%ecx\n\t"                // Caller-save...
      "call *%%eax\n\t"               // Call the code
      "pop %%ecx\n\t"
      "pop %%ebp\n\t"
      "pop %%esi\n\t"
      "pop %%edi\n\t"
      "pop %%ebx"
      : "=A" (result) // Use the result from eax:edx
      : "a"(fptr) // This places code into eax.
      : "memory");
  return result;
#elif defined(__x86_64__)
  uint64_t result;
  __asm__ __volatile__(
      "push %%rbx\n\t"
      "push %%rdi\n\t"
      "push %%rsi\n\t"
      "push %%rbp\n\t"
      "push %%rcx\n\t"
      "push %%rdx\n\t"
      "push %%r8\n\t"
      "push %%r9\n\t"
      "push %%r10\n\t"
      "push %%r11\n\t"
      "push %%r12\n\t"
      "push %%r13\n\t"
      "push %%r14\n\t"
      "push %%r15\n\t"                // 14 * 8B => no padding for 16B alignment necessary
      "call *%%rax\n\t"               // Call the code
      "pop %%r15\n\t"
      "pop %%r14\n\t"
      "pop %%r13\n\t"
      "pop %%r12\n\t"
      "pop %%r11\n\t"
      "pop %%r10\n\t"
      "pop %%r9\n\t"
      "pop %%r8\n\t"
      "pop %%rdx\n\t"
      "pop %%rcx\n\t"
      "pop %%rbp\n\t"
      "pop %%rsi\n\t"
      "pop %%rdi\n\t"
      "pop %%rbx"
      : "=a" (result) // Use the result from eax
      : "a"(fptr) // This places code into rax.
      : "memory");
  return result;
#elif defined(__arm__)
  uint32_t result1;
  uint32_t result2;
  __asm__ __volatile__(
      "push {r0-r12, lr}\n\t"         // Save state, 14*4B = 56B, need 8B padding.
      "sub sp, sp, #8\n\t"
      "blx %[code]\n\t"
      "add sp, sp, #8\n\t"
      "mov r12, r0\n\t"
      "mov lr, r1\n\t"
      "pop {r0-r11}\n\t"
      "mov %[result1], r12\n\t"
      "mov %[result2], lr\n\t"
      "pop {r12, lr}\n\t"             // Really, really hope r12 & lr are not out
      : [result1] "=r" (result1), [result2] "=r" (result2)
      : [code] "r" (fptr)
      : "memory");
  return static_cast<uint64_t>(result1) | (static_cast<uint64_t>(result2) << 32);
#elif defined(__aarch64__)
  uint64_t result;
  __asm__ __volatile__(
      "str x30, [sp, #-16]!\n\t"
      "stp x28, x29, [sp, #-16]!\n\t"
      "stp x26, x27, [sp, #-16]!\n\t"
      "stp x24, x25, [sp, #-16]!\n\t"
      "stp x22, x23, [sp, #-16]!\n\t"
      "stp x20, x21, [sp, #-16]!\n\t"
      "stp x18, x19, [sp, #-16]!\n\t"
      "stp x16, x17, [sp, #-16]!\n\t"
      "stp x14, x15, [sp, #-16]!\n\t"
      "stp x12, x13, [sp, #-16]!\n\t"
      "stp x10, x11, [sp, #-16]!\n\t"
      "stp x8, x9, [sp, #-16]!\n\t"
      "stp x6, x7, [sp, #-16]!\n\t"
      "stp x4, x5, [sp, #-16]!\n\t"
      "stp x2, x3, [sp, #-16]!\n\t"
      "stp x0, x1, [sp, #-16]!\n\t"
      "blr %[code]\n\t"
      "mov x30, x0\n\t"
      "ldp x0, x1, [sp], #16\n\t"
      "ldp x2, x3, [sp], #16\n\t"
      "ldp x4, x5, [sp], #16\n\t"
      "ldp x6, x7, [sp], #16\n\t"
      "ldp x8, x9, [sp], #16\n\t"
      "ldp x10, x11, [sp], #16\n\t"
      "ldp x12, x13, [sp], #16\n\t"
      "ldp x14, x15, [sp], #16\n\t"
      "ldp x16, x17, [sp], #16\n\t"
      "ldp x18, x19, [sp], #16\n\t"
      "ldp x20, x21, [sp], #16\n\t"
      "ldp x22, x23, [sp], #16\n\t"
      "ldp x24, x25, [sp], #16\n\t"
      "ldp x26, x27, [sp], #16\n\t"
      "ldp x28, x29, [sp], #16\n\t"
      "mov %[result], x30\n\t"
      "ldr x30, [sp], #16\n\t"        // Really, really hope lr is not out
      : [result] "=r" (result)
      : [code] "r" (fptr)
      : "memory");
  return result;
#else
  return fptr();
#endif
}

template <typename Expected>
static void Run(const InternalCodeAllocator& allocator,
                const CodeGenerator& codegen,
                bool has_result,
                Expected expected) {
  CommonCompilerTest::MakeExecutable(allocator.GetMemory(), allocator.GetSize());
  uint8_t* f = allocator.GetMemory();
  DCHECK(kRuntimeISA == codegen.GetInstructionSet() ||
         (kRuntimeISA == kArm && codegen.GetInstructionSet() == kThumb2));
  if (codegen.GetInstructionSet() == kThumb2) {
    // For thumb we need the bottom bit set.
    f = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(f) | 0x1);
  }

  static_assert(IsPowerOfTwo(sizeof(Expected)) && sizeof(Expected) <= 8, "Unsupported type");
  static_assert(!std::is_floating_point<Expected>::value, "Floating point unsupported");
  Expected result;
  if (sizeof(Expected) == 8) {
    typedef uint64_t (*fptr)();
    result = static_cast<Expected>(Run64(reinterpret_cast<fptr>(f)));
  } else {
    typedef uint32_t (*fptr)();
    result = static_cast<Expected>(Run32(reinterpret_cast<fptr>(f)));
  }
  if (has_result) {
    ASSERT_EQ(result, expected);
  }
}

template <typename Expected>
static void RunCodeBaseline(HGraph* graph, bool has_result, Expected expected) {
  InternalCodeAllocator allocator;

  CompilerOptions compiler_options;
  x86::CodeGeneratorX86 codegenX86(graph, compiler_options);
  // We avoid doing a stack overflow check that requires the runtime being setup,
  // by making sure the compiler knows the methods we are running are leaf methods.
  codegenX86.CompileBaseline(&allocator, true);
  if (kRuntimeISA == kX86) {
    Run(allocator, codegenX86, has_result, expected);
  }

  std::unique_ptr<const ArmInstructionSetFeatures> features(
      ArmInstructionSetFeatures::FromCppDefines());
  TestCodeGeneratorARM codegenARM(graph, *features.get(), compiler_options);
  codegenARM.CompileBaseline(&allocator, true);
  if (kRuntimeISA == kArm || kRuntimeISA == kThumb2) {
    Run(allocator, codegenARM, has_result, expected);
  }

  x86_64::CodeGeneratorX86_64 codegenX86_64(graph, compiler_options);
  codegenX86_64.CompileBaseline(&allocator, true);
  if (kRuntimeISA == kX86_64) {
    Run(allocator, codegenX86_64, has_result, expected);
  }

  arm64::CodeGeneratorARM64 codegenARM64(graph, compiler_options);
  codegenARM64.CompileBaseline(&allocator, true);
  if (kRuntimeISA == kArm64) {
    Run(allocator, codegenARM64, has_result, expected);
  }
}

template <typename Expected>
static void RunCodeOptimized(CodeGenerator* codegen,
                             HGraph* graph,
                             std::function<void(HGraph*)> hook_before_codegen,
                             bool has_result,
                             Expected expected) {
  SsaLivenessAnalysis liveness(*graph, codegen);
  liveness.Analyze();

  RegisterAllocator register_allocator(graph->GetArena(), codegen, liveness);
  register_allocator.AllocateRegisters();
  hook_before_codegen(graph);

  InternalCodeAllocator allocator;
  codegen->CompileOptimized(&allocator);
  Run(allocator, *codegen, has_result, expected);
}

template <typename Expected>
static void RunCodeOptimized(HGraph* graph,
                             std::function<void(HGraph*)> hook_before_codegen,
                             bool has_result,
                             Expected expected) {
  CompilerOptions compiler_options;
  if (kRuntimeISA == kArm || kRuntimeISA == kThumb2) {
    TestCodeGeneratorARM codegenARM(graph,
                                    *ArmInstructionSetFeatures::FromCppDefines(),
                                    compiler_options);
    RunCodeOptimized(&codegenARM, graph, hook_before_codegen, has_result, expected);
  } else if (kRuntimeISA == kArm64) {
    arm64::CodeGeneratorARM64 codegenARM64(graph, compiler_options);
    RunCodeOptimized(&codegenARM64, graph, hook_before_codegen, has_result, expected);
  } else if (kRuntimeISA == kX86) {
    x86::CodeGeneratorX86 codegenX86(graph, compiler_options);
    RunCodeOptimized(&codegenX86, graph, hook_before_codegen, has_result, expected);
  } else if (kRuntimeISA == kX86_64) {
    x86_64::CodeGeneratorX86_64 codegenX86_64(graph, compiler_options);
    RunCodeOptimized(&codegenX86_64, graph, hook_before_codegen, has_result, expected);
  }
}

static void TestCode(const uint16_t* data, bool has_result = false, int32_t expected = 0) {
  ArenaPool pool;
  ArenaAllocator arena(&pool);
  HGraphBuilder builder(&arena);
  const DexFile::CodeItem* item = reinterpret_cast<const DexFile::CodeItem*>(data);
  HGraph* graph = builder.BuildGraph(*item);
  ASSERT_NE(graph, nullptr);
  // Remove suspend checks, they cannot be executed in this context.
  RemoveSuspendChecks(graph);
  RunCodeBaseline(graph, has_result, expected);
}

static void TestCodeLong(const uint16_t* data, bool has_result, int64_t expected) {
  ArenaPool pool;
  ArenaAllocator arena(&pool);
  HGraphBuilder builder(&arena, Primitive::kPrimLong);
  const DexFile::CodeItem* item = reinterpret_cast<const DexFile::CodeItem*>(data);
  HGraph* graph = builder.BuildGraph(*item);
  ASSERT_NE(graph, nullptr);
  // Remove suspend checks, they cannot be executed in this context.
  RemoveSuspendChecks(graph);
  RunCodeBaseline(graph, has_result, expected);
}

TEST(CodegenTest, ReturnVoid) {
  const uint16_t data[] = ZERO_REGISTER_CODE_ITEM(Instruction::RETURN_VOID);
  TestCode(data);
}

TEST(CodegenTest, CFG1) {
  const uint16_t data[] = ZERO_REGISTER_CODE_ITEM(
    Instruction::GOTO | 0x100,
    Instruction::RETURN_VOID);

  TestCode(data);
}

TEST(CodegenTest, CFG2) {
  const uint16_t data[] = ZERO_REGISTER_CODE_ITEM(
    Instruction::GOTO | 0x100,
    Instruction::GOTO | 0x100,
    Instruction::RETURN_VOID);

  TestCode(data);
}

TEST(CodegenTest, CFG3) {
  const uint16_t data1[] = ZERO_REGISTER_CODE_ITEM(
    Instruction::GOTO | 0x200,
    Instruction::RETURN_VOID,
    Instruction::GOTO | 0xFF00);

  TestCode(data1);

  const uint16_t data2[] = ZERO_REGISTER_CODE_ITEM(
    Instruction::GOTO_16, 3,
    Instruction::RETURN_VOID,
    Instruction::GOTO_16, 0xFFFF);

  TestCode(data2);

  const uint16_t data3[] = ZERO_REGISTER_CODE_ITEM(
    Instruction::GOTO_32, 4, 0,
    Instruction::RETURN_VOID,
    Instruction::GOTO_32, 0xFFFF, 0xFFFF);

  TestCode(data3);
}

TEST(CodegenTest, CFG4) {
  const uint16_t data[] = ZERO_REGISTER_CODE_ITEM(
    Instruction::RETURN_VOID,
    Instruction::GOTO | 0x100,
    Instruction::GOTO | 0xFE00);

  TestCode(data);
}

TEST(CodegenTest, CFG5) {
  const uint16_t data[] = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::IF_EQ, 3,
    Instruction::GOTO | 0x100,
    Instruction::RETURN_VOID);

  TestCode(data);
}

TEST(CodegenTest, IntConstant) {
  const uint16_t data[] = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::RETURN_VOID);

  TestCode(data);
}

TEST(CodegenTest, Return1) {
  const uint16_t data[] = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::RETURN | 0);

  TestCode(data, true, 0);
}

TEST(CodegenTest, Return2) {
  const uint16_t data[] = TWO_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::CONST_4 | 0 | 1 << 8,
    Instruction::RETURN | 1 << 8);

  TestCode(data, true, 0);
}

TEST(CodegenTest, Return3) {
  const uint16_t data[] = TWO_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::CONST_4 | 1 << 8 | 1 << 12,
    Instruction::RETURN | 1 << 8);

  TestCode(data, true, 1);
}

TEST(CodegenTest, ReturnIf1) {
  const uint16_t data[] = TWO_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::CONST_4 | 1 << 8 | 1 << 12,
    Instruction::IF_EQ, 3,
    Instruction::RETURN | 0 << 8,
    Instruction::RETURN | 1 << 8);

  TestCode(data, true, 1);
}

TEST(CodegenTest, ReturnIf2) {
  const uint16_t data[] = TWO_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 0 | 0,
    Instruction::CONST_4 | 1 << 8 | 1 << 12,
    Instruction::IF_EQ | 0 << 4 | 1 << 8, 3,
    Instruction::RETURN | 0 << 8,
    Instruction::RETURN | 1 << 8);

  TestCode(data, true, 0);
}

// Exercise bit-wise (one's complement) not-int instruction.
#define NOT_INT_TEST(TEST_NAME, INPUT, EXPECTED_OUTPUT) \
TEST(CodegenTest, TEST_NAME) {                          \
  const int32_t input = INPUT;                          \
  const uint16_t input_lo = Low16Bits(input);           \
  const uint16_t input_hi = High16Bits(input);          \
  const uint16_t data[] = TWO_REGISTERS_CODE_ITEM(      \
      Instruction::CONST | 0 << 8, input_lo, input_hi,  \
      Instruction::NOT_INT | 1 << 8 | 0 << 12 ,         \
      Instruction::RETURN | 1 << 8);                    \
                                                        \
  TestCode(data, true, EXPECTED_OUTPUT);                \
}

NOT_INT_TEST(ReturnNotIntMinus2, -2, 1)
NOT_INT_TEST(ReturnNotIntMinus1, -1, 0)
NOT_INT_TEST(ReturnNotInt0, 0, -1)
NOT_INT_TEST(ReturnNotInt1, 1, -2)
NOT_INT_TEST(ReturnNotIntINT32_MIN, -2147483648, 2147483647)  // (2^31) - 1
NOT_INT_TEST(ReturnNotIntINT32_MINPlus1, -2147483647, 2147483646)  // (2^31) - 2
NOT_INT_TEST(ReturnNotIntINT32_MAXMinus1, 2147483646, -2147483647)  // -(2^31) - 1
NOT_INT_TEST(ReturnNotIntINT32_MAX, 2147483647, -2147483648)  // -(2^31)

#undef NOT_INT_TEST

// Exercise bit-wise (one's complement) not-long instruction.
#define NOT_LONG_TEST(TEST_NAME, INPUT, EXPECTED_OUTPUT)                 \
TEST(CodegenTest, TEST_NAME) {                                           \
  const int64_t input = INPUT;                                           \
  const uint16_t word0 = Low16Bits(Low32Bits(input));   /* LSW. */       \
  const uint16_t word1 = High16Bits(Low32Bits(input));                   \
  const uint16_t word2 = Low16Bits(High32Bits(input));                   \
  const uint16_t word3 = High16Bits(High32Bits(input)); /* MSW. */       \
  const uint16_t data[] = FOUR_REGISTERS_CODE_ITEM(                      \
      Instruction::CONST_WIDE | 0 << 8, word0, word1, word2, word3,      \
      Instruction::NOT_LONG | 2 << 8 | 0 << 12,                          \
      Instruction::RETURN_WIDE | 2 << 8);                                \
                                                                         \
  TestCodeLong(data, true, EXPECTED_OUTPUT);                             \
}

NOT_LONG_TEST(ReturnNotLongMinus2, INT64_C(-2), INT64_C(1))
NOT_LONG_TEST(ReturnNotLongMinus1, INT64_C(-1), INT64_C(0))
NOT_LONG_TEST(ReturnNotLong0, INT64_C(0), INT64_C(-1))
NOT_LONG_TEST(ReturnNotLong1, INT64_C(1), INT64_C(-2))

NOT_LONG_TEST(ReturnNotLongINT32_MIN,
              INT64_C(-2147483648),
              INT64_C(2147483647))  // (2^31) - 1
NOT_LONG_TEST(ReturnNotLongINT32_MINPlus1,
              INT64_C(-2147483647),
              INT64_C(2147483646))  // (2^31) - 2
NOT_LONG_TEST(ReturnNotLongINT32_MAXMinus1,
              INT64_C(2147483646),
              INT64_C(-2147483647))  // -(2^31) - 1
NOT_LONG_TEST(ReturnNotLongINT32_MAX,
              INT64_C(2147483647),
              INT64_C(-2147483648))  // -(2^31)

// Note that the C++ compiler won't accept
// INT64_C(-9223372036854775808) (that is, INT64_MIN) as a valid
// int64_t literal, so we use INT64_C(-9223372036854775807)-1 instead.
NOT_LONG_TEST(ReturnNotINT64_MIN,
              INT64_C(-9223372036854775807)-1,
              INT64_C(9223372036854775807));  // (2^63) - 1
NOT_LONG_TEST(ReturnNotINT64_MINPlus1,
              INT64_C(-9223372036854775807),
              INT64_C(9223372036854775806));  // (2^63) - 2
NOT_LONG_TEST(ReturnNotLongINT64_MAXMinus1,
              INT64_C(9223372036854775806),
              INT64_C(-9223372036854775807));  // -(2^63) - 1
NOT_LONG_TEST(ReturnNotLongINT64_MAX,
              INT64_C(9223372036854775807),
              INT64_C(-9223372036854775807)-1);  // -(2^63)

#undef NOT_LONG_TEST

TEST(CodegenTest, IntToLongOfLongToInt) {
  const int64_t input = INT64_C(4294967296);             // 2^32
  const uint16_t word0 = Low16Bits(Low32Bits(input));    // LSW.
  const uint16_t word1 = High16Bits(Low32Bits(input));
  const uint16_t word2 = Low16Bits(High32Bits(input));
  const uint16_t word3 = High16Bits(High32Bits(input));  // MSW.
  const uint16_t data[] = FIVE_REGISTERS_CODE_ITEM(
      Instruction::CONST_WIDE | 0 << 8, word0, word1, word2, word3,
      Instruction::CONST_WIDE | 2 << 8, 1, 0, 0, 0,
      Instruction::ADD_LONG | 0, 0 << 8 | 2,             // v0 <- 2^32 + 1
      Instruction::LONG_TO_INT | 4 << 8 | 0 << 12,
      Instruction::INT_TO_LONG | 2 << 8 | 4 << 12,
      Instruction::RETURN_WIDE | 2 << 8);

  TestCodeLong(data, true, 1);
}

TEST(CodegenTest, ReturnAdd1) {
  const uint16_t data[] = TWO_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 3 << 12 | 0,
    Instruction::CONST_4 | 4 << 12 | 1 << 8,
    Instruction::ADD_INT, 1 << 8 | 0,
    Instruction::RETURN);

  TestCode(data, true, 7);
}

TEST(CodegenTest, ReturnAdd2) {
  const uint16_t data[] = TWO_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 3 << 12 | 0,
    Instruction::CONST_4 | 4 << 12 | 1 << 8,
    Instruction::ADD_INT_2ADDR | 1 << 12,
    Instruction::RETURN);

  TestCode(data, true, 7);
}

TEST(CodegenTest, ReturnAdd3) {
  const uint16_t data[] = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 4 << 12 | 0 << 8,
    Instruction::ADD_INT_LIT8, 3 << 8 | 0,
    Instruction::RETURN);

  TestCode(data, true, 7);
}

TEST(CodegenTest, ReturnAdd4) {
  const uint16_t data[] = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 4 << 12 | 0 << 8,
    Instruction::ADD_INT_LIT16, 3,
    Instruction::RETURN);

  TestCode(data, true, 7);
}

TEST(CodegenTest, NonMaterializedCondition) {
  ArenaPool pool;
  ArenaAllocator allocator(&pool);

  HGraph* graph = new (&allocator) HGraph(&allocator);
  HBasicBlock* entry = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(entry);
  graph->SetEntryBlock(entry);
  entry->AddInstruction(new (&allocator) HGoto());

  HBasicBlock* first_block = new (&allocator) HBasicBlock(graph);
  graph->AddBlock(first_block);
  entry->AddSuccessor(first_block);
  HIntConstant* constant0 = new (&allocator) HIntConstant(0);
  entry->AddInstruction(constant0);
  HIntConstant* constant1 = new (&allocator) HIntConstant(1);
  entry->AddInstruction(constant1);
  HEqual* equal = new (&allocator) HEqual(constant0, constant0);
  first_block->AddInstruction(equal);
  first_block->AddInstruction(new (&allocator) HIf(equal));

  HBasicBlock* then = new (&allocator) HBasicBlock(graph);
  HBasicBlock* else_ = new (&allocator) HBasicBlock(graph);
  HBasicBlock* exit = new (&allocator) HBasicBlock(graph);

  graph->AddBlock(then);
  graph->AddBlock(else_);
  graph->AddBlock(exit);
  first_block->AddSuccessor(then);
  first_block->AddSuccessor(else_);
  then->AddSuccessor(exit);
  else_->AddSuccessor(exit);

  exit->AddInstruction(new (&allocator) HExit());
  then->AddInstruction(new (&allocator) HReturn(constant0));
  else_->AddInstruction(new (&allocator) HReturn(constant1));

  ASSERT_TRUE(equal->NeedsMaterialization());
  graph->BuildDominatorTree();
  PrepareForRegisterAllocation(graph).Run();
  ASSERT_FALSE(equal->NeedsMaterialization());

  auto hook_before_codegen = [](HGraph* graph_in) {
    HBasicBlock* block = graph_in->GetEntryBlock()->GetSuccessors().Get(0);
    HParallelMove* move = new (graph_in->GetArena()) HParallelMove(graph_in->GetArena());
    block->InsertInstructionBefore(move, block->GetLastInstruction());
  };

  RunCodeOptimized(graph, hook_before_codegen, true, 0);
}

#define MUL_TEST(TYPE, TEST_NAME)                     \
  TEST(CodegenTest, Return ## TEST_NAME) {            \
    const uint16_t data[] = TWO_REGISTERS_CODE_ITEM(  \
      Instruction::CONST_4 | 3 << 12 | 0,             \
      Instruction::CONST_4 | 4 << 12 | 1 << 8,        \
      Instruction::MUL_ ## TYPE, 1 << 8 | 0,          \
      Instruction::RETURN);                           \
                                                      \
    TestCode(data, true, 12);                         \
  }                                                   \
                                                      \
  TEST(CodegenTest, Return ## TEST_NAME ## 2addr) {   \
    const uint16_t data[] = TWO_REGISTERS_CODE_ITEM(  \
      Instruction::CONST_4 | 3 << 12 | 0,             \
      Instruction::CONST_4 | 4 << 12 | 1 << 8,        \
      Instruction::MUL_ ## TYPE ## _2ADDR | 1 << 12,  \
      Instruction::RETURN);                           \
                                                      \
    TestCode(data, true, 12);                         \
  }

MUL_TEST(INT, MulInt);
MUL_TEST(LONG, MulLong);

TEST(CodegenTest, ReturnMulIntLit8) {
  const uint16_t data[] = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 4 << 12 | 0 << 8,
    Instruction::MUL_INT_LIT8, 3 << 8 | 0,
    Instruction::RETURN);

  TestCode(data, true, 12);
}

TEST(CodegenTest, ReturnMulIntLit16) {
  const uint16_t data[] = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 4 << 12 | 0 << 8,
    Instruction::MUL_INT_LIT16, 3,
    Instruction::RETURN);

  TestCode(data, true, 12);
}

TEST(CodegenTest, MaterializedCondition1) {
  // Check that condition are materialized correctly. A materialized condition
  // should yield `1` if it evaluated to true, and `0` otherwise.
  // We force the materialization of comparisons for different combinations of
  // inputs and check the results.

  int lhs[] = {1, 2, -1, 2, 0xabc};
  int rhs[] = {2, 1, 2, -1, 0xabc};

  for (size_t i = 0; i < arraysize(lhs); i++) {
    ArenaPool pool;
    ArenaAllocator allocator(&pool);
    HGraph* graph = new (&allocator) HGraph(&allocator);

    HBasicBlock* entry_block = new (&allocator) HBasicBlock(graph);
    graph->AddBlock(entry_block);
    graph->SetEntryBlock(entry_block);
    entry_block->AddInstruction(new (&allocator) HGoto());
    HBasicBlock* code_block = new (&allocator) HBasicBlock(graph);
    graph->AddBlock(code_block);
    HBasicBlock* exit_block = new (&allocator) HBasicBlock(graph);
    graph->AddBlock(exit_block);
    exit_block->AddInstruction(new (&allocator) HExit());

    entry_block->AddSuccessor(code_block);
    code_block->AddSuccessor(exit_block);
    graph->SetExitBlock(exit_block);

    HIntConstant cst_lhs(lhs[i]);
    code_block->AddInstruction(&cst_lhs);
    HIntConstant cst_rhs(rhs[i]);
    code_block->AddInstruction(&cst_rhs);
    HLessThan cmp_lt(&cst_lhs, &cst_rhs);
    code_block->AddInstruction(&cmp_lt);
    HReturn ret(&cmp_lt);
    code_block->AddInstruction(&ret);

    auto hook_before_codegen = [](HGraph* graph_in) {
      HBasicBlock* block = graph_in->GetEntryBlock()->GetSuccessors().Get(0);
      HParallelMove* move = new (graph_in->GetArena()) HParallelMove(graph_in->GetArena());
      block->InsertInstructionBefore(move, block->GetLastInstruction());
    };

    RunCodeOptimized(graph, hook_before_codegen, true, lhs[i] < rhs[i]);
  }
}

TEST(CodegenTest, MaterializedCondition2) {
  // Check that HIf correctly interprets a materialized condition.
  // We force the materialization of comparisons for different combinations of
  // inputs. An HIf takes the materialized combination as input and returns a
  // value that we verify.

  int lhs[] = {1, 2, -1, 2, 0xabc};
  int rhs[] = {2, 1, 2, -1, 0xabc};


  for (size_t i = 0; i < arraysize(lhs); i++) {
    ArenaPool pool;
    ArenaAllocator allocator(&pool);
    HGraph* graph = new (&allocator) HGraph(&allocator);

    HBasicBlock* entry_block = new (&allocator) HBasicBlock(graph);
    graph->AddBlock(entry_block);
    graph->SetEntryBlock(entry_block);
    entry_block->AddInstruction(new (&allocator) HGoto());

    HBasicBlock* if_block = new (&allocator) HBasicBlock(graph);
    graph->AddBlock(if_block);
    HBasicBlock* if_true_block = new (&allocator) HBasicBlock(graph);
    graph->AddBlock(if_true_block);
    HBasicBlock* if_false_block = new (&allocator) HBasicBlock(graph);
    graph->AddBlock(if_false_block);
    HBasicBlock* exit_block = new (&allocator) HBasicBlock(graph);
    graph->AddBlock(exit_block);
    exit_block->AddInstruction(new (&allocator) HExit());

    graph->SetEntryBlock(entry_block);
    entry_block->AddSuccessor(if_block);
    if_block->AddSuccessor(if_true_block);
    if_block->AddSuccessor(if_false_block);
    if_true_block->AddSuccessor(exit_block);
    if_false_block->AddSuccessor(exit_block);
    graph->SetExitBlock(exit_block);

    HIntConstant cst_lhs(lhs[i]);
    if_block->AddInstruction(&cst_lhs);
    HIntConstant cst_rhs(rhs[i]);
    if_block->AddInstruction(&cst_rhs);
    HLessThan cmp_lt(&cst_lhs, &cst_rhs);
    if_block->AddInstruction(&cmp_lt);
    // We insert a temporary to separate the HIf from the HLessThan and force
    // the materialization of the condition.
    HTemporary force_materialization(0);
    if_block->AddInstruction(&force_materialization);
    HIf if_lt(&cmp_lt);
    if_block->AddInstruction(&if_lt);

    HIntConstant cst_lt(1);
    if_true_block->AddInstruction(&cst_lt);
    HReturn ret_lt(&cst_lt);
    if_true_block->AddInstruction(&ret_lt);
    HIntConstant cst_ge(0);
    if_false_block->AddInstruction(&cst_ge);
    HReturn ret_ge(&cst_ge);
    if_false_block->AddInstruction(&ret_ge);

    auto hook_before_codegen = [](HGraph* graph_in) {
      HBasicBlock* block = graph_in->GetEntryBlock()->GetSuccessors().Get(0);
      HParallelMove* move = new (graph_in->GetArena()) HParallelMove(graph_in->GetArena());
      block->InsertInstructionBefore(move, block->GetLastInstruction());
    };

    RunCodeOptimized(graph, hook_before_codegen, true, lhs[i] < rhs[i]);
  }
}

TEST(CodegenTest, ReturnDivIntLit8) {
  const uint16_t data[] = ONE_REGISTER_CODE_ITEM(
    Instruction::CONST_4 | 4 << 12 | 0 << 8,
    Instruction::DIV_INT_LIT8, 3 << 8 | 0,
    Instruction::RETURN);

  TestCode(data, true, 1);
}

TEST(CodegenTest, ReturnDivInt2Addr) {
  const uint16_t data[] = TWO_REGISTERS_CODE_ITEM(
    Instruction::CONST_4 | 4 << 12 | 0,
    Instruction::CONST_4 | 2 << 12 | 1 << 8,
    Instruction::DIV_INT_2ADDR | 1 << 12,
    Instruction::RETURN);

  TestCode(data, true, 2);
}

}  // namespace art
