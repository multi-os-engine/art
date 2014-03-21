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

#ifndef ART_COMPILER_DEX_QUICK_ARM64_ARM64_LIR_H_
#define ART_COMPILER_DEX_QUICK_ARM64_ARM64_LIR_H_

#include "dex/compiler_internals.h"

namespace art {

/*
 * Runtime register usage conventions.
 *
 * r0-r3: Argument registers in both Dalvik and C/C++ conventions.
 *        However, for Dalvik->Dalvik calls we'll pass the target's Method*
 *        pointer in r0 as a hidden arg0. Otherwise used as codegen scratch
 *        registers.
 * r0-r1: As in C/C++ r0 is 32-bit return register and r0/r1 is 64-bit
 * r4   : (rARM_SUSPEND) is reserved (suspend check/debugger assist)
 * r5   : Callee save (promotion target)
 * r6   : Callee save (promotion target)
 * r7   : Callee save (promotion target)
 * r8   : Callee save (promotion target)
 * r9   : (rARM_SELF) is reserved (pointer to thread-local storage)
 * r10  : Callee save (promotion target)
 * r11  : Callee save (promotion target)
 * r12  : Scratch, may be trashed by linkage stubs
 * r13  : (sp) is reserved
 * r14  : (lr) is reserved
 * r15  : (pc) is reserved
 *
 * 5 core temps that codegen can use (r0, r1, r2, r3, r12)
 * 7 core registers that can be used for promotion
 *
 * Floating pointer registers
 * s0-s31
 * d0-d15, where d0={s0,s1}, d1={s2,s3}, ... , d15={s30,s31}
 *
 * s16-s31 (d8-d15) preserved across C calls
 * s0-s15 (d0-d7) trashed across C calls
 *
 * s0-s15/d0-d7 used as codegen temp/scratch
 * s16-s31/d8-d31 can be used for promotion.
 *
 * Calling convention
 *     o On a call to a Dalvik method, pass target's Method* in r0
 *     o r1-r3 will be used for up to the first 3 words of arguments
 *     o Arguments past the first 3 words will be placed in appropriate
 *       out slots by the caller.
 *     o If a 64-bit argument would span the register/memory argument
 *       boundary, it will instead be fully passed in the frame.
 *     o Maintain a 16-byte stack alignment
 *
 *  Stack frame diagram (stack grows down, higher addresses at top):
 *
 * +------------------------+
 * | IN[ins-1]              |  {Note: resides in caller's frame}
 * |       .                |
 * | IN[0]                  |
 * | caller's Method*       |
 * +========================+  {Note: start of callee's frame}
 * | spill region           |  {variable sized - will include lr if non-leaf.}
 * +------------------------+
 * | ...filler word...      |  {Note: used as 2nd word of V[locals-1] if long]
 * +------------------------+
 * | V[locals-1]            |
 * | V[locals-2]            |
 * |      .                 |
 * |      .                 |
 * | V[1]                   |
 * | V[0]                   |
 * +------------------------+
 * |  0 to 3 words padding  |
 * +------------------------+
 * | OUT[outs-1]            |
 * | OUT[outs-2]            |
 * |       .                |
 * | OUT[0]                 |
 * | cur_method*            | <<== sp w/ 16-byte alignment
 * +========================+
 */

// Offset to distinguish FP regs.
#define ARM_FP_REG_OFFSET 32
// Offset to distinguish DP FP regs.
#define ARM_FP_DOUBLE (32 | 64)
// First FP callee save.
#define ARM_FP_CALLEE_SAVE_BASE 16
// Reg types.
#define ARM_REGTYPE(x) ((x) & ARM_FP_DOUBLE)
#define ARM_FPREG(x) (ARM_REGTYPE(x) != 0)
#define ARM_DOUBLEREG(x) (ARM_REGTYPE(x) == ARM_FP_DOUBLE)
#define ARM_SINGLEREG(x) (ARM_REGTYPE(x) == ARM_FP_REG_OFFSET)

/*
 * Note: the low register of a floating point pair is sufficient to
 * create the name of a double, but require both names to be passed to
 * allow for asserts to verify that the pair is consecutive if significant
 * rework is done in this area.  Also, it is a good reminder in the calling
 * code that reg locations always describe doubles as a pair of singles.
 */
#define ARM_S2D(x, y) ((x) | ARM_FP_DOUBLE)
// Mask to strip off fp flags.
#define ARM_FP_REG_MASK (ARM_FP_REG_OFFSET - 1)

enum ArmResourceEncodingPos {
  kArmGPReg0   = 0,
  kArmRegLR    = 30,
  kArmRegSP    = 31,
  kArmFPReg0   = 32,
  kArmRegEnd   = 64,
};

#define ENCODE_ARM_REG_SP           (1ULL << kArmRegSP)
#define ENCODE_ARM_REG_LR           (1ULL << kArmRegLR)

#define IS_SIGNED_IMM(size, value) \
  ((value) >= -(1 << ((size) - 1)) && (value) < (1 << ((size) - 1)))
#define IS_SIGNED_IMM7(value) IS_SIGNED_IMM(7, value)
#define IS_SIGNED_IMM9(value) IS_SIGNED_IMM(9, value)
#define IS_SIGNED_IMM12(value) IS_SIGNED_IMM(12, value)
#define IS_SIGNED_IMM19(value) IS_SIGNED_IMM(19, value)

// Special cases.
#define IS_SIGNED_IMM8(value) (((int8_t)(value)) == (value))

// Quick macro used to define the registers.
#define A64_REGISTER_CODE_LIST(R) \
  R(0)  R(1)  R(2)  R(3)  R(4)  R(5)  R(6)  R(7) \
  R(8)  R(9)  R(10) R(11) R(12) R(13) R(14) R(15) \
  R(16) R(17) R(18) R(19) R(20) R(21) R(22) R(23) \
  R(24) R(25) R(26) R(27) R(28) R(29) R(30) R(31)

enum ArmNativeRegisterPool {
  rARM_ZR = -1,  // Note: we rely on (rARM_ZR & 31) == 31
  rARM_SP = 31,
  rARM_LR = 30,
  rARM_SUSPEND = 4,
  rARM_SELF  = 9,
  r0   = 0,
  r1   = 1,
  r2   = 2,
  r3   = 3,
  r5   = 5,
  r6   = 6,
  r7   = 7,
  r8   = 8,
  r10  = 10,
  r11  = 11,
  r12  = 12,
  r31sp   = 31,
  r30lr   = 30,
  r15pc   = 15,

