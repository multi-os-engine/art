/*
 * Copyright (C) 2012 The Android Open Source Project
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

#include "codegen_x86.h"
#include "dex/quick/mir_to_lir-inl.h"
#include "x86_lir.h"

namespace art {

// Macro to templatize functions and instantiate them.
#define X86MIR2LIR(ret, sig) \
  template ret X86Mir2Lir<4>::sig; \
  template ret X86Mir2Lir<8>::sig; \
  template <size_t pointer_size> ret X86Mir2Lir<pointer_size>::sig


X86MIR2LIR(void, GenArithOpFloat(Instruction::Code opcode,
                                RegLocation rl_dest, RegLocation rl_src1, RegLocation rl_src2)) {
  X86OpCode op = kX86Nop;
  RegLocation rl_result;

  /*
   * Don't attempt to optimize register usage since these opcodes call out to
   * the handlers.
   */
  switch (opcode) {
    case Instruction::ADD_FLOAT_2ADDR:
    case Instruction::ADD_FLOAT:
      op = kX86AddssRR;
      break;
    case Instruction::SUB_FLOAT_2ADDR:
    case Instruction::SUB_FLOAT:
      op = kX86SubssRR;
      break;
    case Instruction::DIV_FLOAT_2ADDR:
    case Instruction::DIV_FLOAT:
      op = kX86DivssRR;
      break;
    case Instruction::MUL_FLOAT_2ADDR:
    case Instruction::MUL_FLOAT:
      op = kX86MulssRR;
      break;
    case Instruction::REM_FLOAT_2ADDR:
    case Instruction::REM_FLOAT:
      this->FlushAllRegs();   // Send everything to home location
      this->CallRuntimeHelperRegLocationRegLocation(QUICK_ENTRYPOINT_OFFSET(pointer_size, pFmodf),
                                                    rl_src1, rl_src2, false);
      rl_result = this->GetReturn(true);
      this->StoreValue(rl_dest, rl_result);
      return;
    case Instruction::NEG_FLOAT:
      GenNegFloat(rl_dest, rl_src1);
      return;
    default:
      LOG(FATAL) << "Unexpected opcode: " << opcode;
  }
  rl_src1 = this->LoadValue(rl_src1, kFPReg);
  rl_src2 = this->LoadValue(rl_src2, kFPReg);
  rl_result = this->EvalLoc(rl_dest, kFPReg, true);
  RegStorage r_dest = rl_result.reg;
  RegStorage r_src1 = rl_src1.reg;
  RegStorage r_src2 = rl_src2.reg;
  if (r_dest == r_src2) {
    r_src2 = this->AllocTempSingle();
    OpRegCopy(r_src2, r_dest);
  }
  OpRegCopy(r_dest, r_src1);
  this->NewLIR2(op, r_dest.GetReg(), r_src2.GetReg());
  this->StoreValue(rl_dest, rl_result);
}

X86MIR2LIR(void, GenArithOpDouble(Instruction::Code opcode,
                                 RegLocation rl_dest, RegLocation rl_src1, RegLocation rl_src2)) {
  DCHECK(rl_dest.wide);
  DCHECK(rl_dest.fp);
  DCHECK(rl_src1.wide);
  DCHECK(rl_src1.fp);
  DCHECK(rl_src2.wide);
  DCHECK(rl_src2.fp);
  X86OpCode op = kX86Nop;
  RegLocation rl_result;

  switch (opcode) {
    case Instruction::ADD_DOUBLE_2ADDR:
    case Instruction::ADD_DOUBLE:
      op = kX86AddsdRR;
      break;
    case Instruction::SUB_DOUBLE_2ADDR:
    case Instruction::SUB_DOUBLE:
      op = kX86SubsdRR;
      break;
    case Instruction::DIV_DOUBLE_2ADDR:
    case Instruction::DIV_DOUBLE:
      op = kX86DivsdRR;
      break;
    case Instruction::MUL_DOUBLE_2ADDR:
    case Instruction::MUL_DOUBLE:
      op = kX86MulsdRR;
      break;
    case Instruction::REM_DOUBLE_2ADDR:
    case Instruction::REM_DOUBLE:
      this->FlushAllRegs();   // Send everything to home location
      this->CallRuntimeHelperRegLocationRegLocation(QUICK_ENTRYPOINT_OFFSET(pointer_size, pFmod),
                                                    rl_src1, rl_src2, false);
      rl_result = this->GetReturnWide(true);
      this->StoreValueWide(rl_dest, rl_result);
      return;
    case Instruction::NEG_DOUBLE:
      GenNegDouble(rl_dest, rl_src1);
      return;
    default:
      LOG(FATAL) << "Unexpected opcode: " << opcode;
  }
  rl_src1 = this->LoadValueWide(rl_src1, kFPReg);
  rl_src2 = this->LoadValueWide(rl_src2, kFPReg);
  rl_result = this->EvalLoc(rl_dest, kFPReg, true);
  if (rl_result.reg == rl_src2.reg) {
    rl_src2.reg = this->AllocTempDouble();
    OpRegCopy(rl_src2.reg, rl_result.reg);
  }
  OpRegCopy(rl_result.reg, rl_src1.reg);
  this->NewLIR2(op, rl_result.reg.GetReg(), rl_src2.reg.GetReg());
  this->StoreValueWide(rl_dest, rl_result);
}

