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

#include "base/arena_containers.h"
#include "base/bit_vector-inl.h"
#include "base/value_object.h"
#include "memory_region.h"
#include "nodes.h"
#include "stack_map.h"

namespace art {

/**
 * Collects and builds stack maps for a method. All the stack maps
 * for a method are placed in a CodeInfo object.
 */
class StackMapStream : public ValueObject {
  // Encoding helper class which has read-only access to the stack map data.
  // This gives us some confidence that the encoding process did not mangle the inputs and
  // that the CheckCodeInfo method is still comparing the encoded data to the original inputs.
  struct Encoder {
    Encoder(ArenaAllocator* allocator, const StackMapStream* inputs)
        : inputs_(inputs),
          encoded_header_(allocator->Adapter(kArenaAllocStackMapStream)),
          encoded_dex_register_maps_(allocator->Adapter(kArenaAllocStackMapStream)),
          encoded_inline_infos_(allocator->Adapter(kArenaAllocStackMapStream)) {
      encoded_header_.reserve(16);
      encoded_dex_register_maps_.reserve(64);
    }

    size_t PrepareForFillIn();
    void FillIn(MemoryRegion region);

   private:
    size_t PrepareInlineInfos(const InlineInfoEncoding& encoding);
    size_t PrepareDexRegisterMaps();

    const StackMapStream* inputs_;
    ArenaVector<uint8_t> encoded_header_;
    ArenaVector<uint8_t> encoded_dex_register_maps_;
    ArenaVector<uint8_t> encoded_inline_infos_;

    DISALLOW_COPY_AND_ASSIGN(Encoder);
  };

 public:
  explicit StackMapStream(ArenaAllocator* allocator)
      : stack_maps_(allocator->Adapter(kArenaAllocStackMapStream)),
        current_stack_map_(nullptr),
        inline_infos_(allocator->Adapter(kArenaAllocStackMapStream)),
        current_inline_info_(nullptr),
        dex_register_locations_(allocator->Adapter(kArenaAllocStackMapStream)),
        empty_stack_mask_(allocator, 0, false),
        encoder_(allocator, this) {
    stack_maps_.reserve(10);
    dex_register_locations_.reserve(64);
  }

  // See runtime/stack_map.h to know what these fields contain.
  struct StackMapEntry {
    uint32_t dex_pc;
    uint32_t native_pc_offset;
    uint32_t register_mask;
    BitVector* stack_mask;
    uint32_t num_dex_registers;
    uint32_t num_dex_registers_including_inlined;
    uint32_t inlining_depth;
    size_t dex_register_locations_start_index;
    size_t inline_infos_start_index;

    // Internal state which the encoder is allowed to touch.
    mutable uint32_t dex_register_map_offset;
    mutable uint32_t inline_info_index;
  };

  struct InlineInfoEntry {
    uint32_t dex_pc;
    uint32_t method_index;
    InvokeType invoke_type;
    uint32_t num_dex_registers;
    size_t dex_register_locations_start_index;
  };

  void BeginStackMapEntry(uint32_t dex_pc,
                          uint32_t native_pc_offset,
                          uint32_t register_mask,
                          BitVector* stack_mask,
                          uint32_t num_dex_registers,
                          uint8_t inlining_depth);
  void EndStackMapEntry();

  void AddDexRegisterEntry(DexRegisterLocation::Kind kind, int32_t value);

  void BeginInlineInfoEntry(uint32_t method_index,
                            uint32_t dex_pc,
                            InvokeType invoke_type,
                            uint32_t num_dex_registers);
  void EndInlineInfoEntry();

  size_t GetNumberOfStackMaps() const {
    return stack_maps_.size();
  }

  const StackMapEntry& GetStackMap(size_t i) const {
    return stack_maps_[i];
  }

  void SetStackMapNativePcOffset(size_t i, uint32_t native_pc_offset) {
    stack_maps_[i].native_pc_offset = native_pc_offset;
  }

  // Prepares the stream to fill in a memory region. Must be called before FillIn.
  // Returns the size (in bytes) needed to store this stream.
  size_t PrepareForFillIn() {
    DCHECK(current_stack_map_ == nullptr);
    DCHECK(current_inline_info_ == nullptr);
    return encoder_.PrepareForFillIn();
  }

  // Fill the given memory region with encoded stack map data.
  void FillIn(MemoryRegion region) {
    encoder_.FillIn(region);

    if (kIsDebugBuild) {
      CheckCodeInfo(region);
    }
  }

  // Verify encoded stack map data by reading it back and comparing it to the inputs.
  void CheckCodeInfo(MemoryRegion region) const;

 private:
  ArenaVector<StackMapEntry> stack_maps_;
  StackMapEntry* current_stack_map_;
  ArenaVector<InlineInfoEntry> inline_infos_;
  InlineInfoEntry* current_inline_info_;
  ArenaVector<DexRegisterLocation> dex_register_locations_;
  ArenaBitVector empty_stack_mask_;
  Encoder encoder_;

  DISALLOW_COPY_AND_ASSIGN(StackMapStream);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_
