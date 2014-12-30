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

// All intrinsics supported by the optimizing compiler. Format is name, then whether it is
// static-or-direct.

#define INTRINSICS_LIST(V) \
  V(DoubleDoubleToRawLongBits, true) \
  V(DoubleLongBitsToDouble, true) \
  V(FloatFloatToRawIntBits, true) \
  V(FloatIntBitsToFloat, true) \
  V(IntegerReverse, true) \
  V(IntegerReverseBytes, true) \
  V(LongReverse, true) \
  V(LongReverseBytes, true) \
  V(ShortReverseBytes, true) \
  V(MathAbsDouble, true) \
  V(MathAbsFloat, true) \
  V(MathAbsLong, true) \
  V(MathAbsInt, true) \
  V(MathMinDoubleDouble, true) \
  V(MathMinFloatFloat, true) \
  V(MathMinLongLong, true) \
  V(MathMinIntInt, true) \
  V(MathMaxDoubleDouble, true) \
  V(MathMaxFloatFloat, true) \
  V(MathMaxLongLong, true) \
  V(MathMaxIntInt, true) \
  V(MathSqrt, true) \
  V(MathCeil, true) \
  V(MathFloor, true) \
  V(MathRint, true) \
  V(MathRoundDouble, true) \
  V(MathRoundFloat, true) \
  V(SystemArrayCopyChar, true) \
  V(ThreadCurrentThread, true) \
  V(MemoryPeekByte, true) \
  V(MemoryPeekIntNative, true) \
  V(MemoryPeekLongNative, true) \
  V(MemoryPeekShortNative, true) \
  V(MemoryPokeByte, true) \
  V(MemoryPokeIntNative, true) \
  V(MemoryPokeLongNative, true) \
  V(MemoryPokeShortNative, true) \
  V(StringCharAt, true) \
  V(StringCompareTo, true) \
  V(StringIsEmpty, true) \
  V(StringIndexOf, true) \
  V(StringIndexOfAfter, true) \
  V(StringLength, true) \
  V(UnsafeCASInt, true) \
  V(UnsafeCASLong, true) \
  V(UnsafeCASObject, true) \
  V(UnsafeGet, true) \
  V(UnsafeGetVolatile, true) \
  V(UnsafeGetLong, true) \
  V(UnsafeGetLongVolatile, true) \
  V(UnsafePut, true) \
  V(UnsafePutOrdered, true) \
  V(UnsafePutVolatile, true) \
  V(UnsafePutObject, true) \
  V(UnsafePutObjectOrdered, true) \
  V(UnsafePutObjectVolatile, true) \
  V(UnsafePutLong, true) \
  V(UnsafePutLongOrdered, true) \
  V(UnsafePutLongVolatile, true) \
  \
  V(ReferenceGetReferent, false)

#endif  // ART_COMPILER_OPTIMIZING_INTRINSICS_LIST_H_
#undef ART_COMPILER_OPTIMIZING_INTRINSICS_LIST_H_   // #define is only for lint.
