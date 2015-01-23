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

#ifndef ART_RUNTIME_GC_ACCOUNTING_BITMAP_H_
#define ART_RUNTIME_GC_ACCOUNTING_BITMAP_H_

#include <limits.h>
#include <stdint.h>
#include <memory>
#include <set>
#include <vector>

#include "base/mutex.h"
#include "globals.h"
#include "object_callbacks.h"

namespace art {

class MemMap;

namespace gc {
namespace accounting {

class Bitmap {
 public:
  // Create and initialize a bitmap with size num_bits. Storage is allocated with a MemMap.
  static Bitmap* Create(const std::string& name, size_t num_bits);

  // Initialize a space bitmap using the provided mem_map as the live bits. Takes ownership of the
  // mem map. The address range covered starts at heap_begin and is of size equal to heap_capacity.
  // Objects are kAlignement-aligned.
  static Bitmap* CreateFromMemMap(MemMap* mem_map, size_t num_bits);

  // offset is the difference from base to a index.
  static ALWAYS_INLINE constexpr size_t BitIndexToWordIndex(uintptr_t offset) {
    return offset / kBitsPerBitmapWord;
  }

  template<typename T>
  static ALWAYS_INLINE constexpr T WordIndexToBitIndex(T word_index) {
    return static_cast<T>(word_index * kBitsPerBitmapWord);
  }

  static ALWAYS_INLINE constexpr uintptr_t BitIndexToMask(uintptr_t bit_index) {
    return static_cast<uintptr_t>(1) << (bit_index % kBitsPerBitmapWord);
  }

  ALWAYS_INLINE bool Set(size_t bit_index) {
    return Modify<true>(bit_index);
  }

  ALWAYS_INLINE bool Clear(size_t bit_index) {
    return Modify<false>(bit_index);
  }

  // Returns true if the object was previously marked.
  bool AtomicTestAndSet(size_t bit_index);

  // Fill the bitmap with zeroes.  Returns the bitmap's memory to the system as a side-effect.
  void Clear();

  bool Test(size_t bit_index) const;

  // Visit the all the set bits range [visit_begin, visit_end) where visit_begin and visit_end are
  // bit indices visitor is called with the index of each set bit.
  template <typename Visitor>
  void VisitSetBits(uintptr_t visit_begin, size_t visit_end, const Visitor& visitor) const;

  void CopyFrom(Bitmap* source_bitmap);

  // Starting address of our internal storage.
  uintptr_t* Begin() {
    return bitmap_begin_;
  }

  // Size of our bitmap in bits.
  size_t BitmapSize() const {
    return bitmap_size_;
  }

  // Check that a bit index is valid with a DCHECK.
  ALWAYS_INLINE void CheckValidBitIndex(size_t bit_index) const {
    DCHECK_LT(bit_index, BitmapSize());
  }

  std::string Dump() const;

 protected:
  static constexpr size_t kBitsPerBitmapWord = sizeof(uintptr_t) * kBitsPerByte;

  // TODO: heap_end_ is initialized so that the heap bitmap is empty, this doesn't require the -1,
  // however, we document that this is expected on heap_end_
  Bitmap(MemMap* mem_map, size_t bitmap_size);

  // Allocate the mem-map for a bitmap based on how many bits are required.
  static MemMap* AllocateMemMap(const std::string& name, size_t num_bits);

  template<bool kSetBit>
  bool Modify(uintptr_t bit_index);

  // Backing storage for bitmap.
  std::unique_ptr<MemMap> mem_map_;

  // This bitmap itself, word sized for efficiency in scanning.
  uintptr_t* const bitmap_begin_;

  // Number of bits in the bitmap.
  const size_t bitmap_size_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Bitmap);
};

// One T per kAlignment in range (start, end]
template<size_t kAlignment>
class MemoryRangeBitmap : public Bitmap {
 public:
  static MemoryRangeBitmap* Create(const std::string& name, uintptr_t cover_begin,
                                   uintptr_t cover_end);
  static MemoryRangeBitmap* CreateFromMemMap(MemMap* mem_map, uintptr_t cover_begin,
                                             size_t num_bits);

  // Beginning of the memory range that the bitmap covers.
  ALWAYS_INLINE uintptr_t CoverBegin() const {
    return cover_begin_;
  }

  // End of the memory range that the bitmap covers.
  ALWAYS_INLINE uintptr_t CoverEnd() const {
    return cover_end_;
  }

  // Return the address associated with a bit index.
  ALWAYS_INLINE uintptr_t AddrFromBitIndex(size_t bit_index) const {
    return CoverBegin() + bit_index * kAlignment;
  }

  // Return the address associated with a bit index.
  ALWAYS_INLINE uintptr_t BitIndexFromAddr(uintptr_t addr) const {
    DCHECK(HasAddress(addr)) << addr;
    return (addr - CoverBegin()) / kAlignment;
  }

  ALWAYS_INLINE bool HasAddress(const uintptr_t bit_index) const {
    return cover_begin_ <= bit_index && bit_index < cover_end_;
  }

 private:
  MemoryRangeBitmap(MemMap* mem_map, uintptr_t begin, size_t num_bits)
     : Bitmap(mem_map, num_bits), cover_begin_(begin), cover_end_(begin + kAlignment * num_bits) {
  }

  uintptr_t const cover_begin_;
  uintptr_t const cover_end_;
};

}  // namespace accounting
}  // namespace gc
}  // namespace art

#endif  // ART_RUNTIME_GC_ACCOUNTING_BITMAP_H_
