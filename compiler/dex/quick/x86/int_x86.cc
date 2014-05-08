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

/* This file contains codegen for the X86 ISA */

#include "codegen_x86.h"
#include "dex/quick/mir_to_lir-inl.h"
#include "mirror/array.h"
#include "x86_lir.h"

namespace art {

// Macro to templatize functions and instantiate them.
#define X86MIR2LIR(ret, sig) \
  template ret X86Mir2Lir<4>::sig; \
  template ret X86Mir2Lir<8>::sig; \
  template <size_t pointer_size> ret X86Mir2Lir<pointer_size>::sig

/*
 * Compare two 64-bit values
 *    x = y     return  0
 *    x < y     return -1
 *    x > y     return  1
 */
X86MIR2LIR(void, GenCmpLong(RegLocation rl_dest, RegLocation rl_src1,
                           RegLocation rl_src2)) {
  this->FlushAllRegs();
  LockCallTemps();  // Prepare for explicit register usage
  RegStorage r_tmp1 = RegStorage::MakeRegPair(rs_r0, rs_r1);
  RegStorage r_tmp2 = RegStorage::MakeRegPair(rs_r2, rs_r3);
  this->LoadValueDirectWideFixed(rl_src1, r_tmp1);
  this->LoadValueDirectWideFixed(rl_src2, r_tmp2);
  // Compute (r1:r0) = (r1:r0) - (r3:r2)
  OpRegReg(kOpSub, rs_r0, rs_r2);  // r0 = r0 - r2
  OpRegReg(kOpSbc, rs_r1, rs_r3);  // r1 = r1 - r3 - CF
  this->NewLIR2(kX86Set8R, rs_r2.GetReg(), kX86CondL);  // r2 = (r1:r0) < (r3:r2) ? 1 : 0
  this->NewLIR2(kX86Movzx8RR, rs_r2.GetReg(), rs_r2.GetReg());
  OpReg(kOpNeg, rs_r2);         // r2 = -r2
  OpRegReg(kOpOr, rs_r0, rs_r1);   // r0 = high | low - sets ZF
  this->NewLIR2(kX86Set8R, rs_r0.GetReg(), kX86CondNz);  // r0 = (r1:r0) != (r3:r2) ? 1 : 0
  this->NewLIR2(kX86Movzx8RR, r0, r0);
  OpRegReg(kOpOr, rs_r0, rs_r2);   // r0 = r0 | r2
  RegLocation rl_result = LocCReturn();
  this->StoreValue(rl_dest, rl_result);
}

X86ConditionCode X86ConditionEncoding(ConditionCode cond) {
  switch (cond) {
    case kCondEq: return kX86CondEq;
    case kCondNe: return kX86CondNe;
    case kCondCs: return kX86CondC;
    case kCondCc: return kX86CondNc;
    case kCondUlt: return kX86CondC;
    case kCondUge: return kX86CondNc;
    case kCondMi: return kX86CondS;
    case kCondPl: return kX86CondNs;
    case kCondVs: return kX86CondO;
    case kCondVc: return kX86CondNo;
    case kCondHi: return kX86CondA;
    case kCondLs: return kX86CondBe;
    case kCondGe: return kX86CondGe;
    case kCondLt: return kX86CondL;
    case kCondGt: return kX86CondG;
    case kCondLe: return kX86CondLe;
    case kCondAl:
    case kCondNv: LOG(FATAL) << "Should not reach here";
  }
  return kX86CondO;
}

X86MIR2LIR(LIR*, OpCmpBranch(ConditionCode cond, RegStorage src1, RegStorage src2, LIR* target)) {
  this->NewLIR2(kX86Cmp32RR, src1.GetReg(), src2.GetReg());
  X86ConditionCode cc = X86ConditionEncoding(cond);
  LIR* branch = this->NewLIR2(kX86Jcc8, 0 /* lir operand for Jcc offset */ ,
                              cc);
  branch->target = target;
  return branch;
}

X86MIR2LIR(LIR*, OpCmpImmBranch(ConditionCode cond, RegStorage reg,
                                int check_value, LIR* target)) {
  if ((check_value == 0) && (cond == kCondEq || cond == kCondNe)) {
    // TODO: when check_value == 0 and reg is rCX, use the jcxz/nz opcode
    this->NewLIR2(kX86Test32RR, reg.GetReg(), reg.GetReg());
  } else {
    this->NewLIR2(IS_SIMM8(check_value) ? kX86Cmp32RI8 : kX86Cmp32RI, reg.GetReg(), check_value);
  }
  X86ConditionCode cc = X86ConditionEncoding(cond);
  LIR* branch = this->NewLIR2(kX86Jcc8, 0 /* lir operand for Jcc offset */ , cc);
  branch->target = target;
  return branch;
}

X86MIR2LIR(LIR*, OpRegCopyNoInsert(RegStorage r_dest, RegStorage r_src)) {
  // If src or dest is a pair, we'll be using low reg.
  if (r_dest.IsPair()) {
    r_dest = r_dest.GetLow();
  }
  if (r_src.IsPair()) {
    r_src = r_src.GetLow();
  }
  if (r_dest.IsFloat() || r_src.IsFloat())
    return OpFpRegCopy(r_dest, r_src);
  LIR* res = this->RawLIR(this->current_dalvik_offset_, kX86Mov32RR,
                          r_dest.GetReg(), r_src.GetReg());
  if (!(this->cu_->disable_opt & (1 << kSafeOptimizations)) && r_dest == r_src) {
    res->flags.is_nop = true;
  }
  return res;
}

X86MIR2LIR(void, OpRegCopy(RegStorage r_dest, RegStorage r_src)) {
  if (r_dest != r_src) {
    LIR *res = OpRegCopyNoInsert(r_dest, r_src);
    this->AppendLIR(res);
  }
}

X86MIR2LIR(void, OpRegCopyWide(RegStorage r_dest, RegStorage r_src)) {
  if (r_dest != r_src) {
    bool dest_fp = r_dest.IsFloat();
    bool src_fp = r_src.IsFloat();
    if (dest_fp) {
      if (src_fp) {
        OpRegCopy(r_dest, r_src);
      } else {
        // TODO: Prevent this from happening in the code. The result is often
        // unused or could have been loaded more easily from memory.
        this->NewLIR2(kX86MovdxrRR, r_dest.GetReg(), r_src.GetLowReg());
        RegStorage r_tmp = this->AllocTempDouble();
        this->NewLIR2(kX86MovdxrRR, r_tmp.GetReg(), r_src.GetHighReg());
        this->NewLIR2(kX86PunpckldqRR, r_dest.GetReg(), r_tmp.GetReg());
        this->FreeTemp(r_tmp);
      }
    } else {
      if (src_fp) {
        this->NewLIR2(kX86MovdrxRR, r_dest.GetLowReg(), r_src.GetReg());
        RegStorage temp_reg = this->AllocTempDouble();
        this->NewLIR2(kX86MovsdRR, temp_reg.GetReg(), r_src.GetReg());
        this->NewLIR2(kX86PsrlqRI, temp_reg.GetReg(), 32);
        this->NewLIR2(kX86MovdrxRR, r_dest.GetHighReg(), temp_reg.GetReg());
      } else {
        DCHECK(r_dest.IsPair());
        DCHECK(r_src.IsPair());
        // Handle overlap
        if (r_src.GetHighReg() == r_dest.GetLowReg() && r_src.GetLowReg() == r_dest.GetHighReg()) {
          // Deal with cycles.
          RegStorage temp_reg = this->AllocTemp();
          OpRegCopy(temp_reg, r_dest.GetHigh());
          OpRegCopy(r_dest.GetHigh(), r_dest.GetLow());
          OpRegCopy(r_dest.GetLow(), temp_reg);
          this->FreeTemp(temp_reg);
        } else if (r_src.GetHighReg() == r_dest.GetLowReg()) {
          OpRegCopy(r_dest.GetHigh(), r_src.GetHigh());
          OpRegCopy(r_dest.GetLow(), r_src.GetLow());
        } else {
          OpRegCopy(r_dest.GetLow(), r_src.GetLow());
          OpRegCopy(r_dest.GetHigh(), r_src.GetHigh());
        }
      }
    }
  }
}

X86MIR2LIR(void, GenSelect(BasicBlock* bb, MIR* mir)) {
  RegLocation rl_result;
  RegLocation rl_src = this->mir_graph_->GetSrc(mir, 0);
  RegLocation rl_dest = this->mir_graph_->GetDest(mir);
  rl_src = this->LoadValue(rl_src, kCoreReg);
  ConditionCode ccode = mir->meta.ccode;

  // The kMirOpSelect has two variants, one for constants and one for moves.
  const bool is_constant_case = (mir->ssa_rep->num_uses == 1);

  if (is_constant_case) {
    int true_val = mir->dalvikInsn.vB;
    int false_val = mir->dalvikInsn.vC;
    rl_result = this->EvalLoc(rl_dest, kCoreReg, true);

    /*
     * For ccode == kCondEq:
     *
     * 1) When the true case is zero and result_reg is not same as src_reg:
     *     xor result_reg, result_reg
     *     cmp $0, src_reg
     *     mov t1, $false_case
     *     cmovnz result_reg, t1
     * 2) When the false case is zero and result_reg is not same as src_reg:
     *     xor result_reg, result_reg
     *     cmp $0, src_reg
     *     mov t1, $true_case
     *     cmovz result_reg, t1
     * 3) All other cases (we do compare first to set eflags):
     *     cmp $0, src_reg
     *     mov result_reg, $false_case
     *     mov t1, $true_case
     *     cmovz result_reg, t1
     */
    const bool result_reg_same_as_src =
        (rl_src.location == kLocPhysReg && rl_src.reg.GetReg() == rl_result.reg.GetReg());
    const bool true_zero_case = (true_val == 0 && false_val != 0 && !result_reg_same_as_src);
    const bool false_zero_case = (false_val == 0 && true_val != 0 && !result_reg_same_as_src);
    const bool catch_all_case = !(true_zero_case || false_zero_case);

    if (true_zero_case || false_zero_case) {
      OpRegReg(kOpXor, rl_result.reg, rl_result.reg);
    }

    if (true_zero_case || false_zero_case || catch_all_case) {
      OpRegImm(kOpCmp, rl_src.reg, 0);
    }

    if (catch_all_case) {
      OpRegImm(kOpMov, rl_result.reg, false_val);
    }

    if (true_zero_case || false_zero_case || catch_all_case) {
      ConditionCode cc = true_zero_case ? this->NegateComparison(ccode) : ccode;
      int immediateForTemp = true_zero_case ? false_val : true_val;
      RegStorage temp1_reg = this->AllocTemp();
      OpRegImm(kOpMov, temp1_reg, immediateForTemp);

      OpCondRegReg(kOpCmov, cc, rl_result.reg, temp1_reg);

      this->FreeTemp(temp1_reg);
    }
  } else {
    RegLocation rl_true = this->mir_graph_->GetSrc(mir, 1);
    RegLocation rl_false = this->mir_graph_->GetSrc(mir, 2);
    rl_true = this->LoadValue(rl_true, kCoreReg);
    rl_false = this->LoadValue(rl_false, kCoreReg);
    rl_result = this->EvalLoc(rl_dest, kCoreReg, true);

    /*
     * For ccode == kCondEq:
     *
     * 1) When true case is already in place:
     *     cmp $0, src_reg
     *     cmovnz result_reg, false_reg
     * 2) When false case is already in place:
     *     cmp $0, src_reg
     *     cmovz result_reg, true_reg
     * 3) When neither cases are in place:
     *     cmp $0, src_reg
     *     mov result_reg, false_reg
     *     cmovz result_reg, true_reg
     */

    // kMirOpSelect is generated just for conditional cases when comparison is done with zero.
    OpRegImm(kOpCmp, rl_src.reg, 0);

    if (rl_result.reg.GetReg() == rl_true.reg.GetReg()) {
      OpCondRegReg(kOpCmov, this->NegateComparison(ccode), rl_result.reg, rl_false.reg);
    } else if (rl_result.reg.GetReg() == rl_false.reg.GetReg()) {
      OpCondRegReg(kOpCmov, ccode, rl_result.reg, rl_true.reg);
    } else {
      OpRegCopy(rl_result.reg, rl_false.reg);
      OpCondRegReg(kOpCmov, ccode, rl_result.reg, rl_true.reg);
    }
  }

  this->StoreValue(rl_dest, rl_result);
}

X86MIR2LIR(void, GenFusedLongCmpBranch(BasicBlock* bb, MIR* mir)) {
  LIR* taken = &this->block_label_list_[bb->taken];
  RegLocation rl_src1 = this->mir_graph_->GetSrcWide(mir, 0);
  RegLocation rl_src2 = this->mir_graph_->GetSrcWide(mir, 2);
  ConditionCode ccode = mir->meta.ccode;

  if (rl_src1.is_const) {
    std::swap(rl_src1, rl_src2);
    ccode = this->FlipComparisonOrder(ccode);
  }
  if (rl_src2.is_const) {
    // Do special compare/branch against simple const operand
    int64_t val = this->mir_graph_->ConstantValueWide(rl_src2);
    GenFusedLongCmpImmBranch(bb, rl_src1, val, ccode);
    return;
  }

  this->FlushAllRegs();
  this->LockCallTemps();  // Prepare for explicit register usage
  RegStorage r_tmp1 = RegStorage::MakeRegPair(rs_r0, rs_r1);
  RegStorage r_tmp2 = RegStorage::MakeRegPair(rs_r2, rs_r3);
  this->LoadValueDirectWideFixed(rl_src1, r_tmp1);
  this->LoadValueDirectWideFixed(rl_src2, r_tmp2);
  // Swap operands and condition code to prevent use of zero flag.
  if (ccode == kCondLe || ccode == kCondGt) {
    // Compute (r3:r2) = (r3:r2) - (r1:r0)
    OpRegReg(kOpSub, rs_r2, rs_r0);  // r2 = r2 - r0
    OpRegReg(kOpSbc, rs_r3, rs_r1);  // r3 = r3 - r1 - CF
  } else {
    // Compute (r1:r0) = (r1:r0) - (r3:r2)
    OpRegReg(kOpSub, rs_r0, rs_r2);  // r0 = r0 - r2
    OpRegReg(kOpSbc, rs_r1, rs_r3);  // r1 = r1 - r3 - CF
  }
  switch (ccode) {
    case kCondEq:
    case kCondNe:
      OpRegReg(kOpOr, rs_r0, rs_r1);  // r0 = r0 | r1
      break;
    case kCondLe:
      ccode = kCondGe;
      break;
    case kCondGt:
      ccode = kCondLt;
      break;
    case kCondLt:
    case kCondGe:
      break;
    default:
      LOG(FATAL) << "Unexpected ccode: " << ccode;
  }
  OpCondBranch(ccode, taken);
}

X86MIR2LIR(void, GenFusedLongCmpImmBranch(BasicBlock* bb, RegLocation rl_src1,
                                          int64_t val, ConditionCode ccode)) {
  int32_t val_lo = Low32Bits(val);
  int32_t val_hi = High32Bits(val);
  LIR* taken = &this->block_label_list_[bb->taken];
  rl_src1 = this->LoadValueWide(rl_src1, kCoreReg);
  bool is_equality_test = ccode == kCondEq || ccode == kCondNe;
  if (is_equality_test && val != 0) {
    rl_src1 = this->ForceTempWide(rl_src1);
  }
  RegStorage low_reg = rl_src1.reg.GetLow();
  RegStorage high_reg = rl_src1.reg.GetHigh();

  if (is_equality_test) {
    // We can simpolify of comparing for ==, != to 0.
    if (val == 0) {
      if (this->IsTemp(low_reg)) {
        OpRegReg(kOpOr, low_reg, high_reg);
        // We have now changed it; ignore the old values.
        this->Clobber(rl_src1.reg);
      } else {
        RegStorage t_reg = this->AllocTemp();
        OpRegRegReg(kOpOr, t_reg, low_reg, high_reg);
        this->FreeTemp(t_reg);
      }
      OpCondBranch(ccode, taken);
      return;
    }

    // Need to compute the actual value for ==, !=.
    OpRegImm(kOpSub, low_reg, val_lo);
    this->NewLIR2(kX86Sbb32RI, high_reg.GetReg(), val_hi);
    OpRegReg(kOpOr, high_reg, low_reg);
    this->Clobber(rl_src1.reg);
  } else if (ccode == kCondLe || ccode == kCondGt) {
    // Swap operands and condition code to prevent use of zero flag.
    RegStorage tmp = this->AllocTypedTempWide(false, kCoreReg);
    this->LoadConstantWide(tmp, val);
    OpRegReg(kOpSub, tmp.GetLow(), low_reg);
    OpRegReg(kOpSbc, tmp.GetHigh(), high_reg);
    ccode = (ccode == kCondLe) ? kCondGe : kCondLt;
    this->FreeTemp(tmp);
  } else {
    // We can use a compare for the low word to set CF.
    OpRegImm(kOpCmp, low_reg, val_lo);
    if (this->IsTemp(high_reg)) {
      this->NewLIR2(kX86Sbb32RI, high_reg.GetReg(), val_hi);
      // We have now changed it; ignore the old values.
      this->Clobber(rl_src1.reg);
    } else {
      // mov temp_reg, high_reg; sbb temp_reg, high_constant
      RegStorage t_reg = this->AllocTemp();
      OpRegCopy(t_reg, high_reg);
      this->NewLIR2(kX86Sbb32RI, t_reg.GetReg(), val_hi);
      this->FreeTemp(t_reg);
    }
  }

  OpCondBranch(ccode, taken);
}

X86MIR2LIR(void, CalculateMagicAndShift(int divisor, int& magic, int& shift)) {
  // It does not make sense to calculate magic and shift for zero divisor.
  DCHECK_NE(divisor, 0);

  /* According to H.S.Warren's Hacker's Delight Chapter 10 and
   * T,Grablund, P.L.Montogomery's Division by invariant integers using multiplication.
   * The magic number M and shift S can be calculated in the following way:
   * Let nc be the most positive value of numerator(n) such that nc = kd - 1,
   * where divisor(d) >=2.
   * Let nc be the most negative value of numerator(n) such that nc = kd + 1,
   * where divisor(d) <= -2.
   * Thus nc can be calculated like:
   * nc = 2^31 + 2^31 % d - 1, where d >= 2
   * nc = -2^31 + (2^31 + 1) % d, where d >= 2.
   *
   * So the shift p is the smallest p satisfying
   * 2^p > nc * (d - 2^p % d), where d >= 2
   * 2^p > nc * (d + 2^p % d), where d <= -2.
   *
   * the magic number M is calcuated by
   * M = (2^p + d - 2^p % d) / d, where d >= 2
   * M = (2^p - d - 2^p % d) / d, where d <= -2.
   *
   * Notice that p is always bigger than or equal to 32, so we just return 32-p as
   * the shift number S.
   */

  int32_t p = 31;
  const uint32_t two31 = 0x80000000U;

  // Initialize the computations.
  uint32_t abs_d = (divisor >= 0) ? divisor : -divisor;
  uint32_t tmp = two31 + (static_cast<uint32_t>(divisor) >> 31);
  uint32_t abs_nc = tmp - 1 - tmp % abs_d;
  uint32_t quotient1 = two31 / abs_nc;
  uint32_t remainder1 = two31 % abs_nc;
  uint32_t quotient2 = two31 / abs_d;
  uint32_t remainder2 = two31 % abs_d;

  /*
   * To avoid handling both positive and negative divisor, Hacker's Delight
   * introduces a method to handle these 2 cases together to avoid duplication.
   */
  uint32_t delta;
  do {
    p++;
    quotient1 = 2 * quotient1;
    remainder1 = 2 * remainder1;
    if (remainder1 >= abs_nc) {
      quotient1++;
      remainder1 = remainder1 - abs_nc;
    }
    quotient2 = 2 * quotient2;
    remainder2 = 2 * remainder2;
    if (remainder2 >= abs_d) {
      quotient2++;
      remainder2 = remainder2 - abs_d;
    }
    delta = abs_d - remainder2;
  } while (quotient1 < delta || (quotient1 == delta && remainder1 == 0));

  magic = (divisor > 0) ? (quotient2 + 1) : (-quotient2 - 1);
  shift = p - 32;
}

X86MIR2LIR(RegLocation, GenDivRemLit(RegLocation rl_dest, RegStorage reg_lo, int lit, bool is_div)) {
  LOG(FATAL) << "Unexpected use of GenDivRemLit for x86";
  return rl_dest;
}

X86MIR2LIR(RegLocation, GenDivRemLit(RegLocation rl_dest, RegLocation rl_src,
                                    int imm, bool is_div)) {
  // Use a multiply (and fixup) to perform an int div/rem by a constant.

  // We have to use fixed registers, so flush all the temps.
  this->FlushAllRegs();
  this->LockCallTemps();  // Prepare for explicit register usage.

  // Assume that the result will be in EDX.
  RegLocation rl_result = {kLocPhysReg, 0, 0, 0, 0, 0, 0, 0, 1, rs_r2, INVALID_SREG, INVALID_SREG};

  // handle div/rem by 1 special case.
  if (imm == 1) {
    if (is_div) {
      // x / 1 == x.
      this->StoreValue(rl_result, rl_src);
    } else {
      // x % 1 == 0.
      this->LoadConstantNoClobber(rs_r0, 0);
      // For this case, return the result in EAX.
      rl_result.reg.SetReg(r0);
    }
  } else if (imm == -1) {  // handle 0x80000000 / -1 special case.
    if (is_div) {
      LIR *minint_branch = 0;
      this->LoadValueDirectFixed(rl_src, rs_r0);
      OpRegImm(kOpCmp, rs_r0, 0x80000000);
      minint_branch = this->NewLIR2(kX86Jcc8, 0, kX86CondEq);

      // for x != MIN_INT, x / -1 == -x.
      this->NewLIR1(kX86Neg32R, r0);

      LIR* branch_around = this->NewLIR1(kX86Jmp8, 0);
      // The target for cmp/jmp above.
      minint_branch->target = this->NewLIR0(kPseudoTargetLabel);
      // EAX already contains the right value (0x80000000),
      branch_around->target = this->NewLIR0(kPseudoTargetLabel);
    } else {
      // x % -1 == 0.
      this->LoadConstantNoClobber(rs_r0, 0);
    }
    // For this case, return the result in EAX.
    rl_result.reg.SetReg(r0);
  } else {
    CHECK(imm <= -2 || imm >= 2);
    // Use H.S.Warren's Hacker's Delight Chapter 10 and
    // T,Grablund, P.L.Montogomery's Division by invariant integers using multiplication.
    int magic, shift;
    CalculateMagicAndShift(imm, magic, shift);

    /*
     * For imm >= 2,
     *     int(n/imm) = floor(n/imm) = floor(M*n/2^S), while n > 0
     *     int(n/imm) = ceil(n/imm) = floor(M*n/2^S) +1, while n < 0.
     * For imm <= -2,
     *     int(n/imm) = ceil(n/imm) = floor(M*n/2^S) +1 , while n > 0
     *     int(n/imm) = floor(n/imm) = floor(M*n/2^S), while n < 0.
     * We implement this algorithm in the following way:
     * 1. multiply magic number m and numerator n, get the higher 32bit result in EDX
     * 2. if imm > 0 and magic < 0, add numerator to EDX
     *    if imm < 0 and magic > 0, sub numerator from EDX
     * 3. if S !=0, SAR S bits for EDX
     * 4. add 1 to EDX if EDX < 0
     * 5. Thus, EDX is the quotient
     */

    // Numerator into EAX.
    RegStorage numerator_reg;
    if (!is_div || (imm > 0 && magic < 0) || (imm < 0 && magic > 0)) {
      // We will need the value later.
      if (rl_src.location == kLocPhysReg) {
        // We can use it directly.
        DCHECK(rl_src.reg.GetReg() != rs_r0.GetReg() && rl_src.reg.GetReg() != rs_r2.GetReg());
        numerator_reg = rl_src.reg;
      } else {
        numerator_reg = rs_r1;
        this->LoadValueDirectFixed(rl_src, numerator_reg);
      }
      OpRegCopy(rs_r0, numerator_reg);
    } else {
      // Only need this once.  Just put it into EAX.
      this->LoadValueDirectFixed(rl_src, rs_r0);
    }

    // EDX = magic.
    LoadConstantNoClobber(rs_r2, magic);

    // EDX:EAX = magic & dividend.
    this->NewLIR1(kX86Imul32DaR, rs_r2.GetReg());

    if (imm > 0 && magic < 0) {
      // Add numerator to EDX.
      DCHECK(numerator_reg.Valid());
      this->NewLIR2(kX86Add32RR, rs_r2.GetReg(), numerator_reg.GetReg());
    } else if (imm < 0 && magic > 0) {
      DCHECK(numerator_reg.Valid());
      this->NewLIR2(kX86Sub32RR, rs_r2.GetReg(), numerator_reg.GetReg());
    }

    // Do we need the shift?
    if (shift != 0) {
      // Shift EDX by 'shift' bits.
      this->NewLIR2(kX86Sar32RI, rs_r2.GetReg(), shift);
    }

    // Add 1 to EDX if EDX < 0.

    // Move EDX to EAX.
    OpRegCopy(rs_r0, rs_r2);

    // Move sign bit to bit 0, zeroing the rest.
    this->NewLIR2(kX86Shr32RI, rs_r2.GetReg(), 31);

    // EDX = EDX + EAX.
    this->NewLIR2(kX86Add32RR, rs_r2.GetReg(), rs_r0.GetReg());

    // Quotient is in EDX.
    if (!is_div) {
      // We need to compute the remainder.
      // Remainder is divisor - (quotient * imm).
      DCHECK(numerator_reg.Valid());
      OpRegCopy(rs_r0, numerator_reg);

      // EAX = numerator * imm.
      OpRegRegImm(kOpMul, rs_r2, rs_r2, imm);

      // EDX -= EAX.
      this->NewLIR2(kX86Sub32RR, rs_r0.GetReg(), rs_r2.GetReg());

      // For this case, return the result in EAX.
      rl_result.reg.SetReg(r0);
    }
  }

  return rl_result;
}

X86MIR2LIR(RegLocation, GenDivRem(RegLocation rl_dest, RegStorage reg_lo, RegStorage reg_hi,
                                 bool is_div)) {
  LOG(FATAL) << "Unexpected use of GenDivRem for x86";
  return rl_dest;
}

X86MIR2LIR(RegLocation, GenDivRem(RegLocation rl_dest, RegLocation rl_src1,
                                 RegLocation rl_src2, bool is_div, bool check_zero)) {
  // We have to use fixed registers, so flush all the temps.
  this->FlushAllRegs();
  this->LockCallTemps();  // Prepare for explicit register usage.

  // Load LHS into EAX.
  this->LoadValueDirectFixed(rl_src1, rs_r0);

  // Load RHS into EBX.
  this->LoadValueDirectFixed(rl_src2, rs_r1);

  // Copy LHS sign bit into EDX.
  this->NewLIR0(kx86Cdq32Da);

  if (check_zero) {
    // Handle division by zero case.
    this->GenDivZeroCheck(rs_r1);
  }

  // Have to catch 0x80000000/-1 case, or we will get an exception!
  OpRegImm(kOpCmp, rs_r1, -1);
  LIR *minus_one_branch = this->NewLIR2(kX86Jcc8, 0, kX86CondNe);

  // RHS is -1.
  OpRegImm(kOpCmp, rs_r0, 0x80000000);
  LIR * minint_branch = this->NewLIR2(kX86Jcc8, 0, kX86CondNe);

  // In 0x80000000/-1 case.
  if (!is_div) {
    // For DIV, EAX is already right. For REM, we need EDX 0.
    LoadConstantNoClobber(rs_r2, 0);
  }
  LIR* done = this->NewLIR1(kX86Jmp8, 0);

  // Expected case.
  minus_one_branch->target = this->NewLIR0(kPseudoTargetLabel);
  minint_branch->target = minus_one_branch->target;
  this->NewLIR1(kX86Idivmod32DaR, rs_r1.GetReg());
  done->target = this->NewLIR0(kPseudoTargetLabel);

  // Result is in EAX for div and EDX for rem.
  RegLocation rl_result = {kLocPhysReg, 0, 0, 0, 0, 0, 0, 0, 1, rs_r0, INVALID_SREG, INVALID_SREG};
  if (!is_div) {
    rl_result.reg.SetReg(r2);
  }
  return rl_result;
}

X86MIR2LIR(bool, GenInlinedMinMaxInt(CallInfo* info, bool is_min)) {
  DCHECK(this->cu_->instruction_set == kX86 || this->cu_->instruction_set == kX86_64);

  // Get the two arguments to the invoke and place them in GP registers.
  RegLocation rl_src1 = info->args[0];
  RegLocation rl_src2 = info->args[1];
  rl_src1 = this->LoadValue(rl_src1, kCoreReg);
  rl_src2 = this->LoadValue(rl_src2, kCoreReg);

  RegLocation rl_dest = this->InlineTarget(info);
  RegLocation rl_result = this->EvalLoc(rl_dest, kCoreReg, true);

  /*
   * If the result register is the same as the second element, then we need to be careful.
   * The reason is that the first copy will inadvertently clobber the second element with
   * the first one thus yielding the wrong result. Thus we do a swap in that case.
   */
  if (rl_result.reg.GetReg() == rl_src2.reg.GetReg()) {
    std::swap(rl_src1, rl_src2);
  }

  // Pick the first integer as min/max.
  OpRegCopy(rl_result.reg, rl_src1.reg);

  // If the integers are both in the same register, then there is nothing else to do
  // because they are equal and we have already moved one into the result.
  if (rl_src1.reg.GetReg() != rl_src2.reg.GetReg()) {
    // It is possible we didn't pick correctly so do the actual comparison now.
    OpRegReg(kOpCmp, rl_src1.reg, rl_src2.reg);

    // Conditionally move the other integer into the destination register.
    ConditionCode condition_code = is_min ? kCondGt : kCondLt;
    OpCondRegReg(kOpCmov, condition_code, rl_result.reg, rl_src2.reg);
  }

  this->StoreValue(rl_dest, rl_result);
  return true;
}

X86MIR2LIR(bool, GenInlinedPeek(CallInfo* info, OpSize size)) {
  RegLocation rl_src_address = info->args[0];  // long address
  rl_src_address = this->NarrowRegLoc(rl_src_address);  // ignore high half in info->args[1]
  RegLocation rl_dest = size == k64 ? this->InlineTargetWide(info) : this->InlineTarget(info);
  RegLocation rl_address = this->LoadValue(rl_src_address, kCoreReg);
  RegLocation rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
  // Unaligned access is allowed on x86.
  LoadBaseDisp(rl_address.reg, 0, rl_result.reg, size);
  if (size == k64) {
    this->StoreValueWide(rl_dest, rl_result);
  } else {
    DCHECK(size == kSignedByte || size == kSignedHalf || size == k32);
    this->StoreValue(rl_dest, rl_result);
  }
  return true;
}

X86MIR2LIR(bool, GenInlinedPoke(CallInfo* info, OpSize size)) {
  RegLocation rl_src_address = info->args[0];  // long address
  rl_src_address = this->NarrowRegLoc(rl_src_address);  // ignore high half in info->args[1]
  RegLocation rl_src_value = info->args[2];  // [size] value
  RegLocation rl_address = this->LoadValue(rl_src_address, kCoreReg);
  if (size == k64) {
    // Unaligned access is allowed on x86.
    RegLocation rl_value = this->LoadValueWide(rl_src_value, kCoreReg);
    StoreBaseDisp(rl_address.reg, 0, rl_value.reg, size);
  } else {
    DCHECK(size == kSignedByte || size == kSignedHalf || size == k32);
    // Unaligned access is allowed on x86.
    RegLocation rl_value = this->LoadValue(rl_src_value, kCoreReg);
    StoreBaseDisp(rl_address.reg, 0, rl_value.reg, size);
  }
  return true;
}

X86MIR2LIR(void, OpLea(RegStorage r_base, RegStorage reg1, RegStorage reg2, int scale, int offset)) {
  this->NewLIR5(kX86Lea32RA, r_base.GetReg(), reg1.GetReg(), reg2.GetReg(), scale, offset);
}

template <size_t pointer_size>
void X86Mir2Lir<pointer_size>::OpTlsCmp(ThreadOffset<pointer_size> offset, int val) {
  this->NewLIR2(kX86Cmp16TI8, offset.Int32Value(), val);
}
template void X86Mir2Lir<4>::OpTlsCmp(ThreadOffset<4> offset, int val);
template void X86Mir2Lir<8>::OpTlsCmp(ThreadOffset<8> offset, int val);

template <size_t pointer_size>
static bool IsInReg(X86Mir2Lir<pointer_size> *pMir2Lir, const RegLocation &rl, RegStorage reg) {
  return rl.reg.Valid() && rl.reg.GetReg() == reg.GetReg() && (pMir2Lir->IsLive(reg) || rl.home);
}

X86MIR2LIR(bool, GenInlinedCas(CallInfo* info, bool is_long, bool is_object)) {
  DCHECK(this->cu_->instruction_set == kX86 || this->cu_->instruction_set == kX86_64);
  // Unused - RegLocation rl_src_unsafe = info->args[0];
  RegLocation rl_src_obj = info->args[1];  // Object - known non-null
  RegLocation rl_src_offset = info->args[2];  // long low
  rl_src_offset = this->NarrowRegLoc(rl_src_offset);  // ignore high half in info->args[3]
  RegLocation rl_src_expected = info->args[4];  // int, long or Object
  // If is_long, high half is in info->args[5]
  RegLocation rl_src_new_value = info->args[is_long ? 6 : 5];  // int, long or Object
  // If is_long, high half is in info->args[7]

  if (is_long) {
    // TODO: avoid unnecessary loads of SI and DI when the values are in registers.
    // TODO: CFI support.
    this->FlushAllRegs();
    this->LockCallTemps();
    RegStorage r_tmp1 = RegStorage::MakeRegPair(rs_rAX, rs_rDX);
    RegStorage r_tmp2 = RegStorage::MakeRegPair(rs_rBX, rs_rCX);
    this->LoadValueDirectWideFixed(rl_src_expected, r_tmp1);
    this->LoadValueDirectWideFixed(rl_src_new_value, r_tmp2);
    this->NewLIR1(kX86Push32R, rs_rDI.GetReg());
    this->MarkTemp(rs_rDI);
    this->LockTemp(rs_rDI);
    this->NewLIR1(kX86Push32R, rs_rSI.GetReg());
    this->MarkTemp(rs_rSI);
    this->LockTemp(rs_rSI);
    const int push_offset = 4 /* push edi */ + 4 /* push esi */;
    int srcObjSp = IsInReg(this, rl_src_obj, rs_rSI) ? 0
                : (IsInReg(this, rl_src_obj, rs_rDI) ? 4
                : (this->SRegOffset(rl_src_obj.s_reg_low) + push_offset));
    // FIXME: needs 64-bit update.
    this->LoadWordDisp(TargetReg(kSp), srcObjSp, rs_rDI);
    int srcOffsetSp = IsInReg(this, rl_src_offset, rs_rSI) ? 0
                   : (IsInReg(this, rl_src_offset, rs_rDI) ? 4
                   : (this->SRegOffset(rl_src_offset.s_reg_low) + push_offset));
    this->LoadWordDisp(TargetReg(kSp), srcOffsetSp, rs_rSI);
    this->NewLIR4(kX86LockCmpxchg8bA, rs_rDI.GetReg(), rs_rSI.GetReg(), 0, 0);

    // After a store we need to insert barrier in case of potential load. Since the
    // locked cmpxchg has full barrier semantics, only a scheduling barrier will be generated.
    GenMemBarrier(kStoreLoad);

    this->FreeTemp(rs_rSI);
    this->UnmarkTemp(rs_rSI);
    this->NewLIR1(kX86Pop32R, rs_rSI.GetReg());
    this->FreeTemp(rs_rDI);
    this->UnmarkTemp(rs_rDI);
    this->NewLIR1(kX86Pop32R, rs_rDI.GetReg());
    FreeCallTemps();
  } else {
    // EAX must hold expected for CMPXCHG. Neither rl_new_value, nor r_ptr may be in EAX.
    this->FlushReg(rs_r0);
    this->Clobber(rs_r0);
    this->LockTemp(rs_r0);

    RegLocation rl_object = this->LoadValue(rl_src_obj, kCoreReg);
    RegLocation rl_new_value = this->LoadValue(rl_src_new_value, kCoreReg);

    if (is_object && !this->mir_graph_->IsConstantNullRef(rl_new_value)) {
      // Mark card for object assuming new value is stored.
      this->FreeTemp(rs_r0);  // Temporarily release EAX for MarkGCCard().
      this->MarkGCCard(rl_new_value.reg, rl_object.reg);
      this->LockTemp(rs_r0);
    }

    RegLocation rl_offset = this->LoadValue(rl_src_offset, kCoreReg);
    this->LoadValueDirect(rl_src_expected, rs_r0);
    this->NewLIR5(kX86LockCmpxchgAR, rl_object.reg.GetReg(), rl_offset.reg.GetReg(), 0, 0,
                  rl_new_value.reg.GetReg());

    // After a store we need to insert barrier in case of potential load. Since the
    // locked cmpxchg has full barrier semantics, only a scheduling barrier will be generated.
    GenMemBarrier(kStoreLoad);

    this->FreeTemp(rs_r0);
  }

  // Convert ZF to boolean
  RegLocation rl_dest = this->InlineTarget(info);  // boolean place for result
  RegLocation rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
  this->NewLIR2(kX86Set8R, rl_result.reg.GetReg(), kX86CondZ);
  this->NewLIR2(kX86Movzx8RR, rl_result.reg.GetReg(), rl_result.reg.GetReg());
  this->StoreValue(rl_dest, rl_result);
  return true;
}

X86MIR2LIR(LIR*, OpPcRelLoad(RegStorage reg, LIR* target)) {
  CHECK(base_of_code_ != nullptr);

  // Address the start of the method
  RegLocation rl_method = this->mir_graph_->GetRegLocation(base_of_code_->s_reg_low);
  this->LoadValueDirectFixed(rl_method, reg);
  store_method_addr_used_ = true;

  // Load the proper value from the literal area.
  // We don't know the proper offset for the value, so pick one that will force
  // 4 byte offset.  We will fix this up in the assembler later to have the right
  // value.
  LIR *res = this->RawLIR(this->current_dalvik_offset_, kX86Mov32RM, reg.GetReg(), reg.GetReg(), 256,
                          0, 0, target);
  res->target = target;
  res->flags.fixup = kFixupLoad;
  this->SetMemRefType(res, true, kLiteral);
  store_method_addr_used_ = true;
  return res;
}

X86MIR2LIR(LIR*, OpVldm(RegStorage r_base, int count)) {
  LOG(FATAL) << "Unexpected use of OpVldm for x86";
  return NULL;
}

X86MIR2LIR(LIR*, OpVstm(RegStorage r_base, int count)) {
  LOG(FATAL) << "Unexpected use of OpVstm for x86";
  return NULL;
}

X86MIR2LIR(void, GenMultiplyByTwoBitMultiplier(RegLocation rl_src,
                                              RegLocation rl_result, int lit,
                                              int first_bit, int second_bit)) {
  RegStorage t_reg = this->AllocTemp();
  OpRegRegImm(kOpLsl, t_reg, rl_src.reg, second_bit - first_bit);
  OpRegRegReg(kOpAdd, rl_result.reg, rl_src.reg, t_reg);
  this->FreeTemp(t_reg);
  if (first_bit != 0) {
    OpRegRegImm(kOpLsl, rl_result.reg, rl_result.reg, first_bit);
  }
}

X86MIR2LIR(void, GenDivZeroCheckWide(RegStorage reg)) {
  DCHECK(reg.IsPair());  // TODO: allow 64BitSolo.
  // We are not supposed to clobber the incoming storage, so allocate a temporary.
  RegStorage t_reg = this->AllocTemp();

  // Doing an OR is a quick way to check if both registers are zero. This will set the flags.
  OpRegRegReg(kOpOr, t_reg, reg.GetLow(), reg.GetHigh());

  // In case of zero, throw ArithmeticException.
  this->GenDivZeroCheck(kCondEq);

  // The temp is no longer needed so free it at this time.
  this->FreeTemp(t_reg);
}

X86MIR2LIR(void, GenArrayBoundsCheck(RegStorage index,
                                    RegStorage array_base,
                                    int len_offset)) {
  class ArrayBoundsCheckSlowPath : public Mir2Lir<pointer_size>::LIRSlowPath {
   public:
    ArrayBoundsCheckSlowPath(Mir2Lir<pointer_size>* m2l, LIR* branch,
                             RegStorage index, RegStorage array_base, int32_t len_offset)
        : Mir2Lir<pointer_size>::LIRSlowPath(m2l, m2l->GetCurrentDexPc(), branch),
          index_(index), array_base_(array_base), len_offset_(len_offset) {
    }

    void Compile() OVERRIDE {
      this->m2l_->ResetRegPool();
      this->m2l_->ResetDefTracking();
      this->GenerateTargetLabel(kPseudoThrowTarget);

      RegStorage new_index = index_;
      // Move index out of kArg1, either directly to kArg0, or to kArg2.
      if (index_.GetReg() == this->m2l_->TargetReg(kArg1).GetReg()) {
        if (array_base_.GetReg() == this->m2l_->TargetReg(kArg0).GetReg()) {
          this->m2l_->OpRegCopy(this->m2l_->TargetReg(kArg2), index_);
          new_index = this->m2l_->TargetReg(kArg2);
        } else {
          this->m2l_->OpRegCopy(this->m2l_->TargetReg(kArg0), index_);
          new_index = this->m2l_->TargetReg(kArg0);
        }
      }
      // Load array length to kArg1.
      this->m2l_->OpRegMem(kOpMov, this->m2l_->TargetReg(kArg1), array_base_, len_offset_);
      this->m2l_->CallRuntimeHelperRegReg(QUICK_ENTRYPOINT_OFFSET(pointer_size,
                                                                        pThrowArrayBounds),
                                          new_index, this->m2l_->TargetReg(kArg1), true);
    }

   private:
    const RegStorage index_;
    const RegStorage array_base_;
    const int32_t len_offset_;
  };

  OpRegMem(kOpCmp, index, array_base, len_offset);
  LIR* branch = OpCondBranch(kCondUge, nullptr);
  this->AddSlowPath(new (this->arena_) ArrayBoundsCheckSlowPath(this, branch,
                                                                index, array_base, len_offset));
}

X86MIR2LIR(void, GenArrayBoundsCheck(int32_t index,
                                    RegStorage array_base,
                                    int32_t len_offset)) {
  class ArrayBoundsCheckSlowPath : public Mir2Lir<pointer_size>::LIRSlowPath {
   public:
    ArrayBoundsCheckSlowPath(Mir2Lir<pointer_size>* m2l, LIR* branch,
                             int32_t index, RegStorage array_base, int32_t len_offset)
        : Mir2Lir<pointer_size>::LIRSlowPath(m2l, m2l->GetCurrentDexPc(), branch),
          index_(index), array_base_(array_base), len_offset_(len_offset) {
    }

    void Compile() OVERRIDE {
      this->m2l_->ResetRegPool();
      this->m2l_->ResetDefTracking();
      this->GenerateTargetLabel(kPseudoThrowTarget);

      // Load array length to kArg1.
      this->m2l_->OpRegMem(kOpMov, this->m2l_->TargetReg(kArg1), array_base_, len_offset_);
      this->m2l_->LoadConstant(this->m2l_->TargetReg(kArg0), index_);
      this->m2l_->CallRuntimeHelperRegReg(QUICK_ENTRYPOINT_OFFSET(pointer_size, pThrowArrayBounds),
                                          this->m2l_->TargetReg(kArg0),
                                          this->m2l_->TargetReg(kArg1), true);
    }

   private:
    const int32_t index_;
    const RegStorage array_base_;
    const int32_t len_offset_;
  };

  this->NewLIR3(IS_SIMM8(index) ? kX86Cmp32MI8 : kX86Cmp32MI, array_base.GetReg(), len_offset, index);
  LIR* branch = OpCondBranch(kCondLs, nullptr);
  this->AddSlowPath(new (this->arena_) ArrayBoundsCheckSlowPath(this, branch,
                                                                index, array_base, len_offset));
}

// Test suspend flag, return target of taken suspend branch
X86MIR2LIR(LIR*, OpTestSuspend(LIR* target)) {
  OpTlsCmp(Thread::ThreadFlagsOffset<pointer_size>(), 0);
  return OpCondBranch((target == NULL) ? kCondNe : kCondEq, target);
}

// Decrement register and branch on condition
X86MIR2LIR(LIR*, OpDecAndBranch(ConditionCode c_code, RegStorage reg, LIR* target)) {
  OpRegImm(kOpSub, reg, 1);
  return OpCondBranch(c_code, target);
}

X86MIR2LIR(bool, SmallLiteralDivRem(Instruction::Code dalvik_opcode, bool is_div,
                                   RegLocation rl_src, RegLocation rl_dest, int lit)) {
  LOG(FATAL) << "Unexpected use of smallLiteralDive in x86";
  return false;
}

X86MIR2LIR(bool, EasyMultiply(RegLocation rl_src, RegLocation rl_dest, int lit)) {
  LOG(FATAL) << "Unexpected use of easyMultiply in x86";
  return false;
}

X86MIR2LIR(LIR*, OpIT(ConditionCode cond, const char* guide)) {
  LOG(FATAL) << "Unexpected use of OpIT in x86";
  return NULL;
}

X86MIR2LIR(void, OpEndIT(LIR* it)) {
  LOG(FATAL) << "Unexpected use of OpEndIT in x86";
}

X86MIR2LIR(void, GenImulRegImm(RegStorage dest, RegStorage src, int val)) {
  switch (val) {
    case 0:
      this->NewLIR2(kX86Xor32RR, dest.GetReg(), dest.GetReg());
      break;
    case 1:
      OpRegCopy(dest, src);
      break;
    default:
      OpRegRegImm(kOpMul, dest, src, val);
      break;
  }
}

X86MIR2LIR(void, GenImulMemImm(RegStorage dest, int sreg, int displacement, int val)) {
  LIR *m;
  switch (val) {
    case 0:
      this->NewLIR2(kX86Xor32RR, dest.GetReg(), dest.GetReg());
      break;
    case 1:
      LoadBaseDisp(rs_rX86_SP, displacement, dest, k32);
      break;
    default:
      m = this->NewLIR4(IS_SIMM8(val) ? kX86Imul32RMI8 : kX86Imul32RMI, dest.GetReg(),
                        rs_rX86_SP.GetReg(), displacement, val);
      this->AnnotateDalvikRegAccess(m, displacement >> 2, true /* is_load */, true /* is_64bit */);
      break;
  }
}

X86MIR2LIR(void, GenMulLong(Instruction::Code, RegLocation rl_dest, RegLocation rl_src1,
                           RegLocation rl_src2)) {
  if (rl_src1.is_const) {
    std::swap(rl_src1, rl_src2);
  }
  // Are we multiplying by a constant?
  if (rl_src2.is_const) {
    // Do special compare/branch against simple const operand
    int64_t val = this->mir_graph_->ConstantValueWide(rl_src2);
    if (val == 0) {
      RegLocation rl_result = this->EvalLocWide(rl_dest, kCoreReg, true);
      OpRegReg(kOpXor, rl_result.reg.GetLow(), rl_result.reg.GetLow());
      OpRegReg(kOpXor, rl_result.reg.GetHigh(), rl_result.reg.GetHigh());
      this->StoreValueWide(rl_dest, rl_result);
      return;
    } else if (val == 1) {
      this->StoreValueWide(rl_dest, rl_src1);
      return;
    } else if (val == 2) {
      GenAddLong(Instruction::ADD_LONG, rl_dest, rl_src1, rl_src1);
      return;
    } else if (IsPowerOfTwo(val)) {
      int shift_amount = this->LowestSetBit(val);
      if (!this->BadOverlap(rl_src1, rl_dest)) {
        rl_src1 = this->LoadValueWide(rl_src1, kCoreReg);
        RegLocation rl_result = GenShiftImmOpLong(Instruction::SHL_LONG, rl_dest,
                                                  rl_src1, shift_amount);
        this->StoreValueWide(rl_dest, rl_result);
        return;
      }
    }

    // Okay, just bite the bullet and do it.
    int32_t val_lo = Low32Bits(val);
    int32_t val_hi = High32Bits(val);
    this->FlushAllRegs();
    this->LockCallTemps();  // Prepare for explicit register usage.
    rl_src1 = this->UpdateLocWide(rl_src1);
    bool src1_in_reg = rl_src1.location == kLocPhysReg;
    int displacement = this->SRegOffset(rl_src1.s_reg_low);

    // ECX <- 1H * 2L
    // EAX <- 1L * 2H
    if (src1_in_reg) {
      GenImulRegImm(rs_r1, rl_src1.reg.GetHigh(), val_lo);
      GenImulRegImm(rs_r0, rl_src1.reg.GetLow(), val_hi);
    } else {
      GenImulMemImm(rs_r1, this->GetSRegHi(rl_src1.s_reg_low), displacement + HIWORD_OFFSET, val_lo);
      GenImulMemImm(rs_r0, rl_src1.s_reg_low, displacement + LOWORD_OFFSET, val_hi);
    }

    // ECX <- ECX + EAX  (2H * 1L) + (1H * 2L)
    this->NewLIR2(kX86Add32RR, rs_r1.GetReg(), rs_r0.GetReg());

    // EAX <- 2L
    LoadConstantNoClobber(rs_r0, val_lo);

    // EDX:EAX <- 2L * 1L (double precision)
    if (src1_in_reg) {
      this->NewLIR1(kX86Mul32DaR, rl_src1.reg.GetLowReg());
    } else {
      LIR *m = this->NewLIR2(kX86Mul32DaM, rs_rX86_SP.GetReg(), displacement + LOWORD_OFFSET);
      this->AnnotateDalvikRegAccess(m, (displacement + LOWORD_OFFSET) >> 2,
                                    true /* is_load */, true /* is_64bit */);
    }

    // EDX <- EDX + ECX (add high words)
    this->NewLIR2(kX86Add32RR, rs_r2.GetReg(), rs_r1.GetReg());

    // Result is EDX:EAX
    RegLocation rl_result = {kLocPhysReg, 1, 0, 0, 0, 0, 0, 0, 1,
                             RegStorage::MakeRegPair(rs_r0, rs_r2), INVALID_SREG, INVALID_SREG};
    this->StoreValueWide(rl_dest, rl_result);
    return;
  }

  // Nope.  Do it the hard way
  // Check for V*V.  We can eliminate a multiply in that case, as 2L*1H == 2H*1L.
  bool is_square = this->mir_graph_->SRegToVReg(rl_src1.s_reg_low) ==
                   this->mir_graph_->SRegToVReg(rl_src2.s_reg_low);

  this->FlushAllRegs();
  this->LockCallTemps();  // Prepare for explicit register usage.
  rl_src1 = this->UpdateLocWide(rl_src1);
  rl_src2 = this->UpdateLocWide(rl_src2);

  // At this point, the VRs are in their home locations.
  bool src1_in_reg = rl_src1.location == kLocPhysReg;
  bool src2_in_reg = rl_src2.location == kLocPhysReg;

  // ECX <- 1H
  if (src1_in_reg) {
    this->NewLIR2(kX86Mov32RR, rs_r1.GetReg(), rl_src1.reg.GetHighReg());
  } else {
    LoadBaseDisp(rs_rX86_SP, this->SRegOffset(rl_src1.s_reg_low) + HIWORD_OFFSET, rs_r1, k32);
  }

  if (is_square) {
    // Take advantage of the fact that the values are the same.
    // ECX <- ECX * 2L  (1H * 2L)
    if (src2_in_reg) {
      this->NewLIR2(kX86Imul32RR, rs_r1.GetReg(), rl_src2.reg.GetLowReg());
    } else {
      int displacement = this->SRegOffset(rl_src2.s_reg_low);
      LIR *m = this->NewLIR3(kX86Imul32RM, rs_r1.GetReg(), rs_rX86_SP.GetReg(),
                             displacement + LOWORD_OFFSET);
      this->AnnotateDalvikRegAccess(m, (displacement + LOWORD_OFFSET) >> 2,
                                    true /* is_load */, true /* is_64bit */);
    }

    // ECX <- 2*ECX (2H * 1L) + (1H * 2L)
    this->NewLIR2(kX86Add32RR, rs_r1.GetReg(), rs_r1.GetReg());
  } else {
    // EAX <- 2H
    if (src2_in_reg) {
      this->NewLIR2(kX86Mov32RR, rs_r0.GetReg(), rl_src2.reg.GetHighReg());
    } else {
      LoadBaseDisp(rs_rX86_SP, this->SRegOffset(rl_src2.s_reg_low) + HIWORD_OFFSET, rs_r0, k32);
    }

    // EAX <- EAX * 1L  (2H * 1L)
    if (src1_in_reg) {
      this->NewLIR2(kX86Imul32RR, rs_r0.GetReg(), rl_src1.reg.GetLowReg());
    } else {
      int displacement = this->SRegOffset(rl_src1.s_reg_low);
      LIR *m = this->NewLIR3(kX86Imul32RM, rs_r0.GetReg(), rs_rX86_SP.GetReg(),
                             displacement + LOWORD_OFFSET);
      this->AnnotateDalvikRegAccess(m, (displacement + LOWORD_OFFSET) >> 2,
                                    true /* is_load */, true /* is_64bit */);
    }

    // ECX <- ECX * 2L  (1H * 2L)
    if (src2_in_reg) {
      this->NewLIR2(kX86Imul32RR, rs_r1.GetReg(), rl_src2.reg.GetLowReg());
    } else {
      int displacement = this->SRegOffset(rl_src2.s_reg_low);
      LIR *m = this->NewLIR3(kX86Imul32RM, rs_r1.GetReg(), rs_rX86_SP.GetReg(),
                             displacement + LOWORD_OFFSET);
      this->AnnotateDalvikRegAccess(m, (displacement + LOWORD_OFFSET) >> 2,
                                    true /* is_load */, true /* is_64bit */);
    }

    // ECX <- ECX + EAX  (2H * 1L) + (1H * 2L)
    this->NewLIR2(kX86Add32RR, rs_r1.GetReg(), rs_r0.GetReg());
  }

  // EAX <- 2L
  if (src2_in_reg) {
    this->NewLIR2(kX86Mov32RR, rs_r0.GetReg(), rl_src2.reg.GetLowReg());
  } else {
    LoadBaseDisp(rs_rX86_SP, this->SRegOffset(rl_src2.s_reg_low) + LOWORD_OFFSET, rs_r0, k32);
  }

  // EDX:EAX <- 2L * 1L (double precision)
  if (src1_in_reg) {
    this->NewLIR1(kX86Mul32DaR, rl_src1.reg.GetLowReg());
  } else {
    int displacement = this->SRegOffset(rl_src1.s_reg_low);
    LIR *m = this->NewLIR2(kX86Mul32DaM, rs_rX86_SP.GetReg(), displacement + LOWORD_OFFSET);
    this->AnnotateDalvikRegAccess(m, (displacement + LOWORD_OFFSET) >> 2,
                                  true /* is_load */, true /* is_64bit */);
  }

  // EDX <- EDX + ECX (add high words)
  this->NewLIR2(kX86Add32RR, rs_r2.GetReg(), rs_r1.GetReg());

  // Result is EDX:EAX
  RegLocation rl_result = {kLocPhysReg, 1, 0, 0, 0, 0, 0, 0, 1,
                           RegStorage::MakeRegPair(rs_r0, rs_r2), INVALID_SREG, INVALID_SREG};
  this->StoreValueWide(rl_dest, rl_result);
}

X86MIR2LIR(void, GenLongRegOrMemOp(RegLocation rl_dest, RegLocation rl_src,
                                  Instruction::Code op)) {
  DCHECK_EQ(rl_dest.location, kLocPhysReg);
  X86OpCode x86op = GetOpcode(op, rl_dest, rl_src, false);
  if (rl_src.location == kLocPhysReg) {
    // Both operands are in registers.
    // But we must ensure that rl_src is in pair
    rl_src = this->EvalLocWide(rl_src, kCoreReg, true);
    if (rl_dest.reg.GetLowReg() == rl_src.reg.GetHighReg()) {
      // The registers are the same, so we would clobber it before the use.
      RegStorage temp_reg = this->AllocTemp();
      OpRegCopy(temp_reg, rl_dest.reg);
      rl_src.reg.SetHighReg(temp_reg.GetReg());
    }
    this->NewLIR2(x86op, rl_dest.reg.GetLowReg(), rl_src.reg.GetLowReg());

    x86op = GetOpcode(op, rl_dest, rl_src, true);
    this->NewLIR2(x86op, rl_dest.reg.GetHighReg(), rl_src.reg.GetHighReg());
    this->FreeTemp(rl_src.reg);
    return;
  }

  // RHS is in memory.
  DCHECK((rl_src.location == kLocDalvikFrame) ||
         (rl_src.location == kLocCompilerTemp));
  int r_base = TargetReg(kSp).GetReg();
  int displacement = this->SRegOffset(rl_src.s_reg_low);

  LIR *lir = this->NewLIR3(x86op, rl_dest.reg.GetLowReg(), r_base, displacement + LOWORD_OFFSET);
  this->AnnotateDalvikRegAccess(lir, (displacement + LOWORD_OFFSET) >> 2,
                                true /* is_load */, true /* is64bit */);
  x86op = GetOpcode(op, rl_dest, rl_src, true);
  lir = this->NewLIR3(x86op, rl_dest.reg.GetHighReg(), r_base, displacement + HIWORD_OFFSET);
  this->AnnotateDalvikRegAccess(lir, (displacement + HIWORD_OFFSET) >> 2,
                                true /* is_load */, true /* is64bit */);
}

X86MIR2LIR(void, GenLongArith(RegLocation rl_dest, RegLocation rl_src, Instruction::Code op)) {
  rl_dest = this->UpdateLocWide(rl_dest);
  if (rl_dest.location == kLocPhysReg) {
    // Ensure we are in a register pair
    RegLocation rl_result = this->EvalLocWide(rl_dest, kCoreReg, true);

    rl_src = this->UpdateLocWide(rl_src);
    GenLongRegOrMemOp(rl_result, rl_src, op);
    this->StoreFinalValueWide(rl_dest, rl_result);
    return;
  }

  // It wasn't in registers, so it better be in memory.
  DCHECK((rl_dest.location == kLocDalvikFrame) ||
         (rl_dest.location == kLocCompilerTemp));
  rl_src = this->LoadValueWide(rl_src, kCoreReg);

  // Operate directly into memory.
  X86OpCode x86op = GetOpcode(op, rl_dest, rl_src, false);
  int r_base = TargetReg(kSp).GetReg();
  int displacement = this->SRegOffset(rl_dest.s_reg_low);

  LIR *lir = this->NewLIR3(x86op, r_base, displacement + LOWORD_OFFSET, rl_src.reg.GetLowReg());
  this->AnnotateDalvikRegAccess(lir, (displacement + LOWORD_OFFSET) >> 2,
                                true /* is_load */, true /* is64bit */);
  this->AnnotateDalvikRegAccess(lir, (displacement + LOWORD_OFFSET) >> 2,
                                false /* is_load */, true /* is64bit */);
  x86op = GetOpcode(op, rl_dest, rl_src, true);
  lir = this->NewLIR3(x86op, r_base, displacement + HIWORD_OFFSET, rl_src.reg.GetHighReg());
  this->AnnotateDalvikRegAccess(lir, (displacement + HIWORD_OFFSET) >> 2,
                                true /* is_load */, true /* is64bit */);
  this->AnnotateDalvikRegAccess(lir, (displacement + HIWORD_OFFSET) >> 2,
                                false /* is_load */, true /* is64bit */);
  this->FreeTemp(rl_src.reg);
}

X86MIR2LIR(void, GenLongArith(RegLocation rl_dest, RegLocation rl_src1,
                             RegLocation rl_src2, Instruction::Code op,
                             bool is_commutative)) {
  // Is this really a 2 operand operation?
  switch (op) {
    case Instruction::ADD_LONG_2ADDR:
    case Instruction::SUB_LONG_2ADDR:
    case Instruction::AND_LONG_2ADDR:
    case Instruction::OR_LONG_2ADDR:
    case Instruction::XOR_LONG_2ADDR:
      GenLongArith(rl_dest, rl_src2, op);
      return;
    default:
      break;
  }

  if (rl_dest.location == kLocPhysReg) {
    RegLocation rl_result = this->LoadValueWide(rl_src1, kCoreReg);

    // We are about to clobber the LHS, so it needs to be a temp.
    rl_result = this->ForceTempWide(rl_result);

    // Perform the operation using the RHS.
    rl_src2 = this->UpdateLocWide(rl_src2);
    GenLongRegOrMemOp(rl_result, rl_src2, op);

    // And now record that the result is in the temp.
    this->StoreFinalValueWide(rl_dest, rl_result);
    return;
  }

  // It wasn't in registers, so it better be in memory.
  DCHECK((rl_dest.location == kLocDalvikFrame) ||
         (rl_dest.location == kLocCompilerTemp));
  rl_src1 = this->UpdateLocWide(rl_src1);
  rl_src2 = this->UpdateLocWide(rl_src2);

  // Get one of the source operands into temporary register.
  rl_src1 = this->LoadValueWide(rl_src1, kCoreReg);
  if (this->IsTemp(rl_src1.reg.GetLow()) && this->IsTemp(rl_src1.reg.GetHigh())) {
    GenLongRegOrMemOp(rl_src1, rl_src2, op);
  } else if (is_commutative) {
    rl_src2 = this->LoadValueWide(rl_src2, kCoreReg);
    // We need at least one of them to be a temporary.
    if (!(this->IsTemp(rl_src2.reg.GetLow()) && this->IsTemp(rl_src2.reg.GetHigh()))) {
      rl_src1 = this->ForceTempWide(rl_src1);
      GenLongRegOrMemOp(rl_src1, rl_src2, op);
    } else {
      GenLongRegOrMemOp(rl_src2, rl_src1, op);
      this->StoreFinalValueWide(rl_dest, rl_src2);
      return;
    }
  } else {
    // Need LHS to be the temp.
    rl_src1 = this->ForceTempWide(rl_src1);
    GenLongRegOrMemOp(rl_src1, rl_src2, op);
  }

  this->StoreFinalValueWide(rl_dest, rl_src1);
}

X86MIR2LIR(void, GenAddLong(Instruction::Code opcode, RegLocation rl_dest,
                           RegLocation rl_src1, RegLocation rl_src2)) {
  GenLongArith(rl_dest, rl_src1, rl_src2, opcode, true);
}

X86MIR2LIR(void, GenSubLong(Instruction::Code opcode, RegLocation rl_dest,
                           RegLocation rl_src1, RegLocation rl_src2)) {
  GenLongArith(rl_dest, rl_src1, rl_src2, opcode, false);
}

X86MIR2LIR(void, GenAndLong(Instruction::Code opcode, RegLocation rl_dest,
                           RegLocation rl_src1, RegLocation rl_src2)) {
  GenLongArith(rl_dest, rl_src1, rl_src2, opcode, true);
}

X86MIR2LIR(void, GenOrLong(Instruction::Code opcode, RegLocation rl_dest,
                          RegLocation rl_src1, RegLocation rl_src2)) {
  GenLongArith(rl_dest, rl_src1, rl_src2, opcode, true);
}

X86MIR2LIR(void, GenXorLong(Instruction::Code opcode, RegLocation rl_dest,
                           RegLocation rl_src1, RegLocation rl_src2)) {
  GenLongArith(rl_dest, rl_src1, rl_src2, opcode, true);
}

X86MIR2LIR(void, GenNegLong(RegLocation rl_dest, RegLocation rl_src)) {
  rl_src = this->LoadValueWide(rl_src, kCoreReg);
  RegLocation rl_result = this->ForceTempWide(rl_src);
  if (((rl_dest.location == kLocPhysReg) && (rl_src.location == kLocPhysReg)) &&
      ((rl_dest.reg.GetLowReg() == rl_src.reg.GetHighReg()))) {
    // The registers are the same, so we would clobber it before the use.
    RegStorage temp_reg = this->AllocTemp();
    OpRegCopy(temp_reg, rl_result.reg);
    rl_result.reg.SetHighReg(temp_reg.GetReg());
  }
  OpRegReg(kOpNeg, rl_result.reg.GetLow(), rl_result.reg.GetLow());    // rLow = -rLow
  OpRegImm(kOpAdc, rl_result.reg.GetHigh(), 0);                   // rHigh = rHigh + CF
  OpRegReg(kOpNeg, rl_result.reg.GetHigh(), rl_result.reg.GetHigh());  // rHigh = -rHigh
  this->StoreValueWide(rl_dest, rl_result);
}

template <size_t pointer_size>
void X86Mir2Lir<pointer_size>::OpRegThreadMem(OpKind op, RegStorage r_dest,
                                              ThreadOffset<pointer_size> thread_offset) {
  X86OpCode opcode = kX86Bkpt;
  switch (op) {
  case kOpCmp: opcode = kX86Cmp32RT;  break;
  case kOpMov: opcode = kX86Mov32RT;  break;
  default:
    LOG(FATAL) << "Bad opcode: " << op;
    break;
  }
  this->NewLIR2(opcode, r_dest.GetReg(), thread_offset.Int32Value());
}
template void X86Mir2Lir<4>::OpRegThreadMem(OpKind op, RegStorage r_dest,
                                            ThreadOffset<4> thread_offset);
template void X86Mir2Lir<8>::OpRegThreadMem(OpKind op, RegStorage r_dest,
                                            ThreadOffset<8> thread_offset);

/*
 * Generate array load
 */
X86MIR2LIR(void, GenArrayGet(int opt_flags, OpSize size, RegLocation rl_array,
                            RegLocation rl_index, RegLocation rl_dest, int scale)) {
  RegisterClass reg_class = this->RegClassBySize(size);
  int len_offset = mirror::Array::LengthOffset().Int32Value();
  RegLocation rl_result;
  rl_array = this->LoadValue(rl_array, kCoreReg);

  int data_offset;
  if (size == k64 || size == kDouble) {
    data_offset = mirror::Array::DataOffset(sizeof(int64_t)).Int32Value();
  } else {
    data_offset = mirror::Array::DataOffset(sizeof(int32_t)).Int32Value();
  }

  bool constant_index = rl_index.is_const;
  int32_t constant_index_value = 0;
  if (!constant_index) {
    rl_index = this->LoadValue(rl_index, kCoreReg);
  } else {
    constant_index_value = this->mir_graph_->ConstantValue(rl_index);
    // If index is constant, just fold it into the data offset
    data_offset += constant_index_value << scale;
    // treat as non array below
    rl_index.reg = RegStorage::InvalidReg();
  }

  /* null object? */
  this->GenNullCheck(rl_array.reg, opt_flags);

  if (!(opt_flags & MIR_IGNORE_RANGE_CHECK)) {
    if (constant_index) {
      GenArrayBoundsCheck(constant_index_value, rl_array.reg, len_offset);
    } else {
      GenArrayBoundsCheck(rl_index.reg, rl_array.reg, len_offset);
    }
  }
  rl_result = this->EvalLoc(rl_dest, reg_class, true);
  LoadBaseIndexedDisp(rl_array.reg, rl_index.reg, scale, data_offset, rl_result.reg, size);
  if ((size == k64) || (size == kDouble)) {
    this->StoreValueWide(rl_dest, rl_result);
  } else {
    this->StoreValue(rl_dest, rl_result);
  }
}

/*
 * Generate array store
 *
 */
X86MIR2LIR(void, GenArrayPut(int opt_flags, OpSize size, RegLocation rl_array,
                            RegLocation rl_index, RegLocation rl_src, int scale, bool card_mark)) {
  RegisterClass reg_class = this->RegClassBySize(size);
  int len_offset = mirror::Array::LengthOffset().Int32Value();
  int data_offset;

  if (size == k64 || size == kDouble) {
    data_offset = mirror::Array::DataOffset(sizeof(int64_t)).Int32Value();
  } else {
    data_offset = mirror::Array::DataOffset(sizeof(int32_t)).Int32Value();
  }

  rl_array = this->LoadValue(rl_array, kCoreReg);
  bool constant_index = rl_index.is_const;
  int32_t constant_index_value = 0;
  if (!constant_index) {
    rl_index = this->LoadValue(rl_index, kCoreReg);
  } else {
    // If index is constant, just fold it into the data offset
    constant_index_value = this->mir_graph_->ConstantValue(rl_index);
    data_offset += constant_index_value << scale;
    // treat as non array below
    rl_index.reg = RegStorage::InvalidReg();
  }

  /* null object? */
  this->GenNullCheck(rl_array.reg, opt_flags);

  if (!(opt_flags & MIR_IGNORE_RANGE_CHECK)) {
    if (constant_index) {
      GenArrayBoundsCheck(constant_index_value, rl_array.reg, len_offset);
    } else {
      GenArrayBoundsCheck(rl_index.reg, rl_array.reg, len_offset);
    }
  }
  if ((size == k64) || (size == kDouble)) {
    rl_src = this->LoadValueWide(rl_src, reg_class);
  } else {
    rl_src = this->LoadValue(rl_src, reg_class);
  }
  // If the src reg can't be byte accessed, move it to a temp first.
  if ((size == kSignedByte || size == kUnsignedByte) &&
      rl_src.reg.GetRegNum() >= rs_rX86_SP.GetRegNum()) {
    RegStorage temp = this->AllocTemp();
    OpRegCopy(temp, rl_src.reg);
    StoreBaseIndexedDisp(rl_array.reg, rl_index.reg, scale, data_offset, temp, size);
  } else {
    StoreBaseIndexedDisp(rl_array.reg, rl_index.reg, scale, data_offset, rl_src.reg, size);
  }
  if (card_mark) {
    // Free rl_index if its a temp. Ensures there are 2 free regs for card mark.
    if (!constant_index) {
      this->FreeTemp(rl_index.reg);
    }
    this->MarkGCCard(rl_src.reg, rl_array.reg);
  }
}

X86MIR2LIR(RegLocation, GenShiftImmOpLong(Instruction::Code opcode, RegLocation rl_dest,
                                         RegLocation rl_src, int shift_amount)) {
  RegLocation rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
  switch (opcode) {
    case Instruction::SHL_LONG:
    case Instruction::SHL_LONG_2ADDR:
      DCHECK_NE(shift_amount, 1);  // Prevent a double store from happening.
      if (shift_amount == 32) {
        OpRegCopy(rl_result.reg.GetHigh(), rl_src.reg.GetLow());
        this->LoadConstant(rl_result.reg.GetLow(), 0);
      } else if (shift_amount > 31) {
        OpRegCopy(rl_result.reg.GetHigh(), rl_src.reg.GetLow());
        this->FreeTemp(rl_src.reg.GetHigh());
        this->NewLIR2(kX86Sal32RI, rl_result.reg.GetHighReg(), shift_amount - 32);
        this->LoadConstant(rl_result.reg.GetLow(), 0);
      } else {
        OpRegCopy(rl_result.reg, rl_src.reg);
        OpRegCopy(rl_result.reg.GetHigh(), rl_src.reg.GetHigh());
        this->NewLIR3(kX86Shld32RRI, rl_result.reg.GetHighReg(), rl_result.reg.GetLowReg(),
                      shift_amount);
        this->NewLIR2(kX86Sal32RI, rl_result.reg.GetLowReg(), shift_amount);
      }
      break;
    case Instruction::SHR_LONG:
    case Instruction::SHR_LONG_2ADDR:
      if (shift_amount == 32) {
        OpRegCopy(rl_result.reg.GetLow(), rl_src.reg.GetHigh());
        OpRegCopy(rl_result.reg.GetHigh(), rl_src.reg.GetHigh());
        this->NewLIR2(kX86Sar32RI, rl_result.reg.GetHighReg(), 31);
      } else if (shift_amount > 31) {
        OpRegCopy(rl_result.reg.GetLow(), rl_src.reg.GetHigh());
        OpRegCopy(rl_result.reg.GetHigh(), rl_src.reg.GetHigh());
        this->NewLIR2(kX86Sar32RI, rl_result.reg.GetLowReg(), shift_amount - 32);
        this->NewLIR2(kX86Sar32RI, rl_result.reg.GetHighReg(), 31);
      } else {
        OpRegCopy(rl_result.reg, rl_src.reg);
        OpRegCopy(rl_result.reg.GetHigh(), rl_src.reg.GetHigh());
        this->NewLIR3(kX86Shrd32RRI, rl_result.reg.GetLowReg(), rl_result.reg.GetHighReg(),
                      shift_amount);
        this->NewLIR2(kX86Sar32RI, rl_result.reg.GetHighReg(), shift_amount);
      }
      break;
    case Instruction::USHR_LONG:
    case Instruction::USHR_LONG_2ADDR:
      if (shift_amount == 32) {
        OpRegCopy(rl_result.reg.GetLow(), rl_src.reg.GetHigh());
        this->LoadConstant(rl_result.reg.GetHigh(), 0);
      } else if (shift_amount > 31) {
        OpRegCopy(rl_result.reg.GetLow(), rl_src.reg.GetHigh());
        this->NewLIR2(kX86Shr32RI, rl_result.reg.GetLowReg(), shift_amount - 32);
        this->LoadConstant(rl_result.reg.GetHigh(), 0);
      } else {
        OpRegCopy(rl_result.reg, rl_src.reg);
        OpRegCopy(rl_result.reg.GetHigh(), rl_src.reg.GetHigh());
        this->NewLIR3(kX86Shrd32RRI, rl_result.reg.GetLowReg(), rl_result.reg.GetHighReg(),
                      shift_amount);
        this->NewLIR2(kX86Shr32RI, rl_result.reg.GetHighReg(), shift_amount);
      }
      break;
    default:
      LOG(FATAL) << "Unexpected case";
  }
  return rl_result;
}

X86MIR2LIR(void, GenShiftImmOpLong(Instruction::Code opcode, RegLocation rl_dest,
                                  RegLocation rl_src, RegLocation rl_shift)) {
  // Per spec, we only care about low 6 bits of shift amount.
  int shift_amount = this->mir_graph_->ConstantValue(rl_shift) & 0x3f;
  if (shift_amount == 0) {
    rl_src = this->LoadValueWide(rl_src, kCoreReg);
    this->StoreValueWide(rl_dest, rl_src);
    return;
  } else if (shift_amount == 1 &&
            (opcode ==  Instruction::SHL_LONG || opcode == Instruction::SHL_LONG_2ADDR)) {
    // Need to handle this here to avoid calling StoreValueWide twice.
    GenAddLong(Instruction::ADD_LONG, rl_dest, rl_src, rl_src);
    return;
  }
  if (this->BadOverlap(rl_src, rl_dest)) {
    this->GenShiftOpLong(opcode, rl_dest, rl_src, rl_shift);
    return;
  }
  rl_src = this->LoadValueWide(rl_src, kCoreReg);
  RegLocation rl_result = GenShiftImmOpLong(opcode, rl_dest, rl_src, shift_amount);
  this->StoreValueWide(rl_dest, rl_result);
}

X86MIR2LIR(void, GenArithImmOpLong(Instruction::Code opcode,
                                  RegLocation rl_dest, RegLocation rl_src1, RegLocation rl_src2)) {
  switch (opcode) {
    case Instruction::ADD_LONG:
    case Instruction::AND_LONG:
    case Instruction::OR_LONG:
    case Instruction::XOR_LONG:
      if (rl_src2.is_const) {
        GenLongLongImm(rl_dest, rl_src1, rl_src2, opcode);
      } else {
        DCHECK(rl_src1.is_const);
        GenLongLongImm(rl_dest, rl_src2, rl_src1, opcode);
      }
      break;
    case Instruction::SUB_LONG:
    case Instruction::SUB_LONG_2ADDR:
      if (rl_src2.is_const) {
        GenLongLongImm(rl_dest, rl_src1, rl_src2, opcode);
      } else {
        GenSubLong(opcode, rl_dest, rl_src1, rl_src2);
      }
      break;
    case Instruction::ADD_LONG_2ADDR:
    case Instruction::OR_LONG_2ADDR:
    case Instruction::XOR_LONG_2ADDR:
    case Instruction::AND_LONG_2ADDR:
      if (rl_src2.is_const) {
        GenLongImm(rl_dest, rl_src2, opcode);
      } else {
        DCHECK(rl_src1.is_const);
        GenLongLongImm(rl_dest, rl_src2, rl_src1, opcode);
      }
      break;
    default:
      // Default - bail to non-const handler.
      this->GenArithOpLong(opcode, rl_dest, rl_src1, rl_src2);
      break;
  }
}

X86MIR2LIR(bool, IsNoOp(Instruction::Code op, int32_t value)) {
  switch (op) {
    case Instruction::AND_LONG_2ADDR:
    case Instruction::AND_LONG:
      return value == -1;
    case Instruction::OR_LONG:
    case Instruction::OR_LONG_2ADDR:
    case Instruction::XOR_LONG:
    case Instruction::XOR_LONG_2ADDR:
      return value == 0;
    default:
      return false;
  }
}

X86MIR2LIR(X86OpCode, GetOpcode(Instruction::Code op, RegLocation dest, RegLocation rhs,
                               bool is_high_op)) {
  bool rhs_in_mem = rhs.location != kLocPhysReg;
  bool dest_in_mem = dest.location != kLocPhysReg;
  DCHECK(!rhs_in_mem || !dest_in_mem);
  switch (op) {
    case Instruction::ADD_LONG:
    case Instruction::ADD_LONG_2ADDR:
      if (dest_in_mem) {
        return is_high_op ? kX86Adc32MR : kX86Add32MR;
      } else if (rhs_in_mem) {
        return is_high_op ? kX86Adc32RM : kX86Add32RM;
      }
      return is_high_op ? kX86Adc32RR : kX86Add32RR;
    case Instruction::SUB_LONG:
    case Instruction::SUB_LONG_2ADDR:
      if (dest_in_mem) {
        return is_high_op ? kX86Sbb32MR : kX86Sub32MR;
      } else if (rhs_in_mem) {
        return is_high_op ? kX86Sbb32RM : kX86Sub32RM;
      }
      return is_high_op ? kX86Sbb32RR : kX86Sub32RR;
    case Instruction::AND_LONG_2ADDR:
    case Instruction::AND_LONG:
      if (dest_in_mem) {
        return kX86And32MR;
      }
      return rhs_in_mem ? kX86And32RM : kX86And32RR;
    case Instruction::OR_LONG:
    case Instruction::OR_LONG_2ADDR:
      if (dest_in_mem) {
        return kX86Or32MR;
      }
      return rhs_in_mem ? kX86Or32RM : kX86Or32RR;
    case Instruction::XOR_LONG:
    case Instruction::XOR_LONG_2ADDR:
      if (dest_in_mem) {
        return kX86Xor32MR;
      }
      return rhs_in_mem ? kX86Xor32RM : kX86Xor32RR;
    default:
      LOG(FATAL) << "Unexpected opcode: " << op;
      return kX86Add32RR;
  }
}

X86MIR2LIR(X86OpCode, GetOpcode(Instruction::Code op, RegLocation loc, bool is_high_op,
                               int32_t value)) {
  bool in_mem = loc.location != kLocPhysReg;
  bool byte_imm = IS_SIMM8(value);
  DCHECK(in_mem || !loc.reg.IsFloat());
  switch (op) {
    case Instruction::ADD_LONG:
    case Instruction::ADD_LONG_2ADDR:
      if (byte_imm) {
        if (in_mem) {
          return is_high_op ? kX86Adc32MI8 : kX86Add32MI8;
        }
        return is_high_op ? kX86Adc32RI8 : kX86Add32RI8;
      }
      if (in_mem) {
        return is_high_op ? kX86Adc32MI : kX86Add32MI;
      }
      return is_high_op ? kX86Adc32RI : kX86Add32RI;
    case Instruction::SUB_LONG:
    case Instruction::SUB_LONG_2ADDR:
      if (byte_imm) {
        if (in_mem) {
          return is_high_op ? kX86Sbb32MI8 : kX86Sub32MI8;
        }
        return is_high_op ? kX86Sbb32RI8 : kX86Sub32RI8;
      }
      if (in_mem) {
        return is_high_op ? kX86Sbb32MI : kX86Sub32MI;
      }
      return is_high_op ? kX86Sbb32RI : kX86Sub32RI;
    case Instruction::AND_LONG_2ADDR:
    case Instruction::AND_LONG:
      if (byte_imm) {
        return in_mem ? kX86And32MI8 : kX86And32RI8;
      }
      return in_mem ? kX86And32MI : kX86And32RI;
    case Instruction::OR_LONG:
    case Instruction::OR_LONG_2ADDR:
      if (byte_imm) {
        return in_mem ? kX86Or32MI8 : kX86Or32RI8;
      }
      return in_mem ? kX86Or32MI : kX86Or32RI;
    case Instruction::XOR_LONG:
    case Instruction::XOR_LONG_2ADDR:
      if (byte_imm) {
        return in_mem ? kX86Xor32MI8 : kX86Xor32RI8;
      }
      return in_mem ? kX86Xor32MI : kX86Xor32RI;
    default:
      LOG(FATAL) << "Unexpected opcode: " << op;
      return kX86Add32MI;
  }
}

X86MIR2LIR(void, GenLongImm(RegLocation rl_dest, RegLocation rl_src, Instruction::Code op)) {
  DCHECK(rl_src.is_const);
  int64_t val = this->mir_graph_->ConstantValueWide(rl_src);
  int32_t val_lo = Low32Bits(val);
  int32_t val_hi = High32Bits(val);
  rl_dest = this->UpdateLocWide(rl_dest);

  // Can we just do this into memory?
  if ((rl_dest.location == kLocDalvikFrame) ||
      (rl_dest.location == kLocCompilerTemp)) {
    int r_base = TargetReg(kSp).GetReg();
    int displacement = this->SRegOffset(rl_dest.s_reg_low);

    if (!IsNoOp(op, val_lo)) {
      X86OpCode x86op = GetOpcode(op, rl_dest, false, val_lo);
      LIR *lir = this->NewLIR3(x86op, r_base, displacement + LOWORD_OFFSET, val_lo);
      this->AnnotateDalvikRegAccess(lir, (displacement + LOWORD_OFFSET) >> 2,
                                    true /* is_load */, true /* is64bit */);
      this->AnnotateDalvikRegAccess(lir, (displacement + LOWORD_OFFSET) >> 2,
                                    false /* is_load */, true /* is64bit */);
    }
    if (!IsNoOp(op, val_hi)) {
      X86OpCode x86op = GetOpcode(op, rl_dest, true, val_hi);
      LIR *lir = this->NewLIR3(x86op, r_base, displacement + HIWORD_OFFSET, val_hi);
      this->AnnotateDalvikRegAccess(lir, (displacement + HIWORD_OFFSET) >> 2,
                                    true /* is_load */, true /* is64bit */);
      this->AnnotateDalvikRegAccess(lir, (displacement + HIWORD_OFFSET) >> 2,
                                    false /* is_load */, true /* is64bit */);
    }
    return;
  }

  RegLocation rl_result = this->EvalLocWide(rl_dest, kCoreReg, true);
  DCHECK_EQ(rl_result.location, kLocPhysReg);
  DCHECK(!rl_result.reg.IsFloat());

  if (!IsNoOp(op, val_lo)) {
    X86OpCode x86op = GetOpcode(op, rl_result, false, val_lo);
    this->NewLIR2(x86op, rl_result.reg.GetLowReg(), val_lo);
  }
  if (!IsNoOp(op, val_hi)) {
    X86OpCode x86op = GetOpcode(op, rl_result, true, val_hi);
    this->NewLIR2(x86op, rl_result.reg.GetHighReg(), val_hi);
  }
  this->StoreValueWide(rl_dest, rl_result);
}

X86MIR2LIR(void, GenLongLongImm(RegLocation rl_dest, RegLocation rl_src1,
                               RegLocation rl_src2, Instruction::Code op)) {
  DCHECK(rl_src2.is_const);
  int64_t val = this->mir_graph_->ConstantValueWide(rl_src2);
  int32_t val_lo = Low32Bits(val);
  int32_t val_hi = High32Bits(val);
  rl_dest = this->UpdateLocWide(rl_dest);
  rl_src1 = this->UpdateLocWide(rl_src1);

  // Can we do this directly into the destination registers?
  if (rl_dest.location == kLocPhysReg && rl_src1.location == kLocPhysReg &&
      rl_dest.reg.GetLowReg() == rl_src1.reg.GetLowReg() &&
      rl_dest.reg.GetHighReg() == rl_src1.reg.GetHighReg() && !rl_dest.reg.IsFloat()) {
    if (!IsNoOp(op, val_lo)) {
      X86OpCode x86op = GetOpcode(op, rl_dest, false, val_lo);
      this->NewLIR2(x86op, rl_dest.reg.GetLowReg(), val_lo);
    }
    if (!IsNoOp(op, val_hi)) {
      X86OpCode x86op = GetOpcode(op, rl_dest, true, val_hi);
      this->NewLIR2(x86op, rl_dest.reg.GetHighReg(), val_hi);
    }

    this->StoreFinalValueWide(rl_dest, rl_dest);
    return;
  }

  rl_src1 = this->LoadValueWide(rl_src1, kCoreReg);
  DCHECK_EQ(rl_src1.location, kLocPhysReg);

  // We need the values to be in a temporary
  RegLocation rl_result = this->ForceTempWide(rl_src1);
  if (!IsNoOp(op, val_lo)) {
    X86OpCode x86op = GetOpcode(op, rl_result, false, val_lo);
    this->NewLIR2(x86op, rl_result.reg.GetLowReg(), val_lo);
  }
  if (!IsNoOp(op, val_hi)) {
    X86OpCode x86op = GetOpcode(op, rl_result, true, val_hi);
    this->NewLIR2(x86op, rl_result.reg.GetHighReg(), val_hi);
  }

  this->StoreFinalValueWide(rl_dest, rl_result);
}

// For final classes there are no sub-classes to check and so we can answer the instance-of
// question with simple comparisons. Use compares to memory and SETEQ to optimize for x86.
X86MIR2LIR(void, GenInstanceofFinal(bool use_declaring_class, uint32_t type_idx,
                                   RegLocation rl_dest, RegLocation rl_src)) {
  RegLocation object = this->LoadValue(rl_src, kCoreReg);
  RegLocation rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
  RegStorage result_reg = rl_result.reg;

  // SETcc only works with EAX..EDX.
  if (result_reg == object.reg || result_reg.GetRegNum() >= rs_rX86_SP.GetRegNum()) {
    result_reg = AllocTypedTemp(false, kCoreReg);
    DCHECK_LT(result_reg.GetRegNum(), rs_rX86_SP.GetRegNum());
  }

  // Assume that there is no match.
  this->LoadConstant(result_reg, 0);
  LIR* null_branchover = OpCmpImmBranch(kCondEq, object.reg, 0, NULL);

  RegStorage check_class = AllocTypedTemp(false, kCoreReg);

  // If Method* is already in a register, we can save a copy.
  RegLocation rl_method = this->mir_graph_->GetMethodLoc();
  int32_t offset_of_type = mirror::Array::DataOffset(sizeof(mirror::Class*)).Int32Value() +
    (sizeof(mirror::Class*) * type_idx);

  if (rl_method.location == kLocPhysReg) {
    if (use_declaring_class) {
      this->LoadRefDisp(rl_method.reg, mirror::ArtMethod::DeclaringClassOffset().Int32Value(),
                        check_class);
    } else {
      this->LoadRefDisp(rl_method.reg, mirror::ArtMethod::DexCacheResolvedTypesOffset().Int32Value(),
                        check_class);
      this->LoadRefDisp(check_class, offset_of_type, check_class);
    }
  } else {
    this->LoadCurrMethodDirect(check_class);
    if (use_declaring_class) {
      this->LoadRefDisp(check_class, mirror::ArtMethod::DeclaringClassOffset().Int32Value(),
                        check_class);
    } else {
      this->LoadRefDisp(check_class, mirror::ArtMethod::DexCacheResolvedTypesOffset().Int32Value(),
                        check_class);
      this->LoadRefDisp(check_class, offset_of_type, check_class);
    }
  }

  // Compare the computed class to the class in the object.
  DCHECK_EQ(object.location, kLocPhysReg);
  OpRegMem(kOpCmp, check_class, object.reg, mirror::Object::ClassOffset().Int32Value());

  // Set the low byte of the result to 0 or 1 from the compare condition code.
  this->NewLIR2(kX86Set8R, result_reg.GetReg(), kX86CondEq);

  LIR* target = this->NewLIR0(kPseudoTargetLabel);
  null_branchover->target = target;
  this->FreeTemp(check_class);
  if (this->IsTemp(result_reg)) {
    OpRegCopy(rl_result.reg, result_reg);
    this->FreeTemp(result_reg);
  }
  this->StoreValue(rl_dest, rl_result);
}

X86MIR2LIR(void, GenInstanceofCallingHelper(bool needs_access_check, bool type_known_final,
                                           bool type_known_abstract, bool use_declaring_class,
                                           bool can_assume_type_is_in_dex_cache,
                                           uint32_t type_idx, RegLocation rl_dest,
                                           RegLocation rl_src)) {
  this->FlushAllRegs();
  // May generate a call - use explicit registers.
  this->LockCallTemps();
  this->LoadCurrMethodDirect(TargetReg(kArg1));  // kArg1 gets current Method*.
  RegStorage class_reg = TargetReg(kArg2);  // kArg2 will hold the Class*.
  // Reference must end up in kArg0.
  if (needs_access_check) {
    // Check we have access to type_idx and if not throw IllegalAccessError,
    // Caller function returns Class* in kArg0.
    this->CallRuntimeHelperImm(QUICK_ENTRYPOINT_OFFSET(pointer_size, pInitializeTypeAndVerifyAccess),
                               type_idx, true);
    OpRegCopy(class_reg, TargetReg(kRet0));
    this->LoadValueDirectFixed(rl_src, TargetReg(kArg0));
  } else if (use_declaring_class) {
    this->LoadValueDirectFixed(rl_src, TargetReg(kArg0));
    this->LoadRefDisp(TargetReg(kArg1), mirror::ArtMethod::DeclaringClassOffset().Int32Value(),
                      class_reg);
  } else {
    // Load dex cache entry into class_reg (kArg2).
    this->LoadValueDirectFixed(rl_src, TargetReg(kArg0));
    this->LoadRefDisp(TargetReg(kArg1), mirror::ArtMethod::DexCacheResolvedTypesOffset().Int32Value(),
                      class_reg);
    int32_t offset_of_type =
        mirror::Array::DataOffset(sizeof(mirror::Class*)).Int32Value() + (sizeof(mirror::Class*)
        * type_idx);
    this->LoadRefDisp(class_reg, offset_of_type, class_reg);
    if (!can_assume_type_is_in_dex_cache) {
      // Need to test presence of type in dex cache at runtime.
      LIR* hop_branch = OpCmpImmBranch(kCondNe, class_reg, 0, NULL);
      // Type is not resolved. Call out to helper, which will return resolved type in kRet0/kArg0.
      this->CallRuntimeHelperImm(QUICK_ENTRYPOINT_OFFSET(pointer_size, pInitializeType), type_idx,
                                 true);
      OpRegCopy(TargetReg(kArg2), TargetReg(kRet0));  // Align usage with fast path.
      this->LoadValueDirectFixed(rl_src, TargetReg(kArg0));  /* Reload Ref. */
      // Rejoin code paths
      LIR* hop_target = this->NewLIR0(kPseudoTargetLabel);
      hop_branch->target = hop_target;
    }
  }
  /* kArg0 is ref, kArg2 is class. If ref==null, use directly as bool result. */
  RegLocation rl_result = this->GetReturn(false);

  // SETcc only works with EAX..EDX.
  DCHECK_LT(rl_result.reg.GetRegNum(), 4);

  // Is the class NULL?
  LIR* branch1 = OpCmpImmBranch(kCondEq, TargetReg(kArg0), 0, NULL);

  /* Load object->klass_. */
  DCHECK_EQ(mirror::Object::ClassOffset().Int32Value(), 0);
  this->LoadRefDisp(TargetReg(kArg0),  mirror::Object::ClassOffset().Int32Value(), TargetReg(kArg1));
  /* kArg0 is ref, kArg1 is ref->klass_, kArg2 is class. */
  LIR* branchover = nullptr;
  if (type_known_final) {
    // Ensure top 3 bytes of result are 0.
    this->LoadConstant(rl_result.reg, 0);
    OpRegReg(kOpCmp, TargetReg(kArg1), TargetReg(kArg2));
    // Set the low byte of the result to 0 or 1 from the compare condition code.
    this->NewLIR2(kX86Set8R, rl_result.reg.GetReg(), kX86CondEq);
  } else {
    if (!type_known_abstract) {
      this->LoadConstant(rl_result.reg, 1);     // Assume result succeeds.
      branchover = OpCmpBranch(kCondEq, TargetReg(kArg1), TargetReg(kArg2), NULL);
    }
    OpRegCopy(TargetReg(kArg0), TargetReg(kArg2));
    OpThreadMem(kOpBlx, QUICK_ENTRYPOINT_OFFSET(pointer_size, pInstanceofNonTrivial));
  }
  // TODO: only clobber when type isn't final?
  ClobberCallerSave();
  /* Branch targets here. */
  LIR* target = this->NewLIR0(kPseudoTargetLabel);
  this->StoreValue(rl_dest, rl_result);
  branch1->target = target;
  if (branchover != nullptr) {
    branchover->target = target;
  }
}

X86MIR2LIR(void, GenArithOpInt(Instruction::Code opcode, RegLocation rl_dest,
                              RegLocation rl_lhs, RegLocation rl_rhs)) {
  OpKind op = kOpBkpt;
  bool is_div_rem = false;
  bool unary = false;
  bool shift_op = false;
  bool is_two_addr = false;
  RegLocation rl_result;
  switch (opcode) {
    case Instruction::NEG_INT:
      op = kOpNeg;
      unary = true;
      break;
    case Instruction::NOT_INT:
      op = kOpMvn;
      unary = true;
      break;
    case Instruction::ADD_INT_2ADDR:
      is_two_addr = true;
      // Fallthrough
    case Instruction::ADD_INT:
      op = kOpAdd;
      break;
    case Instruction::SUB_INT_2ADDR:
      is_two_addr = true;
      // Fallthrough
    case Instruction::SUB_INT:
      op = kOpSub;
      break;
    case Instruction::MUL_INT_2ADDR:
      is_two_addr = true;
      // Fallthrough
    case Instruction::MUL_INT:
      op = kOpMul;
      break;
    case Instruction::DIV_INT_2ADDR:
      is_two_addr = true;
      // Fallthrough
    case Instruction::DIV_INT:
      op = kOpDiv;
      is_div_rem = true;
      break;
    /* NOTE: returns in kArg1 */
    case Instruction::REM_INT_2ADDR:
      is_two_addr = true;
      // Fallthrough
    case Instruction::REM_INT:
      op = kOpRem;
      is_div_rem = true;
      break;
    case Instruction::AND_INT_2ADDR:
      is_two_addr = true;
      // Fallthrough
    case Instruction::AND_INT:
      op = kOpAnd;
      break;
    case Instruction::OR_INT_2ADDR:
      is_two_addr = true;
      // Fallthrough
    case Instruction::OR_INT:
      op = kOpOr;
      break;
    case Instruction::XOR_INT_2ADDR:
      is_two_addr = true;
      // Fallthrough
    case Instruction::XOR_INT:
      op = kOpXor;
      break;
    case Instruction::SHL_INT_2ADDR:
      is_two_addr = true;
      // Fallthrough
    case Instruction::SHL_INT:
      shift_op = true;
      op = kOpLsl;
      break;
    case Instruction::SHR_INT_2ADDR:
      is_two_addr = true;
      // Fallthrough
    case Instruction::SHR_INT:
      shift_op = true;
      op = kOpAsr;
      break;
    case Instruction::USHR_INT_2ADDR:
      is_two_addr = true;
      // Fallthrough
    case Instruction::USHR_INT:
      shift_op = true;
      op = kOpLsr;
      break;
    default:
      LOG(FATAL) << "Invalid word arith op: " << opcode;
  }

    // Can we convert to a two address instruction?
  if (!is_two_addr &&
        (this->mir_graph_->SRegToVReg(rl_dest.s_reg_low) ==
            this->mir_graph_->SRegToVReg(rl_lhs.s_reg_low))) {
      is_two_addr = true;
    }

  // Get the div/rem stuff out of the way.
  if (is_div_rem) {
    rl_result = GenDivRem(rl_dest, rl_lhs, rl_rhs, op == kOpDiv, true);
    this->StoreValue(rl_dest, rl_result);
    return;
  }

  if (unary) {
    rl_lhs = this->LoadValue(rl_lhs, kCoreReg);
    rl_result = this->UpdateLoc(rl_dest);
    rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
    OpRegReg(op, rl_result.reg, rl_lhs.reg);
  } else {
    if (shift_op) {
      // X86 doesn't require masking and must use ECX.
      RegStorage t_reg = TargetReg(kCount);  // rCX
      this->LoadValueDirectFixed(rl_rhs, t_reg);
      if (is_two_addr) {
        // Can we do this directly into memory?
        rl_result = this->UpdateLoc(rl_dest);
        rl_rhs = this->LoadValue(rl_rhs, kCoreReg);
        if (rl_result.location != kLocPhysReg) {
          // Okay, we can do this into memory
          OpMemReg(op, rl_result, t_reg.GetReg());
          this->FreeTemp(t_reg);
          return;
        } else if (!rl_result.reg.IsFloat()) {
          // Can do this directly into the result register
          OpRegReg(op, rl_result.reg, t_reg);
          this->FreeTemp(t_reg);
          this->StoreFinalValue(rl_dest, rl_result);
          return;
        }
      }
      // Three address form, or we can't do directly.
      rl_lhs = this->LoadValue(rl_lhs, kCoreReg);
      rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
      OpRegRegReg(op, rl_result.reg, rl_lhs.reg, t_reg);
      this->FreeTemp(t_reg);
    } else {
      // Multiply is 3 operand only (sort of).
      if (is_two_addr && op != kOpMul) {
        // Can we do this directly into memory?
        rl_result = this->UpdateLoc(rl_dest);
        if (rl_result.location == kLocPhysReg) {
          // Ensure res is in a core reg
          rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
          // Can we do this from memory directly?
          rl_rhs = this->UpdateLoc(rl_rhs);
          if (rl_rhs.location != kLocPhysReg) {
            OpRegMem(op, rl_result.reg, rl_rhs);
            this->StoreFinalValue(rl_dest, rl_result);
            return;
          } else if (!rl_rhs.reg.IsFloat()) {
            OpRegReg(op, rl_result.reg, rl_rhs.reg);
            this->StoreFinalValue(rl_dest, rl_result);
            return;
          }
        }
        rl_rhs = this->LoadValue(rl_rhs, kCoreReg);
        if (rl_result.location != kLocPhysReg) {
          // Okay, we can do this into memory.
          OpMemReg(op, rl_result, rl_rhs.reg.GetReg());
          return;
        } else if (!rl_result.reg.IsFloat()) {
          // Can do this directly into the result register.
          OpRegReg(op, rl_result.reg, rl_rhs.reg);
          this->StoreFinalValue(rl_dest, rl_result);
          return;
        } else {
          rl_lhs = this->LoadValue(rl_lhs, kCoreReg);
          rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
          OpRegRegReg(op, rl_result.reg, rl_lhs.reg, rl_rhs.reg);
        }
      } else {
        // Try to use reg/memory instructions.
        rl_lhs = this->UpdateLoc(rl_lhs);
        rl_rhs = this->UpdateLoc(rl_rhs);
        // We can't optimize with FP registers.
        if (!IsOperationSafeWithoutTemps(rl_lhs, rl_rhs)) {
          // Something is difficult, so fall back to the standard case.
          rl_lhs = this->LoadValue(rl_lhs, kCoreReg);
          rl_rhs = this->LoadValue(rl_rhs, kCoreReg);
          rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
          OpRegRegReg(op, rl_result.reg, rl_lhs.reg, rl_rhs.reg);
        } else {
          // We can optimize by moving to result and using memory operands.
          if (rl_rhs.location != kLocPhysReg) {
            // Force LHS into result.
            // We should be careful with order here
            // If rl_dest and rl_lhs points to the same VR we should load first
            // If the are different we should find a register first for dest
            if (this->mir_graph_->SRegToVReg(rl_dest.s_reg_low) ==
                this->mir_graph_->SRegToVReg(rl_lhs.s_reg_low)) {
              rl_lhs = this->LoadValue(rl_lhs, kCoreReg);
              rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
            } else {
              rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
              this->LoadValueDirect(rl_lhs, rl_result.reg);
            }
            OpRegMem(op, rl_result.reg, rl_rhs);
          } else if (rl_lhs.location != kLocPhysReg) {
            // RHS is in a register; LHS is in memory.
            if (op != kOpSub) {
              // Force RHS into result and operate on memory.
              rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
              OpRegCopy(rl_result.reg, rl_rhs.reg);
              OpRegMem(op, rl_result.reg, rl_lhs);
            } else {
              // Subtraction isn't commutative.
              rl_lhs = this->LoadValue(rl_lhs, kCoreReg);
              rl_rhs = this->LoadValue(rl_rhs, kCoreReg);
              rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
              OpRegRegReg(op, rl_result.reg, rl_lhs.reg, rl_rhs.reg);
            }
          } else {
            // Both are in registers.
            rl_lhs = this->LoadValue(rl_lhs, kCoreReg);
            rl_rhs = this->LoadValue(rl_rhs, kCoreReg);
            rl_result = this->EvalLoc(rl_dest, kCoreReg, true);
            OpRegRegReg(op, rl_result.reg, rl_lhs.reg, rl_rhs.reg);
          }
        }
      }
    }
  }
  this->StoreValue(rl_dest, rl_result);
}

X86MIR2LIR(bool, IsOperationSafeWithoutTemps(RegLocation rl_lhs, RegLocation rl_rhs)) {
  // If we have non-core registers, then we can't do good things.
  if (rl_lhs.location == kLocPhysReg && rl_lhs.reg.IsFloat()) {
    return false;
  }
  if (rl_rhs.location == kLocPhysReg && rl_rhs.reg.IsFloat()) {
    return false;
  }

  // Everything will be fine :-).
  return true;
}
}  // namespace art
