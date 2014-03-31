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

#include "assembler_x86_64.h"

#include "utils/assembler_test.h"

namespace art {

TEST(AssemblerX86_64, CreateBuffer) {
  AssemblerBuffer buffer;
  AssemblerBuffer::EnsureCapacity ensured(&buffer);
  buffer.Emit<uint8_t>(0x42);
  ASSERT_EQ(static_cast<size_t>(1), buffer.Size());
  buffer.Emit<int32_t>(42);
  ASSERT_EQ(static_cast<size_t>(5), buffer.Size());
}

class AssemblerX86_64Test : public AssemblerTest<x86_64::X86_64Assembler> {
 protected:
  // Use the x86-64 prebuilt.
  const char* GetAssemblerCommand() OVERRIDE {
    return "prebuilts/gcc/linux-x86/x86/x86_64-linux-android-4.8/bin/x86_64-linux-android-as";
  }

  // Use the x86-64 prebuilt.
  const char* GetObjdumpCommand() OVERRIDE {
    return "prebuilts/gcc/linux-x86/x86/x86_64-linux-android-4.8/bin/"
           "x86_64-linux-android-objdump -h";
  }
};


const char* pushq_test(x86_64::X86_64Assembler* assembler) {
  assembler->pushq(x86_64::CpuRegister(x86_64::RAX));
  assembler->pushq(x86_64::CpuRegister(x86_64::RBX));
  assembler->pushq(x86_64::CpuRegister(x86_64::RCX));
  assembler->pushq(x86_64::CpuRegister(x86_64::RDX));
  assembler->pushq(x86_64::CpuRegister(x86_64::RBP));
  assembler->pushq(x86_64::CpuRegister(x86_64::RSP));
  assembler->pushq(x86_64::CpuRegister(x86_64::RSI));
  assembler->pushq(x86_64::CpuRegister(x86_64::RDI));
  assembler->pushq(x86_64::CpuRegister(x86_64::R9));
  assembler->pushq(x86_64::CpuRegister(x86_64::R10));
  assembler->pushq(x86_64::CpuRegister(x86_64::R11));
  assembler->pushq(x86_64::CpuRegister(x86_64::R12));
  assembler->pushq(x86_64::CpuRegister(x86_64::R13));
  assembler->pushq(x86_64::CpuRegister(x86_64::R14));
  assembler->pushq(x86_64::CpuRegister(x86_64::R15));

  return "pushq %rax\npushq %rbx\npushq %rcx\npushq %rdx\npushq %rbp\npushq %rsp\n"
         "pushq %rsi\npushq %rdi\npushq %r9\npushq %r10\npushq %r11\npushq %r12\n"
         "pushq %r13\npushq %r14\npushq %r15\n";
}

TEST_F(AssemblerX86_64Test, SimplePush) {
  Driver(pushq_test);
}


const char* simple_arithmetic_test(x86_64::X86_64Assembler* assembler) {
  assembler->addq(x86_64::CpuRegister(x86_64::RAX), x86_64::Immediate(0x1234));
  assembler->addl(x86_64::CpuRegister(x86_64::RAX), x86_64::Immediate(0x1234));

  return "addq $0x1234, %rax\naddl $0x1234, %eax\n";
}

TEST_F(AssemblerX86_64Test, SimpleArithmetic) {
  Driver(simple_arithmetic_test);
}

}  // namespace art
