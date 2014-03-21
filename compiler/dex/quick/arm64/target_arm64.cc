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

#include "codegen_arm64.h"

#include <inttypes.h>
#include <string>

#include "dex/compiler_internals.h"
#include "dex/quick/mir_to_lir-inl.h"

namespace art {

static int core_regs[] = {r0, r1, r2, r3, rARM_SUSPEND, r5, r6, r7, r8, rARM_SELF, r10,
                         r11, r12, rARM_SP, rARM_LR, r15pc};
static int ReservedRegs[] = {rARM_SUSPEND, rARM_SELF, rARM_SP, rARM_LR, r15pc};
static int FpRegs[] = {fr0, fr1, fr2, fr3, fr4, fr5, fr6, fr7,
                       fr8, fr9, fr10, fr11, fr12, fr13, fr14, fr15,
                       fr16, fr17, fr18, fr19, fr20, fr21, fr22, fr23,
                       fr24, fr25, fr26, fr27, fr28, fr29, fr30, fr31};
static int core_temps[] = {r0, r1, r2, r3, r12};
static int fp_temps[] = {fr0, fr1, fr2, fr3, fr4, fr5, fr6, fr7,
                        fr8, fr9, fr10, fr11, fr12, fr13, fr14, fr15};

RegLocation Arm64Mir2Lir::LocCReturn() {
  return arm_loc_c_return;
}

RegLocation Arm64Mir2Lir::LocCReturnWide() {
  return arm_loc_c_return_wide;
}

RegLocation Arm64Mir2Lir::LocCReturnFloat() {
  return arm_loc_c_return_float;
}

RegLocation Arm64Mir2Lir::LocCReturnDouble() {
  return arm_loc_c_return_double;
}

// Return a target-dependent special register.
int Arm64Mir2Lir::TargetReg(SpecialTargetRegister reg) {
  int res = INVALID_REG;
  switch (reg) {
    case kSelf: res = rARM_SELF; break;
    case kSuspend: res =  rARM_SUSPEND; break;
    case kLr: res =  rARM_LR; break;
    case kPc: res =  rARM_PC; break;
    case kSp: res =  rARM_SP; break;
    case kArg0: res = rARM_ARG0; break;
    case kArg1: res = rARM_ARG1; break;
    case kArg2: res = rARM_ARG2; break;
    case kArg3: res = rARM_ARG3; break;
    case kFArg0: res = rARM_FARG0; break;
    case kFArg1: res = rARM_FARG1; break;
    case kFArg2: res = rARM_FARG2; break;
    case kFArg3: res = rARM_FARG3; break;
    case kRet0: res = rARM_RET0; break;
    case kRet1: res = rARM_RET1; break;
    case kInvokeTgt: res = rARM_INVOKE_TGT; break;
    case kHiddenArg: res = r12; break;
    case kHiddenFpArg: res = INVALID_REG; break;
    case kCount: res = rARM_COUNT; break;
  }
  return res;
}

int Arm64Mir2Lir::GetArgMappingToPhysicalReg(int arg_num) {
  // For the 32-bit internal ABI, the first 3 arguments are passed in registers.
  switch (arg_num) {
    case 0:
      return rARM_ARG1;
    case 1:
      return rARM_ARG2;
    case 2:
      return rARM_ARG3;
    default:
      return INVALID_REG;
  }
}

// Create a double from a pair of singles.
int Arm64Mir2Lir::S2d(int low_reg, int high_reg) {
  return ARM_S2D(low_reg, high_reg);
}

// Return mask to strip off fp reg flags and bias.
uint32_t Arm64Mir2Lir::FpRegMask() {
  return ARM_FP_REG_MASK;
}

// True if both regs single, both core or both double.
bool Arm64Mir2Lir::SameRegType(int reg1, int reg2) {
  return (ARM_REGTYPE(reg1) == ARM_REGTYPE(reg2));
}

/*
 * Decode the register id.
 */
uint64_t Arm64Mir2Lir::GetRegMaskCommon(int reg) {
  if (LIKELY(reg >= 0)) {
    return UINT64_C(1) << (reg & 0x1f);
  } else {
    // Pseudo register xzr/wzr: it is more an immediate rather than a true register.
    DCHECK_EQ(reg, rARM_ZR);
    return 0;
  }
}

uint64_t Arm64Mir2Lir::GetPCUseDefEncoding() {
  LOG(FATAL) << "Unexpected call to GetPCUseDefEncoding for Arm64";
  return 0ULL;
}

// Arm64 specific setup.  TODO: inline?:
void Arm64Mir2Lir::SetupTargetResourceMasks(LIR* lir, uint64_t flags) {
  DCHECK_EQ(cu_->instruction_set, kArm64);
  DCHECK(!lir->flags.use_def_invalid);

  // These flags are somewhat uncommon - bypass if we can.
  if ((flags & (REG_DEF_SP | REG_USE_SP | REG_DEF_LR)) != 0) {
    if (flags & REG_DEF_SP) {
      lir->u.m.def_mask |= ENCODE_ARM_REG_SP;
    }

    if (flags & REG_USE_SP) {
      lir->u.m.use_mask |= ENCODE_ARM_REG_SP;
    }

    if (flags & REG_DEF_LR) {
      lir->u.m.def_mask |= ENCODE_ARM_REG_LR;
    }
  }
}

ArmConditionCode Arm64Mir2Lir::ArmConditionEncoding(ConditionCode ccode) {
  ArmConditionCode res;
  switch (ccode) {
    case kCondEq: res = kArmCondEq; break;
    case kCondNe: res = kArmCondNe; break;
    case kCondCs: res = kArmCondCs; break;
    case kCondCc: res = kArmCondCc; break;
    case kCondUlt: res = kArmCondCc; break;
    case kCondUge: res = kArmCondCs; break;
    case kCondMi: res = kArmCondMi; break;
    case kCondPl: res = kArmCondPl; break;
    case kCondVs: res = kArmCondVs; break;
    case kCondVc: res = kArmCondVc; break;
    case kCondHi: res = kArmCondHi; break;
    case kCondLs: res = kArmCondLs; break;
    case kCondGe: res = kArmCondGe; break;
    case kCondLt: res = kArmCondLt; break;
    case kCondGt: res = kArmCondGt; break;
    case kCondLe: res = kArmCondLe; break;
    case kCondAl: res = kArmCondAl; break;
    case kCondNv: res = kArmCondNv; break;
    default:
      LOG(FATAL) << "Bad condition code " << ccode;
      res = static_cast<ArmConditionCode>(0);  // Quiet gcc
  }
  return res;
}

static const char* core_reg_names[16] = {
  "r0",
  "r1",
  "r2",
  "r3",
  "r4",
  "r5",
  "r6",
  "r7",
  "r8",
  "rSELF",
  "r10",
  "r11",
  "r12",
  "sp",
  "lr",
  "pc",
};

static const char *shift_names[4] = {
  "lsl",
  "lsr",
  "asr",
  "ror"
};

static const char* extend_names[8] = {
  "uxtb",
  "uxth",
  "uxtw",
  "uxtx",
  "sxtb",
  "sxth",
  "sxtw",
  "sxtx",
};

/* Decode and print a register extension (e.g. ", uxtb #1") */
static void DecodeRegExtendOrShift(int operand, char *buf, size_t buf_size) {
  if ((operand & (1 << 6)) == 0) {
    const char *shift_name = shift_names[(operand >> 7) & 0x3];
    int amount = operand & 0x3f;
    snprintf(buf, buf_size, ", %s #%d", shift_name, amount);
  } else {
    const char *extend_name = extend_names[(operand >> 3) & 0x7];
    int amount = operand & 0x7;
    if (amount == 0) {
      snprintf(buf, buf_size, ", %s", extend_name);
    } else {
      snprintf(buf, buf_size, ", %s #%d", extend_name, amount);
    }
  }
}

#define BIT_MASK(w) ((UINT64_C(1) << (w)) - UINT64_C(1))

static uint64_t RotateRight(uint64_t value, unsigned rotate, unsigned width) {
  DCHECK_LE(width, 64U);
  rotate &= 63;
  value = value & BIT_MASK(width);
  return ((value & BIT_MASK(rotate)) << (width - rotate)) | (value >> rotate);
}

static uint64_t RepeatBitsAcrossReg(bool is_wide, uint64_t value, unsigned width) {
  unsigned i;
  unsigned reg_size = (is_wide) ? 64 : 32;
  uint64_t result = value & BIT_MASK(width);
  DCHECK_NE(width, reg_size);
  for (i = width; i < reg_size; i *= 2) {
    result |= (result << i);
  }
  DCHECK_EQ(i, reg_size);
  return result;
}

/**
 * @brief Decode an immediate in the form required by logical instructions.
 *
 * @param is_wide Whether @p value encodes a 64-bit (as opposed to 32-bit) immediate.
 * @param value The encoded logical immediates that is to be decoded.
 * @return The decoded logical immediate.
 * @note This is the inverse of Arm64Mir2Lir::EncodeLogicalImmediate().
 */
uint64_t Arm64Mir2Lir::DecodeLogicalImmediate(bool is_wide, int value) {
  unsigned n     = (value >> 12) & 0x01;
  unsigned imm_r = (value >>  6) & 0x3f;
  unsigned imm_s = (value >>  0) & 0x3f;

  // An integer is constructed from the n, imm_s and imm_r bits according to
  // the following table:
  //
  // N   imms immr  size S             R
  // 1 ssssss rrrrrr 64  UInt(ssssss) UInt(rrrrrr)
  // 0 0sssss xrrrrr 32  UInt(sssss)  UInt(rrrrr)
  // 0 10ssss xxrrrr 16  UInt(ssss)   UInt(rrrr)
  // 0 110sss xxxrrr 8   UInt(sss)    UInt(rrr)
  // 0 1110ss xxxxrr 4   UInt(ss)     UInt(rr)
  // 0 11110s xxxxxr 2   UInt(s)      UInt(r)
  // (s bits must not be all set)
  //
  // A pattern is constructed of size bits, where the least significant S+1
  // bits are set. The pattern is rotated right by R, and repeated across a
  // 32 or 64-bit value, depending on destination register width.

  if (n == 1) {
    DCHECK_NE(imm_s, 0x3fU);
    uint64_t bits = BIT_MASK(imm_s + 1);
    return RotateRight(bits, imm_r, 64);
  } else {
    DCHECK_NE((imm_s >> 1), 0x1fU);
    for (unsigned width = 0x20; width >= 0x2; width >>= 1) {
      if ((imm_s & width) == 0) {
        unsigned mask = (unsigned)(width - 1);
        DCHECK_NE((imm_s & mask), mask);
        uint64_t bits = BIT_MASK((imm_s & mask) + 1);
        return RepeatBitsAcrossReg(is_wide, RotateRight(bits, imm_r & mask, width), width);
      }
    }
  }
  return 0;
}

/**
 * @brief Decode an 8-bit single point number encoded with EncodeImmSingle().
 */
static float DecodeImmSingle(uint8_t small_float) {
  int mantissa = (small_float & 0x0f) + 0x10;
  int sign = ((small_float & 0x80) == 0) ? 1 : -1;
  float signed_mantissa = static_cast<float>(sign*mantissa);
  int exponent = (((small_float >> 4) & 0x7) + 4) & 0x7;
  return signed_mantissa*static_cast<float>(1 << exponent)*0.0078125f;
}

static char*  DecodeFPCSRegList(int count, int base, char* buf, size_t buf_size) {
  snprintf(buf, buf_size, "s%d", base);
  for (int i = 1; i < count; i++) {
    snprintf(buf + strlen(buf), buf_size - strlen(buf), ", s%d", base + i);
  }
  return buf;
}

static int32_t ExpandImmediate(int value) {
  int32_t mode = (value & 0xf00) >> 8;
  uint32_t bits = value & 0xff;
  switch (mode) {
    case 0:
      return bits;
     case 1:
      return (bits << 16) | bits;
     case 2:
      return (bits << 24) | (bits << 8);
     case 3:
      return (bits << 24) | (bits << 16) | (bits << 8) | bits;
    default:
      break;
  }
  bits = (bits | 0x80) << 24;
  return bits >> (((value & 0xf80) >> 7) - 8);
}

static const char* cc_names[] = {"eq", "ne", "cs", "cc", "mi", "pl", "vs", "vc",
                                 "hi", "ls", "ge", "lt", "gt", "le", "al", "nv"};
/*
 * Interpret a format string and build a string no longer than size
 * See format key in Assemble.c.
 */
std::string Arm64Mir2Lir::BuildInsnString(const char* fmt, LIR* lir, unsigned char* base_addr) {
  std::string buf;
  int i;
  const char* fmt_end = &fmt[strlen(fmt)];
  char tbuf[256];
  const char* name;
  char nc;
  while (fmt < fmt_end) {
    int operand;
    if (*fmt == '!') {
      fmt++;
      DCHECK_LT(fmt, fmt_end);
      nc = *fmt++;
      if (nc == '!') {
        strcpy(tbuf, "!");
      } else {
         DCHECK_LT(fmt, fmt_end);
         DCHECK_LT(static_cast<unsigned>(nc-'0'), 4U);
         operand = lir->operands[nc-'0'];
         switch (*fmt++) {
           case 'H':
             if (operand != 0) {
               snprintf(tbuf, arraysize(tbuf), ", %s %d", shift_names[operand & 0x3], operand >> 2);
             } else {
               strcpy(tbuf, "");
             }
             break;
           case 'e':  {
               // Omit ", uxtw #0" in strings like "add w0, w1, w3, uxtw #0" and
               // ", uxtx #0" in strings like "add x0, x1, x3, uxtx #0"
               int omittable = ((IS_WIDE(lir->opcode)) ? EncodeExtend(kA64Uxtw, 0) :
                                EncodeExtend(kA64Uxtw, 0));
               if (LIKELY(operand == omittable)) {
                 strcpy(tbuf, "");
               } else {
                 DecodeRegExtendOrShift(operand, tbuf, arraysize(tbuf));
               }
             }
             break;
           case 'o':
             // Omit ", lsl #0"
             if (LIKELY(operand == EncodeShift(kA64Lsl, 0))) {
               strcpy(tbuf, "");
             } else {
               DecodeRegExtendOrShift(operand, tbuf, arraysize(tbuf));
             }
             break;
           case 'B':
             switch (operand) {
               case kSY:
                 name = "sy";
                 break;
               case kST:
                 name = "st";
                 break;
               case kISH:
                 name = "ish";
                 break;
               case kISHST:
                 name = "ishst";
                 break;
               case kNSH:
                 name = "nsh";
                 break;
               case kNSHST:
                 name = "shst";
                 break;
               default:
                 name = "DecodeError2";
                 break;
             }
             strcpy(tbuf, name);
             break;
           case 'b':
             strcpy(tbuf, "0000");
             for (i = 3; i >= 0; i--) {
               tbuf[i] += operand & 1;
               operand >>= 1;
             }
             break;
           case 'n':
             // TODO(Arm64): remove this.
             operand = ~ExpandImmediate(operand);
             snprintf(tbuf, arraysize(tbuf), "%d [%#x]", operand, operand);
             break;
           case 'm':
             // TODO(Arm64): remove this.
             operand = ExpandImmediate(operand);
             snprintf(tbuf, arraysize(tbuf), "%d [%#x]", operand, operand);
             break;
           case 's':
             snprintf(tbuf, arraysize(tbuf), "s%d", operand & ARM_FP_REG_MASK);
             break;
           case 'S':
             snprintf(tbuf, arraysize(tbuf), "d%d", operand & ARM_FP_REG_MASK);
             break;
           case 'f':
             snprintf(tbuf, arraysize(tbuf), "%c%d", (IS_FWIDE(lir->opcode)) ? 'd' : 's',
                      operand & ARM_FP_REG_MASK);
             break;
           case 'h':
             snprintf(tbuf, arraysize(tbuf), "%04x", operand);
             break;
           case 'l': {
               bool is_wide = IS_WIDE(lir->opcode);
               uint64_t imm = DecodeLogicalImmediate(is_wide, operand);
               snprintf(tbuf, arraysize(tbuf), "%" PRId64 " (%#" PRIx64 ")", imm, imm);
             }
             break;
           case 'I':
             snprintf(tbuf, arraysize(tbuf), "%f", DecodeImmSingle(operand));
             break;
           case 'M':
             if (LIKELY(operand == 0))
               strcpy(tbuf, "");
             else
               snprintf(tbuf, arraysize(tbuf), ", lsl #%d", 16*operand);
             break;
           case 'd':
             snprintf(tbuf, arraysize(tbuf), "%d", operand);
             break;
           case 'C':
             DCHECK_LT(operand, static_cast<int>(
                 sizeof(core_reg_names)/sizeof(core_reg_names[0])));
             snprintf(tbuf, arraysize(tbuf), "%s", core_reg_names[operand]);
             break;
           case 'w':
             if (LIKELY(operand != rARM_ZR))
               snprintf(tbuf, arraysize(tbuf), "w%d", operand);
             else
               strcpy(tbuf, "wzr");
             break;
           case 'W':
             if (LIKELY(operand != rARM_SP))
               snprintf(tbuf, arraysize(tbuf), "w%d", operand);
             else
               strcpy(tbuf, "wsp");
             break;
           case 'x':
             if (LIKELY(operand != rARM_ZR))
               snprintf(tbuf, arraysize(tbuf), "x%d", operand);
             else
               strcpy(tbuf, "xzr");
             break;
           case 'X':
             if (LIKELY(operand != rARM_SP))
               snprintf(tbuf, arraysize(tbuf), "x%d", operand);
             else
               strcpy(tbuf, "sp");
             break;
           case 'D':
             snprintf(tbuf, arraysize(tbuf), "%d", operand*((IS_WIDE(lir->opcode)) ? 8 : 4));
             break;
           case 'E':
             snprintf(tbuf, arraysize(tbuf), "%d", operand*4);
             break;
           case 'F':
             snprintf(tbuf, arraysize(tbuf), "%d", operand*2);
             break;
           case 'c':
             strcpy(tbuf, cc_names[operand]);
             break;
           case 't':
             snprintf(tbuf, arraysize(tbuf), "0x%08" PRIxPTR " (L%p)",
                 reinterpret_cast<uintptr_t>(base_addr) + lir->offset + (operand << 2),
                 lir->target);
             break;
           case 'r': {
               bool is_wide = IS_WIDE(lir->opcode);
               if (LIKELY(operand != rARM_ZR)) {
                 snprintf(tbuf, arraysize(tbuf), "%c%d", (is_wide) ? 'x' : 'w', operand);
               } else {
                 strcpy(tbuf, (is_wide) ? "xzr" : "wzr");
               }
             }
             break;
           case 'R': {
               bool is_wide = IS_WIDE(lir->opcode);
               if (LIKELY(operand != rARM_SP)) {
                 snprintf(tbuf, arraysize(tbuf), "%c%d", (is_wide) ? 'x' : 'w', operand);
               } else {
                 strcpy(tbuf, (is_wide) ? "sp" : "wsp");
               }
             }
             break;
           case 'p':
             snprintf(tbuf, arraysize(tbuf), ".+%d (addr %#" PRIxPTR ")", 4*operand,
                      reinterpret_cast<uintptr_t>(base_addr) + lir->offset + 4*operand);
             break;
           case 'P':
             DecodeFPCSRegList(operand, 16, tbuf, arraysize(tbuf));
             break;
           case 'Q':
             DecodeFPCSRegList(operand, 0, tbuf, arraysize(tbuf));
             break;
           case 'T':
             if (LIKELY(operand == 0))
               strcpy(tbuf, "");
             else if (operand == 1)
               strcpy(tbuf, ", lsl #12");
             else
               strcpy(tbuf, ", DecodeError3");
             break;
           default:
             strcpy(tbuf, "DecodeError1");
             break;
        }
        buf += tbuf;
      }
    } else {
       buf += *fmt++;
    }
  }
  return buf;
}

void Arm64Mir2Lir::DumpResourceMask(LIR* arm_lir, uint64_t mask, const char* prefix) {
  char buf[256];
  buf[0] = 0;

  if (mask == ENCODE_ALL) {
    strcpy(buf, "all");
  } else {
    char num[8];
    int i;

    for (i = 0; i < kArmRegEnd; i++) {
      if (mask & (1ULL << i)) {
        snprintf(num, arraysize(num), "%d ", i);
        strcat(buf, num);
      }
    }

    if (mask & ENCODE_CCODE) {
      strcat(buf, "cc ");
    }
    if (mask & ENCODE_FP_STATUS) {
      strcat(buf, "fpcc ");
    }

    /* Memory bits */
    if (arm_lir && (mask & ENCODE_DALVIK_REG)) {
      snprintf(buf + strlen(buf), arraysize(buf) - strlen(buf), "dr%d%s",
               DECODE_ALIAS_INFO_REG(arm_lir->flags.alias_info),
               DECODE_ALIAS_INFO_WIDE(arm_lir->flags.alias_info) ? "(+1)" : "");
    }
    if (mask & ENCODE_LITERAL) {
      strcat(buf, "lit ");
    }

    if (mask & ENCODE_HEAP_REF) {
      strcat(buf, "heap ");
    }
    if (mask & ENCODE_MUST_NOT_ALIAS) {
      strcat(buf, "noalias ");
    }
  }
  if (buf[0]) {
    LOG(INFO) << prefix << ": " << buf;
  }
}

bool Arm64Mir2Lir::IsUnconditionalBranch(LIR* lir) {
  return (lir->opcode == kA64BUncond);
}

Arm64Mir2Lir::Arm64Mir2Lir(CompilationUnit* cu, MIRGraph* mir_graph, ArenaAllocator* arena)
    : Mir2Lir(cu, mir_graph, arena) {
  // Sanity check - make sure encoding map lines up.
  for (int i = 0; i < kA64Last; i++) {
    if (UNWIDE(Arm64Mir2Lir::EncodingMap[i].opcode) != i) {
      LOG(FATAL) << "Encoding order for " << Arm64Mir2Lir::EncodingMap[i].name
                 << " is wrong: expecting " << i << ", seeing "
                 << static_cast<int>(Arm64Mir2Lir::EncodingMap[i].opcode);
    }
  }
}

Mir2Lir* Arm64CodeGenerator(CompilationUnit* const cu, MIRGraph* const mir_graph,
                            ArenaAllocator* const arena) {
  return new Arm64Mir2Lir(cu, mir_graph, arena);
}

// Alloc a pair of core registers, or a double.
RegStorage Arm64Mir2Lir::AllocTypedTempWide(bool fp_hint, int reg_class) {
  int high_reg;
  int low_reg;

  if (((reg_class == kAnyReg) && fp_hint) || (reg_class == kFPReg)) {
    low_reg = AllocTempDouble();
    high_reg = low_reg + 1;
  } else {
    low_reg = AllocTemp();
    high_reg = AllocTemp();
  }
  return RegStorage(RegStorage::k64BitPair, low_reg, high_reg);
}

int Arm64Mir2Lir::AllocTypedTemp(bool fp_hint, int reg_class) {
  if (((reg_class == kAnyReg) && fp_hint) || (reg_class == kFPReg))
    return AllocTempFloat();
  return AllocTemp();
}

void Arm64Mir2Lir::CompilerInitializeRegAlloc() {
  int num_regs = sizeof(core_regs)/sizeof(*core_regs);
  int num_reserved = sizeof(ReservedRegs)/sizeof(*ReservedRegs);
  int num_temps = sizeof(core_temps)/sizeof(*core_temps);
  int num_fp_regs = sizeof(FpRegs)/sizeof(*FpRegs);
  int num_fp_temps = sizeof(fp_temps)/sizeof(*fp_temps);
  reg_pool_ = static_cast<RegisterPool*>(arena_->Alloc(sizeof(*reg_pool_),
                                                       kArenaAllocRegAlloc));
  reg_pool_->num_core_regs = num_regs;
  reg_pool_->core_regs = reinterpret_cast<RegisterInfo*>
      (arena_->Alloc(num_regs * sizeof(*reg_pool_->core_regs), kArenaAllocRegAlloc));
  reg_pool_->num_fp_regs = num_fp_regs;
  reg_pool_->FPRegs = static_cast<RegisterInfo*>
      (arena_->Alloc(num_fp_regs * sizeof(*reg_pool_->FPRegs), kArenaAllocRegAlloc));
  CompilerInitPool(reg_pool_->core_regs, core_regs, reg_pool_->num_core_regs);
  CompilerInitPool(reg_pool_->FPRegs, FpRegs, reg_pool_->num_fp_regs);
  // Keep special registers from being allocated
  for (int i = 0; i < num_reserved; i++) {
    if (NO_SUSPEND && (ReservedRegs[i] == rARM_SUSPEND)) {
      // To measure cost of suspend check
      continue;
    }
    MarkInUse(ReservedRegs[i]);
  }
  // Mark temp regs - all others not in use can be used for promotion
  for (int i = 0; i < num_temps; i++) {
    MarkTemp(core_temps[i]);
  }
  for (int i = 0; i < num_fp_temps; i++) {
    MarkTemp(fp_temps[i]);
  }

  // Start allocation at r2 in an attempt to avoid clobbering return values
  reg_pool_->next_core_reg = r2;
}

void Arm64Mir2Lir::FreeRegLocTemps(RegLocation rl_keep,
                     RegLocation rl_free) {
  if ((rl_free.reg.GetReg() != rl_keep.reg.GetReg()) && (rl_free.reg.GetReg() != rl_keep.reg.GetHighReg()) &&
    (rl_free.reg.GetHighReg() != rl_keep.reg.GetReg()) && (rl_free.reg.GetHighReg() != rl_keep.reg.GetHighReg())) {
    // No overlap, free both
    FreeTemp(rl_free.reg.GetReg());
    FreeTemp(rl_free.reg.GetHighReg());
  }
}
/*
 * TUNING: is true leaf?  Can't just use METHOD_IS_LEAF to determine as some
 * instructions might call out to C/assembly helper functions.  Until
 * machinery is in place, always spill lr.
 */

void Arm64Mir2Lir::AdjustSpillMask() {
  core_spill_mask_ |= (1 << rARM_LR);
  num_core_spills_++;
}

/*
 * Mark a callee-save fp register as promoted.  Note that
 * vpush/vpop uses contiguous register lists so we must
 * include any holes in the mask.  Associate holes with
 * Dalvik register INVALID_VREG (0xFFFFU).
 */
void Arm64Mir2Lir::MarkPreservedSingle(int v_reg, int reg) {
  DCHECK_GE(reg, ARM_FP_REG_MASK + ARM_FP_CALLEE_SAVE_BASE);
  reg = (reg & ARM_FP_REG_MASK) - ARM_FP_CALLEE_SAVE_BASE;
  // Ensure fp_vmap_table is large enough
  int table_size = fp_vmap_table_.size();
  for (int i = table_size; i < (reg + 1); i++) {
    fp_vmap_table_.push_back(INVALID_VREG);
  }
  // Add the current mapping
  fp_vmap_table_[reg] = v_reg;
  // Size of fp_vmap_table is high-water mark, use to set mask
  num_fp_spills_ = fp_vmap_table_.size();
  fp_spill_mask_ = ((1 << num_fp_spills_) - 1) << ARM_FP_CALLEE_SAVE_BASE;
}

void Arm64Mir2Lir::FlushRegWide(int reg1, int reg2) {
  RegisterInfo* info1 = GetRegInfo(reg1);
  RegisterInfo* info2 = GetRegInfo(reg2);
  DCHECK(info1 && info2 && info1->pair && info2->pair &&
       (info1->partner == info2->reg) &&
       (info2->partner == info1->reg));
  if ((info1->live && info1->dirty) || (info2->live && info2->dirty)) {
    if (!(info1->is_temp && info2->is_temp)) {
      /* Should not happen.  If it does, there's a problem in eval_loc */
      LOG(FATAL) << "Long half-temp, half-promoted";
    }

    info1->dirty = false;
    info2->dirty = false;
    if (mir_graph_->SRegToVReg(info2->s_reg) <
        mir_graph_->SRegToVReg(info1->s_reg))
      info1 = info2;
    int v_reg = mir_graph_->SRegToVReg(info1->s_reg);
    StoreBaseDispWide(rARM_SP, VRegOffset(v_reg), info1->reg, info1->partner);
  }
}

void Arm64Mir2Lir::FlushReg(int reg) {
  RegisterInfo* info = GetRegInfo(reg);
  if (info->live && info->dirty) {
    info->dirty = false;
    int v_reg = mir_graph_->SRegToVReg(info->s_reg);
    StoreBaseDisp(rARM_SP, VRegOffset(v_reg), reg, kWord);
  }
}

/* Give access to the target-dependent FP register encoding to common code */
bool Arm64Mir2Lir::IsFpReg(int reg) {
  return ARM_FPREG(reg);
}

/* Clobber all regs that might be used by an external C call */
void Arm64Mir2Lir::ClobberCallerSave() {
  Clobber(r0);
  Clobber(r1);
  Clobber(r2);
  Clobber(r3);
  Clobber(r12);
  Clobber(r30lr);
  Clobber(fr0);
  Clobber(fr1);
  Clobber(fr2);
  Clobber(fr3);
  Clobber(fr4);
  Clobber(fr5);
  Clobber(fr6);
  Clobber(fr7);
  Clobber(fr8);
  Clobber(fr9);
  Clobber(fr10);
  Clobber(fr11);
  Clobber(fr12);
  Clobber(fr13);
  Clobber(fr14);
  Clobber(fr15);
}

RegLocation Arm64Mir2Lir::GetReturnWideAlt() {
  RegLocation res = LocCReturnWide();
  res.reg.SetReg(r2);
  res.reg.SetHighReg(r3);
  Clobber(r2);
  Clobber(r3);
  MarkInUse(r2);
  MarkInUse(r3);
  MarkPair(res.reg.GetReg(), res.reg.GetHighReg());
  return res;
}

RegLocation Arm64Mir2Lir::GetReturnAlt() {
  RegLocation res = LocCReturn();
  res.reg.SetReg(r1);
  Clobber(r1);
  MarkInUse(r1);
  return res;
}

/* To be used when explicitly managing register use */
void Arm64Mir2Lir::LockCallTemps() {
  LockTemp(r0);
  LockTemp(r1);
  LockTemp(r2);
  LockTemp(r3);
}

/* To be used when explicitly managing register use */
void Arm64Mir2Lir::FreeCallTemps() {
  FreeTemp(r0);
  FreeTemp(r1);
  FreeTemp(r2);
  FreeTemp(r3);
}

int Arm64Mir2Lir::LoadHelper(ThreadOffset offset) {
  LoadWordDisp(rARM_SELF, offset.Int32Value(), rARM_LR);
  return rARM_LR;
}

LIR* Arm64Mir2Lir::CheckSuspendUsingLoad() {
  int tmp = r0;
  LoadWordDisp(rARM_SELF, Thread::ThreadSuspendTriggerOffset().Int32Value(), tmp);
  LIR* load2 = LoadWordDisp(tmp, 0, tmp);
  return load2;
}

uint64_t Arm64Mir2Lir::GetTargetInstFlags(int opcode) {
  DCHECK(!IsPseudoLirOp(opcode));
  return Arm64Mir2Lir::EncodingMap[UNWIDE(opcode)].flags;
}

const char* Arm64Mir2Lir::GetTargetInstName(int opcode) {
  DCHECK(!IsPseudoLirOp(opcode));
  return Arm64Mir2Lir::EncodingMap[UNWIDE(opcode)].name;
}

const char* Arm64Mir2Lir::GetTargetInstFmt(int opcode) {
  DCHECK(!IsPseudoLirOp(opcode));
  return Arm64Mir2Lir::EncodingMap[UNWIDE(opcode)].fmt;
}

}  // namespace art
