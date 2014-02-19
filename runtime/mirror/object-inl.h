/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_MIRROR_OBJECT_INL_H_
#define ART_RUNTIME_MIRROR_OBJECT_INL_H_

#include "object.h"

#include "art_field.h"
#include "art_method.h"
#include "atomic.h"
#include "array-inl.h"
#include "class.h"
#include "lock_word-inl.h"
#include "monitor.h"
#include "runtime.h"
#include "throwable.h"

namespace art {
namespace mirror {

template<bool kVerifyThis>
inline Class* Object::GetClass() {
  return GetFieldObject<Class, kVerifyThis>(OFFSET_OF_OBJECT_MEMBER(Object, klass_), false);
}

inline void Object::SetClass(Class* new_klass) {
  // new_klass may be NULL prior to class linker initialization.
  // We don't mark the card as this occurs as part of object allocation. Not all objects have
  // backing cards, such as large objects.
  // We use non transactional version since we can't undo this write. We also disable checking as
  // we may run in transaction mode here.
  SetFieldObjectWithoutWriteBarrier<false, false, false, kVerifyObjectOnWrites>(
      OFFSET_OF_OBJECT_MEMBER(Object, klass_), new_klass, false);
}

inline LockWord Object::GetLockWord() {
  return LockWord(GetField32(OFFSET_OF_OBJECT_MEMBER(Object, monitor_), true));
}

inline void Object::SetLockWord(LockWord new_val) {
  // Force use of non-transactional mode and do not check.
  SetField32<false, false>(OFFSET_OF_OBJECT_MEMBER(Object, monitor_), new_val.GetValue(), true);
}

inline bool Object::CasLockWord(LockWord old_val, LockWord new_val) {
  // Force use of non-transactional mode and do not check.
  return CasField32<false, false>(OFFSET_OF_OBJECT_MEMBER(Object, monitor_), old_val.GetValue(),
                                  new_val.GetValue());
}

inline uint32_t Object::GetLockOwnerThreadId() {
  return Monitor::GetLockOwnerThreadId(this);
}

inline mirror::Object* Object::MonitorEnter(Thread* self) {
  return Monitor::MonitorEnter(self, this);
}

inline bool Object::MonitorExit(Thread* self) {
  return Monitor::MonitorExit(self, this);
}

inline void Object::Notify(Thread* self) {
  Monitor::Notify(self, this);
}

inline void Object::NotifyAll(Thread* self) {
  Monitor::NotifyAll(self, this);
}

inline void Object::Wait(Thread* self) {
  Monitor::Wait(self, this, 0, 0, true, kWaiting);
}

inline void Object::Wait(Thread* self, int64_t ms, int32_t ns) {
  Monitor::Wait(self, this, ms, ns, true, kTimedWaiting);
}

template<bool kVerifyThis>
inline bool Object::VerifierInstanceOf(Class* klass) {
  DCHECK(klass != NULL);
  DCHECK(GetClass<kVerifyThis>() != NULL);
  return klass->IsInterface() || InstanceOf(klass);
}

template<bool kVerifyThis>
inline bool Object::InstanceOf(Class* klass) {
  DCHECK(klass != NULL);
  DCHECK(GetClass<kVerifyThis>() != NULL);
  return klass->IsAssignableFrom(GetClass<false>());
}

template<bool kVerifyThis>
inline bool Object::IsClass() {
  Class* java_lang_Class = GetClass<kVerifyThis>()->GetClass();
  return GetClass<false>() == java_lang_Class;
}

template<bool kVerifyThis>
inline Class* Object::AsClass() {
  DCHECK(IsClass<kVerifyThis>());
  return down_cast<Class*>(this);
}

template<bool kVerifyThis>
inline bool Object::IsObjectArray() {
  return IsArrayInstance<kVerifyThis>() && !GetClass<false>()->GetComponentType()->IsPrimitive();
}

template<class T, bool kVerifyThis>
inline ObjectArray<T>* Object::AsObjectArray() {
  DCHECK(IsObjectArray<kVerifyThis>());
  return down_cast<ObjectArray<T>*>(this);
}

template<bool kVerifyThis>
inline bool Object::IsArrayInstance() {
  return GetClass<kVerifyThis>()->IsArrayClass();
}

template<bool kVerifyThis>
inline bool Object::IsArtField() {
  return GetClass<kVerifyThis>()->IsArtFieldClass();
}

template<bool kVerifyThis>
inline ArtField* Object::AsArtField() SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  DCHECK(IsArtField<kVerifyThis>());
  return down_cast<ArtField*>(this);
}

template<bool kVerifyThis>
inline bool Object::IsArtMethod() {
  return GetClass<kVerifyThis>()->IsArtMethodClass();
}

template<bool kVerifyThis>
inline ArtMethod* Object::AsArtMethod() {
  DCHECK(IsArtMethod<kVerifyThis>());
  return down_cast<ArtMethod*>(this);
}

template<bool kVerifyThis>
inline bool Object::IsReferenceInstance() {
  return GetClass<kVerifyThis>()->IsReferenceClass();
}

template<bool kVerifyThis>
inline Array* Object::AsArray() {
  DCHECK(IsArrayInstance<kVerifyThis>());
  return down_cast<Array*>(this);
}

template<bool kVerifyThis>
inline BooleanArray* Object::AsBooleanArray() {
  DCHECK(GetClass<kVerifyThis>()->IsArrayClass());
  DCHECK(GetClass<false>()->GetComponentType()->IsPrimitiveBoolean());
  return down_cast<BooleanArray*>(this);
}

template<bool kVerifyThis>
inline ByteArray* Object::AsByteArray() {
  DCHECK(GetClass<kVerifyThis>()->IsArrayClass());
  DCHECK(GetClass<false>()->GetComponentType()->IsPrimitiveByte());
  return down_cast<ByteArray*>(this);
}

template<bool kVerifyThis>
inline ByteArray* Object::AsByteSizedArray() {
  DCHECK(GetClass<kVerifyThis>()->IsArrayClass());
  DCHECK(GetClass<false>()->GetComponentType()->IsPrimitiveByte() ||
         GetClass<false>()->GetComponentType()->IsPrimitiveBoolean());
  return down_cast<ByteArray*>(this);
}

template<bool kVerifyThis>
inline CharArray* Object::AsCharArray() {
  DCHECK(GetClass<kVerifyThis>()->IsArrayClass());
  DCHECK(GetClass<false>()->GetComponentType()->IsPrimitiveChar());
  return down_cast<CharArray*>(this);
}

template<bool kVerifyThis>
inline ShortArray* Object::AsShortArray() {
  DCHECK(GetClass<kVerifyThis>()->IsArrayClass());
  DCHECK(GetClass<false>()->GetComponentType()->IsPrimitiveShort());
  return down_cast<ShortArray*>(this);
}

template<bool kVerifyThis>
inline ShortArray* Object::AsShortSizedArray() {
  DCHECK(GetClass<kVerifyThis>()->IsArrayClass());
  DCHECK(GetClass<false>()->GetComponentType()->IsPrimitiveShort() ||
         GetClass<false>()->GetComponentType()->IsPrimitiveChar());
  return down_cast<ShortArray*>(this);
}

template<bool kVerifyThis>
inline IntArray* Object::AsIntArray() {
  DCHECK(GetClass<kVerifyThis>()->IsArrayClass());
  DCHECK(GetClass<false>()->GetComponentType()->IsPrimitiveInt() ||
         GetClass<false>()->GetComponentType()->IsPrimitiveFloat());
  return down_cast<IntArray*>(this);
}

template<bool kVerifyThis>
inline LongArray* Object::AsLongArray() {
  DCHECK(GetClass<kVerifyThis>()->IsArrayClass());
  DCHECK(GetClass<false>()->GetComponentType()->IsPrimitiveLong() ||
         GetClass<false>()->GetComponentType()->IsPrimitiveDouble());
  return down_cast<LongArray*>(this);
}

template<bool kVerifyThis>
inline FloatArray* Object::AsFloatArray() {
  DCHECK(GetClass<kVerifyThis>()->IsArrayClass());
  DCHECK(GetClass<false>()->GetComponentType()->IsPrimitiveFloat());
  return down_cast<FloatArray*>(this);
}

template<bool kVerifyThis>
inline DoubleArray* Object::AsDoubleArray() {
  DCHECK(GetClass<kVerifyThis>()->IsArrayClass());
  DCHECK(GetClass<false>()->GetComponentType()->IsPrimitiveDouble());
  return down_cast<DoubleArray*>(this);
}

template<bool kVerifyThis>
inline String* Object::AsString() {
  DCHECK(GetClass<kVerifyThis>()->IsStringClass());
  return down_cast<String*>(this);
}

template<bool kVerifyThis>
inline Throwable* Object::AsThrowable() {
  DCHECK(GetClass<kVerifyThis>()->IsThrowableClass());
  return down_cast<Throwable*>(this);
}

template<bool kVerifyThis>
inline bool Object::IsWeakReferenceInstance() {
  return GetClass<kVerifyThis>()->IsWeakReferenceClass();
}

template<bool kVerifyThis>
inline bool Object::IsSoftReferenceInstance() {
  return GetClass<kVerifyThis>()->IsSoftReferenceClass();
}

template<bool kVerifyThis>
inline bool Object::IsFinalizerReferenceInstance() {
  return GetClass<kVerifyThis>()->IsFinalizerReferenceClass();
}

template<bool kVerifyThis>
inline bool Object::IsPhantomReferenceInstance() {
  return GetClass<kVerifyThis>()->IsPhantomReferenceClass();
}

template<bool kVerifyThis>
inline size_t Object::SizeOf() {
  size_t result;
  if (IsArrayInstance<kVerifyThis>()) {
    result = AsArray<false>()->SizeOf<false>();
  } else if (IsClass<false>()) {
    result = AsClass<false>()->SizeOf<false>();
  } else {
    result = GetClass<false>()->GetObjectSize();
  }
  DCHECK_GE(result, sizeof(Object)) << " class=" << PrettyTypeOf(GetClass<false>());
  DCHECK(!IsArtField<false>()  || result == sizeof(ArtField));
  DCHECK(!IsArtMethod<false>() || result == sizeof(ArtMethod));
  return result;
}

template<bool kVerifyThis>
inline int32_t Object::GetField32(MemberOffset field_offset, bool is_volatile) {
  if (kVerifyThis) {
    VerifyObject(this);
  }
  const byte* raw_addr = reinterpret_cast<const byte*>(this) + field_offset.Int32Value();
  const int32_t* word_addr = reinterpret_cast<const int32_t*>(raw_addr);
  if (UNLIKELY(is_volatile)) {
    int32_t result = *(reinterpret_cast<volatile int32_t*>(const_cast<int32_t*>(word_addr)));
    QuasiAtomic::MembarLoadLoad();  // Ensure volatile loads don't re-order.
    return result;
  } else {
    return *word_addr;
  }
}

template<bool kTransactionActive, bool kCheckTransaction>
inline void Object::SetField32(MemberOffset field_offset, int32_t new_value, bool is_volatile,
                               bool this_is_valid) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteField32(this, field_offset, GetField32(field_offset, is_volatile),
                                           is_volatile);
  }
  if (this_is_valid) {
    VerifyObject(this);
  }
  byte* raw_addr = reinterpret_cast<byte*>(this) + field_offset.Int32Value();
  int32_t* word_addr = reinterpret_cast<int32_t*>(raw_addr);
  if (UNLIKELY(is_volatile)) {
    QuasiAtomic::MembarStoreStore();  // Ensure this store occurs after others in the queue.
    *word_addr = new_value;
    QuasiAtomic::MembarStoreLoad();  // Ensure this store occurs before any volatile loads.
  } else {
    *word_addr = new_value;
  }
}

