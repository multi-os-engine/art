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

#include "assembler_thumb2.h"

#include "base/logging.h"
#include "entrypoints/quick/quick_entrypoints.h"
#include "offsets.h"
#include "thread.h"
#include "utils.h"

namespace art {
namespace arm {

void Thumb2Assembler::and_(Register rd, Register rn, const ShifterOperand& so,
                        Condition cond) {
  EmitDataProcessing(cond, AND, 0, rn, rd, so);
}


void Thumb2Assembler::eor(Register rd, Register rn, const ShifterOperand& so,
                       Condition cond) {
  EmitDataProcessing(cond, EOR, 0, rn, rd, so);
}


void Thumb2Assembler::sub(Register rd, Register rn, const ShifterOperand& so,
                       Condition cond) {
  EmitDataProcessing(cond, SUB, 0, rn, rd, so);
}

void Thumb2Assembler::rsb(Register rd, Register rn, const ShifterOperand& so,
                       Condition cond) {
  EmitDataProcessing(cond, RSB, 0, rn, rd, so);
}

void Thumb2Assembler::rsbs(Register rd, Register rn, const ShifterOperand& so,
                        Condition cond) {
  EmitDataProcessing(cond, RSB, 1, rn, rd, so);
}


void Thumb2Assembler::add(Register rd, Register rn, const ShifterOperand& so,
                       Condition cond) {
  EmitDataProcessing(cond, ADD, 0, rn, rd, so);
}


void Thumb2Assembler::adds(Register rd, Register rn, const ShifterOperand& so,
                        Condition cond) {
  EmitDataProcessing(cond, ADD, 1, rn, rd, so);
}


void Thumb2Assembler::subs(Register rd, Register rn, const ShifterOperand& so,
                        Condition cond) {
  EmitDataProcessing(cond, SUB, 1, rn, rd, so);
}


void Thumb2Assembler::adc(Register rd, Register rn, const ShifterOperand& so,
                       Condition cond) {
  EmitDataProcessing(cond, ADC, 0, rn, rd, so);
}


void Thumb2Assembler::sbc(Register rd, Register rn, const ShifterOperand& so,
                       Condition cond) {
  EmitDataProcessing(cond, SBC, 0, rn, rd, so);
}


void Thumb2Assembler::rsc(Register rd, Register rn, const ShifterOperand& so,
                       Condition cond) {
  EmitDataProcessing(cond, RSC, 0, rn, rd, so);
}


void Thumb2Assembler::tst(Register rn, const ShifterOperand& so, Condition cond) {
  CHECK_NE(rn, PC);  // Reserve tst pc instruction for exception handler marker.
  EmitDataProcessing(cond, TST, 1, rn, R0, so);
}


void Thumb2Assembler::teq(Register rn, const ShifterOperand& so, Condition cond) {
  CHECK_NE(rn, PC);  // Reserve teq pc instruction for exception handler marker.
  EmitDataProcessing(cond, TEQ, 1, rn, R0, so);
}


void Thumb2Assembler::cmp(Register rn, const ShifterOperand& so, Condition cond) {
  EmitDataProcessing(cond, CMP, 1, rn, R0, so);
}


void Thumb2Assembler::cmn(Register rn, const ShifterOperand& so, Condition cond) {
  EmitDataProcessing(cond, CMN, 1, rn, R0, so);
}


void Thumb2Assembler::orr(Register rd, Register rn,
                    const ShifterOperand& so, Condition cond) {
  EmitDataProcessing(cond, ORR, 0, rn, rd, so);
}


void Thumb2Assembler::orrs(Register rd, Register rn,
                        const ShifterOperand& so, Condition cond) {
  EmitDataProcessing(cond, ORR, 1, rn, rd, so);
}


void Thumb2Assembler::mov(Register rd, const ShifterOperand& so, Condition cond) {
  EmitDataProcessing(cond, MOV, 0, R0, rd, so);
}


void Thumb2Assembler::movs(Register rd, const ShifterOperand& so, Condition cond) {
  EmitDataProcessing(cond, MOV, 1, R0, rd, so);
}


void Thumb2Assembler::bic(Register rd, Register rn, const ShifterOperand& so,
                       Condition cond) {
  EmitDataProcessing(cond, BIC, 0, rn, rd, so);
}


void Thumb2Assembler::mvn(Register rd, const ShifterOperand& so, Condition cond) {
  EmitDataProcessing(cond, MVN, 0, R0, rd, so);
}


void Thumb2Assembler::mvns(Register rd, const ShifterOperand& so, Condition cond) {
  EmitDataProcessing(cond, MVN, 1, R0, rd, so);
}


void Thumb2Assembler::mul(Register rd, Register rn, Register rm, Condition cond) {
  // Assembler registers rd, rn, rm are encoded as rn, rm, rs.
  EmitMulOp(cond, 0, R0, rd, rn, rm);
}


void Thumb2Assembler::mla(Register rd, Register rn, Register rm, Register ra,
                       Condition cond) {
  // Assembler registers rd, rn, rm, ra are encoded as rn, rm, rs, rd.
  EmitMulOp(cond, B21, ra, rd, rn, rm);
}


void Thumb2Assembler::mls(Register rd, Register rn, Register rm, Register ra,
                       Condition cond) {
  // Assembler registers rd, rn, rm, ra are encoded as rn, rm, rs, rd.
  EmitMulOp(cond, B22 | B21, ra, rd, rn, rm);
}


void Thumb2Assembler::umull(Register rd_lo, Register rd_hi, Register rn,
                         Register rm, Condition cond) {
  // Assembler registers rd_lo, rd_hi, rn, rm are encoded as rd, rn, rm, rs.
  EmitMulOp(cond, B23, rd_lo, rd_hi, rn, rm);
}


void Thumb2Assembler::ldr(Register rd, const Address& ad, Condition cond) {
  EmitLoadStore(cond, true, false, false, false, rd, ad);
}


void Thumb2Assembler::str(Register rd, const Address& ad, Condition cond) {
  EmitLoadStore(cond, false, false, false, false, rd, ad);
}


void Thumb2Assembler::ldrb(Register rd, const Address& ad, Condition cond) {
  EmitLoadStore(cond, true, true, false, false, rd, ad);
}


void Thumb2Assembler::strb(Register rd, const Address& ad, Condition cond) {
  EmitLoadStore(cond, false, true, false, false, rd, ad);
}


void Thumb2Assembler::ldrh(Register rd, const Address& ad, Condition cond) {
  EmitLoadStore(cond, true, false, true, false, rd, ad);
}


void Thumb2Assembler::strh(Register rd, const Address& ad, Condition cond) {
  EmitLoadStore(cond, false, false, true, false, rd, ad);
}


void Thumb2Assembler::ldrsb(Register rd, const Address& ad, Condition cond) {
  EmitLoadStore(cond, true, true, false, true, rd, ad);
}


void Thumb2Assembler::ldrsh(Register rd, const Address& ad, Condition cond) {
  EmitLoadStore(cond, true, false, true, true, rd, ad);
}


void Thumb2Assembler::ldrd(Register rd, const Address& ad, Condition cond) {
  CHECK_EQ(rd % 2, 0);
  // This is different from other loads.  The encoding is like ARM.
  int32_t encoding = B31 | B30 | B29 | B27 | B22 | B20 |
      static_cast<int32_t>(rd) << 12 |
      (static_cast<int32_t>(rd) + 1) << 8 |
      ad.encodingThumbLdrdStrd();
  Emit(encoding);
}


void Thumb2Assembler::strd(Register rd, const Address& ad, Condition cond) {
  CHECK_EQ(rd % 2, 0);
  // This is different from other loads.  The encoding is like ARM.
  int32_t encoding = B31 | B30 | B29 | B27 | B22 |
      static_cast<int32_t>(rd) << 12 |
      (static_cast<int32_t>(rd) + 1) << 8 |
      ad.encodingThumbLdrdStrd();
  Emit(encoding);
}


void Thumb2Assembler::ldm(BlockAddressMode am,
                       Register base,
                       RegList regs,
                       Condition cond) {
  EmitMultiMemOp(cond, am, true, base, regs);
}


void Thumb2Assembler::stm(BlockAddressMode am,
                       Register base,
                       RegList regs,
                       Condition cond) {
  EmitMultiMemOp(cond, am, false, base, regs);
}


bool Thumb2Assembler::vmovs(SRegister sd, float s_imm, Condition cond) {
  uint32_t imm32 = bit_cast<uint32_t, float>(s_imm);
  if (((imm32 & ((1 << 19) - 1)) == 0) &&
      ((((imm32 >> 25) & ((1 << 6) - 1)) == (1 << 5)) ||
       (((imm32 >> 25) & ((1 << 6) - 1)) == ((1 << 5) -1)))) {
    uint8_t imm8 = ((imm32 >> 31) << 7) | (((imm32 >> 29) & 1) << 6) |
        ((imm32 >> 19) & ((1 << 6) -1));
    EmitVFPsss(cond, B23 | B21 | B20 | ((imm8 >> 4)*B16) | (imm8 & 0xf),
               sd, S0, S0);
    return true;
  }
  return false;
}


bool Thumb2Assembler::vmovd(DRegister dd, double d_imm, Condition cond) {
  uint64_t imm64 = bit_cast<uint64_t, double>(d_imm);
  if (((imm64 & ((1LL << 48) - 1)) == 0) &&
      ((((imm64 >> 54) & ((1 << 9) - 1)) == (1 << 8)) ||
       (((imm64 >> 54) & ((1 << 9) - 1)) == ((1 << 8) -1)))) {
    uint8_t imm8 = ((imm64 >> 63) << 7) | (((imm64 >> 61) & 1) << 6) |
        ((imm64 >> 48) & ((1 << 6) -1));
    EmitVFPddd(cond, B23 | B21 | B20 | ((imm8 >> 4)*B16) | B8 | (imm8 & 0xf),
               dd, D0, D0);
    return true;
  }
  return false;
}


void Thumb2Assembler::vadds(SRegister sd, SRegister sn, SRegister sm,
                         Condition cond) {
  EmitVFPsss(cond, B21 | B20, sd, sn, sm);
}


void Thumb2Assembler::vaddd(DRegister dd, DRegister dn, DRegister dm,
                         Condition cond) {
  EmitVFPddd(cond, B21 | B20, dd, dn, dm);
}


void Thumb2Assembler::vsubs(SRegister sd, SRegister sn, SRegister sm,
                         Condition cond) {
  EmitVFPsss(cond, B21 | B20 | B6, sd, sn, sm);
}


void Thumb2Assembler::vsubd(DRegister dd, DRegister dn, DRegister dm,
                         Condition cond) {
  EmitVFPddd(cond, B21 | B20 | B6, dd, dn, dm);
}


void Thumb2Assembler::vmuls(SRegister sd, SRegister sn, SRegister sm,
                         Condition cond) {
  EmitVFPsss(cond, B21, sd, sn, sm);
}


void Thumb2Assembler::vmuld(DRegister dd, DRegister dn, DRegister dm,
                         Condition cond) {
  EmitVFPddd(cond, B21, dd, dn, dm);
}


void Thumb2Assembler::vmlas(SRegister sd, SRegister sn, SRegister sm,
                         Condition cond) {
  EmitVFPsss(cond, 0, sd, sn, sm);
}


void Thumb2Assembler::vmlad(DRegister dd, DRegister dn, DRegister dm,
                         Condition cond) {
  EmitVFPddd(cond, 0, dd, dn, dm);
}


void Thumb2Assembler::vmlss(SRegister sd, SRegister sn, SRegister sm,
                         Condition cond) {
  EmitVFPsss(cond, B6, sd, sn, sm);
}


void Thumb2Assembler::vmlsd(DRegister dd, DRegister dn, DRegister dm,
                         Condition cond) {
  EmitVFPddd(cond, B6, dd, dn, dm);
}


void Thumb2Assembler::vdivs(SRegister sd, SRegister sn, SRegister sm,
                         Condition cond) {
  EmitVFPsss(cond, B23, sd, sn, sm);
}


void Thumb2Assembler::vdivd(DRegister dd, DRegister dn, DRegister dm,
                         Condition cond) {
  EmitVFPddd(cond, B23, dd, dn, dm);
}


void Thumb2Assembler::vabss(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B7 | B6, sd, S0, sm);
}


void Thumb2Assembler::vabsd(DRegister dd, DRegister dm, Condition cond) {
  EmitVFPddd(cond, B23 | B21 | B20 | B7 | B6, dd, D0, dm);
}


void Thumb2Assembler::vnegs(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B16 | B6, sd, S0, sm);
}


