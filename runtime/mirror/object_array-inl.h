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

#ifndef ART_RUNTIME_MIRROR_OBJECT_ARRAY_INL_H_
#define ART_RUNTIME_MIRROR_OBJECT_ARRAY_INL_H_

#include "object_array.h"

#include "gc/heap.h"
#include "mirror/art_field.h"
#include "mirror/class.h"
#include "runtime.h"
#include "sirt_ref.h"
#include "thread.h"

namespace art {
namespace mirror {

template<class T>
inline ObjectArray<T>* ObjectArray<T>::Alloc(Thread* self, Class* object_array_class,
                                             int32_t length, gc::AllocatorType allocator_type) {
  Array* array = Array::Alloc<true>(self, object_array_class, length, sizeof(Object*),
                                    allocator_type);
  if (UNLIKELY(array == nullptr)) {
    return nullptr;
  } else {
    return array->AsObjectArray<T>();
  }
}

template<class T>
inline ObjectArray<T>* ObjectArray<T>::Alloc(Thread* self, Class* object_array_class,
                                             int32_t length) {
  return Alloc(self, object_array_class, length,
               Runtime::Current()->GetHeap()->GetCurrentAllocator());
}

template<class T>
inline T* ObjectArray<T>::Get(int32_t i) const {
  if (UNLIKELY(!CheckIsValidIndex(i))) {
    DCHECK(Thread::Current()->IsExceptionPending());
    return NULL;
  }
  return GetWithoutChecks(i);
}

template<class T>
inline bool ObjectArray<T>::CheckAssignable(T* object) {
  if (object != NULL) {
    Class* element_class = GetClass()->GetComponentType();
    if (UNLIKELY(!object->InstanceOf(element_class))) {
      ThrowArrayStoreException(object);
      return false;
    }
  }
  return true;
}

template<class T>
inline void ObjectArray<T>::Set(int32_t i, T* object) {
  if (LIKELY(CheckIsValidIndex(i) && CheckAssignable(object))) {
    SetWithoutChecks(i, object);
  } else {
    DCHECK(Thread::Current()->IsExceptionPending());
  }
}

template<class T>
inline void ObjectArray<T>::SetNonTransactional(int32_t i, T* object) {
  CHECK(!Runtime::Current()->IsActiveTransaction());
  if (LIKELY(CheckIsValidIndex(i) && CheckAssignable(object))) {
    MemberOffset data_offset(DataOffset(sizeof(Object*)).Int32Value() + i * sizeof(Object*));
    SetFieldObjectNonTransactional(data_offset, object, false);
  } else {
    DCHECK(Thread::Current()->IsExceptionPending());
  }
}

template<class T>
inline void ObjectArray<T>::SetWithoutChecks(int32_t i, T* object) {
  if (Runtime::Current()->IsActiveTransaction()) {
    SetWithoutChecksTransactional(i, object);
  } else {
    SetWithoutChecksNonTransactional(i, object);
  }
}

template<class T>
inline void ObjectArray<T>::SetWithoutChecksTransactional(int32_t i, T* object) {
  DCHECK(CheckIsValidIndex(i));
  DCHECK(CheckAssignable(object));
  MemberOffset data_offset(DataOffset(sizeof(Object*)).Int32Value() + i * sizeof(Object*));
  SetFieldObjectTransactional(data_offset, object, false);
}

template<class T>
inline void ObjectArray<T>::SetWithoutChecksNonTransactional(int32_t i, T* object) {
  DCHECK(CheckIsValidIndex(i));
  DCHECK(CheckAssignable(object));
  MemberOffset data_offset(DataOffset(sizeof(Object*)).Int32Value() + i * sizeof(Object*));
  SetFieldObjectNonTransactional(data_offset, object, false);
}

template<class T>
inline void ObjectArray<T>::SetPtrWithoutChecksNonTransactional(int32_t i, T* object) {
  DCHECK(CheckIsValidIndex(i));
  // TODO enable this check. It fails when writing the image in ImageWriter::FixupObjectArray.
  // DCHECK(CheckAssignable(object));
  MemberOffset data_offset(DataOffset(sizeof(Object*)).Int32Value() + i * sizeof(Object*));
  SetFieldPtrNonTransactional(data_offset, object, false);
}

template<class T>
inline T* ObjectArray<T>::GetWithoutChecks(int32_t i) const {
  DCHECK(CheckIsValidIndex(i));
  MemberOffset data_offset(DataOffset(sizeof(Object*)).Int32Value() + i * sizeof(Object*));
  return GetFieldObject<T*>(data_offset, false);
}

template<class T>
inline void ObjectArray<T>::Copy(const ObjectArray<T>* src, int src_pos,
                                 ObjectArray<T>* dst, int dst_pos,
                                 size_t length) {
  if (src->CheckIsValidIndex(src_pos) &&
      src->CheckIsValidIndex(src_pos + length - 1) &&
      dst->CheckIsValidIndex(dst_pos) &&
      dst->CheckIsValidIndex(dst_pos + length - 1)) {
    MemberOffset src_offset(DataOffset(sizeof(Object*)).Int32Value() + src_pos * sizeof(Object*));
    MemberOffset dst_offset(DataOffset(sizeof(Object*)).Int32Value() + dst_pos * sizeof(Object*));
    Class* array_class = dst->GetClass();
    gc::Heap* heap = Runtime::Current()->GetHeap();
    if (array_class == src->GetClass()) {
      // No need for array store checks if arrays are of the same type
      for (size_t i = 0; i < length; i++) {
        Object* object = src->GetFieldObject<Object*>(src_offset, false);
        heap->VerifyObject(object);
        // directly set field, we do a bulk write barrier at the end
        dst->SetField32NonTransactional(dst_offset, reinterpret_cast<uint32_t>(object), false, true);
        src_offset = MemberOffset(src_offset.Uint32Value() + sizeof(Object*));
        dst_offset = MemberOffset(dst_offset.Uint32Value() + sizeof(Object*));
      }
    } else {
      Class* element_class = array_class->GetComponentType();
      CHECK(!element_class->IsPrimitive());
      for (size_t i = 0; i < length; i++) {
        Object* object = src->GetFieldObject<Object*>(src_offset, false);
        if (object != NULL && !object->InstanceOf(element_class)) {
          dst->ThrowArrayStoreException(object);
          return;
        }
        heap->VerifyObject(object);
        // directly set field, we do a bulk write barrier at the end
        dst->SetField32NonTransactional(dst_offset, reinterpret_cast<uint32_t>(object), false, true);
        src_offset = MemberOffset(src_offset.Uint32Value() + sizeof(Object*));
        dst_offset = MemberOffset(dst_offset.Uint32Value() + sizeof(Object*));
      }
    }
    heap->WriteBarrierArray(dst, dst_pos, length);
  } else {
    DCHECK(Thread::Current()->IsExceptionPending());
  }
}

template<class T>
inline ObjectArray<T>* ObjectArray<T>::CopyOf(Thread* self, int32_t new_length) {
  // We may get copied by a compacting GC.
  SirtRef<ObjectArray<T> > sirt_this(self, this);
  gc::Heap* heap = Runtime::Current()->GetHeap();
  gc::AllocatorType allocator_type = heap->IsMovableObject(this) ? heap->GetCurrentAllocator() :
      heap->GetCurrentNonMovingAllocator();
  ObjectArray<T>* new_array = Alloc(self, GetClass(), new_length, allocator_type);
  if (LIKELY(new_array != nullptr)) {
    Copy(sirt_this.get(), 0, new_array, 0, std::min(sirt_this->GetLength(), new_length));
  }
  return new_array;
}

}  // namespace mirror
}  // namespace art

#endif  // ART_RUNTIME_MIRROR_OBJECT_ARRAY_INL_H_