template<bool kTransactionActive, bool kCheckTransaction>
inline bool Object::CasField32(MemberOffset field_offset, int32_t old_value, int32_t new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteField32(this, field_offset, old_value, true);
  }
  VerifyObject(this);
  byte* raw_addr = reinterpret_cast<byte*>(this) + field_offset.Int32Value();
  volatile int32_t* addr = reinterpret_cast<volatile int32_t*>(raw_addr);
  return __sync_bool_compare_and_swap(addr, old_value, new_value);
}

inline int64_t Object::GetField64(MemberOffset field_offset, bool is_volatile) {
  VerifyObject(this);
  const byte* raw_addr = reinterpret_cast<const byte*>(this) + field_offset.Int32Value();
  const int64_t* addr = reinterpret_cast<const int64_t*>(raw_addr);
  if (UNLIKELY(is_volatile)) {
    int64_t result = QuasiAtomic::Read64(addr);
    QuasiAtomic::MembarLoadLoad();  // Ensure volatile loads don't re-order.
    return result;
  } else {
    return *addr;
  }
}

template<bool kTransactionActive, bool kCheckTransaction>
inline void Object::SetField64(MemberOffset field_offset, int64_t new_value, bool is_volatile,
                               bool this_is_valid) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteField64(this, field_offset, GetField64(field_offset, is_volatile),
                                           is_volatile);
  }
  if (this_is_valid) {
    VerifyObject(this);
  }
  byte* raw_addr = reinterpret_cast<byte*>(this) + field_offset.Int32Value();
  int64_t* addr = reinterpret_cast<int64_t*>(raw_addr);
  if (UNLIKELY(is_volatile)) {
    QuasiAtomic::MembarStoreStore();  // Ensure this store occurs after others in the queue.
    QuasiAtomic::Write64(addr, new_value);
    if (!QuasiAtomic::LongAtomicsUseMutexes()) {
      QuasiAtomic::MembarStoreLoad();  // Ensure this store occurs before any volatile loads.
    } else {
      // Fence from from mutex is enough.
    }
  } else {
    *addr = new_value;
  }
}

