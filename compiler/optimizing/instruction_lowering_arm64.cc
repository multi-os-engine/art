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

#include "instruction_lowering_arm64.h"

namespace art {
namespace arm64 {

void InstructionLoweringArm64::TryExtractArrayAccessAddress(HInstruction* access) {
  DCHECK(access->IsArrayGet() || access->IsArraySet());

  HArrayGet* array_get = nullptr;
  HArraySet* array_set = nullptr;
  HInstruction* index = nullptr;
  if (access->IsArrayGet()) {
    array_get = access->AsArrayGet();
    index = array_get->GetIndex();
  } else if (access->IsArraySet()) {
    array_set = access->AsArraySet();
    index = array_set->GetIndex();
  }

  if (index->IsConstant() ||
      (index->IsBoundsCheck() && index->AsBoundsCheck()->GetIndex()->IsConstant())) {
    // When the index is a constant, all the addressing can be fitted in the
    // memory access instruction.
    return;
  }
  if ((array_set != nullptr) && (array_set->GetComponentType() == Primitive::kPrimNot)) {
    // We need special handling when storing into an array of objects.
    return;
  }

  // Extract the base address computation.
  HBasicBlock* block = access->GetBlock();
  ArenaAllocator* arena = GetGraph()->GetArena();

  HArm64ArrayAccessAddress* address = access->IsArrayGet()
      ?  new (arena) HArm64ArrayAccessAddress(array_get)
      :  new (arena) HArm64ArrayAccessAddress(array_set);
  HIntConstant* null_offset = new (arena) HIntConstant(0);
  block->InsertInstructionBefore(null_offset, access);
  block->InsertInstructionBefore(address, access);
  access->ReplaceInput(address, 0);
  access->ReplaceInput(null_offset, 1);
}

void InstructionLoweringArm64::VisitArrayGet(HArrayGet* instruction) {
  TryExtractArrayAccessAddress(instruction);
}

void InstructionLoweringArm64::VisitArraySet(HArraySet* instruction) {
  TryExtractArrayAccessAddress(instruction);
}

}  // namespace arm64
}  // namespace art