void Thumb2Assembler::vnegd(DRegister dd, DRegister dm, Condition cond) {
  EmitVFPddd(cond, B23 | B21 | B20 | B16 | B6, dd, D0, dm);
}


void Thumb2Assembler::vsqrts(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B16 | B7 | B6, sd, S0, sm);
}

void Thumb2Assembler::vsqrtd(DRegister dd, DRegister dm, Condition cond) {
  EmitVFPddd(cond, B23 | B21 | B20 | B16 | B7 | B6, dd, D0, dm);
}


void Thumb2Assembler::vcvtsd(SRegister sd, DRegister dm, Condition cond) {
  EmitVFPsd(cond, B23 | B21 | B20 | B18 | B17 | B16 | B8 | B7 | B6, sd, dm);
}


void Thumb2Assembler::vcvtds(DRegister dd, SRegister sm, Condition cond) {
  EmitVFPds(cond, B23 | B21 | B20 | B18 | B17 | B16 | B7 | B6, dd, sm);
}


void Thumb2Assembler::vcvtis(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B19 | B18 | B16 | B7 | B6, sd, S0, sm);
}


void Thumb2Assembler::vcvtid(SRegister sd, DRegister dm, Condition cond) {
  EmitVFPsd(cond, B23 | B21 | B20 | B19 | B18 | B16 | B8 | B7 | B6, sd, dm);
}


