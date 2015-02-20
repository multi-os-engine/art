/*
 * Copyright (C) 2015 The Android Open Source Project
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

#ifndef ART_RUNTIME_ARCH_MIPS_ENTRYPOINT_DIRECT_MIPS_H_
#define ART_RUNTIME_ARCH_MIPS_ENTRYPOINT_DIRECT_MIPS_H_

#include "entrypoints/quick/quick_entrypoints_enum.h"

namespace art {

/* Returns true if entrypoint contains direct reference to
   native implementation. The list is required as direct
   entrypoints need additional handling during invocation.*/
static bool IsDirectEntrypoint(QuickEntrypointEnum entrypoint) {
    switch (entrypoint) {
        case kQuickInstanceofNonTrivial :
        case kQuickA64Load :
        case kQuickA64Store :
        case kQuickFmod :
        case kQuickFmodf :
        case kQuickMemcpy :
        case kQuickL2d :
        case kQuickL2f :
        case kQuickD2iz :
        case kQuickF2iz :
        case kQuickD2l :
        case kQuickF2l :
        case kQuickLdiv :
        case kQuickLmod :
        case kQuickLmul :
        case kQuickCmpgDouble :
        case kQuickCmpgFloat :
        case kQuickCmplDouble :
        case kQuickCmplFloat :
          return true;
        default :
          return false;
    }
}

}  // namespace art

#endif  // ART_RUNTIME_ARCH_MIPS_ENTRYPOINT_DIRECT_MIPS_H_
