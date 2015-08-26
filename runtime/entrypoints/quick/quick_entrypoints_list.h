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

#ifndef ART_RUNTIME_ENTRYPOINTS_QUICK_QUICK_ENTRYPOINTS_LIST_H_
#define ART_RUNTIME_ENTRYPOINTS_QUICK_QUICK_ENTRYPOINTS_LIST_H_

// All quick entrypoints.  The format
//    V(name, can trigger gc, return type, argument types)

#define QUICK_ENTRYPOINT_LIST(V) \
  V(AllocArray, true, void*, uint32_t, int32_t, ArtMethod*) \
  V(AllocArrayResolved, true, void*, mirror::Class*, int32_t, ArtMethod*) \
  V(AllocArrayWithAccessCheck, true, void*, uint32_t, int32_t, ArtMethod*) \
  V(AllocObject, true, void*, uint32_t, ArtMethod*) \
  V(AllocObjectResolved, true, void*, mirror::Class*, ArtMethod*) \
  V(AllocObjectInitialized, true, void*, mirror::Class*, ArtMethod*) \
  V(AllocObjectWithAccessCheck, true, void*, uint32_t, ArtMethod*) \
  V(CheckAndAllocArray, true, void*, uint32_t, int32_t, ArtMethod*) \
  V(CheckAndAllocArrayWithAccessCheck, true, void*, uint32_t, int32_t, ArtMethod*) \
  V(AllocStringFromBytes, true, void*, void*, int32_t, int32_t, int32_t) \
  V(AllocStringFromChars, true, void*, int32_t, int32_t, void*) \
  V(AllocStringFromString, true, void*, void*) \
\
  V(InstanceofNonTrivial, true, uint32_t, const mirror::Class*, const mirror::Class*) \
  V(CheckCast, true, void, const mirror::Class*, const mirror::Class*) \
\
  V(InitializeStaticStorage, true, void*, uint32_t) \
  V(InitializeTypeAndVerifyAccess, true, void*, uint32_t) \
  V(InitializeType, true, void*, uint32_t) \
  V(ResolveString, true, void*, uint32_t) \
\
  V(Set8Instance, true, int, uint32_t, void*, int8_t) \
  V(Set8Static, true, int, uint32_t, int8_t) \
  V(Set16Instance, true, int, uint32_t, void*, int16_t) \
  V(Set16Static, true, int, uint32_t, int16_t) \
  V(Set32Instance, true, int, uint32_t, void*, int32_t) \
  V(Set32Static, true, int, uint32_t, int32_t) \
  V(Set64Instance, true, int, uint32_t, void*, int64_t) \
  V(Set64Static, true, int, uint32_t, int64_t) \
  V(SetObjInstance, true, int, uint32_t, void*, void*) \
  V(SetObjStatic, true, int, uint32_t, void*) \
  V(GetByteInstance, true, int8_t, uint32_t, void*) \
  V(GetBooleanInstance, true, uint8_t, uint32_t, void*) \
  V(GetByteStatic, true, int8_t, uint32_t) \
  V(GetBooleanStatic, true, uint8_t, uint32_t) \
  V(GetShortInstance, true, int16_t, uint32_t, void*) \
  V(GetCharInstance, true, uint16_t, uint32_t, void*) \
  V(GetShortStatic, true, int16_t, uint32_t) \
  V(GetCharStatic, true, uint16_t, uint32_t) \
  V(Get32Instance, true, int32_t, uint32_t, void*) \
  V(Get32Static, true, int32_t, uint32_t) \
  V(Get64Instance, true, int64_t, uint32_t, void*) \
  V(Get64Static, true, int64_t, uint32_t) \
  V(GetObjInstance, true, void*, uint32_t, void*) \
  V(GetObjStatic, true, void*, uint32_t) \
\
  V(AputObjectWithNullAndBoundCheck, true, void, mirror::Array*, int32_t, mirror::Object*) \
  V(AputObjectWithBoundCheck, true, void, mirror::Array*, int32_t, mirror::Object*) \
  V(AputObject, true, void, mirror::Array*, int32_t, mirror::Object*) \
  V(HandleFillArrayData, true, void, void*, void*) \
\
  V(JniMethodStart, true, uint32_t, Thread*) \
  V(JniMethodStartSynchronized, true, uint32_t, jobject, Thread*) \
  V(JniMethodEnd, true, void, uint32_t, Thread*) \
  V(JniMethodEndSynchronized, true, void, uint32_t, jobject, Thread*) \
  V(JniMethodEndWithReference, true, mirror::Object*, jobject, uint32_t, Thread*) \
  V(JniMethodEndWithReferenceSynchronized, \
    true, mirror::Object*, jobject, uint32_t, jobject, Thread*) \
  V(QuickGenericJniTrampoline, true, void, ArtMethod*) \