void Thumb2Assembler::vcvtsi(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B19 | B7 | B6, sd, S0, sm);
}


void Thumb2Assembler::vcvtdi(DRegister dd, SRegister sm, Condition cond) {
  EmitVFPds(cond, B23 | B21 | B20 | B19 | B8 | B7 | B6, dd, sm);
}


void Thumb2Assembler::vcvtus(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B19 | B18 | B7 | B6, sd, S0, sm);
}


void Thumb2Assembler::vcvtud(SRegister sd, DRegister dm, Condition cond) {
  EmitVFPsd(cond, B23 | B21 | B20 | B19 | B18 | B8 | B7 | B6, sd, dm);
}


void Thumb2Assembler::vcvtsu(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B19 | B6, sd, S0, sm);
}


void Thumb2Assembler::vcvtdu(DRegister dd, SRegister sm, Condition cond) {
  EmitVFPds(cond, B23 | B21 | B20 | B19 | B8 | B6, dd, sm);
}


void Thumb2Assembler::vcmps(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B18 | B6, sd, S0, sm);
}


void Thumb2Assembler::vcmpd(DRegister dd, DRegister dm, Condition cond) {
  EmitVFPddd(cond, B23 | B21 | B20 | B18 | B6, dd, D0, dm);
}


void Thumb2Assembler::vcmpsz(SRegister sd, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B18 | B16 | B6, sd, S0, S0);
}


void Thumb2Assembler::vcmpdz(DRegister dd, Condition cond) {
  EmitVFPddd(cond, B23 | B21 | B20 | B18 | B16 | B6, dd, D0, D0);
}

void Thumb2Assembler::b(Label* label, Condition cond) {
  EmitBranch(cond, label, false, false);
}


void Thumb2Assembler::bl(Label* label, Condition cond) {
  CHECK_EQ(cond, AL);
  EmitBranch(cond, label, true, false);
}

void Thumb2Assembler::blx(Label* label) {
  EmitBranch(AL, label, true, true);
}

void Thumb2Assembler::MarkExceptionHandler(Label* label) {
  EmitDataProcessing(AL, TST, 1, PC, R0, ShifterOperand(0));
  Label l;
  b(&l);
  EmitBranch(AL, label, false, false);
  Bind(&l);
}

void Thumb2Assembler::EncodeUint32InTstInstructions(uint32_t data) {
  // TODO: Consider using movw ip, <16 bits>.
  while (!IsUint(8, data)) {
    tst(R0, ShifterOperand(data & 0xFF), VS);
    data >>= 8;
  }
  tst(R0, ShifterOperand(data), MI);
}


void Thumb2Assembler::Emit(int32_t value) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  buffer_.Emit<int16_t>(value >> 16);
  buffer_.Emit<int16_t>(value & 0xffff);
}

void Thumb2Assembler::Emit16(int16_t value) {
  AssemblerBuffer::EnsureCapacity ensured(&buffer_);
  buffer_.Emit<int16_t>(value);
}

static int LeadingZeros(uint32_t val) {
  uint32_t alt;
  int32_t n;
  int32_t count;

  count = 16;
  n = 32;
  do {
    alt = val >> count;
    if (alt != 0) {
      n = n - count;
      val = alt;
    }
    count >>= 1;
  } while (count);
  return n - val;
}


int Thumb2Assembler::ModifiedImmediate(uint32_t value) {
  int32_t z_leading;
  int32_t z_trailing;
  uint32_t b0 = value & 0xff;

  /* Note: case of value==0 must use 0:000:0:0000000 encoding */
  if (value <= 0xFF)
    return b0;  // 0:000:a:bcdefgh
  if (value == ((b0 << 16) | b0))
    return (0x1 << 12) | b0; /* 0:001:a:bcdefgh */
  if (value == ((b0 << 24) | (b0 << 16) | (b0 << 8) | b0))
    return (0x3 << 12) | b0; /* 0:011:a:bcdefgh */
  b0 = (value >> 8) & 0xff;
  std::cout << std::hex << ((b0 << 24) | (b0 << 8)) << "\n";
  if (value == ((b0 << 24) | (b0 << 8)))
    return (0x2 << 12) | b0; /* 0:010:a:bcdefgh */
  /* Can we do it with rotation? */
  z_leading = LeadingZeros(value);
  z_trailing = 32 - LeadingZeros(~value & (value - 1));
  /* A run of eight or fewer active bits? */
  if ((z_leading + z_trailing) < 24)
    return -1;  /* No - bail */
  /* left-justify the constant, discarding msb (known to be 1) */
  value <<= z_leading + 1;
  /* Create bcdefgh */
  value >>= 25;

  /* Put it all together */
  uint32_t v = 8 + z_leading;

  uint32_t i = (v & 0b10000) >> 4;
  uint32_t imm3 = (v >> 1) & 0b111;
  uint32_t a = v & 1;
  return value | i << 26 | imm3 << 12 | a << 7;
}

