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
  V(DoubleDoubleToRawLongBits, kStatic, kNoEnvironmentOrCache) \
  V(DoubleLongBitsToDouble, kStatic, kNoEnvironmentOrCache) \
  V(FloatFloatToRawIntBits, kStatic, kNoEnvironmentOrCache) \
  V(FloatIntBitsToFloat, kStatic, kNoEnvironmentOrCache) \
  V(IntegerReverse, kStatic, kNoEnvironmentOrCache) \
  V(IntegerReverseBytes, kStatic, kNoEnvironmentOrCache) \
  V(IntegerNumberOfLeadingZeros, kStatic, kNoEnvironmentOrCache) \
  V(IntegerNumberOfTrailingZeros, kStatic, kNoEnvironmentOrCache) \
  V(IntegerRotateRight, kStatic, kNoEnvironmentOrCache) \
  V(IntegerRotateLeft, kStatic, kNoEnvironmentOrCache) \
  V(LongReverse, kStatic, kNoEnvironmentOrCache) \
  V(LongReverseBytes, kStatic, kNoEnvironmentOrCache) \
  V(LongNumberOfLeadingZeros, kStatic, kNoEnvironmentOrCache) \
  V(LongNumberOfTrailingZeros, kStatic, kNoEnvironmentOrCache) \
  V(LongRotateRight, kStatic, kNoEnvironmentOrCache) \
  V(LongRotateLeft, kStatic, kNoEnvironmentOrCache) \
  V(ShortReverseBytes, kStatic, kNoEnvironmentOrCache) \
  V(MathAbsDouble, kStatic, kNoEnvironmentOrCache) \
  V(MathAbsFloat, kStatic, kNoEnvironmentOrCache) \
  V(MathAbsLong, kStatic, kNoEnvironmentOrCache) \
  V(MathAbsInt, kStatic, kNoEnvironmentOrCache) \
  V(MathMinDoubleDouble, kStatic, kNoEnvironmentOrCache) \
  V(MathMinFloatFloat, kStatic, kNoEnvironmentOrCache) \
  V(MathMinLongLong, kStatic, kNoEnvironmentOrCache) \
  V(MathMinIntInt, kStatic, kNoEnvironmentOrCache) \
  V(MathMaxDoubleDouble, kStatic, kNoEnvironmentOrCache) \
  V(MathMaxFloatFloat, kStatic, kNoEnvironmentOrCache) \
  V(MathMaxLongLong, kStatic, kNoEnvironmentOrCache) \
  V(MathMaxIntInt, kStatic, kNoEnvironmentOrCache) \
  V(MathSqrt, kStatic, kNoEnvironmentOrCache) \
  V(MathCeil, kStatic, kNeedsEnvironmentOrCache) \
  V(MathFloor, kStatic, kNeedsEnvironmentOrCache) \
  V(MathRint, kStatic, kNeedsEnvironmentOrCache) \
  V(MathRoundDouble, kStatic, kNeedsEnvironmentOrCache) \
  V(MathRoundFloat, kStatic, kNeedsEnvironmentOrCache) \
  V(SystemArrayCopyChar, kStatic, kNeedsEnvironmentOrCache) \
  V(SystemArrayCopy, kStatic, kNeedsEnvironmentOrCache) \
  V(ThreadCurrentThread, kStatic, kNeedsEnvironmentOrCache) \
  V(MemoryPeekByte, kStatic, kNoEnvironmentOrCache) \
  V(MemoryPeekIntNative, kStatic, kNoEnvironmentOrCache) \
  V(MemoryPeekLongNative, kStatic, kNoEnvironmentOrCache) \
  V(MemoryPeekShortNative, kStatic, kNoEnvironmentOrCache) \
  V(MemoryPokeByte, kStatic, kNoEnvironmentOrCache) \
  V(MemoryPokeIntNative, kStatic, kNoEnvironmentOrCache) \
  V(MemoryPokeLongNative, kStatic, kNoEnvironmentOrCache) \
  V(MemoryPokeShortNative, kStatic, kNoEnvironmentOrCache) \
  V(StringCharAt, kDirect, kNeedsEnvironmentOrCache) \
  V(StringCompareTo, kDirect, kNeedsEnvironmentOrCache) \
  V(StringEquals, kDirect, kNoEnvironmentOrCache) \
  V(StringGetCharsNoCheck, kDirect, kNeedsEnvironmentOrCache) \
  V(StringIndexOf, kDirect, kNeedsEnvironmentOrCache) \
  V(StringIndexOfAfter, kDirect, kNeedsEnvironmentOrCache) \
  V(StringNewStringFromBytes, kStatic, kNeedsEnvironmentOrCache) \
  V(StringNewStringFromChars, kStatic, kNeedsEnvironmentOrCache) \
  V(StringNewStringFromString, kStatic, kNeedsEnvironmentOrCache) \
  V(UnsafeCASInt, kDirect, kNoEnvironmentOrCache) \
  V(UnsafeCASLong, kDirect, kNoEnvironmentOrCache) \
  V(UnsafeCASObject, kDirect, kNoEnvironmentOrCache) \
  V(UnsafeGet, kDirect, kNoEnvironmentOrCache) \
  V(UnsafeGetVolatile, kDirect, kNoEnvironmentOrCache) \
  V(UnsafeGetObject, kDirect, kNeedsEnvironmentOrCache) \
  V(UnsafeGetObjectVolatile, kDirect, kNeedsEnvironmentOrCache) \
  V(UnsafeGetLong, kDirect, kNoEnvironmentOrCache) \
  V(UnsafeGetLongVolatile, kDirect, kNoEnvironmentOrCache) \
  V(UnsafePut, kDirect, kNoEnvironmentOrCache) \
  V(UnsafePutOrdered, kDirect, kNoEnvironmentOrCache) \
  V(UnsafePutVolatile, kDirect, kNoEnvironmentOrCache) \
  V(UnsafePutObject, kDirect, kNoEnvironmentOrCache) \
  V(UnsafePutObjectOrdered, kDirect, kNoEnvironmentOrCache) \
  V(UnsafePutObjectVolatile, kDirect, kNoEnvironmentOrCache) \
  V(UnsafePutLong, kDirect, kNoEnvironmentOrCache) \
  V(UnsafePutLongOrdered, kDirect, kNoEnvironmentOrCache) \
  V(UnsafePutLongVolatile, kDirect, kNoEnvironmentOrCache) \
  V(ReferenceGetReferent, kDirect, kNoEnvironmentOrCache)

#endif  // ART_COMPILER_OPTIMIZING_INTRINSICS_LIST_H_
#undef ART_COMPILER_OPTIMIZING_INTRINSICS_LIST_H_   // #define is only for lint.
