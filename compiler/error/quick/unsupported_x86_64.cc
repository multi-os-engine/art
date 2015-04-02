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
#include "jni/quick/x86_64/calling_convention_x86_64.h"
#include "jni/quick/calling_convention.h"
#include "linker/relative_patcher.h"
#include "offsets.h"
#include "utils/assembler.h"

namespace art {

Assembler* CreateX86_64Assembler() {
  LOG(FATAL) << "Undefined ISA - x86_64";
  return nullptr;
}

JniCallingConvention* CreateX86_64JniCallingConvention(bool is_static,
                                                       bool is_synchronized,
                                                       const char* shorty) {
  UNUSED(is_static);
  UNUSED(is_synchronized);
  UNUSED(shorty);
  LOG(FATAL) << "Undefined ISA - x86_64";
  return nullptr;
}

ManagedRuntimeCallingConvention* CreateX86_64ManagedRuntimeCallingConvention(
    bool is_static, bool is_synchronized, const char* shorty) {
  UNUSED(is_static);
  UNUSED(is_synchronized);
  UNUSED(shorty);
  LOG(FATAL) << "Undefined ISA - x86_64";
  return nullptr;
}

namespace linker {

RelativePatcher* CreateX86_64RelativePatcher() {
  LOG(FATAL) << "Undefined ISA - x86_64";
  return nullptr;
}

}  // namespace linker

namespace x86_64 {

const std::vector<uint8_t>* CreateTrampoline(ThreadOffset<8> offset) {
  UNUSED(offset);
  LOG(FATAL) << "Undefined ISA - x86_64";
  return nullptr;
}

}  // namespace x86_64

}  // namespace art
