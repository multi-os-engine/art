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

#ifndef ART_COMPILER_OPTIMIZING_INTRINSICS_X86_64_H_
#define ART_COMPILER_OPTIMIZING_INTRINSICS_X86_64_H_

#include "intrinsics.h"

namespace art {

class ArenaAllocator;
class HInvokeStaticOrDirect;
class HInvokeVirtual;

namespace x86_64 {

class CodeGeneratorX86_64;
class X86_64Assembler;

class IntrinsicLocationsBuilderX86_64 : public IntrinsicVisitor {
 public:
  explicit IntrinsicLocationsBuilderX86_64(ArenaAllocator* arena) : arena_(arena) {}

  bool VisitDoubleDoubleToRawLongBits(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitDoubleLongBitsToDouble(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitFloatFloatToRawIntBits(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitFloatIntBitsToFloat(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitIntegerReverseBytes(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitLongReverseBytes(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitShortReverseBytes(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitMathAbsDouble(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathAbsFloat(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathAbsInt(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathAbsLong(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitMathMinDoubleDouble(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathMinFloatFloat(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathMinLongLong(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathMinIntInt(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitMathMaxDoubleDouble(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathMaxFloatFloat(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathMaxLongLong(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathMaxIntInt(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitMathSqrt(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitStringCharAt(HInvokeStaticOrDirect* invoke) OVERRIDE;

  // TODO: These need rep and scasw support in the assembler.
//  bool VisitStringIndexOf(HInvokeStaticOrDirect* invoke) OVERRIDE;
//  bool VisitStringIndexOfAfter(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitMemoryPeekByte(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMemoryPeekIntNative(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMemoryPeekLongNative(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMemoryPeekShortNative(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitMemoryPokeByte(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMemoryPokeIntNative(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMemoryPokeLongNative(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMemoryPokeShortNative(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitThreadCurrentThread(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitUnsafeGet(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafeGetVolatile(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafeGetLong(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafeGetLongVolatile(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitUnsafePut(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafePutOrdered(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafePutVolatile(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafePutObject(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafePutObjectOrdered(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafePutObjectVolatile(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafePutLong(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafePutLongOrdered(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafePutLongVolatile(HInvokeStaticOrDirect* invoke) OVERRIDE;

 private:
  ArenaAllocator* arena_ ATTRIBUTE_UNUSED;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicLocationsBuilderX86_64);
};

class IntrinsicCodeGeneratorX86_64 : public IntrinsicVisitor {
 public:
  explicit IntrinsicCodeGeneratorX86_64(CodeGeneratorX86_64* codegen) : codegen_(codegen) {}

  bool VisitDoubleDoubleToRawLongBits(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitDoubleLongBitsToDouble(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitFloatFloatToRawIntBits(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitFloatIntBitsToFloat(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitIntegerReverseBytes(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitLongReverseBytes(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitShortReverseBytes(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitMathAbsDouble(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathAbsFloat(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathAbsInt(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathAbsLong(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitMathMinDoubleDouble(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathMinFloatFloat(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathMinLongLong(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathMinIntInt(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitMathMaxDoubleDouble(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathMaxFloatFloat(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathMaxLongLong(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMathMaxIntInt(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitMathSqrt(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitStringCharAt(HInvokeStaticOrDirect* invoke) OVERRIDE;

  // TODO: These need rep and scasw support in the assembler.
//  bool VisitStringIndexOf(HInvokeStaticOrDirect* invoke) OVERRIDE;
//  bool VisitStringIndexOfAfter(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitMemoryPeekByte(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMemoryPeekIntNative(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMemoryPeekLongNative(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMemoryPeekShortNative(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitMemoryPokeByte(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMemoryPokeIntNative(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMemoryPokeLongNative(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitMemoryPokeShortNative(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitThreadCurrentThread(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitUnsafeGet(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafeGetVolatile(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafeGetLong(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafeGetLongVolatile(HInvokeStaticOrDirect* invoke) OVERRIDE;

  bool VisitUnsafePut(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafePutOrdered(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafePutVolatile(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafePutObject(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafePutObjectOrdered(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafePutObjectVolatile(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafePutLong(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafePutLongOrdered(HInvokeStaticOrDirect* invoke) OVERRIDE;
  bool VisitUnsafePutLongVolatile(HInvokeStaticOrDirect* invoke) OVERRIDE;

 private:
  X86_64Assembler* GetAssembler();

  ArenaAllocator* GetArena();

  CodeGeneratorX86_64* codegen_;

  DISALLOW_COPY_AND_ASSIGN(IntrinsicCodeGeneratorX86_64);
};

}  // namespace x86_64
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_INTRINSICS_X86_64_H_