bool Thumb2Assembler::Is32BitDataProcessing(Condition cond,
                              Opcode opcode,
                              int set_cc,
                              Register rn,
                              Register rd,
                              const ShifterOperand& so) {
  bool can_contain_high_register = opcode == MOV || opcode == ADD || opcode == SUB;

  if (IsHighRegister(rd) || IsHighRegister(rn)) {
    if (can_contain_high_register) {
      // There are high register instructions available for this opcode.
      return true;
    }
  }

  if (so.IsRegister() && IsHighRegister(so.GetRegister()) && !can_contain_high_register) {
    return true;
  }

  // Check for MOV with an ROR.
  if (opcode == MOV && so.IsRegister() && so.IsShift() && so.GetShift() == ROR) {
    if (so.GetImmediate() != 0) {
      return true;
    }
  }

  bool rn_is_valid = true;

  // Check for single operand instructions and ADD/SUB.
  switch (opcode) {
    case CMP:
    case MOV:
    case TST:
    case MVN:
      rn_is_valid = false;      // There is no Rn for these instructions.
      break;
    case TEQ:
      return true;
      break;
    case ADD:
    case SUB:
      break;
    default:
      if (so.IsRegister() && rd != rn) {
        return true;
      }
  }

  if (so.IsImmediate()) {
    if (rn_is_valid && rn != rd) {
      // The only thumb1 instruction with a register and an immediate are ADD and SUB.  The
      // immediate must be 3 bits.
      if (opcode != ADD && opcode != SUB) {
        return true;
      } else {
        // Check that the immediate is 3 bits for ADD and SUB.
        if (so.GetImmediate() >= 8) {
          return true;
        }
      }
    } else {
      // ADD, SUB, CMP and MOV may be thumb1 only if the immediate is 8 bits.
      if (!(opcode == ADD || opcode == SUB || opcode == MOV || opcode == CMP)) {
        return true;
      } else {
        if (so.GetImmediate() > 255) {
          return true;
        }
      }
    }
  }

  // The instruction can be encoded in 16 bits.
  return false;
}

void Thumb2Assembler::Emit32BitDataProcessing(Condition cond,
                              Opcode opcode,
                              int set_cc,
                              Register rn,
                              Register rd,
                              const ShifterOperand& so) {
  uint8_t thumb_opcode = 0b11111111;
  switch (opcode) {
    case AND: thumb_opcode = 0b0000; break;
    case EOR: thumb_opcode = 0b0100; break;
    case SUB: thumb_opcode = 0b1101; break;
    case RSB: thumb_opcode = 0b1110; break;
    case ADD: thumb_opcode = 0b1000; break;
    case ADC: thumb_opcode = 0b1010; break;
    case SBC: thumb_opcode = 0b1011; break;
    case RSC: break;
    case TST: thumb_opcode = 0b0000; set_cc = true; rd = PC; break;
    case TEQ: thumb_opcode = 0b0100; set_cc = true; rd = PC; break;
    case CMP: thumb_opcode = 0b1101; set_cc = true; rd = PC; break;
    case CMN: thumb_opcode = 0b1000; set_cc = true; rd = PC; break;
    case ORR: thumb_opcode = 0b0010; break;
    case MOV: thumb_opcode = 0b0010; rn = PC; break;
    case BIC: thumb_opcode = 0b0001; break;
    case MVN: thumb_opcode = 0b0011; rn = PC; break;
    default:
      break;
  }

  if (thumb_opcode == 0b11111111) {
    LOG(FATAL) << "Invalid thumb2 opcode " << opcode;
  }

  // Thumb2 encoding
  int32_t encoding = 0;
  if (so.IsImmediate()) {
     // Modified immediate
    uint32_t imm = ModifiedImmediate(so.encodingThumb(2));
    if (imm == static_cast<uint32_t>(-1)) {
      LOG(FATAL) << "Immediate value cannot fit in thumb2 modified immediate";
    }
    encoding = B31 | B30 | B29 | B28 |
         thumb_opcode << 21 |
         set_cc << 20 |
         rn << 16 |
         rd << 8 |
         imm;
  } else if (so.IsRegister()) {
     // Register (possibly shifted)
     encoding = B31 | B30 | B29 | B27 | B25 |
         thumb_opcode << 21 |
         set_cc << 20 |
         rn << 16 |
         rd << 8 |
         so.encodingThumb(2);
  }
  Emit(encoding);
}

void Thumb2Assembler::Emit16BitDataProcessing(Condition cond,
                              Opcode opcode,
                              int set_cc,
                              Register rn,
                              Register rd,
                              const ShifterOperand& so) {
  uint8_t thumb_opcode = 0b11111111;
  // Thumb1
  uint8_t dp_opcode = 0b01;
  uint8_t opcode_shift = 6;
  uint8_t rd_shift = 0;
  uint8_t rn_shift = 3;
  uint8_t immediate_shift = 0;
  bool use_immediate = false;
  uint8_t immediate = 0;

  if (opcode == MOV && so.IsRegister() && so.IsShift()) {
    // Convert shifted mov operand2 into 16 bit opcodes.
    dp_opcode = 0;
    opcode_shift = 11;

    use_immediate = true;
    immediate = so.GetImmediate();
    immediate_shift = 6;

    rn = so.GetRegister();

    switch (so.GetShift()) {
    case LSL: thumb_opcode = 0b00; break;
    case LSR: thumb_opcode = 0b01; break;
    case ASR: thumb_opcode = 0b10; break;
    case ROR:
      // ROR doesn't allow immediates.
      thumb_opcode = 0b111;
      dp_opcode = 0b01;
      opcode_shift = 6;
      use_immediate = false;
      break;
    case RRX: break;
    default:
     break;
    }
  } else {
    if (so.IsImmediate()) {
      use_immediate = true;
      immediate = so.GetImmediate();
    }

    switch (opcode) {
      case AND: thumb_opcode = 0b0000; break;
      case EOR: thumb_opcode = 0b0001; break;
      case SUB:
        dp_opcode = 0;
        if (so.IsRegister()) {
          // T1.
          opcode_shift = 9;
          thumb_opcode = 0b01101;
          immediate = static_cast<uint32_t>(so.GetRegister());
          use_immediate = true;
          immediate_shift = 6;
        } else {
          if (rn != rd) {
            // Must use T1.
            opcode_shift = 9;
            thumb_opcode = 0b01111;
            immediate_shift = 6;
          } else {
            // T2 encoding.
            opcode_shift = 11;
            thumb_opcode = 0b111;
            rd_shift = 8;
            rn_shift = 8;
          }
        }
        break;
      case RSB: thumb_opcode = 0b1001; break;
      case ADD:
        dp_opcode = 0;
        if (so.IsRegister()) {
          // T1.
          opcode_shift = 9;
          thumb_opcode = 0b01100;
          immediate = static_cast<uint32_t>(so.GetRegister());
          use_immediate = true;
          immediate_shift = 6;
        } else {
          // Immediate.
          if (rn != rd) {
            // Must use T1.
            opcode_shift = 9;
            thumb_opcode = 0b01110;
            immediate_shift = 6;
          } else {
            // T2 encoding.
            opcode_shift = 11;
            thumb_opcode = 0b110;
            rd_shift = 8;
            rn_shift = 8;
          }
        }
        break;
      case ADC: thumb_opcode = 0b0101; break;
      case SBC: thumb_opcode = 0b0110; break;
      case RSC: break;
      case TST: thumb_opcode = 0b1000; rn = so.GetRegister(); break;
      case TEQ: break;
      case CMP:
        if (use_immediate) {
          // T2 encoding.
           dp_opcode = 0;
           opcode_shift = 11;
           thumb_opcode = 0b101;
           rd_shift = 8;
           rn_shift = 8;
        } else {
          thumb_opcode = 0b1010;
          rn = so.GetRegister();
        }

        break;
      case CMN: thumb_opcode = 0b1011; rn = so.GetRegister(); break;
      case ORR: thumb_opcode = 0b1100; break;
      case MOV:
        dp_opcode = 0;
        if (use_immediate) {
          // T2 encoding.
          opcode_shift = 11;
          thumb_opcode = 0b100;
          rd_shift = 8;
          rn_shift = 8;
        } else {
          rn = so.GetRegister();
          if (IsHighRegister(rn) || IsHighRegister(rd)) {
            // Special mov for high registers.
            dp_opcode = 0b01;
            opcode_shift = 7;
            // Put the top bit of rd into the bottom bit of the opcode.
            thumb_opcode = 0b0001100 | static_cast<uint32_t>(rd) >> 3;
            rd = static_cast<Register>(static_cast<uint32_t>(rd) & 0b111);
          } else {
            thumb_opcode = 0;
          }
        }
        break;
      case BIC: thumb_opcode = 0b1110; break;
      case MVN: thumb_opcode = 0b1111; rn = so.GetRegister(); break;
      default:
        break;
    }
  }

  if (thumb_opcode == 0b11111111) {
    LOG(FATAL) << "Invalid thumb1 opcode " << opcode;
  }

  int16_t encoding = dp_opcode << 14 |
      (thumb_opcode << opcode_shift) |
      rd << rd_shift |
      rn << rn_shift |
      (use_immediate ? (immediate << immediate_shift) : 0);

  Emit16(encoding);
}


