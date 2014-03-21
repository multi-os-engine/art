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

// The macros below are exclusively used in the encoding map.

// Most generic way of providing two variants for one instruction.
#define CUSTOM_VARIANTS(variant1, variant2) variant1, variant2

// Used for instructions which do not have a wide variant.
#define NO_VARIANTS(variant) \
  CUSTOM_VARIANTS(variant, 0)

// Used for instructions which have a wide variant with the sf bit set to 1.
#define SF_VARIANTS(sf0_skeleton) \
  CUSTOM_VARIANTS(sf0_skeleton, (sf0_skeleton | 0x80000000))

// Used for instructions which have a wide variant with the sf and n bits set to 1.
#define SF_N_VARIANTS(sf0_n0_skeleton) \
  CUSTOM_VARIANTS(sf0_n0_skeleton, (sf0_n0_skeleton | 0x80400000))

// Used for FP instructions which have a single and double precision variants, with he type bits set
// to either 00 or 01.
#define FLOAT_VARIANTS(type00_skeleton) \
  CUSTOM_VARIANTS(type00_skeleton, (type00_skeleton | 0x00400000))

/*
 * opcode: ArmOpcode enum
 * skeleton: pre-designated bit-pattern for this opcode
 * k0: key to applying ds/de
 * ds: dest start bit position
 * de: dest end bit position
 * k1: key to applying s1s/s1e
 * s1s: src1 start bit position
 * s1e: src1 end bit position
 * k2: key to applying s2s/s2e
 * s2s: src2 start bit position
 * s2e: src2 end bit position
 * operands: number of operands (for sanity check purposes)
 * name: mnemonic name
 * fmt: for pretty-printing
 */
#define ENCODING_MAP(opcode, variants, k0, ds, de, k1, s1s, s1e, k2, s2s, s2e, \
                     k3, k3s, k3e, flags, name, fmt, fixup) \
        {variants, {{k0, ds, de}, {k1, s1s, s1e}, {k2, s2s, s2e},          \
                    {k3, k3s, k3e}}, opcode, flags, name, fmt, 4, fixup}

// TODO(Arm64): remove the macros below.
#define OLD_ENCD_MAP(opcode, skeleton, k0, ds, de, k1, s1s, s1e, k2, s2s, s2e, \
                     k3, k3s, k3e, flags, name, fmt, size, fixup) \
        {skeleton, 0, {{k0, ds, de}, {k1, s1s, s1e}, {k2, s2s, s2e},          \
                    {k3, k3s, k3e}}, opcode, flags, name, fmt, size, fixup}
#define kFmtDfp kFmtRegD
#define kFmtSfp kFmtRegS

/* Instruction dump string format keys: !pf, where "!" is the start
 * of the key, "p" is which numeric operand to use and "f" is the
 * print format.
 *
 * [p]ositions:
 *     0 -> operands[0] (dest)
 *     1 -> operands[1] (src1)
 *     2 -> operands[2] (src2)
 *     3 -> operands[3] (extra)
 *
 * [f]ormats:
 *     h-> 4-digit hex
 *     d -> decimal
 *     D -> decimal*4 or decimal*8 depending on the instruction wideness
 *     E -> decimal*4
 *     F -> decimal*2
 *     c -> branch condition (beq, bne, etc.)
 *     t -> pc-relative target
 *     p -> pc-relative address
 *     s -> single precision floating point register
 *     S -> double precision floating point register
 *     f -> single or double precision register (depending on wideness).
 *     I -> 8-bit immediate floating point number
 *     l -> logical immediate
 *     n -> complimented Thumb2 modified immediate
 *     M -> 16-bit shift expression ("" or ", lsl #16" or ", lsl #32"...)
 *     b -> 4-digit binary
 *     B -> dmb option string (sy, st, ish, ishst, nsh, hshst)
 *     H -> operand shift
 *     T -> register shift (either ", lsl #0" or ", lsl #12")
 *     o -> register extend (e.g. uxtb #1) for Word registers
 *     O -> register extend (e.g. uxtb #1) for eXtended registers
 *     C -> core register name
 *     w -> word (32-bit) register wn, or wzr
 *     W -> word (32-bit) register wn, or wsp
 *     x -> extended (64-bit) register xn, or xzr
 *     X -> extended (64-bit) register xn, or sp
 *     r -> register with same wideness as instruction, r31 -> wzr, xzr
 *     R -> register with same wideness as instruction, r31 -> wsp, sp
 *     P -> fp cs register list (base of s16)
 *     Q -> fp cs register list (base of s0)
 *
 *  [!] escape.  To insert "!", use "!!"
 */
