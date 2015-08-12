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
  V(DoubleDoubleToRawLongBits, kStatic, kNeedsEnv) \
  V(DoubleLongBitsToDouble, kStatic, kNeedsEnv) \
  V(FloatFloatToRawIntBits, kStatic, kNeedsEnv) \
  V(FloatIntBitsToFloat, kStatic, kNeedsEnv) \
  V(IntegerReverse, kStatic, kNeedsEnv) \
  V(IntegerReverseBytes, kStatic, kNeedsEnv) \
  V(LongReverse, kStatic, kNeedsEnv) \
  V(LongReverseBytes, kStatic, kNeedsEnv) \
  V(ShortReverseBytes, kStatic, kNeedsEnv) \
  V(MathAbsDouble, kStatic, kNeedsEnv) \
  V(MathAbsFloat, kStatic, kNeedsEnv) \
  V(MathAbsLong, kStatic, kNeedsEnv) \
  V(MathAbsInt, kStatic, kNeedsEnv) \
  V(MathMinDoubleDouble, kStatic, kNeedsEnv) \
  V(MathMinFloatFloat, kStatic, kNeedsEnv) \
  V(MathMinLongLong, kStatic, kNeedsEnv) \
  V(MathMinIntInt, kStatic, kNeedsEnv) \
  V(MathMaxDoubleDouble, kStatic, kNeedsEnv) \
  V(MathMaxFloatFloat, kStatic, kNeedsEnv) \
  V(MathMaxLongLong, kStatic, kNeedsEnv) \
  V(MathMaxIntInt, kStatic, kNeedsEnv) \
  V(MathSqrt, kStatic, kNeedsEnv) \
  V(MathCeil, kStatic, kNeedsEnv) \
  V(MathFloor, kStatic, kNeedsEnv) \
  V(MathRint, kStatic, kNeedsEnv) \
  V(MathRoundDouble, kStatic, kNeedsEnv) \
  V(MathRoundFloat, kStatic, kNeedsEnv) \
  V(SystemArrayCopyChar, kStatic, kNeedsEnv) \
  V(ThreadCurrentThread, kStatic, kNeedsEnv) \
  V(MemoryPeekByte, kStatic, kNeedsEnv) \
  V(MemoryPeekIntNative, kStatic, kNeedsEnv) \
  V(MemoryPeekLongNative, kStatic, kNeedsEnv) \
  V(MemoryPeekShortNative, kStatic, kNeedsEnv) \
  V(MemoryPokeByte, kStatic, kNeedsEnv) \
  V(MemoryPokeIntNative, kStatic, kNeedsEnv) \
  V(MemoryPokeLongNative, kStatic, kNeedsEnv) \
  V(MemoryPokeShortNative, kStatic, kNeedsEnv) \
  V(StringCharAt, kDirect, kNeedsEnv) \
  V(StringCompareTo, kDirect, kNeedsEnv) \
  V(StringGetCharsNoCheck, kDirect, kNeedsEnv) \
  V(StringIndexOf, kDirect, kNeedsEnv) \
  V(StringIndexOfAfter, kDirect, kNeedsEnv) \
  V(StringNewStringFromBytes, kStatic, kNeedsEnv) \
  V(StringNewStringFromChars, kStatic, kNeedsEnv) \
  V(StringNewStringFromString, kStatic, kNeedsEnv) \
  V(UnsafeCASInt, kDirect, kNeedsEnv) \
  V(UnsafeCASLong, kDirect, kNeedsEnv) \
  V(UnsafeCASObject, kDirect, kNeedsEnv) \
  V(UnsafeGet, kDirect, kNeedsEnv) \
  V(UnsafeGetVolatile, kDirect, kNeedsEnv) \
  V(UnsafeGetObject, kDirect, kNeedsEnv) \
  V(UnsafeGetObjectVolatile, kDirect, kNeedsEnv) \
  V(UnsafeGetLong, kDirect, kNeedsEnv) \
  V(UnsafeGetLongVolatile, kDirect, kNeedsEnv) \
  V(UnsafePut, kDirect, kNeedsEnv) \
  V(UnsafePutOrdered, kDirect, kNeedsEnv) \
  V(UnsafePutVolatile, kDirect, kNeedsEnv) \
  V(UnsafePutObject, kDirect, kNeedsEnv) \
  V(UnsafePutObjectOrdered, kDirect, kNeedsEnv) \
  V(UnsafePutObjectVolatile, kDirect, kNeedsEnv) \
  V(UnsafePutLong, kDirect, kNeedsEnv) \
  V(UnsafePutLongOrdered, kDirect, kNeedsEnv) \
  V(UnsafePutLongVolatile, kDirect, kNeedsEnv) \
  V(ReferenceGetReferent, kDirect, kNeedsEnv)

#endif  // ART_COMPILER_OPTIMIZING_INTRINSICS_LIST_H_
#undef ART_COMPILER_OPTIMIZING_INTRINSICS_LIST_H_   // #define is only for lint.
