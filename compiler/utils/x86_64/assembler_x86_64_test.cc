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

  return "pushq %rax\npushq %rbx\npushq %rcx\n";
}

TEST_F(AssemblerX86_64Test, SimplePush) {
  Driver(pushq_test);
}


const char* simple_arithmetic_test(x86_64::X86_64Assembler* assembler) {
  assembler->addq(x86_64::CpuRegister(x86_64::RAX), x86_64::Immediate(0x1234));

  return "addq %rax, $0x1234\n";
}

TEST_F(AssemblerX86_64Test, SimpleArithmetic) {
  Driver(simple_arithmetic_test);
}

}  // namespace art