\
  V(LockObject, true, void, mirror::Object*) \
  V(UnlockObject, true, void, mirror::Object*) \
\
  V(CmpgDouble, false, int32_t, double, double) \
  V(CmpgFloat, false, int32_t, float, float) \
  V(CmplDouble, false, int32_t, double, double) \
  V(CmplFloat, false, int32_t, float, float) \
  V(Fmod, false, double, double, double) \
  V(L2d, false, double, int64_t) \
  V(Fmodf, false, float, float, float) \
  V(L2f, false, float, int64_t) \
  V(D2iz, false, int32_t, double) \
  V(F2iz, false, int32_t, float) \
  V(Idivmod, false, int32_t, int32_t, int32_t) \
  V(D2l, false, int64_t, double) \
  V(F2l, false, int64_t, float) \
  V(Ldiv, false, int64_t, int64_t, int64_t) \
  V(Lmod, false, int64_t, int64_t, int64_t) \
  V(Lmul, false, int64_t, int64_t, int64_t) \
  V(ShlLong, false, uint64_t, uint64_t, uint32_t) \
  V(ShrLong, false, uint64_t, uint64_t, uint32_t) \
  V(UshrLong, false, uint64_t, uint64_t, uint32_t) \
\
  V(IndexOf, true, int32_t, void*, uint32_t, uint32_t, uint32_t) \
  V(StringCompareTo, true, int32_t, void*, void*) \
  V(Memcpy, true, void*, void*, const void*, size_t) \
\
  V(QuickImtConflictTrampoline, true, void, ArtMethod*) \
  V(QuickResolutionTrampoline, true, void, ArtMethod*) \
  V(QuickToInterpreterBridge, true, void, ArtMethod*) \
  V(InvokeDirectTrampolineWithAccessCheck, true, void, uint32_t, void*) \
  V(InvokeInterfaceTrampolineWithAccessCheck, true, void, uint32_t, void*) \
  V(InvokeStaticTrampolineWithAccessCheck, true, void, uint32_t, void*) \
  V(InvokeSuperTrampolineWithAccessCheck, true, void, uint32_t, void*) \
  V(InvokeVirtualTrampolineWithAccessCheck, true, void, uint32_t, void*) \
\
  V(TestSuspend, true, void, void) \
\
  V(DeliverException, true, void, mirror::Object*) \
  V(ThrowArrayBounds, true, void, int32_t, int32_t) \
  V(ThrowDivZero, true, void, void) \
  V(ThrowNoSuchMethod, true, void, int32_t) \
  V(ThrowNullPointer, true, void, void) \
  V(ThrowStackOverflow, true, void, void*) \
  V(Deoptimize, true, void, void) \
\
  V(A64Load, true, int64_t, volatile const int64_t *) \
  V(A64Store, true, void, volatile int64_t *, int64_t) \
\
  V(NewEmptyString, true, void) \
  V(NewStringFromBytes_B, true, void) \
  V(NewStringFromBytes_BI, true, void) \
  V(NewStringFromBytes_BII, true, void) \
  V(NewStringFromBytes_BIII, true, void) \
  V(NewStringFromBytes_BIIString, true, void) \
  V(NewStringFromBytes_BString, true, void) \
  V(NewStringFromBytes_BIICharset, true, void) \
  V(NewStringFromBytes_BCharset, true, void) \
  V(NewStringFromChars_C, true, void) \
  V(NewStringFromChars_CII, true, void) \
  V(NewStringFromChars_IIC, true, void) \
  V(NewStringFromCodePoints, true, void) \
  V(NewStringFromString, true, void) \
  V(NewStringFromStringBuffer, true, void) \
  V(NewStringFromStringBuilder, true, void) \
\
  V(ReadBarrierJni, true, void, mirror::CompressedReference<mirror::Object>*, Thread*) \
  V(ReadBarrierSlow, true, mirror::Object*, mirror::Object*, mirror::Object*, uint32_t)

#endif  // ART_RUNTIME_ENTRYPOINTS_QUICK_QUICK_ENTRYPOINTS_LIST_H_
#undef ART_RUNTIME_ENTRYPOINTS_QUICK_QUICK_ENTRYPOINTS_LIST_H_   // #define is only for lint.
