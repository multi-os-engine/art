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

#include "nodes_arm64.h"

namespace art {

void HArm64ArithWithOp::GetOpInfoFromEncoding(HArm64BitfieldMove* xbfm,
                                              OpKind* op_kind, int* shift_amount) {
  bool is64bit = xbfm->Requires64bitOperation();
  bool sign_extend = xbfm->bitfield_move_type() == HArm64BitfieldMove::kSBFM;
  int immr = xbfm->immr();
  int imms = xbfm->imms();
  // This decoding logic is based on the ARM Architecture Reference Manual.
  int nbits = is64bit ? 64 : 32;
  if (immr == 0) {
    // The operation is equivalent to an extension.
    int width = imms + 1;
    switch (width) {
      case 8:
        *op_kind = sign_extend ? kSXTB : kUXTB;
        break;
      case 16:
        *op_kind = sign_extend ? kSXTH : kUXTH;
        break;
      case 32:
        *op_kind = sign_extend ? kSXTW : kUXTW;
        break;
      default:
        *op_kind = kInvalidOp;
    }
  } else {
    if (imms == nbits - 1) {
      // This is equivalent to a right shift.
      *shift_amount = immr;
      *op_kind = sign_extend ? kASR : kLSR;
    } else {
      // We may have a left shift.
      int shift = nbits - 1 - imms;
      int immr_for_shift = (-shift % nbits + nbits) % nbits;
      if (immr == immr_for_shift) {
        *op_kind = kLSL;
        *shift_amount = shift;
      } else {
        *op_kind = kInvalidOp;
      }
    }
  }
}

}  // namespace art