template<bool kTransactionActive, bool kCheckTransaction>
inline bool Object::CasField64(MemberOffset field_offset, int64_t old_value, int64_t new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteField64(this, field_offset, old_value, true);
  }
  VerifyObject(this);
  byte* raw_addr = reinterpret_cast<byte*>(this) + field_offset.Int32Value();
  volatile int64_t* addr = reinterpret_cast<volatile int64_t*>(raw_addr);
  return QuasiAtomic::Cas64(old_value, new_value, addr);
}

template<class T, bool kVerifyThis, bool kVerifyResult>
inline T* Object::GetFieldObject(MemberOffset field_offset, bool is_volatile) {
  if (kVerifyThis) {
    VerifyObject(this);
  }
  byte* raw_addr = reinterpret_cast<byte*>(this) + field_offset.Int32Value();
  HeapReference<T>* objref_addr = reinterpret_cast<HeapReference<T>*>(raw_addr);
  HeapReference<T> objref = *objref_addr;

  if (UNLIKELY(is_volatile)) {
    QuasiAtomic::MembarLoadLoad();  // Ensure loads don't re-order.
  }
  T* result = objref.AsMirrorPtr();
  if (kVerifyResult && result != nullptr) {
    VerifyObject(result);
  }
  return result;
}

template<bool kTransactionActive, bool kCheckTransaction, bool kVerifyThis, bool kVerifyReference>
inline void Object::SetFieldObjectWithoutWriteBarrier(MemberOffset field_offset, Object* new_value,
                                                      bool is_volatile) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteFieldReference(this, field_offset,
                                                  GetFieldObject<Object>(field_offset, is_volatile),
                                                  true);
  }
  if (kVerifyThis) {
    VerifyObject(this);
  }
  if (kVerifyReference && new_value != nullptr) {
    VerifyObject(new_value);
  }
  HeapReference<Object> objref(HeapReference<Object>::FromMirrorPtr(new_value));
  byte* raw_addr = reinterpret_cast<byte*>(this) + field_offset.Int32Value();
  HeapReference<Object>* objref_addr = reinterpret_cast<HeapReference<Object>*>(raw_addr);
  if (UNLIKELY(is_volatile)) {
    QuasiAtomic::MembarStoreStore();  // Ensure this store occurs after others in the queue.
    objref_addr->Assign(new_value);
    QuasiAtomic::MembarStoreLoad();  // Ensure this store occurs before any loads.
  } else {
    objref_addr->Assign(new_value);
  }
}