  // Define the floating point registers.
#define A64_DEFINE_FP_REGISTERS(nr) \
  fr##nr = nr + ARM_FP_REG_OFFSET, \
  dr##nr = nr + ARM_FP_DOUBLE,
  A64_REGISTER_CODE_LIST(A64_DEFINE_FP_REGISTERS)
#undef A64_DEFINE_FP_REGISTERS
};

// Target-independent aliases.
#define rARM_ARG0 r0
#define rARM_ARG1 r1
#define rARM_ARG2 r2
#define rARM_ARG3 r3
#define rARM_FARG0 r0
#define rARM_FARG1 r1
#define rARM_FARG2 r2
#define rARM_FARG3 r3
#define rARM_RET0 r0
#define rARM_RET1 r1
#define rARM_INVOKE_TGT rARM_LR
#define rARM_PC INVALID_REG
#define rARM_COUNT INVALID_REG

// RegisterLocation templates return values (r0, or r0/r1).
const RegLocation arm_loc_c_return
    {kLocPhysReg, 0, 0, 0, 0, 0, 0, 0, 1, kVectorNotUsed,
     RegStorage(RegStorage::k32BitSolo, r0), INVALID_SREG, INVALID_SREG};
const RegLocation arm_loc_c_return_wide
    {kLocPhysReg, 1, 0, 0, 0, 0, 0, 0, 1, kVectorNotUsed,
     RegStorage(RegStorage::k64BitPair, r0, r1), INVALID_SREG, INVALID_SREG};
const RegLocation arm_loc_c_return_float
    {kLocPhysReg, 0, 0, 0, 0, 0, 0, 0, 1, kVectorNotUsed,
     RegStorage(RegStorage::k32BitSolo, r0), INVALID_SREG, INVALID_SREG};
const RegLocation arm_loc_c_return_double
    {kLocPhysReg, 1, 0, 0, 0, 0, 0, 0, 1, kVectorNotUsed,
     RegStorage(RegStorage::k64BitPair, r0, r1), INVALID_SREG, INVALID_SREG};

/**
 * @brief Shift-type to be applied to a register via EncodeShift().
 */
enum A64ShiftEncodings {
  kA64Lsl = 0x0,
  kA64Lsr = 0x1,
  kA64Asr = 0x2,
  kA64Ror = 0x3
};

/**
 * @brief Extend-type to be applied to a register via EncodeExtend().
 */
enum A64RegExtEncodings {
  kA64Uxtb = 0x0,
  kA64Uxth = 0x1,
  kA64Uxtw = 0x2,
  kA64Uxtx = 0x3,
  kA64Sxtb = 0x4,
  kA64Sxth = 0x5,
  kA64Sxtw = 0x6,
  kA64Sxtx = 0x7
};

#define ENCODE_NO_SHIFT (EncodeShift(kA64Lsl, 0))

/*
 * The following enum defines the list of supported A64 instructions by the
 * assembler. Their corresponding EncodingMap positions will be defined in
 * assemble_arm64.cc.
 */
enum ArmOpcode {
  kA64First = 0,
  kThumbAddRRLH = kA64First,     // add(4)  [01000100] H12[01] rm[5..3] rd[2..0].
  kThumbAddPcRel,    // add(5)  [10100] rd[10..8] imm_8[7..0].
  kThumbLdrRRR,      // ldr(2)  [0101100] rm[8..6] rn[5..3] rd[2..0].
  kThumbLdrPcRel,    // ldr(3)  [01001] rd[10..8] imm_8[7..0].
  kThumbLdrbRRI5,    // ldrb(1) [01111] imm_5[10..6] rn[5..3] rd[2..0].
  kThumbLdrbRRR,     // ldrb(2) [0101110] rm[8..6] rn[5..3] rd[2..0].
  kThumbLdrhRRI5,    // ldrh(1) [10001] imm_5[10..6] rn[5..3] rd[2..0].
  kThumbLdrhRRR,     // ldrh(2) [0101101] rm[8..6] rn[5..3] rd[2..0].
  kThumbLdrsbRRR,    // ldrsb   [0101011] rm[8..6] rn[5..3] rd[2..0].
  kThumbLdrshRRR,    // ldrsh   [0101111] rm[8..6] rn[5..3] rd[2..0].
  kThumbStrRRR,      // str(2)  [0101000] rm[8..6] rn[5..3] rd[2..0].
  kThumbStrbRRI5,    // strb(1) [01110] imm_5[10..6] rn[5..3] rd[2..0].
  kThumbStrbRRR,     // strb(2) [0101010] rm[8..6] rn[5..3] rd[2..0].
  kThumbStrhRRI5,    // strh(1) [10000] imm_5[10..6] rn[5..3] rd[2..0].
  kThumbStrhRRR,     // strh(2) [0101001] rm[8..6] rn[5..3] rd[2..0].
  kThumbSubRRI3,     // sub(1)  [0001111] imm_3[8..6] rn[5..3] rd[2..0]*/
  kThumb2VmlaF64,    // vmla.F64 vd, vn, vm [111011100000] vn[19..16] vd[15..12] [10110000] vm[3..0].
  kThumb2VcvtIF,     // vcvt.F32.S32 vd, vm [1110111010111000] vd[15..12] [10101100] vm[3..0].
  kThumb2VcvtFI,     // vcvt.S32.F32 vd, vm [1110111010111101] vd[15..12] [10101100] vm[3..0].
  kThumb2VcvtDI,     // vcvt.S32.F32 vd, vm [1110111010111101] vd[15..12] [10111100] vm[3..0].
  kThumb2VcvtFd,     // vcvt.F64.F32 vd, vm [1110111010110111] vd[15..12] [10101100] vm[3..0].
  kThumb2VcvtDF,     // vcvt.F32.F64 vd, vm [1110111010110111] vd[15..12] [10111100] vm[3..0].
  kThumb2VcvtF64S32,  // vcvt.F64.S32 vd, vm [1110111010111000] vd[15..12] [10111100] vm[3..0].
  kThumb2VcvtF64U32,  // vcvt.F64.U32 vd, vm [1110111010111000] vd[15..12] [10110100] vm[3..0].
  kThumb2Vsqrts,     // vsqrt.f32 vd, vm [1110111010110001] vd[15..12] [10101100] vm[3..0].
  kThumb2Vsqrtd,     // vsqrt.f64 vd, vm [1110111010110001] vd[15..12] [10111100] vm[3..0].
  kThumb2MovI8M,     // mov(T2) rd, #<const> [11110] i [00001001111] imm3 rd[11..8] imm8.
  kThumb2StrRRI12,   // str(Imm,T3) rd,[rn,#imm12] [111110001100] rn[19..16] rt[15..12] imm12[11..0].
  kThumb2LdrRRI12,   // str(Imm,T3) rd,[rn,#imm12] [111110001100] rn[19..16] rt[15..12] imm12[11..0].
  kThumb2StrRRI8Predec,  // str(Imm,T4) rd,[rn,#-imm8] [111110000100] rn[19..16] rt[15..12] [1100] imm[7..0].
  kThumb2LdrRRI8Predec,  // ldr(Imm,T4) rd,[rn,#-imm8] [111110000101] rn[19..16] rt[15..12] [1100] imm[7..0].
  kThumb2Sel,        // sel rd, rn, rm [111110101010] rn[19-16] rd[11-8] rm[3-0].
  kThumb2LdrRRR,     // ldr rt,[rn,rm,LSL #imm] [111110000101] rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0].
  kThumb2LdrhRRR,    // ldrh rt,[rn,rm,LSL #imm] [111110000101] rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0].
  kThumb2LdrshRRR,   // ldrsh rt,[rn,rm,LSL #imm] [111110000101] rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0].
  kThumb2LdrbRRR,    // ldrb rt,[rn,rm,LSL #imm] [111110000101] rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0].
  kThumb2LdrsbRRR,   // ldrsb rt,[rn,rm,LSL #imm] [111110000101] rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0].
  kThumb2StrhRRR,    // str rt,[rn,rm,LSL #imm] [111110000010] rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0].
  kThumb2StrbRRR,    // str rt,[rn,rm,LSL #imm] [111110000000] rn[19-16] rt[15-12] [000000] imm[5-4] rm[3-0].
  kThumb2LdrhRRI12,  // ldrh rt,[rn,#imm12] [111110001011] rt[15..12] rn[19..16] imm12[11..0].
  kThumb2LdrshRRI12,  // ldrsh rt,[rn,#imm12] [111110011011] rt[15..12] rn[19..16] imm12[11..0].
  kThumb2LdrbRRI12,  // ldrb rt,[rn,#imm12] [111110001001] rt[15..12] rn[19..16] imm12[11..0].
  kThumb2LdrsbRRI12,  // ldrsb rt,[rn,#imm12] [111110011001] rt[15..12] rn[19..16] imm12[11..0].
  kThumb2StrhRRI12,  // strh rt,[rn,#imm12] [111110001010] rt[15..12] rn[19..16] imm12[11..0].
  kThumb2StrbRRI12,  // strb rt,[rn,#imm12] [111110001000] rt[15..12] rn[19..16] imm12[11..0].
  kThumb2RsubRRI8M,  // rsb rd, rn, #<const> [11110] i [011101] rn[19..16] [0] imm3[14..12] rd[11..8] imm8[7..0].
  kThumb2AddRRI8M,   // add rd, rn, #<const> [11110] i [010001] rn[19..16] [0] imm3[14..12] rd[11..8] imm8[7..0].
  kThumb2AdcRRI8M,   // adc rd, rn, #<const> [11110] i [010101] rn[19..16] [0] imm3[14..12] rd[11..8] imm8[7..0].
  kThumb2SubRRI8M,   // sub rd, rn, #<const> [11110] i [011011] rn[19..16] [0] imm3[14..12] rd[11..8] imm8[7..0].
  kThumb2SbcRRI8M,   // sub rd, rn, #<const> [11110] i [010111] rn[19..16] [0] imm3[14..12] rd[11..8] imm8[7..0].
  kThumb2It,         // it [10111111] firstcond[7-4] mask[3-0].
  kThumb2Fmstat,     // fmstat [11101110111100011111101000010000].
  kThumb2Vcmpd,      // vcmp [111011101] D [11011] rd[15-12] [1011] E [1] M [0] rm[3-0].
  kThumb2Vcmps,      // vcmp [111011101] D [11010] rd[15-12] [1011] E [1] M [0] rm[3-0].
  kThumb2LdrPcRel12,  // ldr rd,[pc,#imm12] [1111100011011111] rt[15-12] imm12[11-0].
  kThumb2Fmrs,       // vmov [111011100000] vn[19-16] rt[15-12] [1010] N [0010000].
  kThumb2Fmsr,       // vmov [111011100001] vn[19-16] rt[15-12] [1010] N [0010000].
  kThumb2Fmrrd,      // vmov [111011000100] rt2[19-16] rt[15-12] [101100] M [1] vm[3-0].
  kThumb2Fmdrr,      // vmov [111011000101] rt2[19-16] rt[15-12] [101100] M [1] vm[3-0].
  kThumb2Mla,        // mla [111110110000] rn[19-16] ra[15-12] rd[7-4] [0000] rm[3-0].
  kThumb2Umull,      // umull [111110111010] rn[19-16], rdlo[15-12] rdhi[11-8] [0000] rm[3-0].
  kThumb2Ldrex,      // ldrex [111010000101] rn[19-16] rt[15-12] [1111] imm8[7-0].
  kThumb2Ldrexd,     // ldrexd [111010001101] rn[19-16] rt[15-12] rt2[11-8] [11111111].
  kThumb2Strex,      // strex [111010000100] rn[19-16] rt[15-12] rd[11-8] imm8[7-0].
  kThumb2Strexd,     // strexd [111010001100] rn[19-16] rt[15-12] rt2[11-8] [0111] Rd[3-0].
  kThumb2Clrex,      // clrex [11110011101111111000111100101111].
  kThumb2Dmb,        // dmb [1111001110111111100011110101] option[3-0].
  kThumb2LdrPcReln12,  // ldr rd,[pc,-#imm12] [1111100011011111] rt[15-12] imm12[11-0].
  kThumb2VPopCS,     // vpop <list of callee save fp singles (s16+).
  kThumb2VPushCS,    // vpush <list callee save fp singles (s16+).
  kThumb2Vldms,      // vldms rd, <list>.
  kThumb2Vstms,      // vstms rd, <list>.
  kThumb2AddPCR,     // Thumb2 2-operand add with hard-coded PC target.
  kThumb2Adr,        // Special purpose encoding of ADR for switch tables.
  kThumb2MovImm16LST,  // Special purpose version for switch table use.
  kThumb2MovImm16HST,  // Special purpose version for switch table use.
  kThumb2LdmiaWB,    // ldmia  [111010011001[ rn[19..16] mask[15..0].
  kThumb2OrrRRRs,    // orrs [111010100101] rn[19..16] [0000] rd[11..8] [0000] rm[3..0].
  kThumb2RsubRRR,    // rsb [111010111101] rn[19..16] [0000] rd[11..8] [0000] rm[3..0].
  kThumb2Smull,      // smull [111110111000] rn[19-16], rdlo[15-12] rdhi[11-8] [0000] rm[3-0].
  kThumb2LdrdPcRel8,  // ldrd rt, rt2, pc +-/1024.
  kThumb2LdrdI8,     // ldrd rt, rt2, [rn +-/1024].

