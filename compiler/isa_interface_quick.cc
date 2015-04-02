/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "isa_interface_quick.h"

#include "jni/quick/arm/calling_convention_arm.h"
#include "jni/quick/arm64/calling_convention_arm64.h"
#include "jni/quick/mips/calling_convention_mips.h"
#include "jni/quick/mips64/calling_convention_mips64.h"
#include "jni/quick/x86/calling_convention_x86.h"
#include "jni/quick/x86_64/calling_convention_x86_64.h"
#include "linker/arm/relative_patcher_thumb2.h"
#include "linker/arm64/relative_patcher_arm64.h"
#include "linker/x86/relative_patcher_x86.h"
#include "linker/x86_64/relative_patcher_x86_64.h"
#include "utils/arm/assembler_arm.h"
#include "utils/arm/assembler_arm32.h"
#include "utils/arm64/assembler_arm64.h"
#include "utils/arm/assembler_thumb2.h"
#include "utils/mips/assembler_mips.h"
#include "utils/mips64/assembler_mips64.h"
#include "utils/x86/assembler_x86.h"
#include "utils/x86_64/assembler_x86_64.h"

namespace art {

Assembler* CreateAssembler(InstructionSet instruction_set) {
  switch (instruction_set) {
    case kArm:
      return CreateArm32Assembler();
    case kThumb2:
      return CreateThumb2Assembler();
    case kArm64:
      return CreateArm64Assembler();
    case kMips:
      return CreateMipsAssembler();
    case kMips64:
      return CreateMips64Assembler();
    case kX86:
      return CreateX86Assembler();
    case kX86_64:
      return CreateX86_64Assembler();
    default:
      LOG(FATAL) << "Unknown InstructionSet: " << instruction_set;
      return NULL;
  }
}

JniCallingConvention* CreateJniCallingConvention(bool is_static, bool is_synchronized,
                                                 const char* shorty,
                                                 InstructionSet instruction_set) {
  switch (instruction_set) {
    case kArm:
    case kThumb2:
      return CreateArmJniCallingConvention(is_static, is_synchronized, shorty);
    case kArm64:
      return CreateArm64JniCallingConvention(is_static, is_synchronized, shorty);
    case kMips:
      return CreateMipsJniCallingConvention(is_static, is_synchronized, shorty);
    case kMips64:
      return CreateMips64JniCallingConvention(is_static, is_synchronized, shorty);
    case kX86:
      return CreateX86JniCallingConvention(is_static, is_synchronized, shorty);
    case kX86_64:
      return CreateX86_64JniCallingConvention(is_static, is_synchronized, shorty);
    default:
      LOG(FATAL) << "Unknown InstructionSet: " << instruction_set;
      return NULL;
  }
}

ManagedRuntimeCallingConvention* CreateManagedRuntimeCallingConvention(
    bool is_static, bool is_synchronized, const char* shorty, InstructionSet instruction_set) {
  switch (instruction_set) {
    case kArm:
    case kThumb2:
      return CreateArmManagedRuntimeCallingConvention(is_static, is_synchronized, shorty);
    case kArm64:
      return CreateArm64ManagedRuntimeCallingConvention(is_static, is_synchronized, shorty);
    case kMips:
      return CreateMipsManagedRuntimeCallingConvention(is_static, is_synchronized, shorty);
    case kMips64:
      return CreateMips64ManagedRuntimeCallingConvention(is_static, is_synchronized, shorty);
    case kX86:
      return CreateX86ManagedRuntimeCallingConvention(is_static, is_synchronized, shorty);
    case kX86_64:
      return CreateX86_64ManagedRuntimeCallingConvention(is_static, is_synchronized, shorty);
    default:
      LOG(FATAL) << "Unknown InstructionSet: " << instruction_set;
      return NULL;
  }
}

const std::vector<uint8_t>* CreateTrampolineFor32(InstructionSet isa,
                                                  EntryPointCallingConvention abi,
                                                  ThreadOffset<4> offset) {
  switch (isa) {
    case kArm:
    case kThumb2:
      return arm::CreateTrampoline(abi, offset);
    case kMips:
      return mips::CreateTrampoline(abi, offset);
    case kX86:
      return x86::CreateTrampoline(offset);
    default:
      LOG(FATAL) << "Unexpected InstructionSet: " << isa;
      UNREACHABLE();
  }
}

const std::vector<uint8_t>* CreateTrampolineFor64(InstructionSet isa,
                                                  EntryPointCallingConvention abi,
                                                  ThreadOffset<8> offset) {
  switch (isa) {
    case kArm64:
      return arm64::CreateTrampoline(abi, offset);
    case kMips64:
      return mips64::CreateTrampoline(abi, offset);
    case kX86_64:
      return x86_64::CreateTrampoline(offset);
    default:
      LOG(FATAL) << "Unexpected InstructionSet: " << isa;
      UNREACHABLE();
  }
}

Mir2Lir* GetIsaCodeGenerator(CompilationUnit* cu) {
  Mir2Lir* mir_to_lir = nullptr;
  switch (cu->instruction_set) {
    case kThumb2:
      mir_to_lir = ArmCodeGenerator(cu, cu->mir_graph.get(), &cu->arena);
      break;
    case kArm64:
      mir_to_lir = Arm64CodeGenerator(cu, cu->mir_graph.get(), &cu->arena);
      break;
    case kMips:
      // Fall-through.
    case kMips64:
      mir_to_lir = MipsCodeGenerator(cu, cu->mir_graph.get(), &cu->arena);
      break;
    case kX86:
      // Fall-through.
    case kX86_64:
      mir_to_lir = X86CodeGenerator(cu, cu->mir_graph.get(), &cu->arena);
      break;
    default:
      LOG(FATAL) << "Unexpected instruction set: " << cu->instruction_set;
  }
  return mir_to_lir;
}

namespace linker {

RelativePatcher* CreateRelativePatcher(InstructionSet instruction_set,
                                       RelativePatcherTargetProvider* provider,
                                       const InstructionSetFeatures* features) {
  switch (instruction_set) {
    case kX86:
      return CreateX86RelativePatcher();
    case kX86_64:
      return CreateX86_64RelativePatcher();
    case kArm:
      // Fall through: we generate Thumb2 code for "arm".
    case kThumb2:
      return CreateThumb2RelativePatcher(provider);
    case kArm64:
      return
          CreateArm64RelativePatcher(provider, features);
    default:
      return new RelativePatcherNone;
  }
}

}  // namespace linker

}  // namespace art
