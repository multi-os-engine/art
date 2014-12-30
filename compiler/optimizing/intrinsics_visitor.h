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

#ifndef ART_COMPILER_OPTIMIZING_INTRINSICS_VISITOR_H_
#define ART_COMPILER_OPTIMIZING_INTRINSICS_VISITOR_H_

#include "dex/quick/dex_file_method_inliner.h"
#include "nodes.h"
#include "quick/inline_method_analyser.h"

namespace art {

class IntrinsicVisitor : public ValueObject {
 public:
  virtual ~IntrinsicVisitor() {}

  bool Dispatch(DexFileMethodInliner* const inliner, HInvoke* invoke) {
    if (inliner == nullptr) {
      return false;
    }

    if (invoke->IsInvokeStaticOrDirect()) {
      return Dispatch(inliner, invoke->AsInvokeStaticOrDirect());
    } else if (invoke->IsInvokeVirtual()) {
      return Dispatch(inliner, invoke->AsInvokeVirtual());
    }
    return false;
  }

  bool Dispatch(DexFileMethodInliner* const inliner, HInvokeStaticOrDirect* invoke) {
    if (inliner == nullptr) {
      return false;
    }
    InlineMethod method;
    if (inliner->IsIntrinsic(invoke->GetIndexInDexCache(), &method)) {
      switch (method.opcode) {
        // Bit manipulations.
        case kIntrinsicDoubleCvt:
          if ((method.d.data & kIntrinsicFlagToFloatingPoint) == 0) {
            return VisitDoubleDoubleToRawLongBits(invoke);
          } else {
            return VisitDoubleLongBitsToDouble(invoke);
          }
        case kIntrinsicFloatCvt:
          if ((method.d.data & kIntrinsicFlagToFloatingPoint) == 0) {
            return VisitFloatFloatToRawIntBits(invoke);
          } else {
            return VisitFloatIntBitsToFloat(invoke);
          }
        case kIntrinsicReverseBits:
          switch (static_cast<OpSize>(method.d.data)) {
            case k32:
              return VisitIntegerReverse(invoke);
            case k64:
              return VisitLongReverse(invoke);
            default:
              LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
              UNREACHABLE();
          }
        case kIntrinsicReverseBytes:
          switch (static_cast<OpSize>(method.d.data)) {
            case kSignedHalf:
              return VisitShortReverseBytes(invoke);
            case k32:
              return VisitIntegerReverseBytes(invoke);
            case k64:
              return VisitLongReverseBytes(invoke);
            default:
              LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
              UNREACHABLE();
          }

        // Abs.
        case kIntrinsicAbsDouble:
          return VisitMathAbsDouble(invoke);
        case kIntrinsicAbsFloat:
          return VisitMathAbsFloat(invoke);
        case kIntrinsicAbsInt:
          return VisitMathAbsInt(invoke);
        case kIntrinsicAbsLong:
          return VisitMathAbsLong(invoke);

        // Min/max.
        case kIntrinsicMinMaxDouble:
          if ((method.d.data & kIntrinsicFlagMin) == 0) {
            return VisitMathMaxDoubleDouble(invoke);
          } else {
            return VisitMathMinDoubleDouble(invoke);
          }
        case kIntrinsicMinMaxFloat:
          if ((method.d.data & kIntrinsicFlagMin) == 0) {
            return VisitMathMaxFloatFloat(invoke);
          } else {
            return VisitMathMinFloatFloat(invoke);
          }
        case kIntrinsicMinMaxInt:
          if ((method.d.data & kIntrinsicFlagMin) == 0) {
            return VisitMathMaxIntInt(invoke);
          } else {
            return VisitMathMinIntInt(invoke);
          }
        case kIntrinsicMinMaxLong:
          if ((method.d.data & kIntrinsicFlagMin) == 0) {
            return VisitMathMaxLongLong(invoke);
          } else {
            return VisitMathMinLongLong(invoke);
          }

        // Misc math.
        case kIntrinsicSqrt:
          return VisitMathSqrt(invoke);
        case kIntrinsicCeil:
          return VisitMathCeil(invoke);
        case kIntrinsicFloor:
          return VisitMathFloor(invoke);
        case kIntrinsicRint:
          return VisitMathRint(invoke);
        case kIntrinsicRoundDouble:
          return VisitMathRoundDouble(invoke);
        case kIntrinsicRoundFloat:
          return VisitMathRoundFloat(invoke);

        // System.arraycopy.
        case kIntrinsicSystemArrayCopyCharArray:
          return VisitSystemArrayCopyChar(invoke);

        // Thread.currentThread.
        case kIntrinsicCurrentThread:
          return VisitThreadCurrentThread(invoke);

        // Memory.peek.
        case kIntrinsicPeek:
          switch (static_cast<OpSize>(method.d.data)) {
            case kSignedByte:
              return VisitMemoryPeekByte(invoke);
            case kSignedHalf:
              return VisitMemoryPeekShortNative(invoke);
            case k32:
              return VisitMemoryPeekIntNative(invoke);
            case k64:
              return VisitMemoryPeekLongNative(invoke);
            default:
              LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
              UNREACHABLE();
          }

        // Memory.poke.
        case kIntrinsicPoke:
          switch (static_cast<OpSize>(method.d.data)) {
            case kSignedByte:
              return VisitMemoryPokeByte(invoke);
            case kSignedHalf:
              return VisitMemoryPokeShortNative(invoke);
            case k32:
              return VisitMemoryPokeIntNative(invoke);
            case k64:
              return VisitMemoryPokeLongNative(invoke);
            default:
              LOG(FATAL) << "Unknown/unsupported op size " << method.d.data;
              UNREACHABLE();
          }

        case kIntrinsicReferenceGetReferent:
        case kIntrinsicCas:
        case kIntrinsicUnsafeGet:
        case kIntrinsicUnsafePut:
          // TODO: Implement.
          break;

        default:
          break;
      }
    }

    return false;
  }


