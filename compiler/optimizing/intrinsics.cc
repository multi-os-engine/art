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

#include "art_method.h"
#include "class_linker.h"
#include "dex/quick/dex_file_method_inliner.h"
#include "dex/quick/dex_file_to_method_inliner_map.h"
#include "driver/compiler_driver.h"
#include "invoke_type.h"
#include "mirror/dex_cache-inl.h"
#include "nodes.h"
#include "quick/inline_method_analyser.h"
#include "scoped_thread_state_change.h"
#include "thread-inl.h"
#include "utils.h"

namespace art {

// Function that returns whether an intrinsic is static/direct or virtual.
static inline InvokeType GetIntrinsicInvokeType(Intrinsics i) {
  switch (i) {
    case Intrinsics::kNone:
      return kInterface;  // Non-sensical for intrinsic.
#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache) \
    case Intrinsics::k ## Name:               \
      return IsStatic;
#include "intrinsics_list.h"
INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS
  }
  return kInterface;
}

// Function that returns whether an intrinsic needs an environment or not.
static inline IntrinsicNeedsEnvironmentOrCache NeedsEnvironmentOrCache(Intrinsics i) {
  switch (i) {
    case Intrinsics::kNone:
      return kNeedsEnvironmentOrCache;  // Non-sensical for intrinsic.
#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache) \
    case Intrinsics::k ## Name:               \
      return NeedsEnvironmentOrCache;
#include "intrinsics_list.h"
INTRINSICS_LIST(OPTIMIZING_INTRINSICS)
#undef INTRINSICS_LIST
#undef OPTIMIZING_INTRINSICS
  }
  return kNeedsEnvironmentOrCache;
}

static Primitive::Type GetType(uint64_t data, bool is_op_size) {
  if (is_op_size) {
    switch (static_cast<OpSize>(data)) {
      case kSignedByte:
        return Primitive::kPrimByte;
      case kSignedHalf:
        return Primitive::kPrimShort;
      case k32:
        return Primitive::kPrimInt;
      case k64:
        return Primitive::kPrimLong;
      default:
        LOG(FATAL) << "Unknown/unsupported op size " << data;
        UNREACHABLE();
    }
  } else {
    if ((data & kIntrinsicFlagIsLong) != 0) {
      return Primitive::kPrimLong;
    }
    if ((data & kIntrinsicFlagIsObject) != 0) {
      return Primitive::kPrimNot;
    }
    return Primitive::kPrimInt;
  }
}

static Intrinsics GetIntrinsic(InlineMethod method, InstructionSet instruction_set) {
  if (instruction_set == kMips) {
    return Intrinsics::kNone;
  }
  switch (method.opcode) {
    // Floating-point conversions.
    case kIntrinsicDoubleCvt:
      return ((method.d.data & kIntrinsicFlagToFloatingPoint) == 0) ?
          Intrinsics::kDoubleDoubleToRawLongBits : Intrinsics::kDoubleLongBitsToDouble;
    case kIntrinsicFloatCvt:
      return ((method.d.data & kIntrinsicFlagToFloatingPoint) == 0) ?
          Intrinsics::kFloatFloatToRawIntBits : Intrinsics::kFloatIntBitsToFloat;

    // Bit manipulations.
    case kIntrinsicReverseBits:
      switch (GetType(method.d.data, true)) {
        case Primitive::kPrimInt:
          return Intrinsics::kIntegerReverse;
        case Primitive::kPrimLong:
          return Intrinsics::kLongReverse;
        default:
          LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
          UNREACHABLE();
      }
    case kIntrinsicReverseBytes:
      switch (GetType(method.d.data, true)) {
        case Primitive::kPrimShort:
          return Intrinsics::kShortReverseBytes;
        case Primitive::kPrimInt:
          return Intrinsics::kIntegerReverseBytes;
        case Primitive::kPrimLong:
          return Intrinsics::kLongReverseBytes;
        default:
          LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
          UNREACHABLE();
      }
    case kIntrinsicRotateRight:
      switch (GetType(method.d.data, true)) {
        case Primitive::kPrimInt:
          return Intrinsics::kIntegerRotateRight;
        case Primitive::kPrimLong:
          return Intrinsics::kLongRotateRight;
        default:
          LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
          UNREACHABLE();
      }
    case kIntrinsicRotateLeft:
      switch (GetType(method.d.data, true)) {
        case Primitive::kPrimInt:
          return Intrinsics::kIntegerRotateLeft;
        case Primitive::kPrimLong:
          return Intrinsics::kLongRotateLeft;
        default:
          LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
          UNREACHABLE();
      }

    // Misc data processing.
    case kIntrinsicNumberOfLeadingZeros:
      switch (GetType(method.d.data, true)) {
        case Primitive::kPrimInt:
          return Intrinsics::kIntegerNumberOfLeadingZeros;
        case Primitive::kPrimLong:
          return Intrinsics::kLongNumberOfLeadingZeros;
        default:
          LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
          UNREACHABLE();
      }
    case kIntrinsicNumberOfTrailingZeros:
      switch (GetType(method.d.data, true)) {
        case Primitive::kPrimInt:
          return Intrinsics::kIntegerNumberOfTrailingZeros;
        case Primitive::kPrimLong:
          return Intrinsics::kLongNumberOfTrailingZeros;
        default:
          LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
          UNREACHABLE();
      }

    // Abs.
    case kIntrinsicAbsDouble:
      return Intrinsics::kMathAbsDouble;
    case kIntrinsicAbsFloat:
      return Intrinsics::kMathAbsFloat;
    case kIntrinsicAbsInt:
      return Intrinsics::kMathAbsInt;
    case kIntrinsicAbsLong:
      return Intrinsics::kMathAbsLong;

    // Min/max.
    case kIntrinsicMinMaxDouble:
      return ((method.d.data & kIntrinsicFlagMin) == 0) ?
          Intrinsics::kMathMaxDoubleDouble : Intrinsics::kMathMinDoubleDouble;
    case kIntrinsicMinMaxFloat:
      return ((method.d.data & kIntrinsicFlagMin) == 0) ?
          Intrinsics::kMathMaxFloatFloat : Intrinsics::kMathMinFloatFloat;
    case kIntrinsicMinMaxInt:
      return ((method.d.data & kIntrinsicFlagMin) == 0) ?
          Intrinsics::kMathMaxIntInt : Intrinsics::kMathMinIntInt;
    case kIntrinsicMinMaxLong:
      return ((method.d.data & kIntrinsicFlagMin) == 0) ?
          Intrinsics::kMathMaxLongLong : Intrinsics::kMathMinLongLong;

    // Misc math.
    case kIntrinsicSqrt:
      return Intrinsics::kMathSqrt;
    case kIntrinsicCeil:
      return Intrinsics::kMathCeil;
    case kIntrinsicFloor:
      return Intrinsics::kMathFloor;
    case kIntrinsicRint:
      return Intrinsics::kMathRint;
    case kIntrinsicRoundDouble:
      return Intrinsics::kMathRoundDouble;
    case kIntrinsicRoundFloat:
      return Intrinsics::kMathRoundFloat;

    // System.arraycopy.
    case kIntrinsicSystemArrayCopyCharArray:
      return Intrinsics::kSystemArrayCopyChar;

    case kIntrinsicSystemArrayCopy:
      return Intrinsics::kSystemArrayCopy;

    // Thread.currentThread.
    case kIntrinsicCurrentThread:
      return  Intrinsics::kThreadCurrentThread;

    // Memory.peek.
    case kIntrinsicPeek:
      switch (GetType(method.d.data, true)) {
        case Primitive::kPrimByte:
          return Intrinsics::kMemoryPeekByte;
        case Primitive::kPrimShort:
          return Intrinsics::kMemoryPeekShortNative;
        case Primitive::kPrimInt:
          return Intrinsics::kMemoryPeekIntNative;
        case Primitive::kPrimLong:
          return Intrinsics::kMemoryPeekLongNative;
        default:
          LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
          UNREACHABLE();
      }

    // Memory.poke.
    case kIntrinsicPoke:
      switch (GetType(method.d.data, true)) {
        case Primitive::kPrimByte:
          return Intrinsics::kMemoryPokeByte;
        case Primitive::kPrimShort:
          return Intrinsics::kMemoryPokeShortNative;
        case Primitive::kPrimInt:
          return Intrinsics::kMemoryPokeIntNative;
        case Primitive::kPrimLong:
          return Intrinsics::kMemoryPokeLongNative;
        default:
          LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
          UNREACHABLE();
      }

    // String.
    case kIntrinsicCharAt:
      return Intrinsics::kStringCharAt;
    case kIntrinsicCompareTo:
      return Intrinsics::kStringCompareTo;
    case kIntrinsicEquals:
      return Intrinsics::kStringEquals;
    case kIntrinsicGetCharsNoCheck:
      return Intrinsics::kStringGetCharsNoCheck;
    case kIntrinsicIsEmptyOrLength:
      // The inliner can handle these two cases - and this is the preferred approach
      // since after inlining the call is no longer visible (as opposed to waiting
      // until codegen to handle intrinsic).
      return Intrinsics::kNone;
    case kIntrinsicIndexOf:
      return ((method.d.data & kIntrinsicFlagBase0) == 0) ?
          Intrinsics::kStringIndexOfAfter : Intrinsics::kStringIndexOf;
    case kIntrinsicNewStringFromBytes:
      return Intrinsics::kStringNewStringFromBytes;
    case kIntrinsicNewStringFromChars:
      return Intrinsics::kStringNewStringFromChars;
    case kIntrinsicNewStringFromString:
      return Intrinsics::kStringNewStringFromString;

    case kIntrinsicCas:
      switch (GetType(method.d.data, false)) {
        case Primitive::kPrimNot:
          return Intrinsics::kUnsafeCASObject;
        case Primitive::kPrimInt:
          return Intrinsics::kUnsafeCASInt;
        case Primitive::kPrimLong:
          return Intrinsics::kUnsafeCASLong;
        default:
          LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
          UNREACHABLE();
      }
    case kIntrinsicUnsafeGet: {
      const bool is_volatile = (method.d.data & kIntrinsicFlagIsVolatile);
      switch (GetType(method.d.data, false)) {
        case Primitive::kPrimInt:
          return is_volatile ? Intrinsics::kUnsafeGetVolatile : Intrinsics::kUnsafeGet;
        case Primitive::kPrimLong:
          return is_volatile ? Intrinsics::kUnsafeGetLongVolatile : Intrinsics::kUnsafeGetLong;
        case Primitive::kPrimNot:
          return is_volatile ? Intrinsics::kUnsafeGetObjectVolatile : Intrinsics::kUnsafeGetObject;
        default:
          LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
          UNREACHABLE();
      }
    }
    case kIntrinsicUnsafePut: {
      enum Sync { kNoSync, kVolatile, kOrdered };
      const Sync sync =
          ((method.d.data & kIntrinsicFlagIsVolatile) != 0) ? kVolatile :
          ((method.d.data & kIntrinsicFlagIsOrdered) != 0)  ? kOrdered :
                                                              kNoSync;
      switch (GetType(method.d.data, false)) {
        case Primitive::kPrimInt:
          switch (sync) {
            case kNoSync:
              return Intrinsics::kUnsafePut;
            case kVolatile:
              return Intrinsics::kUnsafePutVolatile;
            case kOrdered:
              return Intrinsics::kUnsafePutOrdered;
          }
          break;
        case Primitive::kPrimLong:
          switch (sync) {
            case kNoSync:
              return Intrinsics::kUnsafePutLong;
            case kVolatile:
              return Intrinsics::kUnsafePutLongVolatile;
            case kOrdered:
              return Intrinsics::kUnsafePutLongOrdered;
          }
          break;
        case Primitive::kPrimNot:
          switch (sync) {
            case kNoSync:
              return Intrinsics::kUnsafePutObject;
            case kVolatile:
              return Intrinsics::kUnsafePutObjectVolatile;
            case kOrdered:
              return Intrinsics::kUnsafePutObjectOrdered;
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
      return Intrinsics::kReferenceGetReferent;

    // Quick inliner cases. Remove after refactoring. They are here so that we can use the
    // compiler to warn on missing cases.

    case kInlineOpNop:
    case kInlineOpReturnArg:
    case kInlineOpNonWideConst:
    case kInlineOpIGet:
    case kInlineOpIPut:
      return Intrinsics::kNone;

    // String init cases, not intrinsics.

    case kInlineStringInit:
      return Intrinsics::kNone;

    // No default case to make the compiler warn on missing cases.
  }
  return Intrinsics::kNone;
}

static bool CheckInvokeType(Intrinsics intrinsic, HInvoke* invoke, const DexFile& dex_file) {
  // The DexFileMethodInliner should have checked whether the methods are agreeing with
  // what we expect, i.e., static methods are called as such. Add another check here for
  // our expectations:
  //
  // Whenever the intrinsic is marked as static, report an error if we find an InvokeVirtual.
  //
  // Whenever the intrinsic is marked as direct and we find an InvokeVirtual, a devirtualization
  // failure occured. We might be in a situation where we have inlined a method that calls an
  // intrinsic, but that method is in a different dex file on which we do not have a
  // verified_method that would have helped the compiler driver sharpen the call. In that case,
  // make sure that the intrinsic is actually for some final method (or in a final class), as
  // otherwise the intrinsics setup is broken.
  //
  // For the last direction, we have intrinsics for virtual functions that will perform a check
  // inline. If the precise type is known, however, the instruction will be sharpened to an
  // InvokeStaticOrDirect.
  InvokeType intrinsic_type = GetIntrinsicInvokeType(intrinsic);
  InvokeType invoke_type = invoke->IsInvokeStaticOrDirect() ?
      invoke->AsInvokeStaticOrDirect()->GetInvokeType() :
      invoke->IsInvokeVirtual() ? kVirtual : kSuper;
  switch (intrinsic_type) {
    case kStatic:
      return (invoke_type == kStatic);

    case kDirect:
      if (invoke_type == kDirect) {
        return true;
      }
      if (invoke_type == kVirtual) {
        ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
        ScopedObjectAccess soa(Thread::Current());
        ArtMethod* art_method =
            class_linker->FindDexCache(soa.Self(), dex_file)->GetResolvedMethod(
                invoke->GetDexMethodIndex(), class_linker->GetImagePointerSize());
        return art_method != nullptr &&
            (art_method->IsFinal() || art_method->GetDeclaringClass()->IsFinal());
      }
      return false;

    case kVirtual:
      // Call might be devirtualized.
      return (invoke_type == kVirtual || invoke_type == kDirect);

    default:
      return false;
  }
}

// TODO: Refactor DexFileMethodInliner and have something nicer than InlineMethod.
void IntrinsicsRecognizer::Run() {
  for (HReversePostOrderIterator it(*graph_); !it.Done(); it.Advance()) {
    HBasicBlock* block = it.Current();
    for (HInstructionIterator inst_it(block->GetInstructions()); !inst_it.Done();
         inst_it.Advance()) {
      HInstruction* inst = inst_it.Current();
      if (inst->IsInvoke()) {
        HInvoke* invoke = inst->AsInvoke();
        InlineMethod method;
        const DexFile& dex_file = invoke->GetDexFile();
        DexFileMethodInliner* inliner = driver_->GetMethodInlinerMap()->GetMethodInliner(&dex_file);
        DCHECK(inliner != nullptr);
        if (inliner->IsIntrinsic(invoke->GetDexMethodIndex(), &method)) {
          Intrinsics intrinsic = GetIntrinsic(method, graph_->GetInstructionSet());

          if (intrinsic != Intrinsics::kNone) {
            if (!CheckInvokeType(intrinsic, invoke, dex_file)) {
              LOG(WARNING) << "Found an intrinsic with unexpected invoke type: "
                  << intrinsic << " for "
                  << PrettyMethod(invoke->GetDexMethodIndex(), invoke->GetDexFile())
                  << invoke->DebugName();
            } else {
              invoke->SetIntrinsic(intrinsic, NeedsEnvironmentOrCache(intrinsic));
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
      os << "None";
      break;
#define OPTIMIZING_INTRINSICS(Name, IsStatic, NeedsEnvironmentOrCache) \
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
