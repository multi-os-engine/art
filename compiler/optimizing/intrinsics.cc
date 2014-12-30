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

#include "intrinsics.h"

#include "dex/quick/dex_file_method_inliner.h"
#include "dex/quick/dex_file_to_method_inliner_map.h"
#include "driver/compiler_driver.h"
#include "nodes.h"
#include "quick/inline_method_analyser.h"

namespace art {

static Primitive::Type GetType(uint64_t data, bool is_op_size) {
  if (is_op_size) {
    switch (static_cast<OpSize>(data)) {
      case kSignedByte:
        return Primitive::Type::kPrimByte;
      case kSignedHalf:
        return Primitive::Type::kPrimShort;
      case k32:
        return Primitive::Type::kPrimInt;
      case k64:
        return Primitive::Type::kPrimLong;
      default:
        LOG(FATAL) << "Unknown/unsupported op size " << data;
        UNREACHABLE();
    }
  } else {
    if ((data & kIntrinsicFlagIsLong) != 0) {
      return Primitive::Type::kPrimLong;
    }
    if ((data & kIntrinsicFlagIsObject) != 0) {
      return Primitive::Type::kPrimNot;
    }
    return Primitive::Type::kPrimInt;
  }
}

// TODO: Refactor DexFileMethodInliner and have something nicer than InlineMethod.
void IntrinsicsRecognizer::Run() {
  DexFileMethodInliner* inliner = driver_->GetMethodInlinerMap()->GetMethodInliner(dex_file_);
  DCHECK(inliner != nullptr);

  for (HReversePostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    HBasicBlock* block = it.Current();
    for (HInstructionIterator inst_it(block->GetInstructions()); !inst_it.Done();
         inst_it.Advance()) {
      HInstruction* inst = inst_it.Current();
      if (inst->IsInvoke()) {
        HInvoke* invoke = inst->AsInvoke();
        InlineMethod method;
        if (inliner->IsIntrinsic(invoke->GetDexMethodIndex(), &method)) {
          Intrinsics intrinsic = Intrinsics::kNone;
          switch (method.opcode) {
            // Floating-point conversions.
            case kIntrinsicDoubleCvt:
              if ((method.d.data & kIntrinsicFlagToFloatingPoint) == 0) {
                intrinsic = Intrinsics::kDoubleDoubleToRawLongBits;
              } else {
                intrinsic = Intrinsics::kDoubleLongBitsToDouble;
              }
              break;
            case kIntrinsicFloatCvt:
              if ((method.d.data & kIntrinsicFlagToFloatingPoint) == 0) {
                intrinsic = Intrinsics::kFloatFloatToRawIntBits;
              } else {
                intrinsic = Intrinsics::kFloatIntBitsToFloat;
              }
              break;

            // Bit manipulations.
            case kIntrinsicReverseBits:
              switch (GetType(method.d.data, true)) {
                case Primitive::Type::kPrimInt:
                  intrinsic = Intrinsics::kIntegerReverse;
                  break;
                case Primitive::Type::kPrimLong:
                  intrinsic = Intrinsics::kLongReverse;
                  break;
                default:
                  LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
                  UNREACHABLE();
              }
              break;
            case kIntrinsicReverseBytes:
              switch (GetType(method.d.data, true)) {
                case Primitive::Type::kPrimShort:
                  intrinsic = Intrinsics::kShortReverseBytes;
                  break;
                case Primitive::Type::kPrimInt:
                  intrinsic = Intrinsics::kIntegerReverseBytes;
                  break;
                case Primitive::Type::kPrimLong:
                  intrinsic = Intrinsics::kLongReverseBytes;
                  break;
                default:
                  LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
                  UNREACHABLE();
              }
              break;

            // Abs.
            case kIntrinsicAbsDouble:
              intrinsic = Intrinsics::kMathAbsDouble;
              break;
            case kIntrinsicAbsFloat:
              intrinsic = Intrinsics::kMathAbsFloat;
              break;
            case kIntrinsicAbsInt:
              intrinsic = Intrinsics::kMathAbsInt;
              break;
            case kIntrinsicAbsLong:
              intrinsic = Intrinsics::kMathAbsLong;
              break;

            // Min/max.
            case kIntrinsicMinMaxDouble:
              if ((method.d.data & kIntrinsicFlagMin) == 0) {
                intrinsic = Intrinsics::kMathMaxDoubleDouble;
              } else {
                intrinsic = Intrinsics::kMathMinDoubleDouble;
              }
              break;
            case kIntrinsicMinMaxFloat:
              if ((method.d.data & kIntrinsicFlagMin) == 0) {
                intrinsic = Intrinsics::kMathMaxFloatFloat;
              } else {
                intrinsic = Intrinsics::kMathMinFloatFloat;
              }
              break;
            case kIntrinsicMinMaxInt:
              if ((method.d.data & kIntrinsicFlagMin) == 0) {
                intrinsic = Intrinsics::kMathMaxIntInt;
              } else {
                intrinsic = Intrinsics::kMathMinIntInt;
              }
              break;
            case kIntrinsicMinMaxLong:
              if ((method.d.data & kIntrinsicFlagMin) == 0) {
                intrinsic = Intrinsics::kMathMaxLongLong;
              } else {
                intrinsic = Intrinsics::kMathMinLongLong;
              }
              break;

            // Misc math.
            case kIntrinsicSqrt:
              intrinsic = Intrinsics::kMathSqrt;
              break;
            case kIntrinsicCeil:
              intrinsic = Intrinsics::kMathCeil;
              break;
            case kIntrinsicFloor:
              intrinsic = Intrinsics::kMathFloor;
              break;
            case kIntrinsicRint:
              intrinsic = Intrinsics::kMathRint;
              break;
            case kIntrinsicRoundDouble:
              intrinsic = Intrinsics::kMathRoundDouble;
              break;
            case kIntrinsicRoundFloat:
              intrinsic = Intrinsics::kMathRoundFloat;
              break;

            // System.arraycopy.
            case kIntrinsicSystemArrayCopyCharArray:
              intrinsic = Intrinsics::kSystemArrayCopyChar;
              break;

            // Thread.currentThread.
            case kIntrinsicCurrentThread:
              intrinsic = Intrinsics::kThreadCurrentThread;
              break;

            // Memory.peek.
            case kIntrinsicPeek:
              switch (GetType(method.d.data, true)) {
                case Primitive::Type::kPrimByte:
                  intrinsic = Intrinsics::kMemoryPeekByte;
                  break;
                case Primitive::Type::kPrimShort:
                  intrinsic = Intrinsics::kMemoryPeekShortNative;
                  break;
                case Primitive::Type::kPrimInt:
                  intrinsic = Intrinsics::kMemoryPeekIntNative;
                  break;
                case Primitive::Type::kPrimLong:
                  intrinsic = Intrinsics::kMemoryPeekLongNative;
                  break;
                default:
                  LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
                  UNREACHABLE();
              }
              break;

            // Memory.poke.
            case kIntrinsicPoke:
              switch (GetType(method.d.data, true)) {
                case Primitive::Type::kPrimByte:
                  intrinsic = Intrinsics::kMemoryPokeByte;
                  break;
                case Primitive::Type::kPrimShort:
                  intrinsic = Intrinsics::kMemoryPokeShortNative;
                  break;
                case Primitive::Type::kPrimInt:
                  intrinsic = Intrinsics::kMemoryPokeIntNative;
                  break;
                case Primitive::Type::kPrimLong:
                  intrinsic = Intrinsics::kMemoryPokeLongNative;
                  break;
                default:
                  LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
                  UNREACHABLE();
              }
              break;

            // String.
            case kIntrinsicCharAt:
              intrinsic = Intrinsics::kStringCharAt;
              break;
            case kIntrinsicCompareTo:
              intrinsic = Intrinsics::kStringCompareTo;
              break;
            case kIntrinsicIsEmptyOrLength:
              if ((method.d.data & kIntrinsicFlagIsEmpty) == 0) {
                intrinsic = Intrinsics::kStringLength;
              } else {
                intrinsic = Intrinsics::kStringIsEmpty;
              }
              break;
            case kIntrinsicIndexOf:
              if ((method.d.data & kIntrinsicFlagBase0) == 0) {
                intrinsic = Intrinsics::kStringIndexOfAfter;
              } else {
                intrinsic = Intrinsics::kStringIndexOf;
              }
              break;

            case kIntrinsicCas:
              switch (GetType(method.d.data, false)) {
                case Primitive::Type::kPrimNot:
                  intrinsic = Intrinsics::kUnsafeCASObject;
                  break;
                case Primitive::Type::kPrimInt:
                  intrinsic = Intrinsics::kUnsafeCASInt;
                  break;
                case Primitive::Type::kPrimLong:
                  intrinsic = Intrinsics::kUnsafeCASLong;
                  break;
                default:
                  LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
                  UNREACHABLE();
              }
              break;
            case kIntrinsicUnsafeGet: {
                const bool is_volatile = (method.d.data & kIntrinsicFlagIsVolatile);
                switch (GetType(method.d.data, false)) {
                  case Primitive::Type::kPrimInt:
                    intrinsic = (is_volatile) ? Intrinsics::kUnsafeGetVolatile :
                              Intrinsics::kUnsafeGet;
                    break;
                  case Primitive::Type::kPrimLong:
                    intrinsic = (is_volatile) ? Intrinsics::kUnsafeGetLongVolatile :
                        Intrinsics::kUnsafeGetLong;
                    break;
                  default:
                    LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
                    UNREACHABLE();
                }
                break;
              }
            case kIntrinsicUnsafePut: {
                enum Sync { kNoSync, kVolatile, kOrdered };
                const Sync sync = ((method.d.data & kIntrinsicFlagIsVolatile) != 0) ? kVolatile :
                                  ((method.d.data & kIntrinsicFlagIsOrdered) != 0) ? kOrdered :
                                  kNoSync;
                switch (GetType(method.d.data, false)) {
                  case Primitive::Type::kPrimInt:
                    switch (sync) {
                      case kNoSync:
                        intrinsic = Intrinsics::kUnsafePut;
                        break;
                      case kVolatile:
                        intrinsic = Intrinsics::kUnsafePutVolatile;
                        break;
                      case kOrdered:
                        intrinsic = Intrinsics::kUnsafePutOrdered;
                        break;
                    }
                    break;
                  case Primitive::Type::kPrimLong:
                    switch (sync) {
                      case kNoSync:
                        intrinsic = Intrinsics::kUnsafePutLong;
                        break;
                      case kVolatile:
                        intrinsic = Intrinsics::kUnsafePutLongVolatile;
                        break;
                      case kOrdered:
                        intrinsic = Intrinsics::kUnsafePutLongOrdered;
                        break;
                    }
                    break;
                  case Primitive::Type::kPrimNot:
                    switch (sync) {
                      case kNoSync:
                        intrinsic = Intrinsics::kUnsafePutObject;
                        break;
                      case kVolatile:
                        intrinsic = Intrinsics::kUnsafePutObjectVolatile;
                        break;
                      case kOrdered:
                        intrinsic = Intrinsics::kUnsafePutObjectOrdered;
                        break;
                    }
                    break;
                  default:
                    LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
                    UNREACHABLE();
                }
                break;
              }

            // Virtual cases.

            case kIntrinsicReferenceGetReferent:
              intrinsic = Intrinsics::kReferenceGetReferent;
              break;

            // Quick inliner cases. Remove after refactoring. They are here so that we can use the
            // compiler to warn on missing cases.

            case kInlineOpNop:
            case kInlineOpReturnArg:
            case kInlineOpNonWideConst:
            case kInlineOpIGet:
            case kInlineOpIPut:
              break;

            // No default case to make the compiler warn on missing cases.
          }
          if (intrinsic != Intrinsics::kNone) {
            // The DexFileMethodInliner should have checked whether the methods are agreeing with
            // what we expect, i.e., static methods are called as such. Add another check here for
            // our expectations:
            // Whenever the intrinsic is marked as static-or-direct, report an error if we find an
            // InvokeVirtual. The other direction is not possible: we have intrinsics for virtual
            // functions that will perform a check inline. If the precise type is known, however,
            // the instruction will be sharpened to an InvokeStaticOrDirect.
            if (IsStaticOrDirectIntrinsic(intrinsic) && !invoke->IsInvokeStaticOrDirect()) {
              LOG(WARNING) << "Found an intrinsic with unexpected virtual invoke type: "
                           << intrinsic << " for "
                           << PrettyMethod(invoke->GetDexMethodIndex(), *dex_file_);
            } else {
              invoke->SetIntrinsic(intrinsic);
            }
          }
        }
      }
    }
  }
}

std::ostream& operator<<(std::ostream& os, const Intrinsics& intrinsic) {
  switch (intrinsic) {
    case Intrinsics::kNone:
      os << "No intrinsic.";
      break;
#define OPTIMIZING_INTRINSICS(Name, IsStatic) \
    case Intrinsics::k ## Name: \
      os << # Name; \
      break;
#include "intrinsics_list.h"
INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef STATIC_INTRINSICS_LIST
#undef VIRTUAL_INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS
  }
  return os;
}

}  // namespace art

