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

#ifndef ART_COMPILER_DRIVER_COMPILED_METHOD_STORAGE_H_
#define ART_COMPILER_DRIVER_COMPILED_METHOD_STORAGE_H_

#include <iosfwd>
#include <memory>

#include "base/macros.h"
#include "length_prefixed_array.h"
#include "utils/array_ref.h"
#include "utils/dedupe_set.h"
#include "utils/swap_space.h"

namespace art {

class LinkerPatch;
class SrcMapElem;

class CompiledMethodStorage {
 public:
  explicit CompiledMethodStorage(int swap_fd);
  ~CompiledMethodStorage();

  void DumpMemoryUsage(std::ostream& os, bool extended) const;

  void SetDedupeEnabled(bool dedupe_enabled) {
    dedupe_enabled_ = dedupe_enabled;
  }
  bool DedupeEnabled() const {
    return dedupe_enabled_;
  }

  SwapAllocator<void>& GetSwapSpaceAllocator() {
    return *swap_space_allocator_.get();
  }

  const LengthPrefixedArray<uint8_t>* DeduplicateCode(const ArrayRef<const uint8_t>& code);
  void ReleaseCode(const LengthPrefixedArray<uint8_t>* code);

  const LengthPrefixedArray<SrcMapElem>* DeduplicateSrcMappingTable(
      const ArrayRef<const SrcMapElem>& src_map);
  void ReleaseSrcMappingTable(const LengthPrefixedArray<SrcMapElem>* src_map);

  const LengthPrefixedArray<uint8_t>* DeduplicateMappingTable(const ArrayRef<const uint8_t>& table);
  void ReleaseMappingTable(const LengthPrefixedArray<uint8_t>* table);

  const LengthPrefixedArray<uint8_t>* DeduplicateVMapTable(const ArrayRef<const uint8_t>& table);
  void ReleaseVMapTable(const LengthPrefixedArray<uint8_t>* table);

  const LengthPrefixedArray<uint8_t>* DeduplicateGCMap(const ArrayRef<const uint8_t>& gc_map);
  void ReleaseGCMap(const LengthPrefixedArray<uint8_t>* gc_map);

  const LengthPrefixedArray<uint8_t>* DeduplicateCFIInfo(const ArrayRef<const uint8_t>& cfi_info);
  void ReleaseCFIInfo(const LengthPrefixedArray<uint8_t>* cfi_info);

  const LengthPrefixedArray<LinkerPatch>* DeduplicateLinkerPatches(
      const ArrayRef<const LinkerPatch>& linker_patches);
  void ReleaseLinkerPatches(const LengthPrefixedArray<LinkerPatch>* linker_patches);

 private:
  static constexpr bool kUseMurmur3Hash = true;