X86MIR2LIR(void, GenLongToFP(RegLocation rl_dest, RegLocation rl_src, bool is_double)) {
  // Compute offsets to the source and destination VRs on stack
  int src_v_reg_offset = this->SRegOffset(rl_src.s_reg_low);
  int dest_v_reg_offset = this->SRegOffset(rl_dest.s_reg_low);

  // Update the in-register state of source.
  rl_src = this->UpdateLocWide(rl_src);

  // If the source is in physical register, then put it in its location on stack.
  if (rl_src.location == kLocPhysReg) {
    typename Mir2Lir<pointer_size>::RegisterInfo* reg_info = this->GetRegInfo(rl_src.reg);

    if (reg_info != nullptr && reg_info->IsTemp()) {
      // Calling FlushSpecificReg because it will only write back VR if it is dirty.
      this->FlushSpecificReg(reg_info);
      // ResetDef to prevent NullifyRange from removing stores.
      this->ResetDef(rl_src.reg);
    } else {
      // It must have been register promoted if it is not a temp but is still in physical
      // register. Since we need it to be in memory to convert, we place it there now.
      StoreBaseDisp(TargetReg(kSp), src_v_reg_offset, rl_src.reg, k64);
    }
  }

  // Push the source virtual register onto the x87 stack.
  LIR *fild64 = this->NewLIR2NoDest(kX86Fild64M, TargetReg(kSp).GetReg(),
                                    src_v_reg_offset + LOWORD_OFFSET);
  this->AnnotateDalvikRegAccess(fild64, (src_v_reg_offset + LOWORD_OFFSET) >> 2,
                                true /* is_load */, true /* is64bit */);

  // Now pop off x87 stack and store it in the destination VR's stack location.
  int opcode = is_double ? kX86Fstp64M : kX86Fstp32M;
  int displacement = is_double ? dest_v_reg_offset + LOWORD_OFFSET : dest_v_reg_offset;
  LIR *fstp = this->NewLIR2NoDest(opcode, TargetReg(kSp).GetReg(), displacement);
  this->AnnotateDalvikRegAccess(fstp, displacement >> 2, false /* is_load */, is_double);

  /*
   * The result is in a physical register if it was in a temp or was register
   * promoted. For that reason it is enough to check if it is in physical
   * register. If it is, then we must do all of the bookkeeping necessary to
   * invalidate temp (if needed) and load in promoted register (if needed).
   * If the result's location is in memory, then we do not need to do anything
   * more since the fstp has already placed the correct value in memory.
   */
  RegLocation rl_result = is_double ? this->UpdateLocWide(rl_dest) : this->UpdateLoc(rl_dest);
  if (rl_result.location == kLocPhysReg) {
    /*
     * We already know that the result is in a physical register but do not know if it is the
     * right class. So we call EvalLoc(Wide) first which will ensure that it will get moved to the
     * correct register class.
     */
    if (is_double) {
      rl_result = this->EvalLocWide(rl_dest, kFPReg, true);

      LoadBaseDisp(TargetReg(kSp), dest_v_reg_offset, rl_result.reg, k64);

      this->StoreFinalValueWide(rl_dest, rl_result);
    } else {
      rl_result = this->EvalLoc(rl_dest, kFPReg, true);

      this->Load32Disp(TargetReg(kSp), dest_v_reg_offset, rl_result.reg);

      this->StoreFinalValue(rl_dest, rl_result);
    }
  }
}

