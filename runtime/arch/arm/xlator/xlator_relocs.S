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

// Modifiers for each relocation
// +0 = 32 bit constant
// +1 = low 16 bits
// +2 = high 16 bits
// +3 = negative value

// Gap between relocation opcodes to allow for modifiers
.set reloc_gap, 8

.set reloc_lo, 1        // take low 16 bits
.set reloc_hi, 2        // take high 16 bits
.set reloc_negative, 3  // make negative
.set reloc_mult4, 4     // multiply by 4
.set reloc_raw, 5       // raw value, don't adjust

.set reloc_const, 0
.set reloc_constB_11n, reloc_const+0*reloc_gap
.set reloc_constB_21s, reloc_const+1*reloc_gap
.set reloc_constB_21h, reloc_const+2*reloc_gap
.set reloc_constB_51l, reloc_const+3*reloc_gap
.set reloc_constB_21t, reloc_const+4*reloc_gap
.set reloc_constC_22t, reloc_const+5*reloc_gap
.set reloc_constC_22b, reloc_const+6*reloc_gap
.set reloc_constC_22s, reloc_const+7*reloc_gap
.set reloc_constB_31i, reloc_const+8*reloc_gap
.set reloc_constB_21c, reloc_const+9*reloc_gap
.set reloc_constB_31c, reloc_const+10*reloc_gap
.set reloc_constB_51l_2, reloc_const+11*reloc_gap         // constB_51l second word

.set reloc_const_special, reloc_const+12*reloc_gap        // special handling

.set reloc_instruction, 100     // the dex instruction

.set reloc_offset, 101          // offset
// lo and hi follow at +1 and +2

.set reloc_helperaddr, 120        // relocation of a call to a helper
// +space for 100 helper follow

.set reloc_dexsize, 200     // size of instruction in dex units

.set reloc_dexpc, 208     // dex pc value (must be on an 8 boundary)
 // lo and hi follow at +1 and +2
 // leave reloc_gap here

.set reloc_helperaddr, 300       // relocation of a call to a helper

// VRegA
.set reloc_vregA, 1000
.set reloc_vregA_10t, reloc_vregA+1*reloc_gap
.set reloc_vregA_10x, reloc_vregA+2*reloc_gap
.set reloc_vregA_11n, reloc_vregA+3*reloc_gap
.set reloc_vregA_11x, reloc_vregA+4*reloc_gap
.set reloc_vregA_12x, reloc_vregA+5*reloc_gap
.set reloc_vregA_20t, reloc_vregA+6*reloc_gap
.set reloc_vregA_21c, reloc_vregA+7*reloc_gap
.set reloc_vregA_21h, reloc_vregA+8*reloc_gap
.set reloc_vregA_21s, reloc_vregA+9*reloc_gap
.set reloc_vregA_21t, reloc_vregA+10*reloc_gap
.set reloc_vregA_22b, reloc_vregA+11*reloc_gap
.set reloc_vregA_22c, reloc_vregA+12*reloc_gap
.set reloc_vregA_22s, reloc_vregA+13*reloc_gap
.set reloc_vregA_22t, reloc_vregA+14*reloc_gap
.set reloc_vregA_22x, reloc_vregA+15*reloc_gap
.set reloc_vregA_23x, reloc_vregA+16*reloc_gap
.set reloc_vregA_30t, reloc_vregA+17*reloc_gap
.set reloc_vregA_31c, reloc_vregA+18*reloc_gap
.set reloc_vregA_31i, reloc_vregA+19*reloc_gap
.set reloc_vregA_31t, reloc_vregA+20*reloc_gap
.set reloc_vregA_32x, reloc_vregA+21*reloc_gap
.set reloc_vregA_35c, reloc_vregA+22*reloc_gap
.set reloc_vregA_3rc, reloc_vregA+23*reloc_gap
.set reloc_vregA_51l, reloc_vregA+24*reloc_gap

// VRegB
.set reloc_vregB, 2000
.set reloc_vregB_11n, reloc_vregB+1*reloc_gap
.set reloc_vregB_12x, reloc_vregB+2*reloc_gap
.set reloc_vregB_21c, reloc_vregB+3*reloc_gap
.set reloc_vregB_21h, reloc_vregB+4*reloc_gap
.set reloc_vregB_21s, reloc_vregB+5*reloc_gap
.set reloc_vregB_21t, reloc_vregB+6*reloc_gap
.set reloc_vregB_22b, reloc_vregB+7*reloc_gap
.set reloc_vregB_22c, reloc_vregB+8*reloc_gap
.set reloc_vregB_22s, reloc_vregB+9*reloc_gap
.set reloc_vregB_22t, reloc_vregB+10*reloc_gap
.set reloc_vregB_22x, reloc_vregB+11*reloc_gap
.set reloc_vregB_23x, reloc_vregB+12*reloc_gap
.set reloc_vregB_31c, reloc_vregB+13*reloc_gap
.set reloc_vregB_31i, reloc_vregB+14*reloc_gap
.set reloc_vregB_31t, reloc_vregB+15*reloc_gap
.set reloc_vregB_32x, reloc_vregB+16*reloc_gap
.set reloc_vregB_35c, reloc_vregB+17*reloc_gap
.set reloc_vregB_3rc, reloc_vregB+18*reloc_gap
.set reloc_vregB_51l, reloc_vregB+19*reloc_gap




// VRegC
.set reloc_vregC, 3000
.set reloc_vregC_22b, reloc_vregC+1*reloc_gap
.set reloc_vregC_22c, reloc_vregC+2*reloc_gap
.set reloc_vregC_22s, reloc_vregC+3*reloc_gap
.set reloc_vregC_22t, reloc_vregC+4*reloc_gap
.set reloc_vregC_23x, reloc_vregC+5*reloc_gap
.set reloc_vregC_35c, reloc_vregC+6*reloc_gap
.set reloc_vregC_3rc, reloc_vregC+7*reloc_gap

