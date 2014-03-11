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

#include "code_generator_arm.h"
#include "utils/assembler.h"
#include "utils/arm/assembler_arm.h"

#define __ reinterpret_cast<ArmAssembler*>(assembler())->

namespace art {
namespace arm {

void CodeGeneratorARM::GenerateFrameEntry() {
  RegList registers = (1 << LR) | (1 << FP);
  __ PushList(registers);
  if (frame_size_ != 0) {
    __ AddConstant(SP, -frame_size_);
  }
}

void CodeGeneratorARM::GenerateFrameExit() {
  RegList registers = (1 << PC) | (1 << FP);
  __ PopList(registers);
}

void CodeGeneratorARM::Bind(Label* label) {
  __ Bind(label);
}

void CodeGeneratorARM::Push(HInstruction* instruction, Location location) {
  __ Push(location.reg<Register>());
}

void CodeGeneratorARM::Move(HInstruction* instruction, Location location) {
  __ Pop(location.reg<Register>());
}

void LocationsBuilderARM::VisitGoto(HGoto* got) {
  got->set_locations(nullptr);
}

void CodeGeneratorARM::VisitGoto(HGoto* got) {
  HBasicBlock* successor = got->GetSuccessor();
  if (graph()->exit_block() == successor) {
    GenerateFrameExit();
  } else if (!GoesToNextBlock(got->block(), successor)) {
    __ b(GetLabelOf(successor));
  }
}

void LocationsBuilderARM::VisitExit(HExit* exit) {
  exit->set_locations(nullptr);
}

void CodeGeneratorARM::VisitExit(HExit* exit) {
  if (kIsDebugBuild) {
    __ Comment("Unreachable");
    __ bkpt(0);
  }
}

void LocationsBuilderARM::VisitIf(HIf* if_instr) {
  LocationSummary* locations = new (graph()->arena()) LocationSummary(if_instr);
  locations->SetInAt(0, R0);
  if_instr->set_locations(locations);
}

void CodeGeneratorARM::VisitIf(HIf* if_instr) {
  __ cmp(if_instr->locations()->InAt(0).reg<Register>(), ShifterOperand(0));
  __ b(GetLabelOf(if_instr->IfFalseSuccessor()), EQ);
  if (!GoesToNextBlock(if_instr->block(), if_instr->IfTrueSuccessor())) {
    __ b(GetLabelOf(if_instr->IfTrueSuccessor()));
  }
}

void LocationsBuilderARM::VisitEqual(HEqual* equal) {
  LocationSummary* locations = new (graph()->arena()) LocationSummary(equal);
  locations->SetInAt(0, R0);
  locations->SetInAt(1, R1);
  locations->SetOut(R0);
  equal->set_locations(locations);
}

void CodeGeneratorARM::VisitEqual(HEqual* equal) {
  LocationSummary* locations = equal->locations();
  __ teq(locations->InAt(0).reg<Register>(),
         ShifterOperand(locations->InAt(1).reg<Register>()));
  __ mov(locations->Out().reg<Register>(), ShifterOperand(1), EQ);
  __ mov(locations->Out().reg<Register>(), ShifterOperand(0), NE);
}

void LocationsBuilderARM::VisitLocal(HLocal* local) {
  local->set_locations(nullptr);
}

void CodeGeneratorARM::VisitLocal(HLocal* local) {
  DCHECK_EQ(local->block(), graph()->entry_block());
  frame_size_ += kWordSize;
}

void LocationsBuilderARM::VisitLoadLocal(HLoadLocal* load) {
  LocationSummary* locations = new (graph()->arena()) LocationSummary(load);
  locations->SetOut(R0);
  load->set_locations(locations);
}

static int32_t GetStackSlot(HLocal* local) {
  return local->reg_number() * kWordSize;
}

void CodeGeneratorARM::VisitLoadLocal(HLoadLocal* load) {
  LocationSummary* locations = load->locations();
  __ LoadFromOffset(kLoadWord, locations->Out().reg<Register>(),
                    FP, GetStackSlot(load->GetLocal()));
}

void LocationsBuilderARM::VisitStoreLocal(HStoreLocal* store) {
  LocationSummary* locations = new (graph()->arena()) LocationSummary(store);
  locations->SetInAt(1, R0);
  store->set_locations(locations);
}

void CodeGeneratorARM::VisitStoreLocal(HStoreLocal* store) {
  LocationSummary* locations = store->locations();
//  __ StoreToOffset(kStoreWord, locations->InAt(1).reg<Register>(),
//                   FP, GetStackSlot(store->GetLocal()));
}

void LocationsBuilderARM::VisitIntConstant(HIntConstant* constant) {
  constant->set_locations(nullptr);
}

void CodeGeneratorARM::VisitIntConstant(HIntConstant* constant) {
  // Will be generated at use site.
}

void LocationsBuilderARM::VisitReturnVoid(HReturnVoid* ret) {
  ret->set_locations(nullptr);
}

void CodeGeneratorARM::VisitReturnVoid(HReturnVoid* ret) {
  GenerateFrameExit();
}

void LocationsBuilderARM::VisitReturn(HReturn* ret) {
  LocationSummary* locations = new (graph()->arena()) LocationSummary(ret);
  locations->SetInAt(0, R0);
  ret->set_locations(locations);
}

void CodeGeneratorARM::VisitReturn(HReturn* ret) {
  DCHECK_EQ(ret->locations()->InAt(0).reg<Register>(), R0);
  GenerateFrameExit();
}

}  // namespace arm
}  // namespace art