X86MIR2LIR(void, GenConversion(Instruction::Code opcode, RegLocation rl_dest,
                              RegLocation rl_src)) {
  RegisterClass rcSrc = kFPReg;
  X86OpCode op = kX86Nop;
  RegLocation rl_result;
  switch (opcode) {
    case Instruction::INT_TO_FLOAT:
      rcSrc = kCoreReg;
      op = kX86Cvtsi2ssRR;
      break;
    case Instruction::DOUBLE_TO_FLOAT:
      rcSrc = kFPReg;
      op = kX86Cvtsd2ssRR;
      break;
    case Instruction::FLOAT_TO_DOUBLE:
      rcSrc = kFPReg;
      op = kX86Cvtss2sdRR;
      break;
    case Instruction::INT_TO_DOUBLE:
      rcSrc = kCoreReg;
      op = kX86Cvtsi2sdRR;
      break;
    case Instruction::FLOAT_TO_INT: {
      rl_src = this->LoadValue(rl_src, kFPReg);
      // In case result vreg is also src vreg, break association to avoid useless copy by EvalLoc()
      this->ClobberSReg(rl_dest.s_reg_low);
      rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
      RegStorage temp_reg = this->AllocTempSingle();

      this->LoadConstant(rl_result.reg, 0x7fffffff);
      this->NewLIR2(kX86Cvtsi2ssRR, temp_reg.GetReg(), rl_result.reg.GetReg());
      this->NewLIR2(kX86ComissRR, rl_src.reg.GetReg(), temp_reg.GetReg());
      LIR* branch_pos_overflow = this->NewLIR2(kX86Jcc8, 0, kX86CondA);
      LIR* branch_na_n = this->NewLIR2(kX86Jcc8, 0, kX86CondP);
      this->NewLIR2(kX86Cvttss2siRR, rl_result.reg.GetReg(), rl_src.reg.GetReg());
      LIR* branch_normal = this->NewLIR1(kX86Jmp8, 0);
      branch_na_n->target = this->NewLIR0(kPseudoTargetLabel);
      this->NewLIR2(kX86Xor32RR, rl_result.reg.GetReg(), rl_result.reg.GetReg());
      branch_pos_overflow->target = this->NewLIR0(kPseudoTargetLabel);
      branch_normal->target = this->NewLIR0(kPseudoTargetLabel);
      this->StoreValue(rl_dest, rl_result);
      return;
    }
    case Instruction::DOUBLE_TO_INT: {
      rl_src = this->LoadValueWide(rl_src, kFPReg);
      // In case result vreg is also src vreg, break association to avoid useless copy by EvalLoc()
      this->ClobberSReg(rl_dest.s_reg_low);
      rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
      RegStorage temp_reg = this->AllocTempDouble();

      this->LoadConstant(rl_result.reg, 0x7fffffff);
      this->NewLIR2(kX86Cvtsi2sdRR, temp_reg.GetReg(), rl_result.reg.GetReg());
      this->NewLIR2(kX86ComisdRR, rl_src.reg.GetReg(), temp_reg.GetReg());
      LIR* branch_pos_overflow = this->NewLIR2(kX86Jcc8, 0, kX86CondA);
      LIR* branch_na_n = this->NewLIR2(kX86Jcc8, 0, kX86CondP);
      this->NewLIR2(kX86Cvttsd2siRR, rl_result.reg.GetReg(), rl_src.reg.GetReg());
      LIR* branch_normal = this->NewLIR1(kX86Jmp8, 0);
      branch_na_n->target = this->NewLIR0(kPseudoTargetLabel);
      this->NewLIR2(kX86Xor32RR, rl_result.reg.GetReg(), rl_result.reg.GetReg());
      branch_pos_overflow->target = this->NewLIR0(kPseudoTargetLabel);
      branch_normal->target = this->NewLIR0(kPseudoTargetLabel);
      this->StoreValue(rl_dest, rl_result);
      return;
    }
    case Instruction::LONG_TO_DOUBLE:
      GenLongToFP(rl_dest, rl_src, true /* is_double */);
      return;
    case Instruction::LONG_TO_FLOAT:
      GenLongToFP(rl_dest, rl_src, false /* is_double */);
      return;
    case Instruction::FLOAT_TO_LONG:
      this->GenConversionCall(QUICK_ENTRYPOINT_OFFSET(pointer_size, pF2l), rl_dest, rl_src);
      return;
    case Instruction::DOUBLE_TO_LONG:
      this->GenConversionCall(QUICK_ENTRYPOINT_OFFSET(pointer_size, pD2l), rl_dest, rl_src);
      return;
    default:
      LOG(INFO) << "Unexpected opcode: " << opcode;
  }
  // At this point, target will be either float or double.
  DCHECK(rl_dest.fp);
  if (rl_src.wide) {
    rl_src = this->LoadValueWide(rl_src, rcSrc);
  } else {
    rl_src = this->LoadValue(rl_src, rcSrc);
  }
  rl_result = this->EvalLoc(rl_dest, kFPReg, true);
  this->NewLIR2(op, rl_result.reg.GetReg(), rl_src.reg.GetReg());
  if (rl_dest.wide) {
    this->StoreValueWide(rl_dest, rl_result);
  } else {
    this->StoreValue(rl_dest, rl_result);
  }
}