void Thumb2Assembler::EmitDataProcessing(Condition cond,
                              Opcode opcode,
                              int set_cc,
                              Register rn,
                              Register rd,
                              const ShifterOperand& so) {
  CHECK_NE(rd, kNoRegister);
  CHECK_EQ(cond, AL);           // No conditions on these

  if (Is32BitDataProcessing(cond, opcode, set_cc, rn, rd, so)) {
    Emit32BitDataProcessing(cond, opcode, set_cc, rn, rd, so);
  } else {
    Emit16BitDataProcessing(cond, opcode, set_cc, rn, rd, so);
  }
}


void Thumb2Assembler::EmitCondBranch(Condition cond, int offset, bool link, bool x) {
  int32_t encoding = B31 | B30 | B29 | B28 | B15;

  if (link) {
    // BL or BLX immediate
    encoding |= B14;
    if (!x) {
      encoding |= B12;
    } else {
      // Bottom bit of offset must be 0
      CHECK_EQ((offset & 1), 0);
    }
  } else {
    if (x) {
       LOG(FATAL) << "Invalid use of BX";
    } else {
      bool can_use_t4 = cond == AL;
      if (offset < 0) {
        can_use_t4 = can_use_t4 && (-offset & 0x00ffffff) == 0;
      } else {
        can_use_t4 = can_use_t4 && (offset & 0x00ffffff) == 0;
      }

      if (can_use_t4) {
        // Can use the T4 encoding allowing a 24 bit offset
        if (!x) {
          encoding |= B12;
        }
      } else {
        // Must be T3 encoding with a 20 bit offset
        encoding |= cond << 22;
      }
    }
  }
  Emit(Thumb2Assembler::EncodeBranchOffset(offset, encoding));
}


// NOTE: this only support immediate offsets, not [rx,ry]
void Thumb2Assembler::EmitLoadStore(Condition cond,
                             bool load,
                             bool byte,
                             bool half,
                             bool is_signed,
                             Register rd,
                             const Address& ad) {
  CHECK_NE(rd, kNoRegister);
  CHECK_EQ(cond, AL);
  bool must_be_32bit = false;
  if (IsHighRegister(rd)) {
    must_be_32bit = true;
  }

  Register rn = ad.GetRegister();
  if (IsHighRegister(ad.GetRegister()) && rn != SP) {
    must_be_32bit = true;
  }

  if (is_signed || ad.GetOffset() < 0 || ad.GetMode() != Address::Offset) {
    must_be_32bit = true;
  }

  if (must_be_32bit) {
    int32_t encoding = B31 | B30 | B29 | B28 | B27 |
                  (load ? B20 : 0) |
                  (is_signed ? B24 : 0) |
                  static_cast<uint32_t>(rd) << 12 |
                  ad.encodingThumb(2) |
                  (byte ? 0 : half ? B21 : B22);
    Emit(encoding);
  } else {
    // 16 bit thumb1
    uint8_t opA = 0;
    bool sp_relative = false;
    Register rn = ad.GetRegister();
    if (byte) {
      opA = 0b0111;
    } else if (half) {
      opA = 0b1000;
    } else {
      if (rn == SP) {
        opA = 0b1001;
        sp_relative = true;
      } else {
        opA = 0b0110;
      }
    }
    int16_t encoding = opA << 12 |
                (load ? B11 : 0);

    int32_t offset = ad.GetOffset();
    CHECK_GE(offset, 0);
    if (sp_relative) {
      // SP relative, 10 bit offset
      CHECK_LT(offset, 1024);
      CHECK_EQ((offset & 0b11), 0);
      encoding |= offset >> 2;
    } else {
      // No SP relative.  The offset is shifted right depending on
      // the size of the load/store.
      encoding |= static_cast<uint32_t>(rd);

      if (byte) {
        // 5 bit offset, no shift
        CHECK_LT(offset, 32);
      } else if (half) {
        // 6 bit offset, shifted by 1.
        CHECK_LT(offset, 64);
        CHECK_EQ((offset & 0b1), 0);
        offset >>= 1;
      } else {
        // 7 bit offset, shifted by 2.
        CHECK_LT(offset, 128);
        CHECK_EQ((offset & 0b11), 0);
        offset >>= 2;
      }
      encoding |= rn << 3 | offset  << 6;
    }

    Emit16(encoding);
  }
}