  // A64 instruction set begins here
#if WITH_A64_HOST_SIMULATOR == 1
  kA64x86Trampoline,  // 8-bytes reserved for a x86 trampoline call.
  kA64x86BlR,        // 8-bytes to call x86 native code in the host simulator.
#endif
  kA64Adc3rrr,       // adc [00011010000] rm[20-16] [000000] rn[9-5] rd[4-0].
  kA64Add4RRdT,      // add [s001000100] imm_12[21-10] rn[9-5] rd[4-0].
  kA64Add4rrro,      // add [00001011000] rm[20-16] option[15-13] imm_3[12-10] rn[9-5] rd[4-0].
  kA64And3Rrl,       // and [00010010] N[22] imm_r[21-16] imm_s[15-10] rn[9-5] rd[4-0].
  kA64And4rrro,      // and [00001010] shift[23-22] [N=0] rm[20-16] imm_6[15-10] rn[9-5] rd[4-0].
  kA64Asr3rrd,       // asr [0001001100] immr[21-16] imms[15-10] rn[9-5] rd[4-0].
  kA64Asr3rrr,       // asr alias of "sbfm arg0, arg1, arg2, {#31/#63}".
  kA64BCond,         // b.cond [01010100] imm_19[23-5] [0] cond[3-0].
  kA64Blr1r,         // blr [1101011000111111000000] rn[9-5] [00000].
  kA64BR,            // br  [1101011000011111000000] rn[9-5] [00000].
  kA64BrkI16,        // brk [11010100001] imm_16[20-5] [00000].
  kA64BUncond,       // b   [00010100] offset_26[25-0].
  kA64CbnzW,         // cbnz[00110101] imm_19[23-5] rt[4-0].
  kA64CbzW,          // cbz [00110100] imm_19[23-5] rt[4-0].
  kA64Cmn3Rro,       // cmn [s0101011001] rm[20-16] option[15-13] imm_3[12-10] rn[9-5] [11111].
  kA64Cmn3RdT,       // cmn [00110001] shift[23-22] imm_12[21-10] rn[9-5] [11111].
  kA64Cmp3Rro,       // cmp [s1101011001] rm[20-16] option[15-13] imm_3[12-10] rn[9-5] [11111].
  kA64Cmp3RdT,       // cmp [01110001] shift[23-22] imm_12[21-10] rn[9-5] [11111].
  kA64Eor3Rrl,       // eor [s10100100] N[22] imm_r[21-16] imm_s[15-10] rn[9-5] rd[4-0].
  kA64Eor4rrro,      // eor [s1001010] shift[23-22] [0] rm[20-16] imm_6[15-10] rn[9-5] rd[4-0].
  kA64Extr4rrrd,     // extr[s00100111N0] rm[20-16] imm_s[15-10] rn[9-5] rd[4-0].
  kA64Fabs2ff,       // fabs[000111100s100000110000] rn[9-5] rd[4-0].
  kA64Fadd3fff,      // fadd[000111100s1] rm[20-16] [001010] rn[9-5] rd[4-0].
  kA64Fdiv3fff,      // fdiv[000111100s1] rm[20-16] [000110] rn[9-5] rd[4-0].
  kA64Fmov2ff,       // fmov[000111100s100000010000] rn[9-5] rd[4-0].
  kA64Fmov2fI,       // fmov[000111100s1] imm_8[20-13] [10000000] rd[4-0].
  kA64Fmov2Sx,       // fmov[1001111001100111000000] rn[9-5] rd[4-0].
  kA64Fmov2sw,       // fmov[0001111000100111000000] rn[9-5] rd[4-0].
  kA64Fmul3fff,      // fmul[000111100s1] rm[20-16] [000010] rn[9-5] rd[4-0].
  kA64Fneg2ff,       // fneg[000111100s100001010000] rn[9-5] rd[4-0].
  kA64Fsub3fff,      // fsub[000111100s1] rm[20-16] [001110] rn[9-5] rd[4-0].
  kA64Ldr2fp,        // ldr [0s011100] imm_19[23-5] rt[4-0].
  kA64Ldr3fXD,       // ldr [1s11110100] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Ldr4fXxF,      // ldr [1s111100011] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64LdrWXI12,      // ldr [10111000010] imm_9[20-12] [01] rn[9-5] rt[4-0].
  kA64LdrXXI12,      // ldr [11111000010] imm_9[20-12] [01] rn[9-5] rt[4-0].
  kA64LdrPostWXI9,   // ldr [10111000010] imm_9[20-12] [01] rn[9-5] rt[4-0].
  kA64LdrPostXXI9,   // ldr [11111000010] imm_9[20-12] [01] rn[9-5] rt[4-0].
  kA64LdpWWXI7,      // ldp [0010100101] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64LdpPostWWXI7,  // ldp [0010100011] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64LdpPostXXXI7,  // ldp [1010100011] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64Ldur3fXd,      // ldur[1s111100010] imm_9[20-12] [00] rn[9-5] rt[4-0].
  kA64Lsl3rrr,       // lsl [s0011010110] rm[20-16] [001000] rn[9-5] rd[4-0].
  kA64Lsr3rrd,       // lsr alias of "ubfm arg0, arg1, arg2, #{31/63}".
  kA64Lsr3rrr,       // lsr [s0011010110] rm[20-16] [001001] rn[9-5] rd[4-0].
  kA64Movk3rdM,      // mov [010100101] hw[22-21] imm_16[20-5] rd[4-0].
  kA64Movn3rdM,      // mov [000100101] hw[22-21] imm_16[20-5] rd[4-0].
  kA64Movz3rdM,      // mov [011100101] hw[22-21] imm_16[20-5] rd[4-0].
  kA64Mov2rr,        // mov [00101010000] rm[20-16] [000000] [11111] rd[4-0].
  kA64Mvn2rr,        // mov [00101010001] rm[20-16] [000000] [11111] rd[4-0].
  kA64Mul3rrr,       // mul [00011011000] rm[20-16] [011111] rn[9-5] rd[4-0].
  kA64Neg3rro,       // neg alias of "sub arg0, rzr, arg1, arg2".
  kA64Orr3Rrl,       // orr [s01100100] N[22] imm_r[21-16] imm_s[15-10] rn[9-5] rd[4-0].
  kA64Orr4rrro,      // orr [s0101010] shift[23-22] [0] rm[20-16] imm_6[15-10] rn[9-5] rd[4-0].
  kA64Ret,           // ret [11010110010111110000001111000000].
  kA64Rev2rr,        // rev [s10110101100000000001x] rn[9-5] rd[4-0].
  kA64Rev162rr,      // rev16[s101101011000000000001] rn[9-5] rd[4-0].
  kA64Ror3rrr,       // ror [s0011010110] rm[20-16] [001011] rn[9-5] rd[4-0].
  kA64Sbc3rrr,       // sbc [s0011010000] rm[20-16] [000000] rn[9-5] rd[4-0].
  kA64Sbfm4rrdd,     // sbfm[0001001100] imm_r[21-16] imm_s[15-10] rn[9-5] rd[4-0].
  kA64Sdiv3rrr,      // sdiv[s0011010110] rm[20-16] [000011] rn[9-5] rd[4-0].
  kA64StpWWXI7,      // stp [0010100101] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64StpPostWWXI7,  // stp [0010100010] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64StpPostXXXI7,  // stp [1010100010] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64StpPreWWXI7,   // stp [0010100110] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64StpPreXXXI7,   // stp [1010100110] imm_7[21-15] rt2[14-10] rn[9-5] rt[4-0].
  kA64Str3fXD,       // str [1s11110100] imm_12[21-10] rn[9-5] rt[4-0].
  kA64Str4fXxF,      // str [1s111100001] rm[20-16] [011] S[12] [10] rn[9-5] rt[4-0].
  kA64StrWXI12,      // str [1011100100] imm_12[21-10] rn[9-5] rt[4-0].
  kA64StrXXI12,      // str [1111100100] imm_12[20-12] rn[9-5] rt[4-0].
  kA64StrWXX,        // str [10111000001] rm[20-16] option[15-13] S[12-12] [10] rn[9-5] rt[4-0].
  kA64StrPostWXI9,   // str [10111000000] imm_9[20-12] [01] rn[9-5] rt[4-0].
  kA64StxrWXX,       // stxr[11001000000] rs[20-16] [011111] rn[9-5] rt[4-0].
  kA64Stur3fXd,      // stur[1s111100000] imm_9[20-12] [00] rn[9-5] rt[4-0].
  kA64Sub4RRdT,      // sub [s101000100] imm_12[21-10] rn[9-5] rd[4-0].
  kA64Sub4rrro,      // sub [s1001011001] rm[20-16] option[15-13] imm_3[12-10] rn[9-5] rd[4-0].
  kA64Subs3rRd,      // subs[s111000100] imm_12[21-10] rn[9-5] rd[4-0].
  kA64Tst3rro,       // tst alias of "ands rzr, arg1, arg2, arg3".
  kA64Ubfm4rrdd,     // ubfm[s10100110] N[22] imm_r[21-16] imm_s[15-10] rn[9-5] rd[4-0].
  kA64Last,
  kA64Wide = 0x1000  // Flag used to select the 64-bit variant of the instruction.
};

/*
 * The A64 instruction set provides two variants for many instructions. For example, "mov wN, wM"
 * and "mov xN, xM" or - for floating point instructions - "mov sN, sM" and "mov dN, dM".
 * It definitely makes sense to exploit these symmetries of the instruction set. We do this via the
 * WIDE, UNWIDE macros. For opcodes that allow it, the wide variant can be obtained by applying the
 * WIDE macro to the non-wide opcode. E.g. WIDE(kA64Sub4RRdT).
 */

// Return the wide and no-wide variants of the given opcode.
#define WIDE(op) ((ArmOpcode)((op) | kA64Wide))
#define UNWIDE(op) ((ArmOpcode)((op) & ~kA64Wide))

// Whether the given opcode is wide.
#define IS_WIDE(op) (((op) & kA64Wide) != 0)

/*
 * Floating point variants. These are just aliases of the macros above which we use for floating
 * point instructions, just for readibility reasons.
 * TODO(Arm64): should we remove these and use the original macros?
 */
#define FWIDE WIDE
#define FUNWIDE UNWIDE
#define IS_FWIDE IS_WIDE

#define OP_KIND_UNWIDE(opcode) (opcode)
#define OP_KIND_IS_WIDE(opcode) (false)

enum ArmOpDmbOptions {
  kSY = 0xf,
  kST = 0xe,
  kISH = 0xb,
  kISHST = 0xa,
  kNSH = 0x7,
  kNSHST = 0x6
};

// Instruction assembly field_loc kind.
enum ArmEncodingKind {
  // All the formats below are encoded in the same way (as a kFmtBitBlt).
  // These are grouped together, for fast handling (e.g. "if (LIKELY(fmt <= kFmtBitBlt)) ...").
  kFmtRegW = 0,  // Word register (w) or wzr.
  kFmtRegX,      // Extended word register (x) or xzr.
  kFmtRegR,      // Register with same wideness as instruction or zr.
  kFmtRegWOrSp,  // Word register (w) or wsp.
  kFmtRegXOrSp,  // Extended word register (x) or sp.
  kFmtRegROrSp,  // Register with same wideness as instruction or sp.
  kFmtRegS,      // Single FP reg.
  kFmtRegD,      // Double FP reg.
  kFmtRegF,      // Single/double FP reg depending on wideness.
  kFmtBitBlt,    // Bit string using end/start.