X86MIR2LIR(void, GenCmpFP(Instruction::Code code, RegLocation rl_dest,
                         RegLocation rl_src1, RegLocation rl_src2)) {
  bool single = (code == Instruction::CMPL_FLOAT) || (code == Instruction::CMPG_FLOAT);
  bool unordered_gt = (code == Instruction::CMPG_DOUBLE) || (code == Instruction::CMPG_FLOAT);
  if (single) {
    rl_src1 = this->LoadValue(rl_src1, kFPReg);
    rl_src2 = this->LoadValue(rl_src2, kFPReg);
  } else {
    rl_src1 = this->LoadValueWide(rl_src1, kFPReg);
    rl_src2 = this->LoadValueWide(rl_src2, kFPReg);
  }
  // In case result vreg is also src vreg, break association to avoid useless copy by EvalLoc()
  this->ClobberSReg(rl_dest.s_reg_low);
  RegLocation rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
  LoadConstantNoClobber(rl_result.reg, unordered_gt ? 1 : 0);
  if (single) {
    this->NewLIR2(kX86UcomissRR, rl_src1.reg.GetReg(), rl_src2.reg.GetReg());
  } else {
    this->NewLIR2(kX86UcomisdRR, rl_src1.reg.GetReg(), rl_src2.reg.GetReg());
  }
  LIR* branch = NULL;
  if (unordered_gt) {
    branch = this->NewLIR2(kX86Jcc8, 0, kX86CondPE);
  }
  // If the result reg can't be byte accessed, use a jump and move instead of a set.
  if (rl_result.reg.GetReg() >= rs_rX86_SP.GetReg()) {
    LIR* branch2 = NULL;
    if (unordered_gt) {
      branch2 = this->NewLIR2(kX86Jcc8, 0, kX86CondA);
      this->NewLIR2(kX86Mov32RI, rl_result.reg.GetReg(), 0x0);
    } else {
      branch2 = this->NewLIR2(kX86Jcc8, 0, kX86CondBe);
      this->NewLIR2(kX86Mov32RI, rl_result.reg.GetReg(), 0x1);
    }
    branch2->target = this->NewLIR0(kPseudoTargetLabel);
  } else {
    this->NewLIR2(kX86Set8R, rl_result.reg.GetReg(), kX86CondA /* above - unsigned > */);
  }
  this->NewLIR2(kX86Sbb32RI, rl_result.reg.GetReg(), 0);
  if (unordered_gt) {
    branch->target = this->NewLIR0(kPseudoTargetLabel);
  }
  this->StoreValue(rl_dest, rl_result);
}

