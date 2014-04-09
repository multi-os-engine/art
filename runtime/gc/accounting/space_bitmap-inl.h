/*
 * Copyright (C) 2008 The Android Open Source Project
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

#ifndef ART_RUNTIME_GC_ACCOUNTING_SPACE_BITMAP_INL_H_
#define ART_RUNTIME_GC_ACCOUNTING_SPACE_BITMAP_INL_H_

#include "space_bitmap.h"

#include "base/logging.h"
#include "dex_file-inl.h"
#include "heap_bitmap.h"
#include "mirror/art_field-inl.h"
#include "mirror/class-inl.h"
#include "mirror/object-inl.h"
#include "mirror/object_array-inl.h"
#include "object_utils.h"
#include "space_bitmap-inl.h"
#include "UniquePtr.h"
#include "utils.h"

namespace art {
namespace gc {
namespace accounting {

template<size_t kAlignment>
inline bool SpaceBitmap<kAlignment>::AtomicTestAndSet(const mirror::Object* obj) {
  uintptr_t addr = reinterpret_cast<uintptr_t>(obj);
  DCHECK_GE(addr, heap_begin_);
  const uintptr_t offset = addr - heap_begin_;
  const size_t index = OffsetToIndex(offset);
  const uword mask = OffsetToMask(offset);
  uword* const address = &bitmap_begin_[index];
  DCHECK_LT(index, bitmap_size_ / kWordSize) << " bitmap_size_ = " << bitmap_size_;
  uword old_word;
  do {
    old_word = *address;
    // Fast path: The bit is already set.
    if ((old_word & mask) != 0) {
      DCHECK(Test(obj));
      return true;
    }
  } while (!__sync_bool_compare_and_swap(address, old_word, old_word | mask));
  DCHECK(Test(obj));
  return false;
}

template<size_t kAlignment>
inline bool SpaceBitmap<kAlignment>::Test(const mirror::Object* obj) const {
  uintptr_t addr = reinterpret_cast<uintptr_t>(obj);
  DCHECK(HasAddress(obj)) << obj;
  DCHECK(bitmap_begin_ != NULL);
  DCHECK_GE(addr, heap_begin_);
  const uintptr_t offset = addr - heap_begin_;
  return (bitmap_begin_[OffsetToIndex(offset)] & OffsetToMask(offset)) != 0;
}

template<size_t kAlignment> template<typename Visitor>
void SpaceBitmap<kAlignment>::VisitMarkedRange(uintptr_t visit_begin, uintptr_t visit_end,
                                   const Visitor& visitor) const {
  DCHECK_LT(visit_begin, visit_end);
#if 0
  for (uintptr_t i = visit_begin; i < visit_end; i += kAlignment) {
    mirror::Object* obj = reinterpret_cast<mirror::Object*>(i);
    if (Test(obj)) {
      visitor(obj);
    }
  }
#else
  DCHECK_LE(heap_begin_, visit_begin);
  DCHECK_LE(visit_end, HeapLimit());

  const uintptr_t offset_start = visit_begin - heap_begin_;
  const uintptr_t offset_end = visit_end - heap_begin_;

  const uintptr_t index_start = OffsetToIndex(offset_start);
  const uintptr_t index_end = OffsetToIndex(offset_end);

  const size_t bit_start = (offset_start / kAlignment) % kBitsPerWord;
  const size_t bit_end = (offset_end / kAlignment) % kBitsPerWord;

  // Index(begin)  ...    Index(end)
  // [xxxxx???][........][????yyyy]
  //      ^                   ^
  //      |                   #---- Bit of visit_end
  //      #---- Bit of visit_begin
  //

  // Left edge.
  uword left_edge = bitmap_begin_[index_start];
  // Mark of lower bits that are not in range.
  left_edge &= ~((static_cast<uword>(1) << bit_start) - 1);

  // Right edge. Either unique, or left_edge.
  uword right_edge;

  if (index_start < index_end) {
    // Left edge != right edge.

    // Traverse left edge.
    if (left_edge != 0) {
      const uintptr_t ptr_base = IndexToOffset(index_start) + heap_begin_;
      do {
        const size_t shift = CTZ(left_edge);
        mirror::Object* obj = reinterpret_cast<mirror::Object*>(ptr_base + shift * kAlignment);
        visitor(obj);
        left_edge ^= (static_cast<uword>(1)) << shift;
      } while (left_edge != 0);
    }

    // Traverse the middle, full part.
    for (size_t i = index_start + 1; i < index_end; ++i) {
      uword w = bitmap_begin_[i];
      if (w != 0) {
        const uintptr_t ptr_base = IndexToOffset(i) + heap_begin_;
        do {
          const size_t shift = CTZ(w);
          mirror::Object* obj = reinterpret_cast<mirror::Object*>(ptr_base + shift * kAlignment);
          visitor(obj);
          w ^= (static_cast<uword>(1)) << shift;
        } while (w != 0);
      }
    }

    // Right edge is unique.
    // But maybe we don't have anything to do: visit_end starts in a new word...
    if (bit_end == 0) {
      // Do not read memory, as it could be after the end of the bitmap.
      right_edge = 0;
    } else {
      right_edge = bitmap_begin_[index_end];
    }
  } else {
    // Right edge = left edge.
    right_edge = left_edge;
  }

  // Right edge handling.
  right_edge &= ((static_cast<uword>(1) << bit_end) - 1);
  if (right_edge != 0) {
    const uintptr_t ptr_base = IndexToOffset(index_end) + heap_begin_;
    do {
      const size_t shift = CTZ(right_edge);
      mirror::Object* obj = reinterpret_cast<mirror::Object*>(ptr_base + shift * kAlignment);
      visitor(obj);
      right_edge ^= (static_cast<uword>(1)) << shift;
    } while (right_edge != 0);
  }
#endif
}

template<size_t kAlignment> template<bool kSetBit>
inline bool SpaceBitmap<kAlignment>::Modify(const mirror::Object* obj) {
  uintptr_t addr = reinterpret_cast<uintptr_t>(obj);
  DCHECK_GE(addr, heap_begin_);
  const uintptr_t offset = addr - heap_begin_;
  const size_t index = OffsetToIndex(offset);
  const uword mask = OffsetToMask(offset);
  DCHECK_LT(index, bitmap_size_ / kWordSize) << " bitmap_size_ = " << bitmap_size_;
  uword* address = &bitmap_begin_[index];
  uword old_word = *address;
  if (kSetBit) {
    *address = old_word | mask;
  } else {
    *address = old_word & ~mask;
  }
  DCHECK_EQ(Test(obj), kSetBit);
  return (old_word & mask) != 0;
}

template<size_t kAlignment>
inline std::ostream& operator << (std::ostream& stream, const SpaceBitmap<kAlignment>& bitmap) {
  return stream
    << bitmap.GetName() << "["
    << "begin=" << reinterpret_cast<const void*>(bitmap.HeapBegin())
    << ",end=" << reinterpret_cast<const void*>(bitmap.HeapLimit())
    << "]";
}

template<size_t kAlignment>
SpaceBitmap<kAlignment>* SpaceBitmap<kAlignment>::CreateFromMemMap(
    const std::string& name, MemMap* mem_map, byte* heap_begin, size_t heap_capacity) {
  CHECK(mem_map != nullptr);
  uword* bitmap_begin = reinterpret_cast<uword*>(mem_map->Begin());
  size_t bitmap_size = OffsetToIndex(RoundUp(heap_capacity, kAlignment * kBitsPerWord)) * kWordSize;
  return new SpaceBitmap(name, mem_map, bitmap_begin, bitmap_size, heap_begin);
}

template<size_t kAlignment>
SpaceBitmap<kAlignment>* SpaceBitmap<kAlignment>::Create(
    const std::string& name, byte* heap_begin, size_t heap_capacity) {
  CHECK(heap_begin != NULL);
  // Round up since heap_capacity is not necessarily a multiple of kAlignment * kBitsPerWord.
  size_t bitmap_size = OffsetToIndex(RoundUp(heap_capacity, kAlignment * kBitsPerWord)) * kWordSize;
  std::string error_msg;
  UniquePtr<MemMap> mem_map(MemMap::MapAnonymous(name.c_str(), NULL, bitmap_size,
                                                 PROT_READ | PROT_WRITE, false, &error_msg));
  if (UNLIKELY(mem_map.get() == nullptr)) {
    LOG(ERROR) << "Failed to allocate bitmap " << name << ": " << error_msg;
    return NULL;
  }
  return CreateFromMemMap(name, mem_map.release(), heap_begin, heap_capacity);
}

template<size_t kAlignment>
void SpaceBitmap<kAlignment>::SetHeapLimit(uintptr_t new_end) {
  DCHECK(IsAligned<kBitsPerWord * kAlignment>(new_end));
  size_t new_size = OffsetToIndex(new_end - heap_begin_) * kWordSize;
  if (new_size < bitmap_size_) {
    bitmap_size_ = new_size;
  }
  // Not sure if doing this trim is necessary, since nothing past the end of the heap capacity
  // should be marked.
}

template<size_t kAlignment>
void SpaceBitmap<kAlignment>::Clear() {
  if (bitmap_begin_ != NULL) {
    // This returns the memory to the system.  Successive page faults will return zeroed memory.
    int result = madvise(bitmap_begin_, bitmap_size_, MADV_DONTNEED);
    if (result == -1) {
      PLOG(FATAL) << "madvise failed";
    }
  }
}

template<size_t kAlignment>
void SpaceBitmap<kAlignment>::CopyFrom(SpaceBitmap* source_bitmap) {
  DCHECK_EQ(Size(), source_bitmap->Size());
  std::copy(source_bitmap->Begin(), source_bitmap->Begin() + source_bitmap->Size() / kWordSize, Begin());
}

template<size_t kAlignment>
void SpaceBitmap<kAlignment>::Walk(ObjectCallback* callback, void* arg) {
  CHECK(bitmap_begin_ != NULL);
  CHECK(callback != NULL);

  uintptr_t end = OffsetToIndex(HeapLimit() - heap_begin_ - 1);
  uword* bitmap_begin = bitmap_begin_;
  for (uintptr_t i = 0; i <= end; ++i) {
    uword w = bitmap_begin[i];
    if (w != 0) {
      uintptr_t ptr_base = IndexToOffset(i) + heap_begin_;
      do {
        const size_t shift = CTZ(w);
        mirror::Object* obj = reinterpret_cast<mirror::Object*>(ptr_base + shift * kAlignment);
        (*callback)(obj, arg);
        w ^= (static_cast<uword>(1)) << shift;
      } while (w != 0);
    }
  }
}

template<size_t kAlignment>
void SpaceBitmap<kAlignment>::SweepWalk(const SpaceBitmap<kAlignment>& live_bitmap,
                                        const SpaceBitmap<kAlignment>& mark_bitmap,
                                        uintptr_t sweep_begin, uintptr_t sweep_end,
                                        SpaceBitmap::SweepCallback* callback, void* arg) {
  CHECK(live_bitmap.bitmap_begin_ != NULL);
  CHECK(mark_bitmap.bitmap_begin_ != NULL);
  CHECK_EQ(live_bitmap.heap_begin_, mark_bitmap.heap_begin_);
  CHECK_EQ(live_bitmap.bitmap_size_, mark_bitmap.bitmap_size_);
  CHECK(callback != NULL);
  CHECK_LE(sweep_begin, sweep_end);
  CHECK_GE(sweep_begin, live_bitmap.heap_begin_);

  if (sweep_end <= sweep_begin) {
    return;
  }

  // TODO: rewrite the callbacks to accept a std::vector<mirror::Object*> rather than a mirror::Object**?
  const size_t buffer_size = kWordSize * kBitsPerWord;
  mirror::Object* pointer_buf[buffer_size];
  mirror::Object** pb = &pointer_buf[0];
  size_t start = OffsetToIndex(sweep_begin - live_bitmap.heap_begin_);
  size_t end = OffsetToIndex(sweep_end - live_bitmap.heap_begin_ - 1);
  CHECK_LT(end, live_bitmap.Size() / kWordSize);
  uword* live = live_bitmap.bitmap_begin_;
  uword* mark = mark_bitmap.bitmap_begin_;
  for (size_t i = start; i <= end; i++) {
    uword garbage = live[i] & ~mark[i];
    if (UNLIKELY(garbage != 0)) {
      uintptr_t ptr_base = IndexToOffset(i) + live_bitmap.heap_begin_;
      do {
        const size_t shift = CTZ(garbage);
        garbage ^= (static_cast<uword>(1)) << shift;
        *pb++ = reinterpret_cast<mirror::Object*>(ptr_base + shift * kAlignment);
      } while (garbage != 0);
      // Make sure that there are always enough slots available for an
      // entire word of one bits.
      if (pb >= &pointer_buf[buffer_size - kBitsPerWord]) {
        (*callback)(pb - &pointer_buf[0], &pointer_buf[0], arg);
        pb = &pointer_buf[0];
      }
    }
  }
  if (pb > &pointer_buf[0]) {
    (*callback)(pb - &pointer_buf[0], &pointer_buf[0], arg);
  }
}

template<size_t kAlignment>
void SpaceBitmap<kAlignment>::WalkInstanceFields(SpaceBitmap<kAlignment>* visited,
                                                 ObjectCallback* callback, mirror::Object* obj,
                                                 mirror::Class* klass, void* arg)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  // Visit fields of parent classes first.
  mirror::Class* super = klass->GetSuperClass();
  if (super != NULL) {
    WalkInstanceFields(visited, callback, obj, super, arg);
  }
  // Walk instance fields
  mirror::ObjectArray<mirror::ArtField>* fields = klass->GetIFields();
  if (fields != NULL) {
    for (int32_t i = 0; i < fields->GetLength(); i++) {
      mirror::ArtField* field = fields->Get(i);
      FieldHelper fh(field);
      if (!fh.IsPrimitiveType()) {
        mirror::Object* value = field->GetObj(obj);
        if (value != NULL) {
          WalkFieldsInOrder(visited, callback, value,  arg);
        }
      }
    }
  }
}

template<size_t kAlignment>
void SpaceBitmap<kAlignment>::WalkFieldsInOrder(SpaceBitmap<kAlignment>* visited,
                                                ObjectCallback* callback, mirror::Object* obj,
                                                void* arg) {
  if (visited->Test(obj)) {
    return;
  }
  // visit the object itself
  (*callback)(obj, arg);
  visited->Set(obj);
  // Walk instance fields of all objects
  mirror::Class* klass = obj->GetClass();
  WalkInstanceFields(visited, callback, obj, klass, arg);
  // Walk static fields of a Class
  if (obj->IsClass()) {
    mirror::ObjectArray<mirror::ArtField>* fields = klass->GetSFields();
    if (fields != NULL) {
      for (int32_t i = 0; i < fields->GetLength(); i++) {
        mirror::ArtField* field = fields->Get(i);
        FieldHelper fh(field);
        if (!fh.IsPrimitiveType()) {
          mirror::Object* value = field->GetObj(NULL);
          if (value != NULL) {
            WalkFieldsInOrder(visited, callback, value, arg);
          }
        }
      }
    }
  } else if (obj->IsObjectArray()) {
    // Walk elements of an object array
    mirror::ObjectArray<mirror::Object>* obj_array = obj->AsObjectArray<mirror::Object>();
    int32_t length = obj_array->GetLength();
    for (int32_t i = 0; i < length; i++) {
      mirror::Object* value = obj_array->Get(i);
      if (value != NULL) {
        WalkFieldsInOrder(visited, callback, value, arg);
      }
    }
  }
}

template<size_t kAlignment>
void SpaceBitmap<kAlignment>::InOrderWalk(ObjectCallback* callback, void* arg) {
  UniquePtr<SpaceBitmap<kAlignment>> visited(
      Create("bitmap for in-order walk", reinterpret_cast<byte*>(heap_begin_),
             IndexToOffset(bitmap_size_ / kWordSize)));
  CHECK(bitmap_begin_ != nullptr);
  CHECK(callback != nullptr);
  uintptr_t end = Size() / kWordSize;
  for (uintptr_t i = 0; i < end; ++i) {
    // Need uint for unsigned shift.
    uword w = bitmap_begin_[i];
    if (UNLIKELY(w != 0)) {
      uintptr_t ptr_base = IndexToOffset(i) + heap_begin_;
      while (w != 0) {
        const size_t shift = CTZ(w);
        mirror::Object* obj = reinterpret_cast<mirror::Object*>(ptr_base + shift * kAlignment);
        WalkFieldsInOrder(visited.get(), callback, obj, arg);
        w ^= (static_cast<uword>(1)) << shift;
      }
    }
  }
}

}  // namespace accounting
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ACCOUNTING_SPACE_BITMAP_INL_H_
