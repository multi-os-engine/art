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

#include "isa_interface_quick.h"
#include "jni_env_ext.h"

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
