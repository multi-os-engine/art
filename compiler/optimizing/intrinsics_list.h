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

#ifndef ART_COMPILER_OPTIMIZING_INTRINSICS_LIST_H_
#define ART_COMPILER_OPTIMIZING_INTRINSICS_LIST_H_

// All intrinsics supported by the optimizing compiler. Format is name, then whether it is expected
// to be a HInvokeStaticOrDirect node (compared to HInvokeVirtual), then whether it requires an
// environment.

#define INTRINSICS_LIST(V) \
  V(DoubleDoubleToRawLongBits, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(DoubleLongBitsToDouble, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(FloatFloatToRawIntBits, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(FloatIntBitsToFloat, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(IntegerReverse, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(IntegerReverseBytes, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(IntegerNumberOfLeadingZeros, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(IntegerNumberOfTrailingZeros, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(IntegerRotateRight, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(IntegerRotateLeft, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(LongReverse, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(LongReverseBytes, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(LongNumberOfLeadingZeros, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(LongNumberOfTrailingZeros, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(LongRotateRight, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(LongRotateLeft, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(ShortReverseBytes, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathAbsDouble, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathAbsFloat, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathAbsLong, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathAbsInt, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathMinDoubleDouble, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathMinFloatFloat, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathMinLongLong, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathMinIntInt, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathMaxDoubleDouble, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathMaxFloatFloat, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathMaxLongLong, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathMaxIntInt, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathCos, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathSin, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathAcos, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathAsin, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathAtan, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathAtan2, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathCbrt, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathCosh, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathExp, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathExpm1, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathHypot, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathLog, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathLog10, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathNextAfter, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathSinh, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathTan, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathTanh, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathSqrt, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathCeil, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathFloor, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathRint, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathRoundDouble, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(MathRoundFloat, kStatic, kNeedsEnvironmentOrCache, kNoSideEffects) \
  V(SystemArrayCopyChar, kStatic, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(SystemArrayCopy, kStatic, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(ThreadCurrentThread, kStatic, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(MemoryPeekByte, kStatic, kNeedsEnvironmentOrCache, kReadSideEffects) \
  V(MemoryPeekIntNative, kStatic, kNeedsEnvironmentOrCache, kReadSideEffects) \
  V(MemoryPeekLongNative, kStatic, kNeedsEnvironmentOrCache, kReadSideEffects) \
  V(MemoryPeekShortNative, kStatic, kNeedsEnvironmentOrCache, kReadSideEffects) \
  V(MemoryPokeByte, kStatic, kNeedsEnvironmentOrCache, kWriteSideEffects) \
  V(MemoryPokeIntNative, kStatic, kNeedsEnvironmentOrCache, kWriteSideEffects) \
  V(MemoryPokeLongNative, kStatic, kNeedsEnvironmentOrCache, kWriteSideEffects) \
  V(MemoryPokeShortNative, kStatic, kNeedsEnvironmentOrCache, kWriteSideEffects) \
  V(StringCharAt, kDirect, kNeedsEnvironmentOrCache, kReadSideEffects) \
  V(StringCompareTo, kDirect, kNeedsEnvironmentOrCache, kReadSideEffects) \
  V(StringEquals, kDirect, kNeedsEnvironmentOrCache, kReadSideEffects) \
  V(StringGetCharsNoCheck, kDirect, kNeedsEnvironmentOrCache, kReadSideEffects) \
  V(StringIndexOf, kDirect, kNeedsEnvironmentOrCache, kReadSideEffects) \
  V(StringIndexOfAfter, kDirect, kNeedsEnvironmentOrCache, kReadSideEffects) \
  V(StringNewStringFromBytes, kStatic, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(StringNewStringFromChars, kStatic, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(StringNewStringFromString, kStatic, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafeCASInt, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafeCASLong, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafeCASObject, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafeGet, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafeGetVolatile, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafeGetObject, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafeGetObjectVolatile, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafeGetLong, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafeGetLongVolatile, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafePut, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafePutOrdered, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafePutVolatile, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafePutObject, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafePutObjectOrdered, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafePutObjectVolatile, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafePutLong, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafePutLongOrdered, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(UnsafePutLongVolatile, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects) \
  V(ReferenceGetReferent, kDirect, kNeedsEnvironmentOrCache, kAllSideEffects)

#endif  // ART_COMPILER_OPTIMIZING_INTRINSICS_LIST_H_
#undef ART_COMPILER_OPTIMIZING_INTRINSICS_LIST_H_   // #define is only for lint.
