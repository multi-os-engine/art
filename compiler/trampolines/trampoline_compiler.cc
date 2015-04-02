/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "trampoline_compiler.h"

#include "jni_env_ext.h"
#include "utils/arm/assembler_arm.h"
#include "utils/arm64/assembler_arm64.h"
#include "utils/mips/assembler_mips.h"
#include "utils/mips64/assembler_mips64.h"
#include "utils/x86/assembler_x86.h"
#include "utils/x86_64/assembler_x86_64.h"
#include "error/unsupported_64_bit.h"

#define __ assembler->

namespace art {

const std::vector<uint8_t>* CreateTrampoline64(InstructionSet isa, EntryPointCallingConvention abi,
                                               ThreadOffset<8> offset) {
  return CreateTrampolineFor64(isa, abi, offset);
}

const std::vector<uint8_t>* CreateTrampoline32(InstructionSet isa, EntryPointCallingConvention abi,
                                               ThreadOffset<4> offset) {
  return CreateTrampolineFor32(isa, abi, offset);
}

}  // namespace art
