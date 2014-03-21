/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#include <stdint.h>

#include "arm64_lir.h"
#include "codegen_arm64.h"
#include "dex/quick/mir_to_lir-inl.h"


namespace art {

/* This file contains codegen for the A64 ISA. */

static int32_t EncodeImmSingle(uint32_t bits) {
  /*
   * Valid values will have the form:
   *
   *   aBbb.bbbc.defg.h000.0000.0000.0000.0000
   *
   * where B = not(b). In other words, if b == 1, then B == 0 and viceversa.
   */

  // bits[19..0] are cleared.
  if ((bits & 0x0007ffff) != 0)
    return -1;

  // bits[29..25] are all set or all cleared.
  uint32_t b_pattern = (bits >> 16) & 0x3e00;
  if (b_pattern != 0 && b_pattern != 0x3e00)
    return -1;

  // bit[30] and bit[29] are opposite.
  if (((bits ^ (bits << 1)) & 0x40000000) == 0)
    return -1;

  // bits: aBbb.bbbc.defg.h000.0000.0000.0000.0000
  // bit7: a000.0000
  uint32_t bit7 = ((bits >> 31) & 0x1) << 7;
  // bit6: 0b00.0000
  uint32_t bit6 = ((bits >> 29) & 0x1) << 6;
  // bit5_to_0: 00cd.efgh
  uint32_t bit5_to_0 = (bits >> 19) & 0x3f;
  return (bit7 | bit6 | bit5_to_0);
}

static int32_t EncodeImmDouble(uint64_t bits) {
  /*
   * Valid values will have the form:
   *
   *   aBbb.bbbb.bbcd.efgh.0000.0000.0000.0000
   *   0000.0000.0000.0000.0000.0000.0000.0000
   *
   * where B = not(b).
   */

  // bits[47..0] are cleared.
  if ((bits & UINT64_C(0xffffffffffff)) != 0)
    return -1;

  // bits[61..54] are all set or all cleared.
  uint32_t b_pattern = (bits >> 48) & 0x3fc0;
  if (b_pattern != 0 && b_pattern != 0x3fc0)
    return -1;

  // bit[62] and bit[61] are opposite.
  if (((bits ^ (bits << 1)) & UINT64_C(0x4000000000000000)) == 0)
    return -1;

  // bit7: a000.0000
  uint32_t bit7 = ((bits >> 63) & 0x1) << 7;
  // bit6: 0b00.0000
  uint32_t bit6 = ((bits >> 61) & 0x1) << 6;
  // bit5_to_0: 00cd.efgh
  uint32_t bit5_to_0 = (bits >> 48) & 0x3f;
  return (bit7 | bit6 | bit5_to_0);
}

LIR* Arm64Mir2Lir::LoadFPConstantValue(int r_dest, int32_t value) {
  DCHECK(ARM_SINGLEREG(r_dest));

  if (value == 0) {
    return NewLIR2(kA64Fmov2sw, r_dest, rARM_ZR);
  } else {
    int32_t encoded_imm = EncodeImmSingle((uint32_t)value);
    if (encoded_imm >= 0) {
      return NewLIR2(kA64Fmov2fI, r_dest, encoded_imm);
    }
  }

  LIR* data_target = ScanLiteralPool(literal_list_, value, 0);
  if (data_target == NULL) {
    data_target = AddWordData(&literal_list_, value);
  }

  LIR* load_pc_rel = RawLIR(current_dalvik_offset_, kA64Ldr2fp,
                            r_dest, 0, 0, 0, 0, data_target);
  SetMemRefType(load_pc_rel, true, kLiteral);
  AppendLIR(load_pc_rel);
  return load_pc_rel;
}

LIR* Arm64Mir2Lir::LoadFPConstantValueWide(int r_dest, int64_t value) {
  DCHECK(ARM_DOUBLEREG(r_dest));
  if (value == 0) {
    return NewLIR2(kA64Fmov2Sx, r_dest, rARM_ZR);
  } else {
    int32_t encoded_imm = EncodeImmDouble(value);
    if (encoded_imm >= 0) {
      return NewLIR2(FWIDE(kA64Fmov2fI), r_dest, encoded_imm);
    }
  }

  // No short form - load from the literal pool.
  int32_t val_lo = Low32Bits(value);
  int32_t val_hi = High32Bits(value);
  LIR* data_target = ScanLiteralPoolWide(literal_list_, val_lo, val_hi);
  if (data_target == NULL) {
    data_target = AddWideData(&literal_list_, val_lo, val_hi);
  }

  DCHECK(ARM_FPREG(r_dest));
  LIR* load_pc_rel = RawLIR(current_dalvik_offset_, FWIDE(kA64Ldr2fp),
                            r_dest, 0, 0, 0, 0, data_target);
  SetMemRefType(load_pc_rel, true, kLiteral);
  AppendLIR(load_pc_rel);
  return load_pc_rel;
}

/*
 * Determine whether value can be encoded as a Thumb2 modified
 * immediate.  If not, return -1.  If so, return i:imm3:a:bcdefgh form.
 */
int Arm64Mir2Lir::ModifiedImmediate(uint32_t value) {
  return -1;
}

static int CountLeadingZeros(bool is_wide, uint64_t value) {
  return (is_wide) ? __builtin_clzl(value) : __builtin_clz((uint32_t)value);
}

static int CountTrailingZeros(bool is_wide, uint64_t value) {
  return (is_wide) ? __builtin_ctzl(value) : __builtin_ctz((uint32_t)value);
}

static int CountSetBits(bool is_wide, uint64_t value) {
  return ((is_wide) ?
          __builtin_popcountl(value) : __builtin_popcount((uint32_t)value));
}

/**
 * @brief Try encoding an immediate in the form required by logical instructions.
 *
 * @param is_wide Whether @p value is a 64-bit (as opposed to 32-bit) value.
 * @param value An integer to be encoded. This is interpreted as 64-bit if @p is_wide is true and as
 *   32-bit if @p is_wide is false.
 * @return A non-negative integer containing the encoded immediate or -1 if the encoding failed.
 * @note This is the inverse of Arm64Mir2Lir::DecodeLogicalImmediate().
 */
int Arm64Mir2Lir::EncodeLogicalImmediate(bool is_wide, uint64_t value) {
  unsigned n, imm_s, imm_r;

  // Logical immediates are encoded using parameters n, imm_s and imm_r using
  // the following table:
  //
  //  N   imms    immr    size        S             R
  //  1  ssssss  rrrrrr    64    UInt(ssssss)  UInt(rrrrrr)
  //  0  0sssss  xrrrrr    32    UInt(sssss)   UInt(rrrrr)
  //  0  10ssss  xxrrrr    16    UInt(ssss)    UInt(rrrr)
  //  0  110sss  xxxrrr     8    UInt(sss)     UInt(rrr)
  //  0  1110ss  xxxxrr     4    UInt(ss)      UInt(rr)
  //  0  11110s  xxxxxr     2    UInt(s)       UInt(r)
  // (s bits must not be all set)
  //
  // A pattern is constructed of size bits, where the least significant S+1
  // bits are set. The pattern is rotated right by R, and repeated across a
  // 32 or 64-bit value, depending on destination register width.
  //
  // To test if an arbitary immediate can be encoded using this scheme, an
  // iterative algorithm is used.
  //

  // 1. If the value has all set or all clear bits, it can't be encoded.
  if (value == 0 || value == ~UINT64_C(0) ||
      (!is_wide && (uint32_t)value == ~UINT32_C(0))) {
    return -1;
  }

  unsigned lead_zero  = CountLeadingZeros(is_wide, value);
  unsigned lead_one   = CountLeadingZeros(is_wide, ~value);
  unsigned trail_zero = CountTrailingZeros(is_wide, value);
  unsigned trail_one  = CountTrailingZeros(is_wide, ~value);
  unsigned set_bits   = CountSetBits(is_wide, value);

  // The fixed bits in the immediate s field.
  // If width == 64 (X reg), start at 0xFFFFFF80.
  // If width == 32 (W reg), start at 0xFFFFFFC0, as the iteration for 64-bit
  // widths won't be executed.
  unsigned width = (is_wide) ? 64 : 32;
  int imm_s_fixed = (is_wide) ? -128 : -64;
  int imm_s_mask = 0x3f;

  for (;;) {
    // 2. If the value is two bits wide, it can be encoded.
    if (width == 2) {
      n = 0;
      imm_s = 0x3C;
      imm_r = (value & 3) - 1;
      break;
    }

    n = (width == 64) ? 1 : 0;
    imm_s = ((imm_s_fixed | (set_bits - 1)) & imm_s_mask);
    if ((lead_zero + set_bits) == width) {
      imm_r = 0;
    } else {
      imm_r = (lead_zero > 0) ? (width - trail_zero) : lead_one;
    }

    // 3. If the sum of leading zeros, trailing zeros and set bits is
    //    equal to the bit width of the value, it can be encoded.
    if (lead_zero + trail_zero + set_bits == width) {
      break;
    }

    // 4. If the sum of leading ones, trailing ones and unset bits in the
    //    value is equal to the bit width of the value, it can be encoded.
    if (lead_one + trail_one + (width - set_bits) == width) {
      break;
    }

    // 5. If the most-significant half of the bitwise value is equal to
    //    the least-significant half, return to step 2 using the
    //    least-significant half of the value.
    uint64_t mask = (UINT64_C(1) << (width >> 1)) - 1;
    if ((value & mask) == ((value >> (width >> 1)) & mask)) {
      width >>= 1;
      set_bits >>= 1;
      imm_s_fixed >>= 1;
      continue;
    }

    // 6. Otherwise, the value can't be encoded.
    return -1;
  }

  return (n << 12 | imm_r << 6 | imm_s);
}

bool Arm64Mir2Lir::InexpensiveConstantInt(int32_t value) {
  return (ModifiedImmediate(value) >= 0) || (ModifiedImmediate(~value) >= 0);
}

bool Arm64Mir2Lir::InexpensiveConstantFloat(int32_t value) {
  return EncodeImmSingle(value) >= 0;
}

bool Arm64Mir2Lir::InexpensiveConstantLong(int64_t value) {
  return InexpensiveConstantInt(High32Bits(value)) && InexpensiveConstantInt(Low32Bits(value));
}

bool Arm64Mir2Lir::InexpensiveConstantDouble(int64_t value) {
  return EncodeImmDouble(value) >= 0;
}

/*
 * Load a immediate using one single instruction when possible; otherwise
 * use a pair of movz and movk instructions.
 *
 * No additional register clobbering operation performed. Use this version when
 * 1) r_dest is freshly returned from AllocTemp or
 * 2) The codegen is under fixed register usage
 */
LIR* Arm64Mir2Lir::LoadConstantNoClobber(int r_dest, int value) {
  LIR* res;

  if (ARM_FPREG(r_dest)) {
    return LoadFPConstantValue(r_dest, value);
  }

  // Loading SP/ZR with an immediate is not supported.
  DCHECK_NE(r_dest, rARM_SP);
  DCHECK_NE(r_dest, rARM_ZR);

  // Compute how many movk, movz instructions are needed to load the value.
  uint16_t high_bits = High16Bits(value);
  uint16_t low_bits = Low16Bits(value);

  bool low_fast = ((uint16_t)(low_bits + 1) <= 1);
  bool high_fast = ((uint16_t)(high_bits + 1) <= 1);

  if (LIKELY(low_fast || high_fast)) {
    // 1 instruction is enough to load the immediate.
    if (LIKELY(low_bits == high_bits)) {
      // Value is either 0 or -1: we can just use wzr.
      ArmOpcode opcode = LIKELY(low_bits == 0) ? kA64Mov2rr : kA64Mvn2rr;
      res = NewLIR2(opcode, r_dest, rARM_ZR);
    } else {
      uint16_t uniform_bits, useful_bits;
      int shift;

      if (LIKELY(high_fast)) {
        shift = 0;
        uniform_bits = high_bits;
        useful_bits = low_bits;
      } else {
        shift = 1;
        uniform_bits = low_bits;
        useful_bits = high_bits;
      }

      if (UNLIKELY(uniform_bits != 0)) {
        res = NewLIR3(kA64Movn3rdM, r_dest, ~useful_bits, shift);
      } else {
        res = NewLIR3(kA64Movz3rdM, r_dest, useful_bits, shift);
      }
    }
  } else {
    // movk, movz require 2 instructions. Try detecting logical immediates.
    int log_imm = EncodeLogicalImmediate(/*is_wide=*/false, value);
    if (log_imm >= 0) {
      res = NewLIR3(kA64Orr3Rrl, r_dest, rARM_ZR, log_imm);
    } else {
      // Use 2 instructions.
      res = NewLIR3(kA64Movz3rdM, r_dest, low_bits, 0);
      NewLIR3(kA64Movk3rdM, r_dest, high_bits, 1);
    }
  }

  return res;
}

LIR* Arm64Mir2Lir::OpUnconditionalBranch(LIR* target) {
  LIR* res = NewLIR1(kA64BUncond, 0 /* offset to be patched  during assembly*/);
  res->target = target;
  return res;
}

LIR* Arm64Mir2Lir::OpCondBranch(ConditionCode cc, LIR* target) {
  LIR* branch = NewLIR2(kA64BCond, ArmConditionEncoding(cc),
                        0 /* offset to be patched */);
  branch->target = target;
  return branch;
}

LIR* Arm64Mir2Lir::OpReg(OpKind op, int r_dest_src) {
  ArmOpcode opcode = kA64BrkI16;
  switch (op) {
    case kOpBlx:
#if WITH_A64_HOST_SIMULATOR == 1
      opcode = kA64x86BlR;
#else
      opcode = kA64Blr1r;
#endif
      break;
    // TODO(Arm64): port kThumbBx.
    // case kOpBx:
    //   opcode = kThumbBx;
    //   break;
    default:
      LOG(FATAL) << "Bad opcode " << op;
  }
  return NewLIR1(opcode, r_dest_src);
}

LIR* Arm64Mir2Lir::OpRegRegShift(OpKind op, int r_dest_src1, int r_src2,
                                 int shift) {
  bool is_wide = OP_KIND_IS_WIDE(op);
  ArmOpcode wide = (is_wide) ? WIDE(0) : UNWIDE(0);
  ArmOpcode opcode = kA64BrkI16;

  switch (OP_KIND_UNWIDE(op)) {
    case kOpCmn:
      opcode = kA64Cmn3Rro;
      break;
    case kOpCmp:
      opcode = kA64Cmp3Rro;
      break;
    case kOpMov:
      opcode = kA64Mov2rr;
      break;
    case kOpMvn:
      opcode = kA64Mvn2rr;
      break;
    case kOpNeg:
      opcode = kA64Neg3rro;
      break;
    case kOpTst:
      opcode = kA64Tst3rro;
      break;
    case kOpRev:
      DCHECK_EQ(shift, 0);
      // Binary, but rm is encoded twice.
      return NewLIR3(kA64Rev2rr | wide, r_dest_src1, r_src2, r_src2);
      break;
    case kOpRevsh:
      // Binary, but rm is encoded twice.
      return NewLIR3(kA64Rev162rr | wide, r_dest_src1, r_src2, r_src2);
      break;
    case kOp2Byte:
      DCHECK_EQ(shift, ENCODE_NO_SHIFT);
      // "sbfx r1, r2, #imm1, #imm2" is "sbfm r1, r2, #imm1, #(imm1 + imm2 - 1)".
      // For now we use sbfm directly.
      return NewLIR4(kA64Sbfm4rrdd | wide, r_dest_src1, r_src2, 0, 7);
    case kOp2Short:
      DCHECK_EQ(shift, ENCODE_NO_SHIFT);
      // For now we use sbfm rather than its alias, sbfx.
      return NewLIR4(kA64Sbfm4rrdd | wide, r_dest_src1, r_src2, 0, 15);
    case kOp2Char:
      // "ubfx r1, r2, #imm1, #imm2" is "ubfm r1, r2, #imm1, #(imm1 + imm2 - 1)".
      // For now we use ubfm directly.
      DCHECK_EQ(shift, ENCODE_NO_SHIFT);
      return NewLIR4(kA64Ubfm4rrdd | wide, r_dest_src1, r_src2, 0, 15);
    default:
      return OpRegRegRegShift(op, r_dest_src1, r_dest_src1, r_src2, shift);
  }

  DCHECK(!IsPseudoLirOp(opcode));
  if (EncodingMap[opcode].flags & IS_BINARY_OP) {
    DCHECK_EQ(shift, ENCODE_NO_SHIFT);
    return NewLIR2(opcode | wide, r_dest_src1, r_src2);
  } else if (EncodingMap[opcode].flags & IS_TERTIARY_OP) {
    ArmEncodingKind kind = EncodingMap[opcode].field_loc[2].kind;
    if (kind == kFmtExtShift || kind == kFmtShift) {
      return NewLIR3(opcode | wide, r_dest_src1, r_src2, shift);
    }
  }

  LOG(FATAL) << "Unexpected encoding operand count";
  return NULL;
}

LIR* Arm64Mir2Lir::OpRegReg(OpKind op, int r_dest_src1, int r_src2) {
  return OpRegRegShift(op, r_dest_src1, r_src2, ENCODE_NO_SHIFT);
}

LIR* Arm64Mir2Lir::OpMovRegMem(int r_dest, int r_base, int offset, MoveType move_type) {
  UNIMPLEMENTED(FATAL);
  return nullptr;
}

LIR* Arm64Mir2Lir::OpMovMemReg(int r_base, int offset, int r_src, MoveType move_type) {
  UNIMPLEMENTED(FATAL);
  return nullptr;
}

LIR* Arm64Mir2Lir::OpCondRegReg(OpKind op, ConditionCode cc, int r_dest, int r_src) {
  LOG(FATAL) << "Unexpected use of OpCondRegReg for Arm";
  return NULL;
}

LIR* Arm64Mir2Lir::OpRegRegRegShift(OpKind op, int r_dest, int r_src1,
                                    int r_src2, int shift) {
  ArmOpcode opcode = kA64BrkI16;
  bool is_wide = OP_KIND_IS_WIDE(op);

  switch (OP_KIND_UNWIDE(op)) {
    case kOpAdd:
      opcode = kA64Add4rrro;
      break;
    case kOpSub:
      opcode = kA64Sub4rrro;
      break;
    /*case kOpRsub:
      opcode = kA64RsubWWW;
      break;*/
    case kOpAdc:
      opcode = kA64Adc3rrr;
      break;
    case kOpAnd:
      opcode = kA64And4rrro;
      break;
    case kOpXor:
      opcode = kA64Eor4rrro;
      break;
    case kOpMul:
      opcode = kA64Mul3rrr;
      break;
    case kOpDiv:
      opcode = kA64Sdiv3rrr;
      break;
    case kOpOr:
      opcode = kA64Orr4rrro;
      break;
    case kOpSbc:
      opcode = kA64Sbc3rrr;
      break;
    case kOpLsl:
      opcode = kA64Lsl3rrr;
      break;
    case kOpLsr:
      opcode = kA64Lsr3rrr;
      break;
    case kOpAsr:
      opcode = kA64Asr3rrr;
      break;
    case kOpRor:
      opcode = kA64Ror3rrr;
      break;
    default:
      LOG(FATAL) << "Bad opcode: " << op;
      break;
  }

  // Check correct usage of the sp register.
  DCHECK(IsExtendEncoding(shift) || r_dest != rARM_SP);
  DCHECK(IsExtendEncoding(shift) || r_src1 != rARM_SP);
  DCHECK_NE(r_src2, rARM_SP);

  // The instructions above belong to two kinds:
  // - 4-operands instructions, where the last operand is a shift/extend immediate,
  // - 3-operands instructions with no shift/extend.
  ArmOpcode widened_opcode = (is_wide) ? WIDE(opcode) : opcode;
  if (EncodingMap[opcode].flags & IS_QUAD_OP) {
    DCHECK_EQ(shift, ENCODE_NO_SHIFT);
    return NewLIR4(widened_opcode, r_dest, r_src1, r_src2, shift);
  } else {
    DCHECK(EncodingMap[opcode].flags & IS_TERTIARY_OP);
    DCHECK_EQ(shift, ENCODE_NO_SHIFT);
    return NewLIR3(widened_opcode, r_dest, r_src1, r_src2);
  }
}

LIR* Arm64Mir2Lir::OpRegRegReg(OpKind op, int r_dest, int r_src1, int r_src2) {
  return OpRegRegRegShift(op, r_dest, r_src1, r_src2, ENCODE_NO_SHIFT);
}

LIR* Arm64Mir2Lir::OpRegRegImm(OpKind op, int r_dest, int r_src1, int value) {
  LIR* res;
  bool neg = (value < 0);
  int64_t abs_value = (neg) ? -value : value;
  ArmOpcode opcode = kA64BrkI16;
  ArmOpcode alt_opcode = kA64BrkI16;
  int32_t log_imm = -1;
  bool is_wide = OP_KIND_IS_WIDE(op);
  ArmOpcode wide = (is_wide) ? WIDE(0) : UNWIDE(0);

  switch (OP_KIND_UNWIDE(op)) {
    case kOpLsl: {
      // "lsl w1, w2, #imm" is an alias of "ubfm w1, w2, #(-imm MOD 32), #(31-imm)"
      // and "lsl x1, x2, #imm" of "ubfm x1, x2, #(-imm MOD 32), #(31-imm)".
      // For now, we just use ubfm directly.
      int max_value = (is_wide) ? 64 : 32;
      return NewLIR4(kA64Ubfm4rrdd | wide, r_dest, r_src1,
                     (-value) & (max_value - 1), max_value - value);
    }
    case kOpLsr:
      return NewLIR3(kA64Lsr3rrd | wide, r_dest, r_src1, value);
    case kOpAsr:
      return NewLIR3(kA64Asr3rrd | wide, r_dest, r_src1, value);
    case kOpRor:
      // "ror r1, r2, #imm" is an alias of "extr r1, r2, r2, #imm".
      // For now, we just use extr directly.
      return NewLIR4(kA64Extr4rrrd | wide, r_dest, r_src1, r_src1, value);
    case kOpAdd:
      neg = !neg;
      // Note: intentional fallthrough
    case kOpSub:
      // Add and sub below read/write sp rather than xzr.
      DCHECK_NE(r_dest, rARM_ZR);
      DCHECK_NE(r_src1, rARM_ZR);
      if (abs_value < 0x1000) {
        opcode = (neg) ? kA64Add4RRdT : kA64Sub4RRdT;
        return NewLIR4(opcode | wide, r_dest, r_src1, abs_value, 0);
      } else if ((abs_value & UINT64_C(0xfff)) == 0 && ((abs_value >> 12) < 0x1000)) {
        opcode = (neg) ? kA64Add4RRdT : kA64Sub4RRdT;
        return NewLIR4(opcode | wide, r_dest, r_src1, abs_value >> 12, 1);
      } else {
        log_imm = -1;
        alt_opcode = (neg) ? kA64Add4rrro : kA64Sub4rrro;
      }
      break;
    /*case kOpRsub:
      opcode = kThumb2RsubRRI8M;
      alt_opcode = kThumb2RsubRRR;
      break;*/
    case kOpAdc:
      log_imm = -1;
      alt_opcode = kA64Adc3rrr;
      break;
    case kOpSbc:
      log_imm = -1;
      alt_opcode = kA64Sbc3rrr;
      break;
    case kOpOr:
      log_imm = EncodeLogicalImmediate(is_wide, value);
      opcode = kA64Orr3Rrl;
      alt_opcode = kA64Orr4rrro;
      break;
    case kOpAnd:
      log_imm = EncodeLogicalImmediate(is_wide, value);
      opcode = kA64And3Rrl;
      alt_opcode = kA64And4rrro;
      break;
    case kOpXor:
      log_imm = EncodeLogicalImmediate(is_wide, value);
      opcode = kA64Eor3Rrl;
      alt_opcode = kA64Eor4rrro;
      break;
    case kOpMul:
      // TUNING: power of 2, shift & add
      log_imm = -1;
      alt_opcode = kA64Mul3rrr;
      break;
    default:
      LOG(FATAL) << "Bad opcode: " << op;
  }

  if (log_imm >= 0) {
    return NewLIR3(opcode | wide, r_dest, r_src1, log_imm);
  } else {
    int r_scratch = AllocTemp();
    LoadConstant(r_scratch, value);
    if (EncodingMap[alt_opcode].flags & IS_QUAD_OP)
      res = NewLIR4(alt_opcode | wide, r_dest, r_src1, r_scratch, 0);
    else
      res = NewLIR3(alt_opcode | wide, r_dest, r_src1, r_scratch);
    FreeTemp(r_scratch);
    return res;
  }
}

LIR* Arm64Mir2Lir::OpRegImm(OpKind op, int r_dest_src1, intptr_t value) {
  ArmOpcode opcode = kA64BrkI16;
  ArmOpcode neg_opcode = kA64BrkI16;
  bool shift;
  bool neg = (value < 0);
  uint64_t abs_value = (neg) ? -value : value;

  if (LIKELY(abs_value < 0x1000)) {
    // abs_value is a 12-bit immediate.
    shift = false;
  } else if ((abs_value & UINT64_C(0xfff)) == 0 && ((abs_value >> 12) < 0x1000)) {
    // abs_value is a shifted 12-bit immediate.
    shift = true;
    abs_value >>= 12;
  } else {
    int r_tmp = AllocTemp();
    LIR* res = LoadConstant(r_tmp, value);
    OpRegReg(op, r_dest_src1, r_tmp);
    FreeTemp(r_tmp);
    return res;
  }

  switch (OP_KIND_UNWIDE(op)) {
    case kOpAdd:
      neg_opcode = kA64Sub4RRdT;
      opcode = kA64Add4RRdT;
      break;
    case kOpSub:
      neg_opcode = kA64Add4RRdT;
      opcode = kA64Sub4RRdT;
      break;
    case kOpCmp:
      neg_opcode = kA64Cmn3RdT;
      opcode = kA64Cmp3RdT;
      break;
    default:
      LOG(FATAL) << "Bad op-kind in OpRegImm: " << op;
      break;
  }

  if (UNLIKELY(neg))
    opcode = neg_opcode;

  if (OP_KIND_IS_WIDE(op))
    opcode = WIDE(opcode);

  if (EncodingMap[opcode].flags & IS_QUAD_OP)
    return NewLIR4(opcode, r_dest_src1, r_dest_src1, abs_value, (shift) ? 1 : 0);
  else
    return NewLIR3(opcode, r_dest_src1, abs_value, (shift) ? 1 : 0);
}

LIR* Arm64Mir2Lir::LoadConstantWide(int r_dest_lo, int r_dest_hi, int64_t value) {
  int target_reg = S2d(r_dest_lo, r_dest_hi);
  if (ARM_FPREG(r_dest_lo)) {
    return LoadFPConstantValueWide(target_reg, value);
  } else {
    // No short form - load from the literal pool.
    int32_t val_lo = Low32Bits(value);
    int32_t val_hi = High32Bits(value);
    LIR* data_target = ScanLiteralPoolWide(literal_list_, val_lo, val_hi);
    if (data_target == NULL) {
      data_target = AddWideData(&literal_list_, val_lo, val_hi);
    }

    LIR* res = RawLIR(current_dalvik_offset_, /*kThumb2LdrdPcRel8*/ kA64BrkI16,
                      r_dest_lo, r_dest_hi, r15pc, 0, 0, data_target);
    SetMemRefType(res, true, kLiteral);
    AppendLIR(res);
    return res;
  }
}

int Arm64Mir2Lir::EncodeShift(int shift_type, int amount) {
  return ((shift_type & 0x3) << 7) | (amount & 0x1f);
}

int Arm64Mir2Lir::EncodeExtend(int extend_type, int amount) {
  return  (1 << 6) | ((extend_type & 0x7) << 3) | (amount & 0x7);
}

bool Arm64Mir2Lir::IsExtendEncoding(int encoded_value) {
  return (1 << 6) & encoded_value;
}

LIR* Arm64Mir2Lir::LoadBaseIndexed(int rBase, int r_index, int r_dest,
                                   int scale, OpSize size) {
  bool all_low_regs = true;
  LIR* load;
  ArmOpcode opcode = kA64BrkI16;
  bool thumb_form = (all_low_regs && (scale == 0));
  int reg_ptr;

  if (ARM_FPREG(r_dest)) {
    if (ARM_SINGLEREG(r_dest)) {
      DCHECK((size == kWord) || (size == kSingle));
      opcode = kA64Ldr3fXD;
      // TODO(Arm64): ^^^ review this.
      size = kSingle;
    } else {
      DCHECK(ARM_DOUBLEREG(r_dest));
      DCHECK((size == kLong) || (size == kDouble));
      DCHECK_EQ((r_dest & 0x1), 0);
      opcode = FWIDE(kA64Ldr3fXD);
      // TODO(Arm64): ^^^ review this.
      size = kDouble;
    }
  } else {
    if (size == kSingle)
      size = kWord;
  }

  switch (size) {
    case kDouble:  // fall-through
    case kSingle:
      reg_ptr = AllocTemp();
      if (scale) {
        NewLIR4(kA64Add4rrro, reg_ptr, rBase, r_index,
                EncodeShift(kA64Lsl, scale));
      } else {
        OpRegRegReg(kOpAdd, reg_ptr, rBase, r_index);
      }
      load = NewLIR3(opcode, r_dest, reg_ptr, 0);
      FreeTemp(reg_ptr);
      return load;
    case kWord:
      opcode = (thumb_form) ? kThumbLdrRRR : kThumb2LdrRRR;
      break;
    case kUnsignedHalf:
      opcode = (thumb_form) ? kThumbLdrhRRR : kThumb2LdrhRRR;
      break;
    case kSignedHalf:
      opcode = (thumb_form) ? kThumbLdrshRRR : kThumb2LdrshRRR;
      break;
    case kUnsignedByte:
      opcode = (thumb_form) ? kThumbLdrbRRR : kThumb2LdrbRRR;
      break;
    case kSignedByte:
      opcode = (thumb_form) ? kThumbLdrsbRRR : kThumb2LdrsbRRR;
      break;
    default:
      LOG(FATAL) << "Bad size: " << size;
  }
  if (thumb_form)
    load = NewLIR3(opcode, r_dest, rBase, r_index);
  else
    load = NewLIR4(opcode, r_dest, rBase, r_index, scale);

  return load;
}

LIR* Arm64Mir2Lir::StoreBaseIndexed(int rBase, int r_index, int r_src,
                                  int scale, OpSize size) {
  LIR* store;
  ArmOpcode opcode = kA64BrkI16;
  int reg_ptr;

  if (ARM_FPREG(r_src)) {
    if (ARM_SINGLEREG(r_src)) {
      DCHECK((size == kWord) || (size == kSingle));
      // opcode = kThumb2Vstrs;
      size = kSingle;
    } else {
      DCHECK(ARM_DOUBLEREG(r_src));
      DCHECK((size == kLong) || (size == kDouble));
      DCHECK_EQ((r_src & 0x1), 0);
      // opcode = kThumb2Vstrd;
      size = kDouble;
    }
  } else {
    if (size == kSingle)
      size = kWord;
  }

  switch (size) {
    case kDouble:  // fall-through
    case kSingle:
      reg_ptr = AllocTemp();
      if (scale) {
        NewLIR4(kA64Add4rrro, reg_ptr, rBase, r_index, EncodeShift(kA64Lsl, scale));
      } else {
        OpRegRegReg(kOpAdd, reg_ptr, rBase, r_index);
      }
      store = NewLIR3(opcode, r_src, reg_ptr, 0);
      FreeTemp(reg_ptr);
      return store;
    case kWord:
      opcode = kA64StrWXX;
      break;
    case kUnsignedHalf:
    case kSignedHalf:
      opcode = kThumb2StrhRRR;
      break;
    case kUnsignedByte:
    case kSignedByte:
      opcode = kThumb2StrbRRR;
      break;
    default:
      LOG(FATAL) << "Bad size: " << size;
  }

  return NewLIR4(opcode, r_src, rBase, r_index, scale);
}

/*
 * Load value from base + displacement.  Optionally perform null check
 * on base (which must have an associated s_reg and MIR).  If not
 * performing null check, incoming MIR can be null.
 */
LIR* Arm64Mir2Lir::LoadBaseDispBody(int rBase, int displacement, int r_dest,
                                    int r_dest_hi, OpSize size, int s_reg) {
  LIR* load = NULL;
  ArmOpcode opcode = kA64BrkI16;
  bool short_form = false;
  bool thumb2Form = (displacement < 4092 && displacement >= 0);
  bool all_low_regs = true;
  int encoded_disp = displacement;
  bool is64bit = false;
  bool already_generated = false;
  switch (size) {
    case kDouble:
    case kLong:
      is64bit = true;
      DCHECK_EQ(encoded_disp & 0x3, 0);
      if (ARM_FPREG(r_dest)) {
        if (ARM_SINGLEREG(r_dest)) {
          DCHECK(ARM_FPREG(r_dest_hi));
          r_dest = S2d(r_dest, r_dest_hi);
        }
        // Currently double values may be misaligned.
        if ((displacement & 0x7) == 0 && displacement >= 0 && displacement <= 32760) {
          // Can use scaled load.
          opcode = FWIDE(kA64Ldr3fXD);
          encoded_disp >>= 3;
          short_form = true;
        } else if (IS_SIGNED_IMM9(displacement)) {
          // Can use unscaled load.
          opcode = FWIDE(kA64Ldur3fXd);
          short_form = true;
        } else {
          short_form = false;
        }
        break;
      } else {
        encoded_disp >>= 2;
        if (IS_SIGNED_IMM7(encoded_disp)) {
          load = NewLIR4(kA64LdpWWXI7, r_dest, r_dest_hi, rBase, encoded_disp);
        } else {
          load = LoadBaseDispBody(rBase, displacement, r_dest, -1, kWord, s_reg);
          LoadBaseDispBody(rBase, displacement + 4, r_dest_hi,
                           -1, kWord, INVALID_SREG);
        }
        already_generated = true;
      }
    case kSingle:
    case kWord:
      if (ARM_FPREG(r_dest)) {
        opcode = kA64Ldr3fXD;
        if (displacement <= 1020) {
          short_form = true;
          encoded_disp >>= 2;
        }
        break;
      }
      if ((rBase == r15pc) &&
          (displacement <= 1020) && (displacement >= 0)) {
        short_form = true;
        encoded_disp >>= 2;
        opcode = kThumbLdrPcRel;
      } else if (displacement <= 16380 && displacement >= 0) {
        DCHECK_EQ((displacement & 0x3), 0);
        short_form = true;
        encoded_disp >>= 2;
        opcode = kA64LdrWXI12;
      }
      break;
    case kUnsignedHalf:
      if (all_low_regs && displacement < 64 && displacement >= 0) {
        DCHECK_EQ((displacement & 0x1), 0);
        short_form = true;
        encoded_disp >>= 1;
        opcode = kThumbLdrhRRI5;
      } else if (displacement < 4092 && displacement >= 0) {
        short_form = true;
        opcode = kThumb2LdrhRRI12;
      }
      break;
    case kSignedHalf:
      if (thumb2Form) {
        short_form = true;
        opcode = kThumb2LdrshRRI12;
      }
      break;
    case kUnsignedByte:
      if (all_low_regs && displacement < 32 && displacement >= 0) {
        short_form = true;
        opcode = kThumbLdrbRRI5;
      } else if (thumb2Form) {
        short_form = true;
        opcode = kThumb2LdrbRRI12;
      }
      break;
    case kSignedByte:
      if (thumb2Form) {
        short_form = true;
        opcode = kThumb2LdrsbRRI12;
      }
      break;
    default:
      LOG(FATAL) << "Bad size: " << size;
  }

  if (!already_generated) {
    if (short_form) {
      load = NewLIR3(opcode, r_dest, rBase, encoded_disp);
    } else {
      int reg_offset = AllocTemp();
      LoadConstant(reg_offset, encoded_disp);
      load = LoadBaseIndexed(rBase, reg_offset, r_dest, 0, size);
      FreeTemp(reg_offset);
    }
  }

  // TODO: in future may need to differentiate Dalvik accesses w/ spills
  if (rBase == rARM_SP) {
    AnnotateDalvikRegAccess(load, displacement >> 2, true /* is_load */, is64bit);
  }
  return load;
}

LIR* Arm64Mir2Lir::LoadBaseDisp(int rBase, int displacement, int r_dest,
                              OpSize size, int s_reg) {
  return LoadBaseDispBody(rBase, displacement, r_dest, -1, size, s_reg);
}

LIR* Arm64Mir2Lir::LoadBaseDispWide(int rBase, int displacement, int r_dest_lo,
                                  int r_dest_hi, int s_reg) {
  return LoadBaseDispBody(rBase, displacement, r_dest_lo, r_dest_hi, kLong, s_reg);
}


LIR* Arm64Mir2Lir::StoreBaseDispBody(int rBase, int displacement,
                                     int r_src, int r_src_hi, OpSize size) {
  LIR* store = NULL;
  ArmOpcode opcode = kA64BrkI16;
  bool short_form = false;
  bool thumb2Form = (displacement < 4092 && displacement >= 0);
  int encoded_disp = displacement;
  bool is64bit = false;
  bool already_generated = false;
  switch (size) {
    case kLong:
    case kDouble:
      is64bit = true;
      DCHECK_EQ(encoded_disp & 0x3, 0);
      if (!ARM_FPREG(r_src)) {
        encoded_disp >>= 2;
        if (IS_SIGNED_IMM7(encoded_disp)) {
          store = NewLIR4(kA64StpWWXI7, r_src, r_src_hi, rBase, encoded_disp);
        } else {
          store = StoreBaseDispBody(rBase, displacement, r_src, -1, kWord);
          StoreBaseDispBody(rBase, displacement + 4, r_src_hi, -1, kWord);
        }
        already_generated = true;
      } else {
        if (ARM_SINGLEREG(r_src)) {
          DCHECK(ARM_FPREG(r_src_hi));
          r_src = S2d(r_src, r_src_hi);
        }

        // Currently double values may be misaligned.
        if ((displacement & 0x7) == 0 && displacement >= 0 && displacement <= 32760) {
          // Can use scaled store.
          opcode = FWIDE(kA64Str3fXD);
          encoded_disp >>= 3;
          short_form = true;
        } else if (IS_SIGNED_IMM9(displacement)) {
          // Can use unscaled store.
          opcode = FWIDE(kA64Stur3fXd);
          short_form = true;
        } else {
          short_form = false;
        }
      }
      break;
    case kSingle:
    case kWord:
      if (ARM_FPREG(r_src)) {
        DCHECK(ARM_SINGLEREG(r_src));
        DCHECK_EQ(encoded_disp & 0x3, 0);
        opcode = kA64Str3fXD;
        if (displacement <= 1020) {
          short_form = true;
          encoded_disp >>= 2;
        }
        break;
      }

      if (displacement <= 16380 && displacement >= 0) {
        DCHECK_EQ((displacement & 0x3), 0);
        short_form = true;
        encoded_disp >>= 2;
        opcode = kA64StrWXI12;
      }
      break;
    case kUnsignedHalf:
    case kSignedHalf:
      if (displacement < 64 && displacement >= 0) {
        DCHECK_EQ((displacement & 0x1), 0);
        short_form = true;
        encoded_disp >>= 1;
        opcode = kThumbStrhRRI5;
      } else if (thumb2Form) {
        short_form = true;
        opcode = kThumb2StrhRRI12;
      }
      break;
    case kUnsignedByte:
    case kSignedByte:
      if (displacement < 32 && displacement >= 0) {
        short_form = true;
        opcode = kThumbStrbRRI5;
      } else if (thumb2Form) {
        short_form = true;
        opcode = kThumb2StrbRRI12;
      }
      break;
    default:
      LOG(FATAL) << "Bad size: " << size;
  }

  if (!already_generated) {
    if (short_form) {
      store = NewLIR3(opcode, r_src, rBase, encoded_disp);
    } else {
      int r_scratch = AllocTemp();
      LoadConstant(r_scratch, encoded_disp);
      store = StoreBaseIndexed(rBase, r_scratch, r_src, 0, size);
      FreeTemp(r_scratch);
    }
  }

  // TODO: In future, may need to differentiate Dalvik & spill accesses
  if (rBase == rARM_SP) {
    AnnotateDalvikRegAccess(store, displacement >> 2, false /* is_load */, is64bit);
  }
  return store;
}

LIR* Arm64Mir2Lir::StoreBaseDisp(int rBase, int displacement, int r_src,
                               OpSize size) {
  return StoreBaseDispBody(rBase, displacement, r_src, -1, size);
}

LIR* Arm64Mir2Lir::StoreBaseDispWide(int rBase, int displacement,
                                   int r_src_lo, int r_src_hi) {
  return StoreBaseDispBody(rBase, displacement, r_src_lo, r_src_hi, kLong);
}

LIR* Arm64Mir2Lir::OpFpRegCopy(int r_dest, int r_src) {
  int opcode = kA64Fmov2ff;
  if (ARM_DOUBLEREG(r_dest)) {
    DCHECK(ARM_DOUBLEREG(r_src));
    opcode = FWIDE(opcode);
  } else if (ARM_SINGLEREG(r_dest)) {
    DCHECK(!ARM_DOUBLEREG(r_src));
    opcode = ARM_SINGLEREG(r_src) ? opcode : kThumb2Fmsr;
  } else {
    DCHECK(ARM_SINGLEREG(r_src));
    opcode = kThumb2Fmrs;
  }

  LIR* res = RawLIR(current_dalvik_offset_, opcode, r_dest, r_src);
  if (!(cu_->disable_opt & (1 << kSafeOptimizations)) && r_dest == r_src) {
    res->flags.is_nop = true;
  }
  return res;
}

LIR* Arm64Mir2Lir::OpThreadMem(OpKind op, ThreadOffset thread_offset) {
  LOG(FATAL) << "Unexpected use of OpThreadMem for Arm";
  return NULL;
}

LIR* Arm64Mir2Lir::OpMem(OpKind op, int rBase, int disp) {
  LOG(FATAL) << "Unexpected use of OpMem for Arm";
  return NULL;
}

LIR* Arm64Mir2Lir::StoreBaseIndexedDisp(int rBase, int r_index, int scale,
                                      int displacement, int r_src, int r_src_hi, OpSize size,
                                      int s_reg) {
  LOG(FATAL) << "Unexpected use of StoreBaseIndexedDisp for Arm";
  return NULL;
}

LIR* Arm64Mir2Lir::OpRegMem(OpKind op, int r_dest, int rBase, int offset) {
  LOG(FATAL) << "Unexpected use of OpRegMem for Arm";
  return NULL;
}

LIR* Arm64Mir2Lir::LoadBaseIndexedDisp(int rBase, int r_index, int scale,
                                     int displacement, int r_dest, int r_dest_hi, OpSize size,
                                     int s_reg) {
  LOG(FATAL) << "Unexpected use of LoadBaseIndexedDisp for Arm";
  return NULL;
}

}  // namespace art