X86MIR2LIR(void, GenFusedFPCmpBranch(BasicBlock* bb, MIR* mir, bool gt_bias,
                                    bool is_double)) {
  LIR* taken = &this->block_label_list_[bb->taken];
  LIR* not_taken = &this->block_label_list_[bb->fall_through];
  LIR* branch = NULL;
  RegLocation rl_src1;
  RegLocation rl_src2;
  if (is_double) {
    rl_src1 = this->mir_graph_->GetSrcWide(mir, 0);
    rl_src2 = this->mir_graph_->GetSrcWide(mir, 2);
    rl_src1 = this->LoadValueWide(rl_src1, kFPReg);
    rl_src2 = this->LoadValueWide(rl_src2, kFPReg);
    this->NewLIR2(kX86UcomisdRR, rl_src1.reg.GetReg(), rl_src2.reg.GetReg());
  } else {
    rl_src1 = this->mir_graph_->GetSrc(mir, 0);
    rl_src2 = this->mir_graph_->GetSrc(mir, 1);
    rl_src1 = this->LoadValue(rl_src1, kFPReg);
    rl_src2 = this->LoadValue(rl_src2, kFPReg);
    this->NewLIR2(kX86UcomissRR, rl_src1.reg.GetReg(), rl_src2.reg.GetReg());
  }
  ConditionCode ccode = mir->meta.ccode;
  switch (ccode) {
    case kCondEq:
      if (!gt_bias) {
        branch = this->NewLIR2(kX86Jcc8, 0, kX86CondPE);
        branch->target = not_taken;
      }
      break;
    case kCondNe:
      if (!gt_bias) {
        branch = this->NewLIR2(kX86Jcc8, 0, kX86CondPE);
        branch->target = taken;
      }
      break;
    case kCondLt:
      if (gt_bias) {
        branch = this->NewLIR2(kX86Jcc8, 0, kX86CondPE);
        branch->target = not_taken;
      }
      ccode = kCondUlt;
      break;
    case kCondLe:
      if (gt_bias) {
        branch = this->NewLIR2(kX86Jcc8, 0, kX86CondPE);
        branch->target = not_taken;
      }
      ccode = kCondLs;
      break;
    case kCondGt:
      if (gt_bias) {
        branch = this->NewLIR2(kX86Jcc8, 0, kX86CondPE);
        branch->target = taken;
      }
      ccode = kCondHi;
      break;
    case kCondGe:
      if (gt_bias) {
        branch = this->NewLIR2(kX86Jcc8, 0, kX86CondPE);
        branch->target = taken;
      }
      ccode = kCondUge;
      break;
    default:
      LOG(FATAL) << "Unexpected ccode: " << ccode;
  }
  OpCondBranch(ccode, taken);
}

X86MIR2LIR(void, GenNegFloat(RegLocation rl_dest, RegLocation rl_src)) {
  RegLocation rl_result;
  rl_src = this->LoadValue(rl_src, kCoreReg);
  rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
  OpRegRegImm(kOpAdd, rl_result.reg, rl_src.reg, 0x80000000);
  this->StoreValue(rl_dest, rl_result);
}

X86MIR2LIR(void, GenNegDouble(RegLocation rl_dest, RegLocation rl_src)) {
  RegLocation rl_result;
  rl_src = this->LoadValueWide(rl_src, kCoreReg);
  rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
  OpRegRegImm(kOpAdd, rl_result.reg.GetHigh(), rl_src.reg.GetHigh(), 0x80000000);
  OpRegCopy(rl_result.reg, rl_src.reg);
  this->StoreValueWide(rl_dest, rl_result);
}

X86MIR2LIR(bool, GenInlinedSqrt(CallInfo* info)) {
  RegLocation rl_src = info->args[0];
  RegLocation rl_dest = this->InlineTargetWide(info);  // double place for result
  rl_src = this->LoadValueWide(rl_src, kFPReg);
  RegLocation rl_result = this->EvalLoc(rl_dest, kFPReg, true);
  this->NewLIR2(kX86SqrtsdRR, rl_result.reg.GetReg(), rl_src.reg.GetReg());
  this->StoreValueWide(rl_dest, rl_result);
  return true;
}



}  // namespace art
