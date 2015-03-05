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

#include "codegen_mips.h"

#include "arch/mips/instruction_set_features_mips.h"
#include "arch/mips/entrypoints_direct_mips.h"
#include "base/logging.h"
#include "dex/quick/mir_to_lir-inl.h"
#include "dex/reg_storage_eq.h"
#include "driver/compiler_driver.h"
#include "mips_lir.h"

namespace art {

/* This file contains codegen for the MIPS32 ISA. */
LIR* MipsMir2Lir::OpFpRegCopy(RegStorage r_dest, RegStorage r_src) {
  int opcode;
  /* must be both DOUBLE or both not DOUBLE */
  DCHECK_EQ(r_dest.IsDouble(), r_src.IsDouble());
  if (r_dest.IsDouble()) {
    opcode = kMipsFmovd;
  } else {
    if (r_dest.IsSingle()) {
      if (r_src.IsSingle()) {
        opcode = kMipsFmovs;
      } else {
        /* note the operands are swapped for the mtc1 instr */
        RegStorage t_opnd = r_src;
        r_src = r_dest;
        r_dest = t_opnd;
        opcode = kMipsMtc1;
      }
    } else {
      DCHECK(r_src.IsSingle());
      opcode = kMipsMfc1;
    }
  }
  LIR* res = RawLIR(current_dalvik_offset_, opcode, r_src.GetReg(), r_dest.GetReg());
  if (!(cu_->disable_opt & (1 << kSafeOptimizations)) && r_dest == r_src) {
    res->flags.is_nop = true;
  }
  return res;
}

bool MipsMir2Lir::InexpensiveConstantInt(int32_t value) {
  // For encodings, see LoadConstantNoClobber below.
  return ((value == 0) || IsUint<16>(value) || IsInt<16>(value));
}

bool MipsMir2Lir::InexpensiveConstantFloat(int32_t value) {
  UNUSED(value);
  return false;  // TUNING
}

bool MipsMir2Lir::InexpensiveConstantLong(int64_t value) {
  UNUSED(value);
  return false;  // TUNING
}

bool MipsMir2Lir::InexpensiveConstantDouble(int64_t value) {
  UNUSED(value);
  return false;  // TUNING
}

/*
 * Load a immediate using a shortcut if possible; otherwise
 * grab from the per-translation literal pool.  If target is
 * a high register, build constant into a low register and copy.
 *
 * No additional register clobbering operation performed. Use this version when
 * 1) r_dest is freshly returned from AllocTemp or
 * 2) The codegen is under fixed register usage
 */
LIR* MipsMir2Lir::LoadConstantNoClobber(RegStorage r_dest, int value) {
  LIR *res;

  RegStorage r_dest_save = r_dest;
  int is_fp_reg = r_dest.IsFloat();
  if (is_fp_reg) {
    DCHECK(r_dest.IsSingle());
    r_dest = AllocTemp();
  }

  /* See if the value can be constructed cheaply */
  if (value == 0) {
    res = NewLIR2(kMipsMove, r_dest.GetReg(), rZERO);
  } else if (IsUint<16>(value)) {
    // Use OR with (unsigned) immediate to encode 16b unsigned int.
    res = NewLIR3(kMipsOri, r_dest.GetReg(), rZERO, value);
  } else if (IsInt<16>(value)) {
    // Use ADD with (signed) immediate to encode 16b signed int.
    res = NewLIR3(kMipsAddiu, r_dest.GetReg(), rZERO, value);
  } else {
    res = NewLIR2(kMipsLui, r_dest.GetReg(), value >> 16);
    if (value & 0xffff)
      NewLIR3(kMipsOri, r_dest.GetReg(), r_dest.GetReg(), value);
  }

  if (is_fp_reg) {
    NewLIR2(kMipsMtc1, r_dest.GetReg(), r_dest_save.GetReg());
    FreeTemp(r_dest);
  }

  return res;
}

LIR* MipsMir2Lir::OpUnconditionalBranch(LIR* target) {
  LIR* res = NewLIR1(kMipsB, 0 /* offset to be patched during assembly*/);
  res->target = target;
  return res;
}

LIR* MipsMir2Lir::OpReg(OpKind op, RegStorage r_dest_src) {
  MipsOpCode opcode = kMipsNop;
  switch (op) {
    case kOpBlx:
      opcode = kMipsJalr;
      break;
    case kOpBx:
      return NewLIR2(kMipsJalr, rZERO, r_dest_src.GetReg());
      break;
    default:
      LOG(FATAL) << "Bad case in OpReg";
  }
  return NewLIR2(opcode, rRA, r_dest_src.GetReg());
}

LIR* MipsMir2Lir::OpRegImm(OpKind op, RegStorage r_dest_src1, int value) {
  LIR *res;
  bool neg = (value < 0);
  int abs_value = (neg) ? -value : value;
  bool short_form = (abs_value & 0xff) == abs_value;
  MipsOpCode opcode = kMipsNop;
  switch (op) {
    case kOpAdd:
      return OpRegRegImm(op, r_dest_src1, r_dest_src1, value);
      break;
    case kOpSub:
      return OpRegRegImm(op, r_dest_src1, r_dest_src1, value);
      break;
    default:
      LOG(FATAL) << "Bad case in OpRegImm";
      break;
  }
  if (short_form) {
    res = NewLIR2(opcode, r_dest_src1.GetReg(), abs_value);
  } else {
    RegStorage r_scratch = AllocTemp();
    res = LoadConstant(r_scratch, value);
    if (op == kOpCmp)
      NewLIR2(opcode, r_dest_src1.GetReg(), r_scratch.GetReg());
    else
      NewLIR3(opcode, r_dest_src1.GetReg(), r_dest_src1.GetReg(), r_scratch.GetReg());
  }
  return res;
}

LIR* MipsMir2Lir::OpRegRegReg(OpKind op, RegStorage r_dest, RegStorage r_src1, RegStorage r_src2) {
  MipsOpCode opcode = kMipsNop;
  switch (op) {
    case kOpAdd:
      opcode = kMipsAddu;
      break;
    case kOpSub:
      opcode = kMipsSubu;
      break;
    case kOpAnd:
      opcode = kMipsAnd;
      break;
    case kOpMul:
      if (isaIsR6_) {
          opcode = kMipsR6Mul;
      } else {
          opcode = kMipsMul;
      }
      break;
    case kOpOr:
      opcode = kMipsOr;
      break;
    case kOpXor:
      opcode = kMipsXor;
      break;
    case kOpLsl:
      opcode = kMipsSllv;
      break;
    case kOpLsr:
      opcode = kMipsSrlv;
      break;
    case kOpAsr:
      opcode = kMipsSrav;
      break;
    case kOpAdc:
    case kOpSbc:
      LOG(FATAL) << "No carry bit on MIPS";
      break;
    default:
      LOG(FATAL) << "bad case in OpRegRegReg";
      break;
  }
  return NewLIR3(opcode, r_dest.GetReg(), r_src1.GetReg(), r_src2.GetReg());
}

LIR* MipsMir2Lir::OpRegRegImm(OpKind op, RegStorage r_dest, RegStorage r_src1, int value) {
  LIR *res;
  MipsOpCode opcode = kMipsNop;
  bool short_form = true;

  switch (op) {
    case kOpAdd:
      if (IS_SIMM16(value)) {
        opcode = kMipsAddiu;
      } else {
        short_form = false;
        opcode = kMipsAddu;
      }
      break;
    case kOpSub:
      if (IS_SIMM16((-value))) {
        value = -value;
        opcode = kMipsAddiu;
      } else {
        short_form = false;
        opcode = kMipsSubu;
      }
      break;
    case kOpLsl:
      DCHECK(value >= 0 && value <= 31);
      opcode = kMipsSll;
      break;
    case kOpLsr:
      DCHECK(value >= 0 && value <= 31);
      opcode = kMipsSrl;
      break;
    case kOpAsr:
      DCHECK(value >= 0 && value <= 31);
      opcode = kMipsSra;
      break;
    case kOpAnd:
      if (IS_UIMM16((value))) {
        opcode = kMipsAndi;
      } else {
        short_form = false;
        opcode = kMipsAnd;
      }
      break;
    case kOpOr:
      if (IS_UIMM16((value))) {
        opcode = kMipsOri;
      } else {
        short_form = false;
        opcode = kMipsOr;
      }
      break;
    case kOpXor:
      if (IS_UIMM16((value))) {
        opcode = kMipsXori;
      } else {
        short_form = false;
        opcode = kMipsXor;
      }
      break;
    case kOpMul:
      short_form = false;
      if (isaIsR6_) {
          opcode = kMipsR6Mul;
      } else {
          opcode = kMipsMul;
      }
      break;
    default:
      LOG(FATAL) << "Bad case in OpRegRegImm";
      break;
  }

  if (short_form) {
    res = NewLIR3(opcode, r_dest.GetReg(), r_src1.GetReg(), value);
  } else {
    if (r_dest != r_src1) {
      res = LoadConstant(r_dest, value);
      NewLIR3(opcode, r_dest.GetReg(), r_src1.GetReg(), r_dest.GetReg());
    } else {
      RegStorage r_scratch = AllocTemp();
      res = LoadConstant(r_scratch, value);
      NewLIR3(opcode, r_dest.GetReg(), r_src1.GetReg(), r_scratch.GetReg());
    }
  }
  return res;
}

LIR* MipsMir2Lir::OpRegReg(OpKind op, RegStorage r_dest_src1, RegStorage r_src2) {
  MipsOpCode opcode = kMipsNop;
  LIR *res;
  switch (op) {
    case kOpMov:
      opcode = kMipsMove;
      break;
    case kOpMvn:
      return NewLIR3(kMipsNor, r_dest_src1.GetReg(), r_src2.GetReg(), rZERO);
    case kOpNeg:
      return NewLIR3(kMipsSubu, r_dest_src1.GetReg(), rZERO, r_src2.GetReg());
    case kOpAdd:
    case kOpAnd:
    case kOpMul:
    case kOpOr:
    case kOpSub:
    case kOpXor:
      return OpRegRegReg(op, r_dest_src1, r_dest_src1, r_src2);
    case kOp2Byte:
      if (cu_->compiler_driver->GetInstructionSetFeatures()->AsMipsInstructionSetFeatures()
          ->IsMipsIsaRevGreaterThanEqual2()) {
        res = NewLIR2(kMipsSeb, r_dest_src1.GetReg(), r_src2.GetReg());
      } else {
        res = OpRegRegImm(kOpLsl, r_dest_src1, r_src2, 24);
        OpRegRegImm(kOpAsr, r_dest_src1, r_dest_src1, 24);
      }
      return res;
    case kOp2Short:
      if (cu_->compiler_driver->GetInstructionSetFeatures()->AsMipsInstructionSetFeatures()
          ->IsMipsIsaRevGreaterThanEqual2()) {
        res = NewLIR2(kMipsSeh, r_dest_src1.GetReg(), r_src2.GetReg());
      } else {
        res = OpRegRegImm(kOpLsl, r_dest_src1, r_src2, 16);
        OpRegRegImm(kOpAsr, r_dest_src1, r_dest_src1, 16);
      }
      return res;
    case kOp2Char:
      return NewLIR3(kMipsAndi, r_dest_src1.GetReg(), r_src2.GetReg(), 0xFFFF);
    default:
      LOG(FATAL) << "Bad case in OpRegReg";
      UNREACHABLE();
  }
  return NewLIR2(opcode, r_dest_src1.GetReg(), r_src2.GetReg());
}

LIR* MipsMir2Lir::OpMovRegMem(RegStorage r_dest, RegStorage r_base, int offset,
                              MoveType move_type) {
  UNUSED(r_dest, r_base, offset, move_type);
  UNIMPLEMENTED(FATAL);
  UNREACHABLE();
}

LIR* MipsMir2Lir::OpMovMemReg(RegStorage r_base, int offset, RegStorage r_src, MoveType move_type) {
  UNUSED(r_base, offset, r_src, move_type);
  UNIMPLEMENTED(FATAL);
  UNREACHABLE();
}

LIR* MipsMir2Lir::OpCondRegReg(OpKind op, ConditionCode cc, RegStorage r_dest, RegStorage r_src) {
  UNUSED(op, cc, r_dest, r_src);
  LOG(FATAL) << "Unexpected use of OpCondRegReg for MIPS";
  UNREACHABLE();
}

LIR* MipsMir2Lir::LoadConstantWide(RegStorage r_dest, int64_t value) {
  LIR *res;
  if (fpuIs32Bit_ || !r_dest.IsFloat()) {
    if (!r_dest.IsPair()) {
      // Form 64-bit pair
      r_dest = Solo64ToPair64(r_dest);
    }
    res = LoadConstantNoClobber(r_dest.GetLow(), Low32Bits(value));
    LoadConstantNoClobber(r_dest.GetHigh(), High32Bits(value));
  } else {
    // Here if have a 64bit FPU and loading the high word of a 64bit float.
    RegStorage r_temp = AllocTemp();
    r_dest = Fp64ToSolo32(r_dest);
    res = LoadConstantNoClobber(r_dest, Low32Bits(value));
    LoadConstantNoClobber(r_temp, High32Bits(value));
    NewLIR2(kMipsMthc1, r_temp.GetReg(), r_dest.GetReg());
    FreeTemp(r_temp);
  }
  return res;
}

/* Load value from base + scaled index. */
LIR* MipsMir2Lir::LoadBaseIndexed(RegStorage r_base, RegStorage r_index, RegStorage r_dest,
                                  int scale, OpSize size) {
  LIR *first = NULL;
  LIR *res;
  MipsOpCode opcode = kMipsNop;
  RegStorage t_reg = AllocTemp();

  if (r_dest.IsFloat()) {
    DCHECK(r_dest.IsSingle());
    DCHECK((size == k32) || (size == kSingle) || (size == kReference));
    size = kSingle;
  } else {
    if (size == kSingle)
      size = k32;
  }

  if (!scale) {
    first = NewLIR3(kMipsAddu, t_reg.GetReg() , r_base.GetReg(), r_index.GetReg());
  } else {
    first = OpRegRegImm(kOpLsl, t_reg, r_index, scale);
    NewLIR3(kMipsAddu, t_reg.GetReg() , r_base.GetReg(), t_reg.GetReg());
  }

  switch (size) {
    case kSingle:
      opcode = kMipsFlwc1;
      break;
    case k32:
    case kReference:
      opcode = kMipsLw;
      break;
    case kUnsignedHalf:
      opcode = kMipsLhu;
      break;
    case kSignedHalf:
      opcode = kMipsLh;
      break;
    case kUnsignedByte:
      opcode = kMipsLbu;
      break;
    case kSignedByte:
      opcode = kMipsLb;
      break;
    default:
      LOG(FATAL) << "Bad case in LoadBaseIndexed";
  }

  res = NewLIR3(opcode, r_dest.GetReg(), 0, t_reg.GetReg());
  FreeTemp(t_reg);
  return (first) ? first : res;
}

/* store value base base + scaled index. */
LIR* MipsMir2Lir::StoreBaseIndexed(RegStorage r_base, RegStorage r_index, RegStorage r_src,
                                   int scale, OpSize size) {
  LIR *first = NULL;
  MipsOpCode opcode = kMipsNop;
  RegStorage t_reg = AllocTemp();

  if (r_src.IsFloat()) {
    DCHECK(r_src.IsSingle());
    DCHECK((size == k32) || (size == kSingle) || (size == kReference));
    size = kSingle;
  } else {
    if (size == kSingle)
      size = k32;
  }

  if (!scale) {
    first = NewLIR3(kMipsAddu, t_reg.GetReg() , r_base.GetReg(), r_index.GetReg());
  } else {
    first = OpRegRegImm(kOpLsl, t_reg, r_index, scale);
    NewLIR3(kMipsAddu, t_reg.GetReg() , r_base.GetReg(), t_reg.GetReg());
  }

  switch (size) {
    case kSingle:
      opcode = kMipsFswc1;
      break;
    case k32:
    case kReference:
      opcode = kMipsSw;
      break;
    case kUnsignedHalf:
    case kSignedHalf:
      opcode = kMipsSh;
      break;
    case kUnsignedByte:
    case kSignedByte:
      opcode = kMipsSb;
      break;
    default:
      LOG(FATAL) << "Bad case in StoreBaseIndexed";
  }
  NewLIR3(opcode, r_src.GetReg(), 0, t_reg.GetReg());
  return first;
}

// FIXME: don't split r_dest into 2 containers.
LIR* MipsMir2Lir::LoadBaseDispBody(RegStorage r_base, int displacement, RegStorage r_dest,
                                   OpSize size) {
/*
 * Load value from base + displacement.  Optionally perform null check
 * on base (which must have an associated s_reg and MIR).  If not
 * performing null check, incoming MIR can be null. IMPORTANT: this
 * code must not allocate any new temps.  If a new register is needed
 * and base and dest are the same, spill some other register to
 * rlp and then restore.
 */
  LIR *res;
  LIR *load = NULL;
  LIR *load2 = NULL;
  MipsOpCode opcode = kMipsNop;
  bool short_form = IS_SIMM16(displacement);
  bool is64bit = false;

  switch (size) {
    case k64:
    case kDouble:
      is64bit = true;
      if (fpuIs32Bit_ && !r_dest.IsPair()) {
        // Form 64-bit pair
        r_dest = Solo64ToPair64(r_dest);
      }
      short_form = IS_SIMM16_2WORD(displacement);
      FALLTHROUGH_INTENDED;
    case k32:
    case kSingle:
    case kReference:
      opcode = kMipsLw;
      if (r_dest.IsFloat()) {
        opcode = kMipsFlwc1;
        if (!is64bit) {
          DCHECK(r_dest.IsSingle());
        } else {
          DCHECK(r_dest.IsDouble());
        }
      }
      DCHECK_EQ((displacement & 0x3), 0);
      break;
    case kUnsignedHalf:
      opcode = kMipsLhu;
      DCHECK_EQ((displacement & 0x1), 0);
      break;
    case kSignedHalf:
      opcode = kMipsLh;
      DCHECK_EQ((displacement & 0x1), 0);
      break;
    case kUnsignedByte:
      opcode = kMipsLbu;
      break;
    case kSignedByte:
      opcode = kMipsLb;
      break;
    default:
      LOG(FATAL) << "Bad case in LoadBaseIndexedBody";
  }

  if (short_form) {
    if (!is64bit) {
      load = res = NewLIR3(opcode, r_dest.GetReg(), displacement, r_base.GetReg());
    } else {
      if (fpuIs32Bit_ || !r_dest.IsFloat()) {
          DCHECK(r_dest.IsPair());
          load = res = NewLIR3(opcode, r_dest.GetLowReg(), displacement + LOWORD_OFFSET, r_base.GetReg());
          load2 = NewLIR3(opcode, r_dest.GetHighReg(), displacement + HIWORD_OFFSET, r_base.GetReg());
      } else {
          // Here if 64bit fpu and r_dest is a 64bit fp register
          RegStorage r_tmp = AllocTemp();
          // FIXME: why is r_dest a 64BitPair here???
          r_dest = Fp64ToSolo32(r_dest);
          load = res = NewLIR3(kMipsFlwc1, r_dest.GetReg(), displacement + LOWORD_OFFSET, r_base.GetReg());
          load2 = NewLIR3(kMipsLw, r_tmp.GetReg(), displacement + HIWORD_OFFSET, r_base.GetReg());
          NewLIR2(kMipsMthc1, r_tmp.GetReg(), r_dest.GetReg());
          FreeTemp(r_tmp);
      }
    }
  } else {
    if (!is64bit) {
      RegStorage r_tmp = (r_base == r_dest || r_dest.IsFloat()) ? AllocTemp() : r_dest;
      res = OpRegRegImm(kOpAdd, r_tmp, r_base, displacement);
      load = NewLIR3(opcode, r_dest.GetReg(), 0, r_tmp.GetReg());
      if (r_tmp != r_dest)
        FreeTemp(r_tmp);
    } else {
      RegStorage r_tmp = AllocTemp();
      res = OpRegRegImm(kOpAdd, r_tmp, r_base, displacement);
      if (fpuIs32Bit_ || !r_dest.IsFloat()) {
          DCHECK(r_dest.IsPair());
          load = NewLIR3(opcode, r_dest.GetLowReg(), LOWORD_OFFSET, r_tmp.GetReg());
          load2 = NewLIR3(opcode, r_dest.GetHighReg(), HIWORD_OFFSET, r_tmp.GetReg());
      } else {
          // Here if 64bit fpu and r_dest is a 64bit fp register
          r_dest = Fp64ToSolo32(r_dest);
          load = res = NewLIR3(kMipsFlwc1, r_dest.GetReg(), LOWORD_OFFSET, r_tmp.GetReg());
          load2 = NewLIR3(kMipsLw, r_tmp.GetReg(), HIWORD_OFFSET, r_tmp.GetReg());
          NewLIR2(kMipsMthc1, r_tmp.GetReg(), r_dest.GetReg());
      }
      FreeTemp(r_tmp);
    }
  }

  if (mem_ref_type_ == ResourceMask::kDalvikReg) {
    DCHECK_EQ(r_base, rs_rMIPS_SP);
    AnnotateDalvikRegAccess(load, (displacement + (is64bit ? LOWORD_OFFSET : 0)) >> 2,
                            true /* is_load */, is64bit /* is64bit */);
    if (is64bit) {
      AnnotateDalvikRegAccess(load2, (displacement + HIWORD_OFFSET) >> 2,
                              true /* is_load */, is64bit /* is64bit */);
    }
  }
  return load;
}

LIR* MipsMir2Lir::LoadBaseDisp(RegStorage r_base, int displacement, RegStorage r_dest,
                               OpSize size, VolatileKind is_volatile) {
  if (UNLIKELY(is_volatile == kVolatile && (size == k64 || size == kDouble))) {
    // Do atomic 64-bit load.
    return GenAtomic64Load(r_base, displacement, r_dest);
  }

  // TODO: base this on target.
  if (size == kWord) {
    size = k32;
  }
  LIR* load;
  load = LoadBaseDispBody(r_base, displacement, r_dest, size);

  if (UNLIKELY(is_volatile == kVolatile)) {
    GenMemBarrier(kLoadAny);
  }

  return load;
}

// FIXME: don't split r_dest into 2 containers.
LIR* MipsMir2Lir::StoreBaseDispBody(RegStorage r_base, int displacement,
                                    RegStorage r_src, OpSize size) {
  LIR *res;
  LIR *store = NULL;
  LIR *store2 = NULL;
  MipsOpCode opcode = kMipsNop;
  bool short_form = IS_SIMM16(displacement);
  bool is64bit = false;

  switch (size) {
    case k64:
    case kDouble:
      is64bit = true;
      if (fpuIs32Bit_ && !r_src.IsPair()) {
        // Form 64-bit pair
        r_src = Solo64ToPair64(r_src);
      }
      short_form = IS_SIMM16_2WORD(displacement);
      FALLTHROUGH_INTENDED;
    case k32:
    case kSingle:
    case kReference:
      opcode = kMipsSw;
      if (r_src.IsFloat()) {
        opcode = kMipsFswc1;
        if (!is64bit) {
          DCHECK(r_src.IsSingle());
        } else {
          DCHECK(r_src.IsDouble());
        }
      }
      DCHECK_EQ((displacement & 0x3), 0);
      break;
    case kUnsignedHalf:
    case kSignedHalf:
      opcode = kMipsSh;
      DCHECK_EQ((displacement & 0x1), 0);
      break;
    case kUnsignedByte:
    case kSignedByte:
      opcode = kMipsSb;
      break;
    default:
      LOG(FATAL) << "Bad case in StoreBaseDispBody";
  }

  if (short_form) {
    if (!is64bit) {
      store = res = NewLIR3(opcode, r_src.GetReg(), displacement, r_base.GetReg());
    } else {
      if (fpuIs32Bit_ || !r_src.IsFloat()) {
          DCHECK(r_src.IsPair());
          store = res = NewLIR3(opcode, r_src.GetLowReg(), displacement + LOWORD_OFFSET, r_base.GetReg());
          store2 = NewLIR3(opcode, r_src.GetHighReg(), displacement + HIWORD_OFFSET, r_base.GetReg());
      } else {
          // Here if 64bit fpu and r_src is a 64bit fp register
          RegStorage r_tmp = AllocTemp();
          r_src = Fp64ToSolo32(r_src);
          store = res = NewLIR3(kMipsFswc1, r_src.GetReg(), displacement + LOWORD_OFFSET, r_base.GetReg());
          NewLIR2(kMipsMfhc1, r_tmp.GetReg(), r_src.GetReg());
          store2 = NewLIR3(kMipsSw, r_tmp.GetReg(), displacement + HIWORD_OFFSET, r_base.GetReg());
          FreeTemp(r_tmp);
      }
    }
  } else {
    RegStorage r_scratch = AllocTemp();
    res = OpRegRegImm(kOpAdd, r_scratch, r_base, displacement);
    if (!is64bit) {
      store =  NewLIR3(opcode, r_src.GetReg(), 0, r_scratch.GetReg());
    } else {
      if (fpuIs32Bit_ || !r_src.IsFloat()) {
          DCHECK(r_src.IsPair());
          store = NewLIR3(opcode, r_src.GetLowReg(), LOWORD_OFFSET, r_scratch.GetReg());
          store2 = NewLIR3(opcode, r_src.GetHighReg(), HIWORD_OFFSET, r_scratch.GetReg());
      } else {
          // Here if 64bit fpu and r_src is a 64bit fp register
          RegStorage r_tmp = AllocTemp();
          r_src = Fp64ToSolo32(r_src);
          store = NewLIR3(kMipsFswc1, r_src.GetReg(), LOWORD_OFFSET, r_scratch.GetReg());
          NewLIR2(kMipsMfhc1, r_tmp.GetReg(), r_src.GetReg());
          store2 = NewLIR3(kMipsSw, r_tmp.GetReg(), HIWORD_OFFSET, r_scratch.GetReg());
          FreeTemp(r_tmp);
      }
    }
    FreeTemp(r_scratch);
  }

  if (mem_ref_type_ == ResourceMask::kDalvikReg) {
    DCHECK_EQ(r_base, rs_rMIPS_SP);
    AnnotateDalvikRegAccess(store, (displacement + (is64bit ? LOWORD_OFFSET : 0)) >> 2,
                            false /* is_load */, is64bit /* is64bit */);
    if (is64bit) {
      AnnotateDalvikRegAccess(store2, (displacement + HIWORD_OFFSET) >> 2,
                              false /* is_load */, is64bit /* is64bit */);
    }
  }

  return res;
}

LIR* MipsMir2Lir::StoreBaseDisp(RegStorage r_base, int displacement, RegStorage r_src,
                                OpSize size, VolatileKind is_volatile) {
  if (is_volatile == kVolatile) {
    // Ensure that prior accesses become visible to other threads first.
    GenMemBarrier(kAnyStore);
  }

  LIR* store;
  if (UNLIKELY(is_volatile == kVolatile && (size == k64 || size == kDouble))) {
    // Do atomic 64-bit load.
    store = GenAtomic64Store(r_base, displacement, r_src);
  } else {
    // TODO: base this on target.
    if (size == kWord) {
      size = k32;
    }
    store = StoreBaseDispBody(r_base, displacement, r_src, size);
  }

  if (UNLIKELY(is_volatile == kVolatile)) {
    // Preserve order with respect to any subsequent volatile loads.
    // We need StoreLoad, but that generally requires the most expensive barrier.
    GenMemBarrier(kAnyAny);
  }

  return store;
}

LIR* MipsMir2Lir::OpMem(OpKind op, RegStorage r_base, int disp) {
  UNUSED(op, r_base, disp);
  LOG(FATAL) << "Unexpected use of OpMem for MIPS";
  UNREACHABLE();
}

LIR* MipsMir2Lir::OpCondBranch(ConditionCode cc, LIR* target) {
  UNUSED(cc, target);
  LOG(FATAL) << "Unexpected use of OpCondBranch for MIPS";
  UNREACHABLE();
}

LIR* MipsMir2Lir::InvokeTrampoline(OpKind op, RegStorage r_tgt, QuickEntrypointEnum trampoline) {
  if (IsDirectEntrypoint(trampoline)) {
    // Reserve argument space on stack (for $a0-$a3) for
    // entrypoints that directly reference native implementations.
    // This is not safe in general, as it violates the frame size
    // of the Quick method, but it is used here only for calling
    // native functions, outside of the runtime.
    OpRegImm(kOpSub, rs_rSP, 16);
    LIR* retVal = OpReg(op, r_tgt);
    OpRegImm(kOpAdd, rs_rSP, 16);
    return retVal;
  }

  return OpReg(op, r_tgt);
}

}  // namespace art
