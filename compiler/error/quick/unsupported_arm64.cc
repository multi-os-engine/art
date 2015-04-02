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

#include "dex/quick/mir_to_lir.h"
#include "driver/compiler_driver.h"
#include "jni/quick/arm64/calling_convention_arm64.h"
#include "jni/quick/calling_convention.h"
#include "linker/relative_patcher.h"
#include "offsets.h"
#include "utils/assembler.h"

namespace art {

Assembler* CreateArm64Assembler() {
  LOG(FATAL) << "Undefined ISA - arm64";
  return nullptr;
}

JniCallingConvention* CreateArm64JniCallingConvention(bool is_static, bool is_synchronized,
                                                      const char* shorty) {
  UNUSED(is_static);
  UNUSED(is_synchronized);
  UNUSED(shorty);
  LOG(FATAL) << "Undefined ISA - arm64";
  return nullptr;
}

ManagedRuntimeCallingConvention* CreateArm64ManagedRuntimeCallingConvention(
    bool is_static, bool is_synchronized, const char* shorty) {
  UNUSED(is_static);
  UNUSED(is_synchronized);
  UNUSED(shorty);
  LOG(FATAL) << "Undefined ISA - arm64";
  return nullptr;
}

Mir2Lir* Arm64CodeGenerator(CompilationUnit* const cu, MIRGraph* const mir_graph,
                            ArenaAllocator* const arena) {
  UNUSED(cu);
  UNUSED(mir_graph);
  UNUSED(arena);
  LOG(FATAL) << "Undefined ISA - arm64";
  return nullptr;
}

namespace arm64 {

const std::vector<uint8_t>* CreateTrampoline(EntryPointCallingConvention abi,
                                             ThreadOffset<8> offset) {
  UNUSED(abi);
  UNUSED(offset);
  LOG(FATAL) << "Undefined ISA - arm64";
  return nullptr;
}

}  // namespace arm64

namespace linker {

RelativePatcher* CreateArm64RelativePatcher(RelativePatcherTargetProvider* provider,
                                            const InstructionSetFeatures* features) {
  UNUSED(provider);
  UNUSED(features);
  LOG(FATAL) << "Undefined ISA - arm64";
  return nullptr;
}

}  // namespace linker

}  // namespace art