template<bool kTransactionActive, bool kCheckTransaction, bool kVerifyThis, bool kVerifyReference>
inline void Object::SetFieldObject(MemberOffset field_offset, Object* new_value, bool is_volatile) {
  SetFieldObjectWithoutWriteBarrier<kTransactionActive, kCheckTransaction, kVerifyThis,
      kVerifyReference>(field_offset, new_value, is_volatile);
  if (new_value != nullptr) {
    CheckFieldAssignment(field_offset, new_value);
    Runtime::Current()->GetHeap()->WriteBarrierField(this, field_offset, new_value);
  }
}

template<bool kTransactionActive, bool kCheckTransaction>
inline bool Object::CasFieldObject(MemberOffset field_offset, Object* old_value, Object* new_value) {
  if (kCheckTransaction) {
    DCHECK_EQ(kTransactionActive, Runtime::Current()->IsActiveTransaction());
  }
  if (kTransactionActive) {
    Runtime::Current()->RecordWriteFieldReference(this, field_offset, old_value, true);
  }
  VerifyObject(this);
  byte* raw_addr = reinterpret_cast<byte*>(this) + field_offset.Int32Value();
  volatile int32_t* addr = reinterpret_cast<volatile int32_t*>(raw_addr);
  HeapReference<Object> old_ref(HeapReference<Object>::FromMirrorPtr(old_value));
  HeapReference<Object> new_ref(HeapReference<Object>::FromMirrorPtr(new_value));
  bool success =  __sync_bool_compare_and_swap(addr, old_ref.reference_, new_ref.reference_);
  if (success) {
    Runtime::Current()->GetHeap()->WriteBarrierField(this, field_offset, new_value);
  }
  return success;
}

inline void Object::VerifyObject(Object* obj) {
  if (kVerifyObjectOnReads || kVerifyObjectOnWrites) {
    Runtime::Current()->GetHeap()->VerifyObject(obj);
  }
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_INL_H_
