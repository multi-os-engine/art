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

#ifndef ART_RUNTIME_ARCH_ARM_XLATOR_XLATOR_RELOCS_H_
#define ART_RUNTIME_ARCH_ARM_XLATOR_XLATOR_RELOCS_H_

namespace art {
namespace arm {


// See xlator_relocs.S for assembly version of these operations.  These must match the
// symbols in the .S file
enum TranslatorRelocations {
  kRelocGap = 8,

  kRelocAll = 0,
  kRelocLo = 1,
  kRelocHi = 2,
  kRelocNegative = 3,
  kRelocMult4 = 4,
  kRelocRaw = 5,

  kRelocConst = 0,
  kRelocConstB_11n = kRelocConst+0*kRelocGap,
  kRelocConstB_21s = kRelocConst+1*kRelocGap,
  kRelocConstB_21h = kRelocConst+2*kRelocGap,
  kRelocConstB_51l = kRelocConst+3*kRelocGap,
  kRelocConstB_21t = kRelocConst+4*kRelocGap,
  kRelocConstC_22t = kRelocConst+5*kRelocGap,
  kRelocConstC_22b = kRelocConst+6*kRelocGap,
  kRelocConstC_22s = kRelocConst+7*kRelocGap,
  kRelocConstB_31i = kRelocConst+8*kRelocGap,
  kRelocConstB_21c = kRelocConst+9*kRelocGap,
  kRelocConstB_31c = kRelocConst+10*kRelocGap,
  kRelocConstB_51l_2 = kRelocConst+11*kRelocGap,          // B_51l second word

  kRelocConstSpecial = kRelocConst+12*kRelocGap,         // special handling

  kRelocEndConsts,

  kRelocInstruction = 100,     // the dex instruction

  kRelocOffset = 101,          // offset
  // lo and hi follow at +1 and +2


  kRelocDexSize = 200,
  kRelocDexPC = 208,

  kRelocHelperAddr = 300,

  // VRegA
  kRelocVregA = 1000,
  kRelocVregA_10t = kRelocVregA+1*kRelocGap,
  kRelocVregA_10x = kRelocVregA+2*kRelocGap,
  kRelocVregA_11n = kRelocVregA+3*kRelocGap,
  kRelocVregA_11x = kRelocVregA+4*kRelocGap,
  kRelocVregA_12x = kRelocVregA+5*kRelocGap,
  kRelocVregA_20t = kRelocVregA+6*kRelocGap,
  kRelocVregA_21c = kRelocVregA+7*kRelocGap,
  kRelocVregA_21h = kRelocVregA+8*kRelocGap,
  kRelocVregA_21s = kRelocVregA+9*kRelocGap,
  kRelocVregA_21t = kRelocVregA+10*kRelocGap,
  kRelocVregA_22b = kRelocVregA+11*kRelocGap,
  kRelocVregA_22c = kRelocVregA+12*kRelocGap,
  kRelocVregA_22s = kRelocVregA+13*kRelocGap,
  kRelocVregA_22t = kRelocVregA+14*kRelocGap,
  kRelocVregA_22x = kRelocVregA+15*kRelocGap,
  kRelocVregA_23x = kRelocVregA+16*kRelocGap,
  kRelocVregA_30t = kRelocVregA+17*kRelocGap,
  kRelocVregA_31c = kRelocVregA+18*kRelocGap,
  kRelocVregA_31i = kRelocVregA+19*kRelocGap,
  kRelocVregA_31t = kRelocVregA+20*kRelocGap,
  kRelocVregA_32x = kRelocVregA+21*kRelocGap,
  kRelocVregA_35c = kRelocVregA+22*kRelocGap,
  kRelocVregA_3rc = kRelocVregA+23*kRelocGap,
  kRelocVregA_51l = kRelocVregA+24*kRelocGap,

  // VRegB
  kRelocVregB = 2000,
  kRelocVregB_11n = kRelocVregB+1*kRelocGap,
  kRelocVregB_12x = kRelocVregB+2*kRelocGap,
  kRelocVregB_21c = kRelocVregB+3*kRelocGap,
  kRelocVregB_21h = kRelocVregB+4*kRelocGap,
  kRelocVregB_21s = kRelocVregB+5*kRelocGap,
  kRelocVregB_21t = kRelocVregB+6*kRelocGap,
  kRelocVregB_22b = kRelocVregB+7*kRelocGap,
  kRelocVregB_22c = kRelocVregB+8*kRelocGap,
  kRelocVregB_22s = kRelocVregB+9*kRelocGap,
  kRelocVregB_22t = kRelocVregB+10*kRelocGap,
  kRelocVregB_22x = kRelocVregB+11*kRelocGap,
  kRelocVregB_23x = kRelocVregB+12*kRelocGap,
  kRelocVregB_31c = kRelocVregB+13*kRelocGap,
  kRelocVregB_31i = kRelocVregB+14*kRelocGap,
  kRelocVregB_31t = kRelocVregB+15*kRelocGap,
  kRelocVregB_32x = kRelocVregB+16*kRelocGap,
  kRelocVregB_35c = kRelocVregB+17*kRelocGap,
  kRelocVregB_3rc = kRelocVregB+18*kRelocGap,
  kRelocVregB_51l = kRelocVregB+19*kRelocGap,

  // VRegC
  kRelocVregC = 3000,
  kRelocVregC_22b = kRelocVregC+1*kRelocGap,
  kRelocVregC_22c = kRelocVregC+2*kRelocGap,
  kRelocVregC_22s = kRelocVregC+3*kRelocGap,
  kRelocVregC_22t = kRelocVregC+4*kRelocGap,
  kRelocVregC_23x = kRelocVregC+5*kRelocGap,
  kRelocVregC_35c = kRelocVregC+6*kRelocGap,
  kRelocVregC_3rc = kRelocVregC+7*kRelocGap,
};       // enum

}       // namespace arm
}       // namespace art

#endif  // ART_RUNTIME_ARCH_ARM_XLATOR_XLATOR_RELOCS_H_