  bool Dispatch(DexFileMethodInliner* const inliner, HInvokeVirtual* invoke) {
    if (inliner == nullptr) {
      return false;
    }

    InlineMethod method;
    if (inliner->IsIntrinsic(invoke->GetMethodIndex(), &method)) {
      switch (method.opcode) {
        // String.
        case kIntrinsicCharAt:
          return VisitStringCharAt(invoke);
        case kIntrinsicCompareTo:
          return VisitStringCompareTo(invoke);
        case kIntrinsicIsEmptyOrLength:
          if ((method.d.data & kIntrinsicFlagIsEmpty) == 0) {
            return VisitStringLength(invoke);
          } else {
            return VisitStringIsEmpty(invoke);
          }
        case kIntrinsicIndexOf:
          if ((method.d.data & kIntrinsicFlagBase0) == 0) {
            return VisitStringIndexOfAfter(invoke);
          } else {
            return VisitStringIndexOf(invoke);
          }

        default:
          break;
      }
    }

    return false;
  }

  /*
   * Double.
   */

  // Double.doubleToRawLongBits(d).
  virtual bool VisitDoubleDoubleToRawLongBits(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Double.longBitsToDouble(l).
  virtual bool VisitDoubleLongBitsToDouble(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  /*
   * Float.
   */

  // Float.floatToRawIntBits(f).
  virtual bool VisitFloatFloatToRawIntBits(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Float.intBitsToFloat(i).
  virtual bool VisitFloatIntBitsToFloat(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  /*
   * Integer.
   */

  // Integer.reverse(i).
  virtual bool VisitIntegerReverse(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Integer.reverseBytes(i).
  virtual bool VisitIntegerReverseBytes(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  /*
   * Long.
   */

  // Long.reverse(i).
  virtual bool VisitLongReverse(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Long.reverseBytes(i).
  virtual bool VisitLongReverseBytes(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  /*
   * Short.
   */

  // Short.reverseBytes(i).
  virtual bool VisitShortReverseBytes(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  /*
   * (Strict)Math.
   */

  // Math.abs(x).
  // TODO: Compare against inlining (+intrinsics).
  virtual bool VisitMathAbsDouble(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }
  virtual bool VisitMathAbsFloat(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }
  virtual bool VisitMathAbsLong(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }
  virtual bool VisitMathAbsInt(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Math.min(x,y).
  // TODO: Compare against inlining.
  virtual bool VisitMathMinDoubleDouble(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }
  virtual bool VisitMathMinFloatFloat(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }
  virtual bool VisitMathMinLongLong(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }
  virtual bool VisitMathMinIntInt(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Math.max(x,y).
  // TODO: Compare against inlining.
  virtual bool VisitMathMaxDoubleDouble(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }
  virtual bool VisitMathMaxFloatFloat(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }
  virtual bool VisitMathMaxLongLong(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }
  virtual bool VisitMathMaxIntInt(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Math.sqrt(x).
  virtual bool VisitMathSqrt(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Math.ceil(x).
  virtual bool VisitMathCeil(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Math.floor(x).
  virtual bool VisitMathFloor(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Math.rint(x).
  virtual bool VisitMathRint(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Math.round(x)
  virtual bool VisitMathRoundDouble(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }
  virtual bool VisitMathRoundFloat(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  /*
   * String.
   */

  // String.charAt(i). TODO: See whether inlining produces good enough code.
  virtual bool VisitStringCharAt(HInvokeVirtual* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // String.compareTo(s).
  virtual bool VisitStringCompareTo(HInvokeVirtual* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // String.isEmpty(). TODO: Inlining may produce better results, as it's mostly a simple getter.
  virtual bool VisitStringIsEmpty(HInvokeVirtual* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // String.indexOf(c).
  virtual bool VisitStringIndexOf(HInvokeVirtual* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // String.indexOf(c, i).
  virtual bool VisitStringIndexOfAfter(HInvokeVirtual* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // String.length(). TODO: Inlining may produce a better result, as it's a simple getter.
  virtual bool VisitStringLength(HInvokeVirtual* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  /*
   * System.
   */

  // Memory.pokeShortNative(l, s).
  virtual bool VisitSystemArrayCopyChar(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  /*
   * Thread.
   */

  // Thread.currentThread().
  virtual bool VisitThreadCurrentThread(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  /*
   * libcore.io.Memory.
   */

  // Memory.peekByte(l).
  virtual bool VisitMemoryPeekByte(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Memory.peekIntNative(l).
  virtual bool VisitMemoryPeekIntNative(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Memory.peekLongNative(l).
  virtual bool VisitMemoryPeekLongNative(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Memory.peekShortNative(l).
  virtual bool VisitMemoryPeekShortNative(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Memory.pokeByte(l, b).
  virtual bool VisitMemoryPokeByte(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Memory.pokeIntNative(l, i).
  virtual bool VisitMemoryPokeIntNative(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Memory.pokeLongNative(l, l).
  virtual bool VisitMemoryPokeLongNative(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // Memory.pokeShortNative(l, s).
  virtual bool VisitMemoryPokeShortNative(HInvokeStaticOrDirect* invoke ATTRIBUTE_UNUSED) {
    return false;
  }

  // TODO: Unsafe, Reference.

 protected:
  IntrinsicVisitor() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(IntrinsicVisitor);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INTRINSICS_VISITOR_H_
