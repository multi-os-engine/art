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

#ifndef ART_COMPILER_OPTIMIZING_INTRINSICS_LIST_H_
#define ART_COMPILER_OPTIMIZING_INTRINSICS_LIST_H_

// All intrinsics supported by the optimizing compiler. Format is name, then args.

#define STATIC_INTRINSICS_LIST(V) \
  V(DoubleDoubleToRawLongBits) \
  V(DoubleLongBitsToDouble) \
  V(FloatFloatToRawIntBits) \
  V(FloatIntBitsToFloat) \
  V(IntegerReverse) \
  V(IntegerReverseBytes) \
  V(LongReverse) \
  V(LongReverseBytes) \
  V(ShortReverseBytes) \
  V(MathAbsDouble) \
  V(MathAbsFloat) \
  V(MathAbsLong) \
  V(MathAbsInt) \
  V(MathMinDoubleDouble) \
  V(MathMinFloatFloat) \
  V(MathMinLongLong) \
  V(MathMinIntInt) \
  V(MathMaxDoubleDouble) \
  V(MathMaxFloatFloat) \
  V(MathMaxLongLong) \
  V(MathMaxIntInt) \
  V(MathSqrt) \
  V(MathCeil) \
  V(MathFloor) \
  V(MathRint) \
  V(MathRoundDouble) \
  V(MathRoundFloat) \
  V(SystemArrayCopyChar) \
  V(ThreadCurrentThread) \
  V(MemoryPeekByte) \
  V(MemoryPeekIntNative) \
  V(MemoryPeekLongNative) \
  V(MemoryPeekShortNative) \
  V(MemoryPokeByte) \
  V(MemoryPokeIntNative) \
  V(MemoryPokeLongNative) \
  V(MemoryPokeShortNative) \
  V(StringCharAt) \
  V(StringCompareTo) \
  V(StringIsEmpty) \
  V(StringIndexOf) \
  V(StringIndexOfAfter) \
  V(StringLength) \
  V(UnsafeCASInt) \
  V(UnsafeCASLong) \
  V(UnsafeCASObject) \
  V(UnsafeGet) \
  V(UnsafeGetVolatile) \
  V(UnsafeGetLong) \
  V(UnsafeGetLongVolatile) \
  V(UnsafePut) \
  V(UnsafePutOrdered) \
  V(UnsafePutVolatile) \
  V(UnsafePutObject) \
  V(UnsafePutObjectOrdered) \
  V(UnsafePutObjectVolatile) \
  V(UnsafePutLong) \
  V(UnsafePutLongOrdered) \
  V(UnsafePutLongVolatile)

#define VIRTUAL_INTRINSICS_LIST(V) \
  V(ReferenceGetReferent)

#endif  // ART_COMPILER_OPTIMIZING_INTRINSICS_LIST_H_
#undef ART_COMPILER_OPTIMIZING_INTRINSICS_LIST_H_   // #define is only for lint.