/* NOTE: must be kept in sync with enum ArmOpcode from arm64_lir.h */
const ArmEncodingMap Arm64Mir2Lir::EncodingMap[kA64Last] = {
    OLD_ENCD_MAP(kThumbAddRRLH,     0x4440,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE01,
                 "add", "!0C, !1C", 2, kFixupNone),
    OLD_ENCD_MAP(kThumbAddPcRel,    0xa000,
                 kFmtBitBlt, 10, 8, kFmtBitBlt, 7, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | IS_BRANCH | NEEDS_FIXUP,
                 "add", "!0C, pc, #!1E", 2, kFixupLoad),
    OLD_ENCD_MAP(kThumbLdrRRR,       0x5800,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldr", "!0C, [!1C, !2C]", 2, kFixupNone),
    OLD_ENCD_MAP(kThumbLdrPcRel,    0x4800,
                 kFmtBitBlt, 10, 8, kFmtBitBlt, 7, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0 | REG_USE_PC
                 | IS_LOAD | NEEDS_FIXUP, "ldr", "!0C, [pc, #!1E]", 2, kFixupLoad),
    OLD_ENCD_MAP(kThumbLdrbRRI5,     0x7800,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldrb", "!0C, [!1C, #2d]", 2, kFixupNone),
    OLD_ENCD_MAP(kThumbLdrbRRR,      0x5c00,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldrb", "!0C, [!1C, !2C]", 2, kFixupNone),
    OLD_ENCD_MAP(kThumbLdrhRRI5,     0x8800,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldrh", "!0C, [!1C, #!2F]", 2, kFixupNone),
    OLD_ENCD_MAP(kThumbLdrhRRR,      0x5a00,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldrh", "!0C, [!1C, !2C]", 2, kFixupNone),
    OLD_ENCD_MAP(kThumbLdrsbRRR,     0x5600,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldrsb", "!0C, [!1C, !2C]", 2, kFixupNone),
    OLD_ENCD_MAP(kThumbLdrshRRR,     0x5e00,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldrsh", "!0C, [!1C, !2C]", 2, kFixupNone),
    OLD_ENCD_MAP(kThumbStrRRR,       0x5000,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE012 | IS_STORE,
                 "str", "!0C, [!1C, !2C]", 2, kFixupNone),
    OLD_ENCD_MAP(kThumbStrbRRI5,     0x7000,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "strb", "!0C, [!1C, #!2d]", 2, kFixupNone),
    OLD_ENCD_MAP(kThumbStrbRRR,      0x5400,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE012 | IS_STORE,
                 "strb", "!0C, [!1C, !2C]", 2, kFixupNone),
    OLD_ENCD_MAP(kThumbStrhRRI5,     0x8000,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 10, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "strh", "!0C, [!1C, #!2F]", 2, kFixupNone),
    OLD_ENCD_MAP(kThumbStrhRRR,      0x5200,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE012 | IS_STORE,
                 "strh", "!0C, [!1C, !2C]", 2, kFixupNone),
    OLD_ENCD_MAP(kThumbSubRRI3,      0x1e00,
                 kFmtBitBlt, 2, 0, kFmtBitBlt, 5, 3, kFmtBitBlt, 8, 6,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "subs", "!0C, !1C, #!2d", 2, kFixupNone),
    OLD_ENCD_MAP(kThumb2VmlaF64,     0xee000b00,
                 kFmtDfp, 22, 12, kFmtDfp, 7, 16, kFmtDfp, 5, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0 | REG_USE012,
                 "vmla", "!0S, !1S, !2S", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2VcvtIF,       0xeeb80ac0,
                 kFmtSfp, 22, 12, kFmtSfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vcvt.f32.s32", "!0s, !1s", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2VcvtFI,       0xeebd0ac0,
                 kFmtSfp, 22, 12, kFmtSfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vcvt.s32.f32 ", "!0s, !1s", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2VcvtDI,       0xeebd0bc0,
                 kFmtSfp, 22, 12, kFmtDfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vcvt.s32.f64 ", "!0s, !1S", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2VcvtFd,       0xeeb70ac0,
                 kFmtDfp, 22, 12, kFmtSfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vcvt.f64.f32 ", "!0S, !1s", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2VcvtDF,       0xeeb70bc0,
                 kFmtSfp, 22, 12, kFmtDfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vcvt.f32.f64 ", "!0s, !1S", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2VcvtF64S32,   0xeeb80bc0,
                 kFmtDfp, 22, 12, kFmtSfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vcvt.f64.s32 ", "!0S, !1s", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2VcvtF64U32,   0xeeb80b40,
                 kFmtDfp, 22, 12, kFmtSfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vcvt.f64.u32 ", "!0S, !1s", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Vsqrts,       0xeeb10ac0,
                 kFmtSfp, 22, 12, kFmtSfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vsqrt.f32 ", "!0s, !1s", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Vsqrtd,       0xeeb10bc0,
                 kFmtDfp, 22, 12, kFmtDfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "vsqrt.f64 ", "!0S, !1S", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2MovI8M, 0xf04f0000, /* no setflags encoding */
                 kFmtBitBlt, 11, 8, kFmtModImm, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0,
                 "mov", "!0C, #!1m", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2StrRRI12,       0xf8c00000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 11, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "str", "!0C, [!1C, #!2d]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2LdrRRI12,       0xf8d00000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 11, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldr", "!0C, [!1C, #!2d]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2StrRRI8Predec,       0xf8400c00,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 8, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "str", "!0C, [!1C, #-!2d]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2LdrRRI8Predec,       0xf8500c00,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 8, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldr", "!0C, [!1C, #-!2d]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Sel,       0xfaa0f080,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE12 | USES_CCODES,
                 "sel", "!0C, !1C, !2C", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2LdrRRR,    0xf8500000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtBitBlt, 5, 4, IS_QUAD_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldr", "!0C, [!1C, !2C, LSL #!3d]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2LdrhRRR,    0xf8300000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtBitBlt, 5, 4, IS_QUAD_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldrh", "!0C, [!1C, !2C, LSL #!3d]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2LdrshRRR,    0xf9300000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtBitBlt, 5, 4, IS_QUAD_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldrsh", "!0C, [!1C, !2C, LSL #!3d]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2LdrbRRR,    0xf8100000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtBitBlt, 5, 4, IS_QUAD_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldrb", "!0C, [!1C, !2C, LSL #!3d]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2LdrsbRRR,    0xf9100000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtBitBlt, 5, 4, IS_QUAD_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldrsb", "!0C, [!1C, !2C, LSL #!3d]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2StrhRRR,    0xf8200000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtBitBlt, 5, 4, IS_QUAD_OP | REG_USE012 | IS_STORE,
                 "strh", "!0C, [!1C, !2C, LSL #!3d]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2StrbRRR,    0xf8000000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtBitBlt, 5, 4, IS_QUAD_OP | REG_USE012 | IS_STORE,
                 "strb", "!0C, [!1C, !2C, LSL #!3d]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2LdrhRRI12,       0xf8b00000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 11, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldrh", "!0C, [!1C, #!2d]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2LdrshRRI12,       0xf9b00000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 11, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldrsh", "!0C, [!1C, #!2d]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2LdrbRRI12,       0xf8900000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 11, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldrb", "!0C, [!1C, #!2d]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2LdrsbRRI12,       0xf9900000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 11, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldrsb", "!0C, [!1C, #!2d]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2StrhRRI12,       0xf8a00000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 11, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "strh", "!0C, [!1C, #!2d]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2StrbRRI12,       0xf8800000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 11, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "strb", "!0C, [!1C, #!2d]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2RsubRRI8M,       0xf1d00000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtModImm, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "rsbs", "!0C,!1C,#!2m", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2AddRRI8M,  0xf1100000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtModImm, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "adds", "!0C, !1C, #!2m", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2AdcRRI8M,  0xf1500000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtModImm, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES | USES_CCODES,
                 "adcs", "!0C, !1C, #!2m", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2SubRRI8M,  0xf1b00000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtModImm, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "subs", "!0C, !1C, #!2m", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2SbcRRI8M,  0xf1700000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtModImm, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES | USES_CCODES,
                 "sbcs", "!0C, !1C, #!2m", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2It,  0xbf00,
                 kFmtBitBlt, 7, 4, kFmtBitBlt, 3, 0, kFmtModImm, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | IS_IT | USES_CCODES,
                 "it:!1b", "!0c", 2, kFixupNone),
    OLD_ENCD_MAP(kThumb2Fmstat,  0xeef1fa10,
                 kFmtUnused, -1, -1, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND | SETS_CCODES,
                 "fmstat", "", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Vcmpd,        0xeeb40b40,
                 kFmtDfp, 22, 12, kFmtDfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_USE01,
                 "vcmp.f64", "!0S, !1S", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Vcmps,        0xeeb40a40,
                 kFmtSfp, 22, 12, kFmtSfp, 5, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_USE01,
                 "vcmp.f32", "!0s, !1s", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2LdrPcRel12,       0xf8df0000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 11, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0 | REG_USE_PC | IS_LOAD | NEEDS_FIXUP,
                 "ldr", "!0C, [r15pc, #!1d]", 4, kFixupLoad),
    OLD_ENCD_MAP(kThumb2Fmrs,       0xee100a10,
                 kFmtBitBlt, 15, 12, kFmtSfp, 7, 16, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "fmrs", "!0C, !1s", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Fmsr,       0xee000a10,
                 kFmtSfp, 7, 16, kFmtBitBlt, 15, 12, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "fmsr", "!0s, !1C", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Fmrrd,       0xec500b10,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtDfp, 5, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF01_USE2,
                 "fmrrd", "!0C, !1C, !2S", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Fmdrr,       0xec400b10,
                 kFmtDfp, 5, 0, kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "fmdrr", "!0S, !1C, !2C", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Mla,  0xfb000000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtBitBlt, 15, 12, IS_QUAD_OP | REG_DEF0_USE123,
                 "mla", "!0C, !1C, !2C, !3C", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Umull,  0xfba00000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16,
                 kFmtBitBlt, 3, 0,
                 IS_QUAD_OP | REG_DEF0 | REG_DEF1 | REG_USE2 | REG_USE3,
                 "umull", "!0C, !1C, !2C, !3C", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Ldrex,       0xe8500f00,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16, kFmtBitBlt, 7, 0,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldrex", "!0C, [!1C, #!2E]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Ldrexd,      0xe8d0007f,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF01_USE2 | IS_LOAD,
                 "ldrexd", "!0C, !1C, [!2C]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Strex,       0xe8400000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 15, 12, kFmtBitBlt, 19, 16,
                 kFmtBitBlt, 7, 0, IS_QUAD_OP | REG_DEF0_USE12 | IS_STORE,
                 "strex", "!0C, !1C, [!2C, #!2E]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Strexd,      0xe8c00070,
                 kFmtBitBlt, 3, 0, kFmtBitBlt, 15, 12, kFmtBitBlt, 11, 8,
                 kFmtBitBlt, 19, 16, IS_QUAD_OP | REG_DEF0_USE123 | IS_STORE,
                 "strexd", "!0C, !1C, !2C, [!3C]", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Clrex,       0xf3bf8f2f,
                 kFmtUnused, -1, -1, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND,
                 "clrex", "", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Dmb,         0xf3bf8f50,
                 kFmtBitBlt, 3, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP,
                 "dmb", "#!0B", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2LdrPcReln12,       0xf85f0000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 11, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0 | REG_USE_PC | IS_LOAD,
                 "ldr", "!0C, [r15pc, -#!1d]", 4, kFixupNone),
    // NOTE: vpop, vpush hard-encoded for s16+ reg list
    OLD_ENCD_MAP(kThumb2VPopCS,       0xecbd8a00,
                 kFmtBitBlt, 7, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_UNARY_OP | REG_DEF_SP | REG_USE_SP | REG_DEF_FPCS_LIST0
                 | IS_LOAD, "vpop", "<!0P>", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2VPushCS,      0xed2d8a00,
                 kFmtBitBlt, 7, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_UNARY_OP | REG_DEF_SP | REG_USE_SP | REG_USE_FPCS_LIST0
                 | IS_STORE, "vpush", "<!0P>", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Vldms,        0xec900a00,
                 kFmtBitBlt, 19, 16, kFmtSfp, 22, 12, kFmtBitBlt, 7, 0,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_USE0 | REG_DEF_FPCS_LIST2
                 | IS_LOAD, "vldms", "!0C, <!2Q>", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Vstms,        0xec800a00,
                 kFmtBitBlt, 19, 16, kFmtSfp, 22, 12, kFmtBitBlt, 7, 0,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_USE0 | REG_USE_FPCS_LIST2
                 | IS_STORE, "vstms", "!0C, <!2Q>", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2AddPCR,      0x4487,
                 kFmtBitBlt, 6, 3, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_UNARY_OP | REG_USE0 | IS_BRANCH | NEEDS_FIXUP,
                 "add", "rPC, !0C", 2, kFixupLabel),
    OLD_ENCD_MAP(kThumb2Adr,         0xf20f0000,
                 kFmtBitBlt, 11, 8, kFmtImm12, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 /* Note: doesn't affect flags */
                 IS_TERTIARY_OP | REG_DEF0 | NEEDS_FIXUP,
                 "adr", "!0C,#!1d", 4, kFixupAdr),
    OLD_ENCD_MAP(kThumb2MovImm16LST,     0xf2400000,
                 kFmtBitBlt, 11, 8, kFmtImm16, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0 | NEEDS_FIXUP,
                 "mov", "!0C, #!1M", 4, kFixupMovImmLST),
    OLD_ENCD_MAP(kThumb2MovImm16HST,     0xf2c00000,
                 kFmtBitBlt, 11, 8, kFmtImm16, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0 | REG_USE0 | NEEDS_FIXUP,
                 "movt", "!0C, #!1M", 4, kFixupMovImmHST),
    OLD_ENCD_MAP(kThumb2LdmiaWB,         0xe8b00000,
                 kFmtBitBlt, 19, 16, kFmtBitBlt, 15, 0, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0_USE0 | REG_DEF_LIST1 | IS_LOAD,
                 "ldmia", "!0C!!, ???", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2OrrRRRs,  0xea500000,
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtShift, -1, -1, IS_QUAD_OP | REG_DEF0_USE12 | SETS_CCODES,
                 "orrs", "!0C, !1C, !2C!3H", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2RsubRRR,  0xebd00000, /* setflags encoding */
                 kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16, kFmtBitBlt, 3, 0,
                 kFmtShift, -1, -1,
                 IS_QUAD_OP | REG_DEF0_USE12 | SETS_CCODES,
                 "rsbs", "!0C, !1C, !2C!3H", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2Smull,  0xfb800000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16,
                 kFmtBitBlt, 3, 0,
                 IS_QUAD_OP | REG_DEF0 | REG_DEF1 | REG_USE2 | REG_USE3,
                 "smull", "!0C, !1C, !2C, !3C", 4, kFixupNone),
    OLD_ENCD_MAP(kThumb2LdrdPcRel8,  0xe9df0000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 11, 8, kFmtBitBlt, 7, 0,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0 | REG_DEF1 | REG_USE_PC | IS_LOAD | NEEDS_FIXUP,
                 "ldrd", "!0C, !1C, [pc, #!2E]", 4, kFixupLoad),
    OLD_ENCD_MAP(kThumb2LdrdI8, 0xe9d00000,
                 kFmtBitBlt, 15, 12, kFmtBitBlt, 11, 8, kFmtBitBlt, 19, 16,
                 kFmtBitBlt, 7, 0,
                 IS_QUAD_OP | REG_DEF0 | REG_DEF1 | REG_USE2 | IS_LOAD,
                 "ldrd", "!0C, !1C, [!2C, #!3E]", 4, kFixupNone),

    // A64 instruction set begins here
#if WITH_A64_HOST_SIMULATOR == 1
    OLD_ENCD_MAP(kA64x86Trampoline, 0x00000000,
                 kFmtUnused, -1, -1, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND,
                 "X86-TRAMPOLINE", "", 12, kFixupNone),
    OLD_ENCD_MAP(kA64x86BlR, 0x00000000,
                 kFmtUnused, -1, -1, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_UNARY_OP | REG_USE0 | IS_BRANCH | REG_DEF_LR,
                 "blr/x86", "!0x", 8, kFixupNone),
#endif
    ENCODING_MAP(WIDE(kA64Adc3rrr), SF_VARIANTS(0x1a000000),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtRegR, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "adc", "!0r, !1r, !2r", kFixupNone),
    ENCODING_MAP(WIDE(kA64Add4RRdT), SF_VARIANTS(0x11000000),
                 kFmtRegROrSp, 4, 0, kFmtRegROrSp, 9, 5, kFmtBitBlt, 21, 10,
                 kFmtBitBlt, 23, 22, IS_QUAD_OP | REG_DEF0_USE1,
                 "add", "!0R, !1R, #!2d!3T", kFixupNone),
    ENCODING_MAP(WIDE(kA64Add4rrro), SF_VARIANTS(0x0b000000),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtRegR, 20, 16,
                 kFmtShift, -1, -1, IS_QUAD_OP | REG_DEF0_USE1,
                 "add", "!0r, !1r, !2r!3o", kFixupNone),
    ENCODING_MAP(WIDE(kA64And3Rrl), SF_VARIANTS(0x12000000),
                 kFmtRegROrSp, 4, 0, kFmtRegR, 9, 5, kFmtBitBlt, 22, 10,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "and", "!0R, !1r, #!2l", kFixupNone),
    ENCODING_MAP(WIDE(kA64And4rrro), SF_VARIANTS(0x0a000000),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtRegR, 20, 16,
                 kFmtShift, -1, -1, IS_QUAD_OP | REG_DEF0_USE12,
                 "and", "!0r, !1r, !2r!3o", kFixupNone),
    ENCODING_MAP(WIDE(kA64Asr3rrd), CUSTOM_VARIANTS(0x13007c00, 0x9340fc00),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtBitBlt, 21, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "asr", "!0r, !1r, #!2d", kFixupNone),
    ENCODING_MAP(WIDE(kA64Asr3rrr), SF_VARIANTS(0x1ac02800),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtRegR, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "asr", "!0r, !1r, !2r", kFixupNone),
    OLD_ENCD_MAP(kA64BCond, 0x54000000,
                 kFmtBitBlt, 3, 0, kFmtBitBlt, 23, 5, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | IS_BRANCH | USES_CCODES |
                 NEEDS_FIXUP, "b.!0c", "!1t", 4, kFixupCondBranch),
    ENCODING_MAP(kA64Blr1r, NO_VARIANTS(0xd63f0000),
                 kFmtRegX, 9, 5, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_UNARY_OP | REG_USE0 | IS_BRANCH | REG_DEF_LR,
                 "blr", "!0x", kFixupNone),
    OLD_ENCD_MAP(kA64BR, 0xd61f0000,
                 kFmtBitBlt, 9, 5, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_UNARY_OP | REG_USE0 | IS_BRANCH,
                 "br", "!0x", 4, kFixupNone),
    OLD_ENCD_MAP(kA64BrkI16, 0xd4200000,
                 kFmtBitBlt, 20, 5, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_UNARY_OP | IS_BRANCH,
                 "brk", "!0d", 4, kFixupNone),
    OLD_ENCD_MAP(kA64BUncond, 0x14000000,
                 kFmtBitBlt, 25, 0, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND | IS_BRANCH | NEEDS_FIXUP,
                 "b", "!0t", 4, kFixupT1Branch),
    OLD_ENCD_MAP(kA64CbnzW, 0x35000000,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 23, 5, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_USE0 | IS_BRANCH | NEEDS_FIXUP,
                 "cbnz", "!0w, !1t", 4, kFixupCBxZ),
    OLD_ENCD_MAP(kA64CbzW, 0x34000000,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 23, 5, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_USE0 | IS_BRANCH  | NEEDS_FIXUP,
                 "cbz", "!0w, !1t", 4, kFixupCBxZ),
    ENCODING_MAP(WIDE(kA64Cmn3Rro), SF_VARIANTS(0x6b20001f),
                 kFmtRegROrSp, 9, 5, kFmtRegR, 20, 16, kFmtExtShift, -1, -1,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | SETS_CCODES,
                 "cmn", "!0R, !1r!2o", kFixupNone),
    ENCODING_MAP(WIDE(kA64Cmn3RdT), SF_VARIANTS(0x3100001f),
                 kFmtRegROrSp, 9, 5, kFmtBitBlt, 21, 10, kFmtBitBlt, 23, 22,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE0 | SETS_CCODES,
                 "cmn", "!0R, #!1d!2T", kFixupNone),
    ENCODING_MAP(WIDE(kA64Cmp3Rro), SF_VARIANTS(0x6b20001f),
                 kFmtRegROrSp, 9, 5, kFmtRegR, 20, 16, kFmtShift, -1, -1,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | SETS_CCODES,
                 "cmp", "!0R, !1r!2o", kFixupNone),
    ENCODING_MAP(WIDE(kA64Cmp3RdT), SF_VARIANTS(0x7100001f),
                 kFmtRegROrSp, 9, 5, kFmtBitBlt, 21, 10, kFmtBitBlt, 23, 22,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE0 | SETS_CCODES,
                 "cmp", "!0R, #!1d!2T", kFixupNone),
    ENCODING_MAP(WIDE(kA64Eor3Rrl), SF_VARIANTS(0x52000000),
                 kFmtRegROrSp, 4, 0, kFmtRegR, 9, 5, kFmtBitBlt, 22, 10,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "eor", "!0R, !1r, #!2l", kFixupNone),
    ENCODING_MAP(WIDE(kA64Eor4rrro), SF_VARIANTS(0x4a000000),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtRegR, 20, 16,
                 kFmtShift, -1, -1, IS_QUAD_OP | REG_DEF0_USE12,
                 "eor", "!0r, !1r, !2r!3o", kFixupNone),
    ENCODING_MAP(WIDE(kA64Extr4rrrd), SF_N_VARIANTS(0x13800000),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtRegR, 20, 16,
                 kFmtBitBlt, 15, 10, IS_QUAD_OP | REG_DEF0_USE12,
                 "extr", "!0r, !1r, !2r, #!3d", kFixupNone),
    ENCODING_MAP(FWIDE(kA64Fabs2ff), FLOAT_VARIANTS(0x1e20c000),
                 kFmtRegF, 4, 0, kFmtRegF, 9, 5, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP| REG_DEF0_USE1,
                 "fabs", "!0f, !1f", kFixupNone),
    ENCODING_MAP(FWIDE(kA64Fadd3fff), FLOAT_VARIANTS(0x1e202800),
                 kFmtRegF, 4, 0, kFmtRegF, 9, 5, kFmtRegF, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "fadd", "!0f, !1f, !2f", kFixupNone),
    ENCODING_MAP(FWIDE(kA64Fdiv3fff), FLOAT_VARIANTS(0x1e201800),
                 kFmtRegF, 4, 0, kFmtRegF, 9, 5, kFmtRegF, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "fdiv", "!0f, !1f, !2f", kFixupNone),
    ENCODING_MAP(FWIDE(kA64Fmov2ff), FLOAT_VARIANTS(0x1e204000),
                 kFmtRegF, 4, 0, kFmtRegF, 9, 5, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "fmov", "!0f, !1f", kFixupNone),
    ENCODING_MAP(FWIDE(kA64Fmov2fI), FLOAT_VARIANTS(0x1e201000),
                 kFmtRegF, 4, 0, kFmtBitBlt, 20, 13, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0,
                 "fmov", "!0f, #!1I", kFixupNone),
    ENCODING_MAP(kA64Fmov2Sx, NO_VARIANTS(0x9e670000),
                 kFmtRegD, 4, 0, kFmtRegX, 9, 5, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "fmov", "!0S, !1x", kFixupNone),
    ENCODING_MAP(kA64Fmov2sw, NO_VARIANTS(0x1e270000),
                 kFmtRegS, 4, 0, kFmtRegW, 9, 5, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "fmov", "!0s, !1w", kFixupNone),
    ENCODING_MAP(FWIDE(kA64Fmul3fff), FLOAT_VARIANTS(0x1e200800),
                 kFmtRegF, 4, 0, kFmtRegF, 9, 5, kFmtRegF, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "fmul", "!0f, !1f, !2f", kFixupNone),
    ENCODING_MAP(FWIDE(kA64Fneg2ff), FLOAT_VARIANTS(0x1e214000),
                 kFmtRegF, 4, 0, kFmtRegF, 9, 5, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "fneg", "!0f, !1f", kFixupNone),
    ENCODING_MAP(FWIDE(kA64Fsub3fff), FLOAT_VARIANTS(0x1e203800),
                 kFmtRegF, 4, 0, kFmtRegF, 9, 5, kFmtRegF, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "fsub", "!0f, !1f, !2f", kFixupNone),
    ENCODING_MAP(FWIDE(kA64Ldr2fp), CUSTOM_VARIANTS(0x1c000000, 0x5c000000),
                 kFmtRegF, 4, 0, kFmtBitBlt, 23, 5, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1,
                 IS_BINARY_OP | REG_DEF0 | REG_USE_PC | IS_LOAD | NEEDS_FIXUP,
                 "ldr", "!0f, !1p", kFixupLoad),
    ENCODING_MAP(FWIDE(kA64Ldr3fXD), CUSTOM_VARIANTS(0xbd400000, 0xfd400000),
                 kFmtRegF, 4, 0, kFmtRegXOrSp, 9, 5, kFmtBitBlt, 21, 10,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldr", "!0f, [!1X, #!2D]", kFixupNone),
    ENCODING_MAP(FWIDE(kA64Ldr4fXxF), CUSTOM_VARIANTS(0xfc606800, 0xbc606800),
                 kFmtRegF, 4, 0, kFmtRegXOrSp, 9, 5, kFmtRegX, 20, 16,
                 kFmtBitBlt, 12, 12, IS_QUAD_OP | REG_DEF0_USE12 | IS_LOAD,
                 "ldr", "!0f, [!1X, !2x, lsl #!3F]", kFixupNone),
    // TODO(Arm64): Change!3F above!
    OLD_ENCD_MAP(kA64LdrWXI12, 0xb9400000,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 9, 5, kFmtBitBlt, 21, 10,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldr", "!0w, [!1X, #!2E]", 4, kFixupNone),
    OLD_ENCD_MAP(kA64LdrXXI12, 0xf9400000,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 9, 5, kFmtBitBlt, 21, 10,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldr", "!0x, [!1X, #!2D]", 4, kFixupNone),
    OLD_ENCD_MAP(kA64LdrPostWXI9,  0xb8400400,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 9, 5, kFmtBitBlt, 20, 12,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF01 | REG_USE1 | IS_LOAD,
                 "ldr", "!0w, [!1X], #!2d", 4, kFixupNone),
    OLD_ENCD_MAP(kA64LdrPostXXI9, 0xf8400400,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 9, 5, kFmtBitBlt, 20, 12,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF01 | REG_USE1 | IS_LOAD,
                 "ldr", "!0x, [!1X], #!2d", 4, kFixupNone),
    OLD_ENCD_MAP(kA64LdpWWXI7, 0x29400000,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 14, 10, kFmtBitBlt, 9, 5,
                 kFmtBitBlt, 21, 15,
                 IS_QUAD_OP | REG_USE2 | REG_DEF012 | IS_LOAD,
                 "ldp", "!0w, !1w, [!2X, #!3E]", 4, kFixupNone),
    OLD_ENCD_MAP(kA64LdpPostWWXI7, 0x28c00000,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 14, 10, kFmtBitBlt, 9, 5,
                 kFmtBitBlt, 21, 15,
                 IS_QUAD_OP | REG_USE2 | REG_DEF012 | IS_LOAD,
                 "ldp", "!0w, !1w, [!2X], #!3E", 4, kFixupNone),
    OLD_ENCD_MAP(kA64LdpPostXXXI7, 0xa8c00000,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 14, 10, kFmtBitBlt, 9, 5,
                 kFmtBitBlt, 21, 15,
                 IS_QUAD_OP | REG_USE2 | REG_DEF012 | IS_LOAD,
                 "ldp", "!0x, !1x, [!2X], #!3D", 4, kFixupNone),
    ENCODING_MAP(FWIDE(kA64Ldur3fXd), CUSTOM_VARIANTS(0xbc400000, 0xfc400000),
                 kFmtRegF, 4, 0, kFmtRegXOrSp, 9, 5, kFmtBitBlt, 20, 12,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | IS_LOAD,
                 "ldur", "!0f, [!1X, #!2d]", kFixupNone),
    ENCODING_MAP(WIDE(kA64Lsl3rrr), SF_VARIANTS(0x1ac02000),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtRegR, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "lsl", "!0r, !1r, !2r", kFixupNone),
    ENCODING_MAP(WIDE(kA64Lsr3rrd), CUSTOM_VARIANTS(0x53007c00, 0xd340fc00),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtBitBlt, 21, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "lsr", "!0r, !1r, #!2d", kFixupNone),
    ENCODING_MAP(WIDE(kA64Lsr3rrr), SF_VARIANTS(0x1ac02400),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtRegR, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "lsr", "!0r, !1r, !2r", kFixupNone),
    ENCODING_MAP(WIDE(kA64Movk3rdM), SF_VARIANTS(0x72800000),
                 kFmtRegR, 4, 0, kFmtBitBlt, 20, 5, kFmtBitBlt, 22, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE0,
                 "movk", "!0r, #!1d!2M", kFixupNone),
    ENCODING_MAP(WIDE(kA64Movn3rdM), SF_VARIANTS(0x12800000),
                 kFmtRegR, 4, 0, kFmtBitBlt, 20, 5, kFmtBitBlt, 22, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0,
                 "movn", "!0r, #!1d!2M", kFixupNone),
    ENCODING_MAP(WIDE(kA64Movz3rdM), SF_VARIANTS(0x52800000),
                 kFmtRegR, 4, 0, kFmtBitBlt, 20, 5, kFmtBitBlt, 22, 21,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0,
                 "movz", "!0r, #!1d!2M", kFixupNone),
    ENCODING_MAP(WIDE(kA64Mov2rr), SF_VARIANTS(0x2a0003e0),
                 kFmtRegR, 4, 0, kFmtRegR, 20, 16, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mov", "!0r, !1r", kFixupNone),
    ENCODING_MAP(WIDE(kA64Mvn2rr), SF_VARIANTS(0x2a2003e0),
                 kFmtRegR, 4, 0, kFmtRegR, 20, 16, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "mvn", "!0r, !1r", kFixupNone),
    ENCODING_MAP(WIDE(kA64Mul3rrr), SF_VARIANTS(0x1b007c00),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtRegR, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "mul", "!0r, !1r, !2r", kFixupNone),
    ENCODING_MAP(WIDE(kA64Neg3rro), SF_VARIANTS(0x4b0003e0),
                 kFmtRegR, 4, 0, kFmtRegR, 20, 16, kFmtExtShift, -1, -1,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "neg", "!0r, !1r!2o", kFixupNone),
    ENCODING_MAP(WIDE(kA64Orr3Rrl), SF_VARIANTS(0x32000000),
                 kFmtRegROrSp, 4, 0, kFmtRegR, 9, 5, kFmtBitBlt, 22, 10,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1,
                 "orr", "!0R, !1r, #!2l", kFixupNone),
    ENCODING_MAP(WIDE(kA64Orr4rrro), SF_VARIANTS(0x2a000000),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtRegR, 20, 16,
                 kFmtShift, -1, -1, IS_QUAD_OP | REG_DEF0_USE12,
                 "orr", "!0r, !1r, !2r!3o", kFixupNone),
    ENCODING_MAP(kA64Ret, NO_VARIANTS(0xd65f03c0),
                 kFmtUnused, -1, -1, kFmtUnused, -1, -1, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, NO_OPERAND | IS_BRANCH,
                 "ret", "", kFixupNone),
    ENCODING_MAP(WIDE(kA64Rev2rr), CUSTOM_VARIANTS(0x5ac00800, 0xdac00c00),
                 kFmtRegR, 11, 8, kFmtRegR, 19, 16, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "rev", "!0r, !1r", kFixupNone),
    ENCODING_MAP(WIDE(kA64Rev162rr), SF_VARIANTS(0xfa90f0b0),
                 kFmtRegR, 11, 8, kFmtRegR, 19, 16, kFmtUnused, -1, -1,
                 kFmtUnused, -1, -1, IS_BINARY_OP | REG_DEF0_USE1,
                 "rev16", "!0r, !1r", kFixupNone),
    ENCODING_MAP(WIDE(kA64Ror3rrr), SF_VARIANTS(0x1ac02c00),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtRegR, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "ror", "!0r, !1r, !2r", kFixupNone),
    ENCODING_MAP(WIDE(kA64Sbc3rrr), SF_VARIANTS(0x5a000000),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtRegR, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "sbc", "!0r, !1r, !2r", kFixupNone),
    ENCODING_MAP(WIDE(kA64Sbfm4rrdd), SF_N_VARIANTS(0x13000000),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtBitBlt, 21, 16,
                 kFmtBitBlt, 15, 10, IS_QUAD_OP | REG_DEF0_USE1,
                 "sbfm", "!0r, !1r, #!2d, #!3d", kFixupNone),
    ENCODING_MAP(WIDE(kA64Sdiv3rrr), SF_VARIANTS(0x1ac00c00),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtRegR, 20, 16,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE12,
                 "sdiv", "!0r, !1r, !2r", kFixupNone),
    OLD_ENCD_MAP(kA64StpWWXI7, 0x29000000,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 14, 10, kFmtBitBlt, 9, 5,
                 kFmtBitBlt, 21, 15,
                 IS_QUAD_OP | REG_DEF2 | REG_USE012 | IS_STORE,
                 "stp", "!0w, !1w, [!2X, #!3E]", 4, kFixupNone),
    OLD_ENCD_MAP(kA64StpPostWWXI7, 0x28800000,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 14, 10, kFmtBitBlt, 9, 5,
                 kFmtBitBlt, 21, 15,
                 IS_QUAD_OP | REG_DEF2 | REG_USE012 | IS_STORE,
                 "stp", "!0w, !1w, [!2X], #!3E", 4, kFixupNone),
    OLD_ENCD_MAP(kA64StpPostXXXI7, 0xa8800000,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 14, 10, kFmtBitBlt, 9, 5,
                 kFmtBitBlt, 21, 15,
                 IS_QUAD_OP | REG_DEF2 | REG_USE012 | IS_STORE,
                 "stp", "!0x, !1x, [!2X], #!3D", 4, kFixupNone),
    OLD_ENCD_MAP(kA64StpPreWWXI7,  0x29800000,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 14, 10, kFmtBitBlt, 9, 5,
                 kFmtBitBlt, 21, 15,
                 IS_QUAD_OP | REG_DEF2 | REG_USE012 | IS_STORE,
                 "stp", "!0w, !1w, [!2X, #!3E]!!", 4, kFixupNone),
    OLD_ENCD_MAP(kA64StpPreXXXI7,  0xa9800000,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 14, 10, kFmtBitBlt, 9, 5,
                 kFmtBitBlt, 21, 15,
                 IS_QUAD_OP | REG_DEF2 | REG_USE012 | IS_STORE,
                 "stp", "!0x, !1x, [!2X, #!3D]!!", 4, kFixupNone),
    ENCODING_MAP(FWIDE(kA64Str3fXD), CUSTOM_VARIANTS(0xbd000000, 0xfd000000),
                 kFmtRegF, 4, 0, kFmtRegXOrSp, 9, 5, kFmtBitBlt, 21, 10,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "str", "!0f, [!1X, #!2D]", kFixupNone),
    // str [ss11110100] imm_12[21-10] rn[9-5] rt[4-0].
    ENCODING_MAP(FWIDE(kA64Str4fXxF), CUSTOM_VARIANTS(0xbc206800, 0xfc206800),
                 kFmtRegF, 4, 0, kFmtRegXOrSp, 9, 5, kFmtRegX, 20, 16,
                 kFmtBitBlt, 12, 12, IS_QUAD_OP | REG_USE012 | IS_STORE,
                 "str", "!0f, [!1X, !2x, lsl #!3F]", kFixupNone),
    // TODO(Arm64): Change!3F above!
    OLD_ENCD_MAP(kA64StrWXI12, 0xb9000000,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 9, 5, kFmtBitBlt, 21, 10,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "str", "!0w, [!1X, #!2E]", 4, kFixupNone),
    OLD_ENCD_MAP(kA64StrXXI12, 0xf9000000,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 9, 5, kFmtBitBlt, 21, 10,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "str", "!0x, [!1X, #!2D]", 4, kFixupNone),
    OLD_ENCD_MAP(kA64StrWXX, 0xb8206800,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 9, 5, kFmtBitBlt, 20, 16,
                 kFmtBitBlt, 12, 12,
                 IS_QUAD_OP | REG_USE012 | IS_STORE,
                 "str", "!0w, [!1X, !2x, lsl #!3F]", 4, kFixupNone),
    OLD_ENCD_MAP(kA64StrPostWXI9, 0xb8000400,
                 kFmtBitBlt, 4, 0, kFmtBitBlt, 9, 5, kFmtBitBlt, 20, 12,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_USE01 | REG_DEF1 | IS_STORE,
                 "str", "!0w, [!1X], #!2d", 4, kFixupNone),
    OLD_ENCD_MAP(kA64StxrWXX, 0xc8007c00,
                 kFmtBitBlt, 20, 16, kFmtBitBlt, 4, 0, kFmtBitBlt, 9, 5,
                 kFmtUnused, -1, -1,
                 IS_TERTIARY_OP | REG_DEF0_USE12 | IS_STORE,
                 "stxr", "!0w, !1x, [!2X]", 4, kFixupNone),
    ENCODING_MAP(FWIDE(kA64Stur3fXd), CUSTOM_VARIANTS(0xbc000000, 0xfc000000),
                 kFmtRegF, 4, 0, kFmtRegXOrSp, 9, 5, kFmtBitBlt, 20, 12,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_USE01 | IS_STORE,
                 "stur", "!0f, [!1X, #!2d]", kFixupNone),
    ENCODING_MAP(WIDE(kA64Sub4RRdT), SF_VARIANTS(0x51000000),
                 kFmtRegROrSp, 4, 0, kFmtRegROrSp, 9, 5, kFmtBitBlt, 21, 10,
                 kFmtBitBlt, 23, 22, IS_QUAD_OP | REG_DEF0_USE1,
                 "sub", "!0R, !1R, #!2d!3T", kFixupNone),
    ENCODING_MAP(WIDE(kA64Sub4rrro), SF_VARIANTS(0x4b000000),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtRegR, 20, 16,
                 kFmtExtShift, -1, -1, IS_QUAD_OP | REG_DEF0_USE12,
                 "sub", "!0r, !1r, !2r!3o", kFixupNone),
    ENCODING_MAP(kA64Subs3rRd, SF_VARIANTS(0x71000000),
                 kFmtRegR, 4, 0, kFmtRegROrSp, 9, 5, kFmtBitBlt, 21, 10,
                 kFmtUnused, -1, -1, IS_TERTIARY_OP | REG_DEF0_USE1 | SETS_CCODES,
                 "subs", "!0r, !1R, #!2d", kFixupNone),
    ENCODING_MAP(WIDE(kA64Tst3rro), SF_VARIANTS(0x6a000000),
                 kFmtRegR, 9, 5, kFmtRegR, 20, 16, kFmtExtShift, -1, -1,
                 kFmtUnused, -1, -1, IS_QUAD_OP | REG_USE01 | SETS_CCODES,
                 "tst", "!0r, !1r!2o", kFixupNone),
    ENCODING_MAP(WIDE(kA64Ubfm4rrdd), SF_N_VARIANTS(0x53000000),
                 kFmtRegR, 4, 0, kFmtRegR, 9, 5, kFmtBitBlt, 21, 16,
                 kFmtBitBlt, 15, 10, IS_QUAD_OP | REG_DEF0_USE1,
                 "ubfm", "!0r, !1r, !2d, !3d", kFixupNone),
};

// new_lir replaces orig_lir in the pcrel_fixup list.
void Arm64Mir2Lir::ReplaceFixup(LIR* prev_lir, LIR* orig_lir, LIR* new_lir) {
  new_lir->u.a.pcrel_next = orig_lir->u.a.pcrel_next;
  if (UNLIKELY(prev_lir == NULL)) {
    first_fixup_ = new_lir;
  } else {
    prev_lir->u.a.pcrel_next = new_lir;
  }
  orig_lir->flags.fixup = kFixupNone;
}

// new_lir is inserted before orig_lir in the pcrel_fixup list.
void Arm64Mir2Lir::InsertFixupBefore(LIR* prev_lir, LIR* orig_lir, LIR* new_lir) {
  new_lir->u.a.pcrel_next = orig_lir;
  if (UNLIKELY(prev_lir == NULL)) {
    first_fixup_ = new_lir;
  } else {
    DCHECK(prev_lir->u.a.pcrel_next == orig_lir);
    prev_lir->u.a.pcrel_next = new_lir;
  }
}

/* Nop, used for aligning code. Nop is an alias for hint #0. */
#define PADDING_NOP (UINT32_C(0xd503201f))

void Arm64Mir2Lir::EncodeLIR(LIR* lir) {
  bool opcode_is_wide = IS_WIDE(lir->opcode);
  ArmOpcode opcode = UNWIDE(lir->opcode);

  if (IsPseudoLirOp(opcode)) {
    return;
  }

#if WITH_A64_HOST_SIMULATOR == 1
  if (UNLIKELY(opcode == kA64x86Trampoline)) {
    uintptr_t addr = QUICK_ENTRYPOINT_OFFSET(pForeignCodeCall).Uint32Value();
    lir->u.a.bytes[0] = 0x90;       // nop
    lir->u.a.bytes[1] = 0x64;       // call *%fs:addr
    lir->u.a.bytes[2] = 0xff;
    lir->u.a.bytes[3] = 0x15;
    lir->u.a.bytes[4] = addr;
    lir->u.a.bytes[5] = addr >> 8;
    lir->u.a.bytes[6] = addr >> 16;
    lir->u.a.bytes[7] = addr >> 24;
    lir->u.a.bytes[8] = frame_size_;
    lir->u.a.bytes[9] = frame_size_ >> 8;
    lir->u.a.bytes[10] = frame_size_ >> 16;
    lir->u.a.bytes[11] = frame_size_ >> 24;
    lir->flags.size = 12;
    return;
  }

  if (UNLIKELY(opcode == kA64x86BlR)) {
    uint32_t operand0 = lir->operands[0];
    lir->u.a.bytes[0] = operand0 << 5;  // brk #(0x8000 + operand[0])
    lir->u.a.bytes[1] = operand0 >> 3;
    lir->u.a.bytes[2] = 0x30;
    lir->u.a.bytes[3] = 0xd4;
    lir->u.a.bytes[4] = 0xe0;           // blr xzr
    lir->u.a.bytes[5] = 0x03;
    lir->u.a.bytes[6] = 0x3f;
    lir->u.a.bytes[7] = 0xd6;
    lir->flags.size = 8;
    return;
  }
#endif

  if (LIKELY(!lir->flags.is_nop)) {
    const ArmEncodingMap *encoder = &EncodingMap[opcode];

    // Select the right variant of the skeleton.
    uint32_t bits = opcode_is_wide ? encoder->xskeleton : encoder->wskeleton;
    DCHECK(!opcode_is_wide || IS_WIDE(encoder->opcode));

    for (int i = 0; i < 4; i++) {
      ArmEncodingKind kind = encoder->field_loc[i].kind;
      uint32_t operand = lir->operands[i];
      uint32_t value;

      if (LIKELY(static_cast<unsigned>(kind) <= kFmtBitBlt)) {
        // Note: this will handle kFmtReg* and kFmtBitBlt.
        // It is not necessary to mask the value for the register cases.

        // TODO(Arm64): make this a ifndef NDEBUG.
        // Register usage checks.
#if 1
// #ifndef NDEBUG
        if (static_cast<unsigned>(kind) < kFmtBitBlt) {
          // Register encodings.
          int reg_name = static_cast<int>(operand);

          switch (kind) {
            case kFmtRegW: case kFmtRegX: case kFmtRegR:
              if (reg_name == rARM_SP) {
                LOG(FATAL) << "Unexpected usage of register sp for " << encoder->name;
              }
              DCHECK_LE(reg_name, 31);
              DCHECK_GE(reg_name, -1);
              break;
            case kFmtRegWOrSp: case kFmtRegXOrSp: case kFmtRegROrSp:
              if (reg_name == rARM_ZR) {
                LOG(FATAL) << "Unexpected usage of register zr for " << encoder->name;
              }
              DCHECK_LE(reg_name, 31);
              DCHECK_GE(reg_name, -1);
              break;
            case kFmtRegS:
              DCHECK_GE(reg_name, 0);
              DCHECK(ARM_DOUBLEREG(reg_name));
              break;
            case kFmtRegD:
              DCHECK_GE(reg_name, 0);
              DCHECK(ARM_SINGLEREG(reg_name));
              break;
            case kFmtRegF:
              DCHECK_GE(reg_name, 0);
              break;
            default:
              LOG(FATAL) << "Bad fmt for arg. " << i << " in " << encoder->name
                         << " (" << kind << ")";
              break;
          }
        }
#endif
        value = (operand << encoder->field_loc[i].start) &
            ((1 << (encoder->field_loc[i].end + 1)) - 1);
        bits |= value;
      } else {
        switch (kind) {
          case kFmtSkip:
            break;  // Nothing to do, but continue to next.
          case kFmtUnused:
            i = 4;  // Done, break out of the enclosing loop.
            break;
          case kFmtFPImm:
            value = ((operand & 0xF0) >> 4) << encoder->field_loc[i].end;
            value |= (operand & 0x0F) << encoder->field_loc[i].start;
            bits |= value;
            break;
          case kFmtBrOffset:
            value = ((operand  & 0x80000) >> 19) << 26;
            value |= ((operand & 0x40000) >> 18) << 11;
            value |= ((operand & 0x20000) >> 17) << 13;
            value |= ((operand & 0x1f800) >> 11) << 16;
            value |= (operand  & 0x007ff);
            bits |= value;
            break;
          case kFmtShift5:
            value = ((operand & 0x1c) >> 2) << 12;
            value |= (operand & 0x03) << 6;
            bits |= value;
            break;
          case kFmtShift:
            DCHECK_EQ(operand & (1 << 6), 0U);
            // Intentional fallthrough.
          case kFmtExtShift:
            value = (operand & 0x3f) << 10;
            value |= ((operand & 0x1c0) >> 6) << 21;
            bits |= value;
            break;
          case kFmtBWidth:
            value = operand - 1;
            bits |= value;
            break;
          case kFmtLsb:
            value = ((operand & 0x1c) >> 2) << 12;
            value |= (operand & 0x03) << 6;
            bits |= value;
            break;
          case kFmtImm6:
            value = ((operand & 0x20) >> 5) << 9;
            value |= (operand & 0x1f) << 3;
            bits |= value;
            break;
          case kFmtImm12:
          case kFmtModImm:
            value = ((operand & 0x800) >> 11) << 26;
            value |= ((operand & 0x700) >> 8) << 12;
            value |= operand & 0x0ff;
            bits |= value;
            break;
          case kFmtImm16:
            value = ((operand & 0x0800) >> 11) << 26;
            value |= ((operand & 0xf000) >> 12) << 16;
            value |= ((operand & 0x0700) >> 8) << 12;
            value |= operand & 0x0ff;
            bits |= value;
            break;
          default:
            LOG(FATAL) << "Bad fmt for arg. " << i << " in " << encoder->name
                       << " (" << kind << ")";
        }
      }
    }

#if WITH_A64_HOST_SIMULATOR == 1
    // TODO: temporary code to catch unported instructions (to be removed).
    if (opcode < kA64x86Trampoline) {
      LOG(WARNING) << "Instruction " << encoder->name  << " not ported to A64?";
    }
#endif

    if (LIKELY(encoder->size == 4)) {
      lir->u.a.bytes[0] = (bits & 0xff);
      lir->u.a.bytes[1] = ((bits >> 8) & 0xff);
      lir->u.a.bytes[2] = ((bits >> 16) & 0xff);
      lir->u.a.bytes[3] = ((bits >> 24) & 0xff);
      lir->flags.size = encoder->size;
    }
  }
}

// Align data offset on 8 byte boundary: it will only contain double-word items, as word immediates
// are better set directly from the code (they will require no more than 2 instructions).
#define ALIGNED_DATA_OFFSET(offset) (((offset) + 0x7) & ~0x7)

// Assemble the LIR into binary instruction format.
void Arm64Mir2Lir::AssembleLIR() {
  LIR* lir;
  LIR* prev_lir;
  cu_->NewTimingSplit("Assemble");
  int assembler_retries = 0;
  CodeOffset starting_offset = EncodeRange(first_lir_insn_, last_lir_insn_, 0);
  data_offset_ = ALIGNED_DATA_OFFSET(starting_offset);
  int32_t offset_adjustment;
  AssignDataOffsets();

  /*
   * Note: generation must be 1 on first pass (to distinguish from initialized state of 0 for non-visited nodes).
   * Start at zero here, and bit will be flipped to 1 on entry to the loop.
   */
  int generation = 0;
  while (true) {
    offset_adjustment = 0;
    AssemblerStatus res = kSuccess;  // Assume success
    generation ^= 1;
    // Note: nodes requring possible fixup linked in ascending order.
    lir = first_fixup_;
    prev_lir = NULL;
    while (lir != NULL) {
      /*
       * NOTE: the lir being considered here will be encoded following the switch (so long as
       * we're not in a retry situation).  However, any new non-pc_rel instructions inserted
       * due to retry must be explicitly encoded at the time of insertion.  Note that
       * inserted instructions don't need use/def flags, but do need size and pc-rel status
       * properly updated.
       */
      lir->offset += offset_adjustment;
      // During pass, allows us to tell whether a node has been updated with offset_adjustment yet.
      lir->flags.generation = generation;
      switch (static_cast<FixupKind>(lir->flags.fixup)) {
        case kFixupLabel:
        case kFixupNone:
        case kFixupVLoad:
          break;
        case kFixupT1Branch: {
          LIR *target_lir = lir->target;
          DCHECK(target_lir);
          intptr_t pc = lir->offset;
          intptr_t target = target_lir->offset +
              ((target_lir->flags.generation == lir->flags.generation) ? 0 : offset_adjustment);
          int delta = target - pc;
          DCHECK((delta & 0x3) == 0 && IS_SIGNED_IMM19(delta >> 2));
          lir->operands[0] = delta >> 2;
          break;
        }
        case kFixupLoad:
        case kFixupCBxZ:
        case kFixupCondBranch: {
          LIR *target_lir = lir->target;
          DCHECK(target_lir);
          intptr_t pc = lir->offset;
          intptr_t target = target_lir->offset +
              ((target_lir->flags.generation == lir->flags.generation) ? 0 : offset_adjustment);
          int delta = target - pc;
          DCHECK((delta & 0x3) == 0 && IS_SIGNED_IMM19(delta >> 2));
          lir->operands[1] = delta >> 2;
          break;
        }
        case kFixupPushPop:
        case kFixupT2Branch:
        case kFixupBlx1:
        case kFixupBl1:
          LOG(FATAL) << "Unexpected case " << lir->flags.fixup;
        case kFixupAdr: {
          EmbeddedData *tab_rec = reinterpret_cast<EmbeddedData*>(UnwrapPointer(lir->operands[2]));
          LIR* target = lir->target;
          int32_t target_disp = (tab_rec != NULL) ?  tab_rec->offset + offset_adjustment
              : target->offset + ((target->flags.generation == lir->flags.generation) ? 0 : offset_adjustment);
          int32_t disp = target_disp - ((lir->offset + 4) & ~3);
          if (disp < 4096) {
            lir->operands[1] = disp;
          } else {
            // convert to ldimm16l, ldimm16h, add tgt, pc, operands[0]
            // TUNING: if this case fires often, it can be improved.  Not expected to be common.
            LIR *new_mov16L =
                RawLIR(lir->dalvik_offset, kThumb2MovImm16LST, lir->operands[0], 0,
                       WrapPointer(lir), WrapPointer(tab_rec), 0, lir->target);
            new_mov16L->flags.size = EncodingMap[new_mov16L->opcode].size;
            new_mov16L->flags.fixup = kFixupMovImmLST;
            new_mov16L->offset = lir->offset;
            // Link the new instruction, retaining lir.
            InsertLIRBefore(lir, new_mov16L);
            lir->offset += new_mov16L->flags.size;
            offset_adjustment += new_mov16L->flags.size;
            InsertFixupBefore(prev_lir, lir, new_mov16L);
            prev_lir = new_mov16L;   // Now we've got a new prev.
            LIR *new_mov16H =
                RawLIR(lir->dalvik_offset, kThumb2MovImm16HST, lir->operands[0], 0,
                       WrapPointer(lir), WrapPointer(tab_rec), 0, lir->target);
            new_mov16H->flags.size = EncodingMap[new_mov16H->opcode].size;
            new_mov16H->flags.fixup = kFixupMovImmHST;
            new_mov16H->offset = lir->offset;
            // Link the new instruction, retaining lir.
            InsertLIRBefore(lir, new_mov16H);
            lir->offset += new_mov16H->flags.size;
            offset_adjustment += new_mov16H->flags.size;
            InsertFixupBefore(prev_lir, lir, new_mov16H);
            prev_lir = new_mov16H;  // Now we've got a new prev.

            offset_adjustment -= lir->flags.size;
            lir->opcode = kThumbAddRRLH;
            // lir->operands[1] = rARM_PC;
            lir->flags.size = EncodingMap[lir->opcode].size;
            offset_adjustment += lir->flags.size;
            // Must stay in fixup list and have offset updated; will be used by LST/HSP pair.
            lir->flags.fixup = kFixupNone;
            res = kRetryAll;
          }
          break;
        }
        case kFixupMovImmLST: {
          // operands[1] should hold disp, [2] has add, [3] has tab_rec
          LIR *addPCInst = reinterpret_cast<LIR*>(UnwrapPointer(lir->operands[2]));
          EmbeddedData *tab_rec = reinterpret_cast<EmbeddedData*>(UnwrapPointer(lir->operands[3]));
          // If tab_rec is null, this is a literal load. Use target
          LIR* target = lir->target;
          int32_t target_disp = tab_rec ? tab_rec->offset : target->offset;
          lir->operands[1] = (target_disp - (addPCInst->offset + 4)) & 0xffff;
          break;
        }
        case kFixupMovImmHST: {
          // operands[1] should hold disp, [2] has add, [3] has tab_rec
          LIR *addPCInst = reinterpret_cast<LIR*>(UnwrapPointer(lir->operands[2]));
          EmbeddedData *tab_rec = reinterpret_cast<EmbeddedData*>(UnwrapPointer(lir->operands[3]));
          // If tab_rec is null, this is a literal load. Use target
          LIR* target = lir->target;
          int32_t target_disp = tab_rec ? tab_rec->offset : target->offset;
          lir->operands[1] =
              ((target_disp - (addPCInst->offset + 4)) >> 16) & 0xffff;
          break;
        }
        default:
          LOG(FATAL) << "Unexpected case " << lir->flags.fixup;
      }
      /*
       * If one of the pc-relative instructions expanded we'll have
       * to make another pass.  Don't bother to fully assemble the
       * instruction.
       */
      if (res == kSuccess) {
        EncodeLIR(lir);
        if (assembler_retries == 0) {
          // Go ahead and fix up the code buffer image.
          for (int i = 0; i < lir->flags.size; i++) {
            code_buffer_[lir->offset + i] = lir->u.a.bytes[i];
          }
        }
      }
      prev_lir = lir;
      lir = lir->u.a.pcrel_next;
    }

    if (res == kSuccess) {
      break;
    } else {
      assembler_retries++;
      if (assembler_retries > MAX_ASSEMBLER_RETRIES) {
        CodegenDump();
        LOG(FATAL) << "Assembler error - too many retries";
      }
      starting_offset += offset_adjustment;
      data_offset_ = ALIGNED_DATA_OFFSET(starting_offset);
      AssignDataOffsets();
    }
  }

  // Rebuild the CodeBuffer if we had to retry; otherwise it should be good as-is.
  if (assembler_retries != 0) {
    code_buffer_.clear();
    for (LIR* lir = first_lir_insn_; lir != NULL; lir = NEXT_LIR(lir)) {
      if (lir->flags.is_nop) {
        continue;
      } else  {
        for (int i = 0; i < lir->flags.size; i++) {
          code_buffer_.push_back(lir->u.a.bytes[i]);
        }
      }
    }
  }

  data_offset_ = ALIGNED_DATA_OFFSET(code_buffer_.size());

  // Install literals
  InstallLiteralPools();

  // Install switch tables
  InstallSwitchTables();

  // Install fill array data
  InstallFillArrayData();

  // Create the mapping table and native offset to reference map.
  cu_->NewTimingSplit("PcMappingTable");
  CreateMappingTables();

  cu_->NewTimingSplit("GcMap");
  CreateNativeGcMap();
}

int Arm64Mir2Lir::GetInsnSize(LIR* lir) {
  DCHECK(!IsPseudoLirOp(lir->opcode));
  return EncodingMap[lir->opcode].size;
}

// Encode instruction bit pattern and assign offsets.
uint32_t Arm64Mir2Lir::EncodeRange(LIR* head_lir, LIR* tail_lir, uint32_t offset) {
  LIR* end_lir = tail_lir->next;

  /*
   * A significant percentage of methods can be assembled in a single pass.  We'll
   * go ahead and build the code image here, leaving holes for pc-relative fixup
   * codes.  If the code size changes during that pass, we'll have to throw away
   * this work - but if not, we're ready to go.
   */
  code_buffer_.reserve(estimated_native_code_size_ + 256);  // Add a little slop.
  LIR* last_fixup = NULL;
  for (LIR* lir = head_lir; lir != end_lir; lir = NEXT_LIR(lir)) {
    lir->offset = offset;
    if (!lir->flags.is_nop) {
      if (lir->flags.fixup != kFixupNone) {
        if (!IsPseudoLirOp(lir->opcode)) {
          lir->flags.size = EncodingMap[lir->opcode].size;
          lir->flags.fixup = EncodingMap[lir->opcode].fixup;
        } else {
          DCHECK_NE(lir->opcode, kPseudoPseudoAlign4);
          lir->flags.size = 0;
          lir->flags.fixup = kFixupLabel;
        }
        // Link into the fixup chain.
        lir->flags.use_def_invalid = true;
        lir->u.a.pcrel_next = NULL;
        if (first_fixup_ == NULL) {
          first_fixup_ = lir;
        } else {
          last_fixup->u.a.pcrel_next = lir;
        }
        last_fixup = lir;
      } else {
        EncodeLIR(lir);
      }
      for (int i = 0; i < lir->flags.size; i++) {
        code_buffer_.push_back(lir->u.a.bytes[i]);
      }
      offset += lir->flags.size;
    }
  }
  return offset;
}

void Arm64Mir2Lir::AssignDataOffsets() {
  /* Set up offsets for literals */
  CodeOffset offset = data_offset_;

  offset = AssignLiteralOffset(offset);

  offset = AssignSwitchTablesOffset(offset);

  total_size_ = AssignFillArrayDataOffset(offset);
}

}  // namespace art
