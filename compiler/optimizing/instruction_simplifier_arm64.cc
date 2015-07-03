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

#include "instruction_simplifier_arm64.h"

#include "mirror/array-inl.h"

namespace art {
namespace arm64 {

void InstructionSimplifierArm64::TryExtractArrayAccessAddress(HInstruction* access) {
  DCHECK(access->IsArrayGet() || access->IsArraySet());

  HArrayGet* array_get = nullptr;
  HArraySet* array_set = nullptr;
  HInstruction* array;
  HInstruction* index;
  size_t access_size;
  if (access->IsArrayGet()) {
    array_get = access->AsArrayGet();
    array = array_get->GetArray();
    index = array_get->GetIndex();
    access_size = Primitive::ComponentSize(array_get->GetType());
  } else {
    DCHECK(access->IsArraySet());
    array_set = access->AsArraySet();
    array = array_set->GetArray();
    index = array_set->GetIndex();
    access_size = Primitive::ComponentSize(array_set->GetComponentType());
  }

  if (index->IsConstant() ||
      (index->IsBoundsCheck() && index->AsBoundsCheck()->GetIndex()->IsConstant())) {
    // When the index is a constant all the addressing can be fitted in the
    // memory access instruction, so do not split the access.
    return;
  }
  if (array_set != nullptr && array_set->NeedsTypeCheck()) {
    // This requires a runtime call.
    return;
  }

  // Proceed to extract the base address computation.
  ArenaAllocator* arena = GetGraph()->GetArena();

  HIntConstant* offset =
      GetGraph()->GetIntConstant(mirror::Array::DataOffset(access_size).Uint32Value());
  HArm64IntermediateAddress* address = new (arena) HArm64IntermediateAddress(array, offset);
  access->GetBlock()->InsertInstructionBefore(address, access);
  access->ReplaceInput(address, 0);
  // Both instructions must depend on GC to prevent any instruction that can
  // trigger GC to be inserted between the two.
  DCHECK(address->GetSideEffects().DependsOn(SideEffects::ChangesGC()));
  access->AddSideEffect(SideEffects::DependsOnGC());
  // TODO: Code generation for HArrayGet and HArraySet will check whether the input address
  // is an HArm64IntermediateAddress and generate appropriate code.
  // We would like to replace the `HArrayGet` and `HArraySet` with custom instructions (maybe
  // `HArm64Load` and `HArm64Store`). We defer these changes because these new instructions would
  // not bring any advantages  yet.
  // Also see the comments in
  // `InstructionCodeGeneratorARM64::VisitArrayGet()` and
  // `InstructionCodeGeneratorARM64::VisitArraySet()`.
  RecordSimplification();
}

void InstructionSimplifierArm64::VisitArrayGet(HArrayGet* instruction) {
  TryExtractArrayAccessAddress(instruction);
}

void InstructionSimplifierArm64::VisitArraySet(HArraySet* instruction) {
  TryExtractArrayAccessAddress(instruction);
}

}  // namespace arm64
}  // namespace art
