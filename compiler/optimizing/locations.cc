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

#include "locations.h"

#include "nodes.h"

namespace art {

LocationSummary::LocationSummary(HInstruction* instruction,
                                 CallKind call_kind,
                                 bool intrinsified)
    : inputs_(instruction->GetBlock()->GetGraph()->GetArena(), instruction->InputCount()),
      temps_(instruction->GetBlock()->GetGraph()->GetArena(), 0),
      environment_(instruction->GetBlock()->GetGraph()->GetArena(),
                   instruction->EnvironmentSize()),
      output_overlaps_(Location::kOutputOverlap),
      call_kind_(call_kind),
      stack_mask_(nullptr),
      register_mask_(0),
      live_registers_(),
      intrinsified_(intrinsified) {
  inputs_.SetSize(instruction->InputCount());
  for (size_t i = 0; i < instruction->InputCount(); ++i) {
    inputs_.Put(i, Location());
  }
  environment_.SetSize(instruction->EnvironmentSize());
  for (size_t i = 0; i < instruction->EnvironmentSize(); ++i) {
    environment_.Put(i, Location());
  }
  instruction->SetLocations(this);

  if (NeedsSafepoint()) {
    ArenaAllocator* arena = instruction->GetBlock()->GetGraph()->GetArena();
    stack_mask_ = new (arena) ArenaBitVector(arena, 0, true);
  }
}


Location Location::RegisterOrConstant(HInstruction* instruction) {
  return instruction->IsConstant()
      ? Location::ConstantLocation(instruction->AsConstant())
      : Location::RequiresRegister();
}

Location Location::ByteRegisterOrConstant(int reg, HInstruction* instruction) {
  return instruction->IsConstant()
      ? Location::ConstantLocation(instruction->AsConstant())
      : Location::RegisterLocation(reg);
}

Location Location::ToLow(ArenaAllocator *arena) const {
  if (IsRegisterPair()) {
    return Location::RegisterLocation(low());
  }
  if (IsFpuRegisterPair()) {
    return Location::FpuRegisterLocation(low());
  }
  if (IsConstant()) {
    // Have to generate a new IntConstant.
    HConstant* constant = GetConstant();
    int64_t val;
    if (constant->IsLongConstant()) {
      val = constant->AsLongConstant()->GetValue();
    } else {
      DCHECK(constant->IsDoubleConstant());
      val = bit_cast<double, int64_t>(constant->AsDoubleConstant()->GetValue());
    }
    HIntConstant* low_const = new (arena) HIntConstant(Low32Bits(val));
    return Location::ConstantLocation(low_const);
  }
  DCHECK(IsDoubleStackSlot()) << *this;
  return Location::StackSlot(GetStackIndex());
}

Location Location::ToHigh(ArenaAllocator *arena) const {
  if (IsRegisterPair()) {
    return Location::RegisterLocation(high());
  }
  if (IsFpuRegisterPair()) {
    return Location::FpuRegisterLocation(high());
  }
  if (IsConstant()) {
    // Have to generate a new IntConstant.
    HConstant* constant = GetConstant();
    int64_t val;
    if (constant->IsLongConstant()) {
      val = constant->AsLongConstant()->GetValue();
    } else {
      DCHECK(constant->IsDoubleConstant());
      val = bit_cast<double, int64_t>(constant->AsDoubleConstant()->GetValue());
    }
    HIntConstant* high_const = new (arena) HIntConstant(High32Bits(val));
    return Location::ConstantLocation(high_const);
  }
  DCHECK(IsDoubleStackSlot()) << *this;
  // Generate the high word of the double stack slot.
  // TODO: remove hardcoded 4.
  return Location::StackSlot(GetHighStackIndex(4));
}

std::ostream& operator<<(std::ostream& os, const Location& location) {
  os << location.DebugString();
  if (location.IsRegister() || location.IsFpuRegister()) {
    os << location.reg();
  } else if (location.IsPair()) {
    os << location.low() << ":" << location.high();
  } else if (location.IsStackSlot() || location.IsDoubleStackSlot()) {
    os << location.GetStackIndex();
  }
  return os;
}

}  // namespace art