  // Less likely formats.
  kFmtUnused,    // Unused field and marks end of formats.
  kFmtModImm,    // Shifted 8-bit immed using [26,14..12,7..0].
  kFmtImm16,     // Zero-extended immed using [26,19..16,14..12,7..0].
  kFmtImm6,      // Encoded branch target using [9,7..3]0.
  kFmtImm12,     // Zero-extended immediate using [26,14..12,7..0].
  kFmtShift,     // Identical to kFmtExtShift, but restricted to shift.
  kFmtExtShift,  // Register extend or shift, 9-bit at [23..21, 15..10].
  kFmtLsb,       // least significant bit using [14..12][7..6].
  kFmtBWidth,    // bit-field width, encoded as width-1.
  kFmtShift5,    // Shift count, [14..12,7..6].
  kFmtBrOffset,  // Signed extended [26,11,13,21-16,10-0]:0.
  kFmtFPImm,     // Encoded floating point immediate.
  kFmtSkip,      // Unused field, but continue to next.
};

// Struct used to define the snippet positions for each A64 opcode.
struct ArmEncodingMap {
  uint32_t wskeleton;
  uint32_t xskeleton;
  struct {
    ArmEncodingKind kind;
    int end;         // end for kFmtBitBlt, 1-bit slice end for FP regs.
    int start;       // start for kFmtBitBlt, 4-bit slice end for FP regs.
  } field_loc[4];
  ArmOpcode opcode;  // can be WIDE()-ned to indicate it has a wide variant.
  uint64_t flags;
  const char* name;
  const char* fmt;
  int size;          // Note: size is in bytes.
  FixupKind fixup;
};

#if 0
// TODO(Arm64): try the following alternative, which fits exactly in one cache line (64 bytes).
struct ArmEncodingMap {
  uint32_t wskeleton;
  uint32_t xskeleton;
  uint64_t flags;
  const char* name;
  const char* fmt;
  struct {
    uint8_t kind;
    int8_t end;         // end for kFmtBitBlt, 1-bit slice end for FP regs.
    int8_t start;       // start for kFmtBitBlt, 4-bit slice end for FP regs.
  } field_loc[4];
  uint32_t fixup;
  uint32_t opcode;         // can be WIDE()-ned to indicate it has a wide variant.
  uint32_t padding[3];
};
#endif

}  // namespace art

#endif  // ART_COMPILER_DEX_QUICK_ARM64_ARM64_LIR_H_