  // DeDuplication data structures.
  template <typename ContentType>
  class DedupeHashFunc {
   public:
    size_t operator()(const ArrayRef<ContentType>& array) const {
      const uint8_t* data = reinterpret_cast<const uint8_t*>(array.data());
      // TODO: More reasonable assertion.
      // static_assert(IsPowerOfTwo(sizeof(ContentType)),
      //    "ContentType is not power of two, don't know whether array layout is as assumed");
      uint32_t len = sizeof(ContentType) * array.size();
      if (kUseMurmur3Hash) {
        static constexpr uint32_t c1 = 0xcc9e2d51;
        static constexpr uint32_t c2 = 0x1b873593;
        static constexpr uint32_t r1 = 15;
        static constexpr uint32_t r2 = 13;
        static constexpr uint32_t m = 5;
        static constexpr uint32_t n = 0xe6546b64;

        uint32_t hash = 0;

        const int nblocks = len / 4;
        typedef __attribute__((__aligned__(1))) uint32_t unaligned_uint32_t;
        const unaligned_uint32_t *blocks = reinterpret_cast<const uint32_t*>(data);
        int i;
        for (i = 0; i < nblocks; i++) {
          uint32_t k = blocks[i];
          k *= c1;
          k = (k << r1) | (k >> (32 - r1));
          k *= c2;

          hash ^= k;
          hash = ((hash << r2) | (hash >> (32 - r2))) * m + n;
        }

        const uint8_t *tail = reinterpret_cast<const uint8_t*>(data + nblocks * 4);
        uint32_t k1 = 0;

        switch (len & 3) {
          case 3:
            k1 ^= tail[2] << 16;
            FALLTHROUGH_INTENDED;
          case 2:
            k1 ^= tail[1] << 8;
            FALLTHROUGH_INTENDED;
          case 1:
            k1 ^= tail[0];

            k1 *= c1;
            k1 = (k1 << r1) | (k1 >> (32 - r1));
            k1 *= c2;
            hash ^= k1;
        }

        hash ^= len;
        hash ^= (hash >> 16);
        hash *= 0x85ebca6b;
        hash ^= (hash >> 13);
        hash *= 0xc2b2ae35;
        hash ^= (hash >> 16);

        return hash;
      } else {
        size_t hash = 0x811c9dc5;
        for (uint32_t i = 0; i < len; ++i) {
          hash = (hash * 16777619) ^ data[i];
        }
        hash += hash << 13;
        hash ^= hash >> 7;
        hash += hash << 3;
        hash ^= hash >> 17;
        hash += hash << 5;
        return hash;
      }
    }
  };

  template <typename T>
  const LengthPrefixedArray<T>* CopyArray(const ArrayRef<const T>& array);

  template <typename T>
  void ReleaseArray(const LengthPrefixedArray<T>* array);

  template <typename T, typename DedupeSetType>
  const LengthPrefixedArray<T>* AllocateOrDeduplicateArray(const ArrayRef<const T>& data,
                                                           DedupeSetType* dedupe_set);

  template <typename T>
  void ReleaseArrayIfNotDeduplicated(const LengthPrefixedArray<T>* array);

  template <typename T>
  class LengthPrefixedArrayAlloc {
   public:
    explicit LengthPrefixedArrayAlloc(CompiledMethodStorage* compiler_driver)
        : compiled_method_storage_(compiler_driver) {
    }

    const LengthPrefixedArray<T>* Copy(const ArrayRef<const T>& array) {
      return compiled_method_storage_->CopyArray(array);
    }

    void Destroy(const LengthPrefixedArray<T>* array) {
      compiled_method_storage_->ReleaseArray(array);
    }

   private:
    CompiledMethodStorage* compiled_method_storage_;
  };

  template <typename T>
  using ArrayDedupeSet = DedupeSet<ArrayRef<const T>,
                                   LengthPrefixedArray<T>,
                                   LengthPrefixedArrayAlloc<T>,
                                   size_t,
                                   DedupeHashFunc<const T>,
                                   4>;

  // Swap pool and allocator used for native allocations. May be file-backed. Needs to be first
  // as other fields rely on this.
  std::unique_ptr<SwapSpace> swap_space_;
  std::unique_ptr<SwapAllocator<void>> swap_space_allocator_;

  bool dedupe_enabled_;

  ArrayDedupeSet<uint8_t> dedupe_code_;
  ArrayDedupeSet<SrcMapElem> dedupe_src_mapping_table_;
  ArrayDedupeSet<uint8_t> dedupe_mapping_table_;
  ArrayDedupeSet<uint8_t> dedupe_vmap_table_;
  ArrayDedupeSet<uint8_t> dedupe_gc_map_;
  ArrayDedupeSet<uint8_t> dedupe_cfi_info_;
  ArrayDedupeSet<LinkerPatch> dedupe_linker_patches_;

  DISALLOW_COPY_AND_ASSIGN(CompiledMethodStorage);
};

}  // namespace art

#endif  // ART_COMPILER_DRIVER_COMPILED_METHOD_STORAGE_H_
