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
#include "jni/quick/arm/calling_convention_arm.h"
#include "jni/quick/calling_convention.h"
#include "linker/relative_patcher.h"
#include "offsets.h"
#include "utils/assembler.h"

namespace art {

Assembler* CreateArm32Assembler() {
  LOG(FATAL) << "Undefined ISA - arm";
  return nullptr;
}

Assembler* CreateThumb2Assembler() {
  LOG(FATAL) << "Undefined ISA - arm";
  return nullptr;
}

JniCallingConvention* CreateArmJniCallingConvention(bool is_static,
                                                    bool is_synchronized,
                                                    const char* shorty) {
  UNUSED(is_static);
  UNUSED(is_synchronized);
  UNUSED(shorty);
  LOG(FATAL) << "Undefined ISA - arm";
  return nullptr;
}

ManagedRuntimeCallingConvention* CreateArmManagedRuntimeCallingConvention(
    bool is_static, bool is_synchronized, const char* shorty) {
  UNUSED(is_static);
  UNUSED(is_synchronized);
  UNUSED(shorty);
  LOG(FATAL) << "Undefined ISA - arm";
  return nullptr;
}

Mir2Lir* ArmCodeGenerator(CompilationUnit* const cu, MIRGraph* const mir_graph,
                          ArenaAllocator* const arena) {
  UNUSED(cu);
  UNUSED(mir_graph);
  UNUSED(arena);
  LOG(FATAL) << "Undefined ISA - arm";
  return nullptr;
}

namespace arm {

const std::vector<uint8_t>* CreateTrampoline(EntryPointCallingConvention abi,
                                             ThreadOffset<4> offset) {
  UNUSED(abi);
  UNUSED(offset);
  LOG(FATAL) << "Undefined ISA - arm";
  return nullptr;
}

}  // namespace arm

namespace linker {

RelativePatcher* CreateThumb2RelativePatcher(RelativePatcherTargetProvider* provider) {
  UNUSED(provider);
  LOG(FATAL) << "Undefined ISA - arm";
  return nullptr;
}

}  // namespace linker

}  // namespace art
