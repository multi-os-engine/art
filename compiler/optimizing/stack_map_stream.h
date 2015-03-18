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

#ifndef ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_
#define ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_

#include "base/bit_vector.h"
#include "base/value_object.h"
#include "memory_region.h"
#include "nodes.h"
#include "stack_map.h"
#include "utils/growable_array.h"

namespace art {

/**
 * Collects and builds stack maps for a method. All the stack maps
 * for a method are placed in a CodeInfo object.
 */
class StackMapStream : public ValueObject {
 public:
  explicit StackMapStream(ArenaAllocator* allocator);

  size_t ComputeNeededSize() const;

  void FillIn(MemoryRegion region);

  void AddStackMapEntry(uint32_t dex_pc, uint32_t native_pc_offset, uint32_t register_mask,
                        BitVector* sp_mask, uint32_t num_dex_registers, uint8_t inlining_depth);
  void AddInlineInfoEntry(uint32_t method_index);
  void AddDexRegisterEntry(uint16_t dex_register, DexRegisterLocation::Kind kind, int32_t value);


 private:
    // See runtime/stack_map.h to know what these fields contain.
  struct StackMapEntry {
    uint32_t dex_pc;
    uint32_t native_pc_offset;
    uint32_t register_mask;
    BitVector* sp_mask;
    uint32_t num_dex_registers;
    uint8_t inlining_depth;
    size_t dex_register_locations_start_index;
    size_t inline_infos_start_index;
    BitVector* live_dex_registers_mask;
  };

  struct InlineInfoEntry {
    uint32_t method_index;
  };

  size_t ComputeStackMaskSize() const;
  size_t ComputeStackMapsSize() const;
  size_t ComputeDexRegisterMapSize(const StackMapEntry& entry) const;
  size_t ComputeDexRegisterMapsSize() const;
  size_t ComputeInlineInfoSize() const;

  size_t ComputeDexRegisterMapsStart() const;
  size_t ComputeInlineInfoStart() const;

 private:
  ArenaAllocator* allocator_;
  GrowableArray<StackMapEntry> stack_maps_;
  GrowableArray<DexRegisterLocation> dex_register_locations_;
  GrowableArray<InlineInfoEntry> inline_infos_;
  int stack_mask_max_;
  size_t number_of_stack_maps_with_inline_info_;

  ART_FRIEND_TEST(StackMapTest, Test1);
  ART_FRIEND_TEST(StackMapTest, Test2);
  ART_FRIEND_TEST(StackMapTest, TestNonLiveDexRegisters);

  DISALLOW_COPY_AND_ASSIGN(StackMapStream);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_