void Thumb2Assembler::EmitMultiMemOp(Condition cond,
                                  BlockAddressMode am,
                                  bool load,
                                  Register base,
                                  RegList regs) {
  CHECK_NE(base, kNoRegister);
  CHECK_EQ(cond, AL);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::EmitShiftImmediate(Condition cond,
                                      Shift opcode,
                                      Register rd,
                                      Register rm,
                                      const ShifterOperand& so) {
  CHECK_NE(cond, kNoCondition);
  CHECK_EQ(so.type(), 1U);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::EmitShiftRegister(Condition cond,
                                     Shift opcode,
                                     Register rd,
                                     Register rm,
                                     const ShifterOperand& so) {
  CHECK_NE(cond, kNoCondition);
  CHECK_EQ(so.type(), 0U);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::EmitBranch(Condition cond, Label* label, bool link, bool x) {
  if (label->IsBound()) {
    EmitCondBranch(cond, label->Position() - buffer_.Size(), link, x);
  } else {
    int position = buffer_.Size();
    // Use the offset field of the branch instruction for linking the sites.
    EmitCondBranch(cond, label->position_, link, x);
    label->LinkTo(position);
  }
}


void Thumb2Assembler::clz(Register rd, Register rm, Condition cond) {
  CHECK_NE(rd, kNoRegister);
  CHECK_NE(rm, kNoRegister);
  CHECK_NE(cond, kNoCondition);
  CHECK_NE(rd, PC);
  CHECK_NE(rm, PC);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::movw(Register rd, uint16_t imm16, Condition cond) {
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::movt(Register rd, uint16_t imm16, Condition cond) {
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::EmitMulOp(Condition cond, int32_t opcode,
                             Register rd, Register rn,
                             Register rm, Register rs) {
  CHECK_NE(rd, kNoRegister);
  CHECK_NE(rn, kNoRegister);
  CHECK_NE(rm, kNoRegister);
  CHECK_NE(rs, kNoRegister);
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}

void Thumb2Assembler::ldrex(Register rt, Register rn, Condition cond) {
  CHECK_NE(rn, kNoRegister);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::strex(Register rd,
                         Register rt,
                         Register rn,
                         Condition cond) {
  CHECK_NE(rn, kNoRegister);
  CHECK_NE(rd, kNoRegister);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::clrex() {
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::nop(Condition cond) {
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::vmovsr(SRegister sn, Register rt, Condition cond) {
  CHECK_NE(sn, kNoSRegister);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(rt, SP);
  CHECK_NE(rt, PC);
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::vmovrs(Register rt, SRegister sn, Condition cond) {
  CHECK_NE(sn, kNoSRegister);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(rt, SP);
  CHECK_NE(rt, PC);
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::vmovsrr(SRegister sm, Register rt, Register rt2,
                           Condition cond) {
  CHECK_NE(sm, kNoSRegister);
  CHECK_NE(sm, S31);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(rt, SP);
  CHECK_NE(rt, PC);
  CHECK_NE(rt2, kNoRegister);
  CHECK_NE(rt2, SP);
  CHECK_NE(rt2, PC);
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::vmovrrs(Register rt, Register rt2, SRegister sm,
                           Condition cond) {
  CHECK_NE(sm, kNoSRegister);
  CHECK_NE(sm, S31);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(rt, SP);
  CHECK_NE(rt, PC);
  CHECK_NE(rt2, kNoRegister);
  CHECK_NE(rt2, SP);
  CHECK_NE(rt2, PC);
  CHECK_NE(rt, rt2);
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::vmovdrr(DRegister dm, Register rt, Register rt2,
                           Condition cond) {
  CHECK_NE(dm, kNoDRegister);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(rt, SP);
  CHECK_NE(rt, PC);
  CHECK_NE(rt2, kNoRegister);
  CHECK_NE(rt2, SP);
  CHECK_NE(rt2, PC);
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::vmovrrd(Register rt, Register rt2, DRegister dm,
                           Condition cond) {
  CHECK_NE(dm, kNoDRegister);
  CHECK_NE(rt, kNoRegister);
  CHECK_NE(rt, SP);
  CHECK_NE(rt, PC);
  CHECK_NE(rt2, kNoRegister);
  CHECK_NE(rt2, SP);
  CHECK_NE(rt2, PC);
  CHECK_NE(rt, rt2);
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::vldrs(SRegister sd, const Address& ad, Condition cond) {
  CHECK_NE(sd, kNoSRegister);
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::vstrs(SRegister sd, const Address& ad, Condition cond) {
  // CHECK_NE(static_cast<Register>(ad.encoding_ & (0xf << kRnShift)), PC);
  CHECK_NE(sd, kNoSRegister);
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::vldrd(DRegister dd, const Address& ad, Condition cond) {
  CHECK_NE(dd, kNoDRegister);
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::vstrd(DRegister dd, const Address& ad, Condition cond) {
  // CHECK_NE(static_cast<Register>(ad.encoding_ & (0xf << kRnShift)), PC);
  CHECK_NE(dd, kNoDRegister);
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::EmitVFPsss(Condition cond, int32_t opcode,
                              SRegister sd, SRegister sn, SRegister sm) {
  CHECK_NE(sd, kNoSRegister);
  CHECK_NE(sn, kNoSRegister);
  CHECK_NE(sm, kNoSRegister);
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::EmitVFPddd(Condition cond, int32_t opcode,
                              DRegister dd, DRegister dn, DRegister dm) {
  CHECK_NE(dd, kNoDRegister);
  CHECK_NE(dn, kNoDRegister);
  CHECK_NE(dm, kNoDRegister);
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::vmovs(SRegister sd, SRegister sm, Condition cond) {
  EmitVFPsss(cond, B23 | B21 | B20 | B6, sd, S0, sm);
}


void Thumb2Assembler::vmovd(DRegister dd, DRegister dm, Condition cond) {
  EmitVFPddd(cond, B23 | B21 | B20 | B6, dd, D0, dm);
}




void Thumb2Assembler::EmitVFPsd(Condition cond, int32_t opcode,
                             SRegister sd, DRegister dm) {
  CHECK_NE(sd, kNoSRegister);
  CHECK_NE(dm, kNoDRegister);
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::EmitVFPds(Condition cond, int32_t opcode,
                             DRegister dd, SRegister sm) {
  CHECK_NE(dd, kNoDRegister);
  CHECK_NE(sm, kNoSRegister);
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}



void Thumb2Assembler::vmstat(Condition cond) {  // VMRS APSR_nzcv, FPSCR
  CHECK_NE(cond, kNoCondition);
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::svc(uint32_t imm24) {
  CHECK(IsUint(24, imm24)) << imm24;
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}


void Thumb2Assembler::bkpt(uint16_t imm16) {
  UNIMPLEMENTED(FATAL) << "Unimplemented thumb instruction";
}

void Thumb2Assembler::blx(Register rm, Condition cond) {
  CHECK_NE(rm, kNoRegister);
  CHECK_EQ(cond, AL);
  int16_t encoding = B14 | B10 | B9 | B8 | B7 | static_cast<int16_t>(rm) << 3;
  Emit16(encoding);
}

void Thumb2Assembler::bx(Register rm, Condition cond) {
  CHECK_NE(rm, kNoRegister);
  CHECK_EQ(cond, AL);
  int16_t encoding = B14 | B10 | B9 | B8 | static_cast<int16_t>(rm) << 3;
  Emit16(encoding);
}

void Thumb2Assembler::Push(Register rd, Condition cond) {
  str(rd, Address(SP, -kRegisterSize, Address::PreIndex), cond);
}

void Thumb2Assembler::Pop(Register rd, Condition cond) {
  ldr(rd, Address(SP, kRegisterSize, Address::PostIndex), cond);
}

void Thumb2Assembler::PushList(RegList regs, Condition cond) {
  stm(DB_W, SP, regs, cond);
}

void Thumb2Assembler::PopList(RegList regs, Condition cond) {
  ldm(IA_W, SP, regs, cond);
}

void Thumb2Assembler::Mov(Register rd, Register rm, Condition cond) {
  if (rd != rm) {
    mov(rd, ShifterOperand(rm), cond);
  }
}


void Thumb2Assembler::Bind(Label* label) {
  CHECK(!label->IsBound());
  int bound_pc = buffer_.Size();
  while (label->IsLinked()) {
    int32_t position = label->Position();
    int32_t next = buffer_.Load<int32_t>(position);
    int32_t encoded = Thumb2Assembler::EncodeBranchOffset(bound_pc - position, next);
    buffer_.Store<int32_t>(position, encoded);
    label->position_ = Thumb2Assembler::DecodeBranchOffset(next);
  }
  label->BindTo(bound_pc);
}


void Thumb2Assembler::Lsl(Register rd, Register rm, uint32_t shift_imm,
                       Condition cond) {
  CHECK_NE(shift_imm, 0u);  // Do not use Lsl if no shift is wanted.
  mov(rd, ShifterOperand(rm, LSL, shift_imm), cond);
}

void Thumb2Assembler::Lsr(Register rd, Register rm, uint32_t shift_imm,
                       Condition cond) {
  CHECK_NE(shift_imm, 0u);  // Do not use Lsr if no shift is wanted.
  if (shift_imm == 32) shift_imm = 0;  // Comply to UAL syntax.
  mov(rd, ShifterOperand(rm, LSR, shift_imm), cond);
}

void Thumb2Assembler::Asr(Register rd, Register rm, uint32_t shift_imm,
                       Condition cond) {
  CHECK_NE(shift_imm, 0u);  // Do not use Asr if no shift is wanted.
  if (shift_imm == 32) shift_imm = 0;  // Comply to UAL syntax.
  mov(rd, ShifterOperand(rm, ASR, shift_imm), cond);
}

void Thumb2Assembler::Ror(Register rd, Register rm, uint32_t shift_imm,
                       Condition cond) {
  CHECK_NE(shift_imm, 0u);  // Use Rrx instruction.
  mov(rd, ShifterOperand(rm, ROR, shift_imm), cond);
}

void Thumb2Assembler::Rrx(Register rd, Register rm, Condition cond) {
  mov(rd, ShifterOperand(rm, ROR, 0), cond);
}

int32_t Thumb2Assembler::EncodeBranchOffset(int32_t offset, int32_t inst) {
  // The offset is off by 4 due to the way the ARM CPUs read PC.
  offset -= 4;
  offset >>= 1;

  uint32_t signbit = (offset >> 31) & 0x1;
  uint32_t i1 = (offset >> 22) & 0x1;
  uint32_t i2 = (offset >> 21) & 0x1;
  uint32_t imm10 = (offset >> 11) & 0x03ff;
  uint32_t imm11 = offset & 0x07ff;
  uint32_t j1 = (i1 ^ signbit) ? 0 : 1;
  uint32_t j2 = (i2 ^ signbit) ? 0 : 1;
  uint32_t value = (signbit << 26) | (j1 << 13) | (j2 << 11) | (imm10 << 16) |
                    imm11;
  return inst | value;
}


int Thumb2Assembler::DecodeBranchOffset(int32_t inst) {
  // Sign-extend, left-shift by 2, then add 8.
  return ((((inst & kBranchOffsetMask) << 8) >> 6) + 8);
}

void Thumb2Assembler::AddConstant(Register rd, int32_t value, Condition cond) {
  AddConstant(rd, rd, value, cond);
}


void Thumb2Assembler::AddConstant(Register rd, Register rn, int32_t value,
                               Condition cond) {
  if (value == 0) {
    if (rd != rn) {
      mov(rd, ShifterOperand(rn), cond);
    }
    return;
  }
  // We prefer to select the shorter code sequence rather than selecting add for
  // positive values and sub for negatives ones, which would slightly improve
  // the readability of generated code for some constants.
  ShifterOperand shifter_op;
  if (ShifterOperand::CanHoldThumb(2, value, &shifter_op)) {
    add(rd, rn, shifter_op, cond);
  } else if (ShifterOperand::CanHoldThumb(2, -value, &shifter_op)) {
    sub(rd, rn, shifter_op, cond);
  } else {
    CHECK(rn != IP);
    if (ShifterOperand::CanHoldThumb(2, ~value, &shifter_op)) {
      mvn(IP, shifter_op, cond);
      add(rd, rn, ShifterOperand(IP), cond);
    } else if (ShifterOperand::CanHoldThumb(2, ~(-value), &shifter_op)) {
      mvn(IP, shifter_op, cond);
      sub(rd, rn, ShifterOperand(IP), cond);
    } else {
      movw(IP, Low16Bits(value), cond);
      uint16_t value_high = High16Bits(value);
      if (value_high != 0) {
        movt(IP, value_high, cond);
      }
      add(rd, rn, ShifterOperand(IP), cond);
    }
  }
}


void Thumb2Assembler::AddConstantSetFlags(Register rd, Register rn, int32_t value,
                                       Condition cond) {
  ShifterOperand shifter_op;
  if (ShifterOperand::CanHoldThumb(2, value, &shifter_op)) {
    adds(rd, rn, shifter_op, cond);
  } else if (ShifterOperand::CanHoldThumb(2, -value, &shifter_op)) {
    subs(rd, rn, shifter_op, cond);
  } else {
    CHECK(rn != IP);
    if (ShifterOperand::CanHoldThumb(2, ~value, &shifter_op)) {
      mvn(IP, shifter_op, cond);
      adds(rd, rn, ShifterOperand(IP), cond);
    } else if (ShifterOperand::CanHoldThumb(2, ~(-value), &shifter_op)) {
      mvn(IP, shifter_op, cond);
      subs(rd, rn, ShifterOperand(IP), cond);
    } else {
      movw(IP, Low16Bits(value), cond);
      uint16_t value_high = High16Bits(value);
      if (value_high != 0) {
        movt(IP, value_high, cond);
      }
      adds(rd, rn, ShifterOperand(IP), cond);
    }
  }
}


void Thumb2Assembler::LoadImmediate(Register rd, int32_t value, Condition cond) {
  ShifterOperand shifter_op;
  if (ShifterOperand::CanHoldThumb(2, value, &shifter_op)) {
    mov(rd, shifter_op, cond);
  } else if (ShifterOperand::CanHoldThumb(2, ~value, &shifter_op)) {
    mvn(rd, shifter_op, cond);
  } else {
    movw(rd, Low16Bits(value), cond);
    uint16_t value_high = High16Bits(value);
    if (value_high != 0) {
      movt(rd, value_high, cond);
    }
  }
}




// Implementation note: this method must emit at most one instruction when
// Address::CanHoldLoadOffsetThumb.
void Thumb2Assembler::LoadFromOffset(LoadOperandType type,
                                  Register reg,
                                  Register base,
                                  int32_t offset,
                                  Condition cond) {
  if (!Address::CanHoldLoadOffsetThumb(2, type, offset)) {
    CHECK(base != IP);
    LoadImmediate(IP, offset, cond);
    add(IP, IP, ShifterOperand(base), cond);
    base = IP;
    offset = 0;
  }
  CHECK(Address::CanHoldLoadOffsetThumb(2, type, offset));
  switch (type) {
    case kLoadSignedByte:
      ldrsb(reg, Address(base, offset), cond);
      break;
    case kLoadUnsignedByte:
      ldrb(reg, Address(base, offset), cond);
      break;
    case kLoadSignedHalfword:
      ldrsh(reg, Address(base, offset), cond);
      break;
    case kLoadUnsignedHalfword:
      ldrh(reg, Address(base, offset), cond);
      break;
    case kLoadWord:
      ldr(reg, Address(base, offset), cond);
      break;
    case kLoadWordPair:
      ldrd(reg, Address(base, offset), cond);
      break;
    default:
      LOG(FATAL) << "UNREACHABLE";
  }
}

// Implementation note: this method must emit at most one instruction when
// Address::CanHoldLoadOffsetThumb, as expected by JIT::GuardedLoadFromOffset.
void Thumb2Assembler::LoadSFromOffset(SRegister reg,
                                   Register base,
                                   int32_t offset,
                                   Condition cond) {
  if (!Address::CanHoldLoadOffsetThumb(2, kLoadSWord, offset)) {
    CHECK_NE(base, IP);
    LoadImmediate(IP, offset, cond);
    add(IP, IP, ShifterOperand(base), cond);
    base = IP;
    offset = 0;
  }
  CHECK(Address::CanHoldLoadOffsetThumb(2, kLoadSWord, offset));
  vldrs(reg, Address(base, offset), cond);
}

// Implementation note: this method must emit at most one instruction when
// Address::CanHoldLoadOffsetThumb, as expected by JIT::GuardedLoadFromOffset.
void Thumb2Assembler::LoadDFromOffset(DRegister reg,
                                   Register base,
                                   int32_t offset,
                                   Condition cond) {
  if (!Address::CanHoldLoadOffsetThumb(2, kLoadDWord, offset)) {
    CHECK_NE(base, IP);
    LoadImmediate(IP, offset, cond);
    add(IP, IP, ShifterOperand(base), cond);
    base = IP;
    offset = 0;
  }
  CHECK(Address::CanHoldLoadOffsetThumb(2, kLoadDWord, offset));
  vldrd(reg, Address(base, offset), cond);
}

// Implementation note: this method must emit at most one instruction when
// Address::CanHoldStoreOffsetThumb.
void Thumb2Assembler::StoreToOffset(StoreOperandType type,
                                 Register reg,
                                 Register base,
                                 int32_t offset,
                                 Condition cond) {
  if (!Address::CanHoldStoreOffsetThumb(2, type, offset)) {
    CHECK(reg != IP);
    CHECK(base != IP);
    LoadImmediate(IP, offset, cond);
    add(IP, IP, ShifterOperand(base), cond);
    base = IP;
    offset = 0;
  }
  CHECK(Address::CanHoldStoreOffsetThumb(2, type, offset));
  switch (type) {
    case kStoreByte:
      strb(reg, Address(base, offset), cond);
      break;
    case kStoreHalfword:
      strh(reg, Address(base, offset), cond);
      break;
    case kStoreWord:
      str(reg, Address(base, offset), cond);
      break;
    case kStoreWordPair:
      strd(reg, Address(base, offset), cond);
      break;
    default:
      LOG(FATAL) << "UNREACHABLE";
  }
}

// Implementation note: this method must emit at most one instruction when
// Address::CanHoldStoreOffsetThumb, as expected by JIT::GuardedStoreToOffset.
void Thumb2Assembler::StoreSToOffset(SRegister reg,
                                  Register base,
                                  int32_t offset,
                                  Condition cond) {
  if (!Address::CanHoldStoreOffsetThumb(2, kStoreSWord, offset)) {
    CHECK_NE(base, IP);
    LoadImmediate(IP, offset, cond);
    add(IP, IP, ShifterOperand(base), cond);
    base = IP;
    offset = 0;
  }
  CHECK(Address::CanHoldStoreOffsetThumb(2, kStoreSWord, offset));
  vstrs(reg, Address(base, offset), cond);
}

// Implementation note: this method must emit at most one instruction when
// Address::CanHoldStoreOffsetThumb, as expected by JIT::GuardedStoreSToOffset.
void Thumb2Assembler::StoreDToOffset(DRegister reg,
                                  Register base,
                                  int32_t offset,
                                  Condition cond) {
  if (!Address::CanHoldStoreOffsetThumb(2, kStoreDWord, offset)) {
    CHECK_NE(base, IP);
    LoadImmediate(IP, offset, cond);
    add(IP, IP, ShifterOperand(base), cond);
    base = IP;
    offset = 0;
  }
  CHECK(Address::CanHoldStoreOffsetThumb(2, kStoreDWord, offset));
  vstrd(reg, Address(base, offset), cond);
}

void Thumb2Assembler::MemoryBarrier(ManagedRegister mscratch) {
  CHECK_EQ(mscratch.AsArm().AsCoreRegister(), R12);
#if ANDROID_SMP != 0
  int32_t encoding = 0xf57ff05f;  // dmb
  Emit(encoding);
#endif
}

}  // namespace arm
}  // namespace art
