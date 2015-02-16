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
#include "stack_map.h"
#include "utils/growable_array.h"

namespace art {

/**
 * Collects and builds stack maps for a method. All the stack maps
 * for a method are placed in a CodeInfo object.
 */
class StackMapStream : public ValueObject {
 public:
  explicit StackMapStream(ArenaAllocator* allocator)
      : dex_register_dictionary_(allocator, 4),
        stack_maps_(allocator, 10),
        dex_register_maps_(allocator, 10 * 4),
        inline_infos_(allocator, 2),
        stack_mask_max_(-1),
        number_of_stack_maps_with_inline_info_(0) {}

  // Compute bytes needed to encode a mask with the given maximum element.
  static uint32_t StackMaskEncodingSize(int max_element) {
    int number_of_bits = max_element + 1;  // Need room for max element too.
    return RoundUp(number_of_bits, kBitsPerByte) / kBitsPerByte;
  }

  // See runtime/stack_map.h to know what these fields contain.
  struct StackMapEntry {
    uint32_t dex_pc;
    uint32_t native_pc_offset;
    uint32_t register_mask;
    BitVector* sp_mask;
    uint32_t num_dex_registers;
    uint8_t inlining_depth;
    size_t dex_register_maps_start_index;
    size_t inline_infos_start_index;
  };

  struct DexRegisterEntry {
    DexRegisterMap::LocationKind kind;
    int32_t value;

    bool operator==(const DexRegisterEntry& other_entry) {
      return kind == other_entry.kind && value == other_entry.value;
    }
  };

  struct InlineInfoEntry {
    uint32_t method_index;
  };

  void AddStackMapEntry(uint32_t dex_pc,
                        uint32_t native_pc_offset,
                        uint32_t register_mask,
                        BitVector* sp_mask,
                        uint32_t num_dex_registers,
                        uint8_t inlining_depth) {
    StackMapEntry entry;
    entry.dex_pc = dex_pc;
    entry.native_pc_offset = native_pc_offset;
    entry.register_mask = register_mask;
    entry.sp_mask = sp_mask;
    entry.num_dex_registers = num_dex_registers;
    entry.inlining_depth = inlining_depth;
    entry.dex_register_maps_start_index = dex_register_maps_.Size();
    entry.inline_infos_start_index = inline_infos_.Size();
    stack_maps_.Add(entry);

    if (sp_mask != nullptr) {
      stack_mask_max_ = std::max(stack_mask_max_, sp_mask->GetHighestBitSet());
    }
    if (inlining_depth > 0) {
      number_of_stack_maps_with_inline_info_++;
    }
  }

  void AddDexRegisterEntry(DexRegisterMap::LocationKind kind, int32_t value) {
    DexRegisterEntry entry;
    entry.kind = kind;
    entry.value = value;
    // Look for Dex register `entry` in the dictionary, insert it
    // if it doesn't exist yet, and get the index of `entry`.
    size_t index = dex_register_dictionary_.AddUnique(entry);
    DCHECK_LE(index, std::numeric_limits<DexRegisterMapIndex>::max());
    dex_register_maps_.Add(index);
  }

  void AddInlineInfoEntry(uint32_t method_index) {
    InlineInfoEntry entry;
    entry.method_index = method_index;
    inline_infos_.Add(entry);
  }

  size_t ComputeNeededSize() const {
    switch (dex_register_map_encoding) {
      case kDexRegisterLocationList:
        return CodeInfo::kFixedSize
            + ComputeStackMapSize()
            + ComputeDexRegisterMapSize()
            + ComputeInlineInfoSize();

      case kDexRegisterLocationDictionary:
        return CodeInfo::kFixedSize
            + ComputeDexRegisterDictionarySize()
            + ComputeStackMapSize()
            + ComputeDexRegisterMapSize()
            + ComputeInlineInfoSize();
        break;
    }
  }

  size_t ComputeStackMapSize() const {
    return stack_maps_.Size() * StackMap::ComputeAlignedStackMapSize(stack_mask_max_);
  }

  size_t ComputeDexRegisterDictionarySize() const {
    switch (dex_register_map_encoding) {
      case kDexRegisterLocationList:
        return 0u;

      case kDexRegisterLocationDictionary:
        return DexRegisterDictionary::kFixedSize
            + dex_register_dictionary_.Size() * DexRegisterDictionary::SingleEntrySize();
    }
  }

  size_t ComputeDexRegisterMapSize() const {
    switch (dex_register_map_encoding) {
      case kDexRegisterLocationList:
        return stack_maps_.Size() * DexRegisterMap::kFixedSize
            // For each dex register entry.
            + dex_register_maps_.Size() * DexRegisterMap::SingleEntrySize();

      case kDexRegisterLocationDictionary:
        return stack_maps_.Size() * DexRegisterTable::kFixedSize
            // For each dex register entry.
            + dex_register_maps_.Size() * DexRegisterTable::SingleEntrySize();
    };
  }

  size_t ComputeInlineInfoSize() const {
    return inline_infos_.Size() * InlineInfo::SingleEntrySize()
      // For encoding the depth.
      + (number_of_stack_maps_with_inline_info_ * InlineInfo::kFixedSize);
  }

  size_t ComputeDexRegisterDictionaryStart() const {
    return CodeInfo::kFixedSize;
  }

  size_t ComputeStackMapsStart() const {
    size_t dictionary_size = ComputeDexRegisterDictionarySize();
    if (kIsDebugBuild
        && dex_register_map_encoding != kDexRegisterLocationDictionary) {
      DCHECK_EQ(dictionary_size, 0u);
    }
    return ComputeDexRegisterDictionaryStart() + dictionary_size;
  }

  size_t ComputeDexRegisterMapStart() const {
    return ComputeStackMapsStart() + ComputeStackMapSize();
  }

  size_t ComputeInlineInfoStart() const {
    return ComputeDexRegisterMapStart() + ComputeDexRegisterMapSize();
  }

  void FillIn(MemoryRegion region) {
    CodeInfo code_info(region);
    code_info.SetOverallSize(region.size());

    size_t stack_mask_size = StackMaskEncodingSize(stack_mask_max_);
    uint8_t* memory_start = region.start();

    MemoryRegion dex_register_maps_region = region.Subregion(
      ComputeDexRegisterMapStart(),
      ComputeDexRegisterMapSize());

    MemoryRegion inline_infos_region = region.Subregion(
      ComputeInlineInfoStart(),
      ComputeInlineInfoSize());

    code_info.SetNumberOfStackMaps(stack_maps_.Size());
    code_info.SetStackMaskSize(stack_mask_size);

    if (dex_register_map_encoding == kDexRegisterLocationDictionary) {
      // Set the Dex register dictionary.
      code_info.SetNumberOfDexRegisterDictionaryEntries(dex_register_dictionary_.Size());
      MemoryRegion dex_register_dictionary_region = region.Subregion(
          ComputeDexRegisterDictionaryStart(),
          ComputeDexRegisterDictionarySize());
      DexRegisterDictionary dex_register_dictionary(dex_register_dictionary_region);
      for (size_t i = 0; i < dex_register_dictionary_.Size(); ++i) {
        DexRegisterEntry register_entry = dex_register_dictionary_.Get(i);
        dex_register_dictionary.SetRegisterInfo(i, register_entry.kind, register_entry.value);
      }
    }

    uintptr_t next_dex_register_map_offset = 0;
    uintptr_t next_inline_info_offset = 0;
    for (size_t i = 0, e = stack_maps_.Size(); i < e; ++i) {
      StackMap stack_map = code_info.GetStackMapAt(i);
      StackMapEntry entry = stack_maps_.Get(i);

      stack_map.SetDexPc(entry.dex_pc);
      stack_map.SetNativePcOffset(entry.native_pc_offset);
      stack_map.SetRegisterMask(entry.register_mask);
      if (entry.sp_mask != nullptr) {
        stack_map.SetStackMask(*entry.sp_mask);
      }

      if (entry.num_dex_registers != 0) {
        switch (dex_register_map_encoding) {
          case kDexRegisterLocationList: {
            // Set the Dex register map.
            MemoryRegion register_region =
                dex_register_maps_region.Subregion(
                    next_dex_register_map_offset,
                    DexRegisterMap::kFixedSize
                    + entry.num_dex_registers * DexRegisterMap::SingleEntrySize());
            next_dex_register_map_offset += register_region.size();
            DexRegisterMap dex_register_map(register_region);

            stack_map.SetDexRegisterMapOffset(register_region.start() - memory_start);

            for (size_t j = 0; j < entry.num_dex_registers; ++j) {
              DexRegisterMapIndex entry_index =
                  dex_register_maps_.Get(entry.dex_register_maps_start_index + j);
              DexRegisterEntry register_entry =
                  dex_register_dictionary_.Get(entry_index);
              dex_register_map.SetRegisterInfo(j, register_entry.kind, register_entry.value);
            }
            break;
          }

          case kDexRegisterLocationDictionary: {
            // Set the Dex register table.
            MemoryRegion register_region =
                dex_register_maps_region.Subregion(
                    next_dex_register_map_offset,
                    DexRegisterMap::kFixedSize
                    + entry.num_dex_registers * sizeof(DexRegisterTable::EntryIndex));
            next_dex_register_map_offset += register_region.size();
            DexRegisterTable dex_register_table(register_region);

            stack_map.SetDexRegisterMapOffset(register_region.start() - memory_start);

            for (size_t j = 0; j < entry.num_dex_registers; ++j) {
              DexRegisterMapIndex entry_index =
                  dex_register_maps_.Get(entry.dex_register_maps_start_index + j);
              dex_register_table.SetEntryIndex(j, entry_index);
            }
            break;
          }
        }
      } else {
        stack_map.SetDexRegisterMapOffset(StackMap::kNoDexRegisterMap);
      }

      // Set the inlining info.
      if (entry.inlining_depth != 0) {
        MemoryRegion inline_region = inline_infos_region.Subregion(
            next_inline_info_offset,
            InlineInfo::kFixedSize + entry.inlining_depth * InlineInfo::SingleEntrySize());
        next_inline_info_offset += inline_region.size();
        InlineInfo inline_info(inline_region);

        stack_map.SetInlineDescriptorOffset(inline_region.start() - memory_start);

        inline_info.SetDepth(entry.inlining_depth);
        for (size_t j = 0; j < entry.inlining_depth; ++j) {
          InlineInfoEntry inline_entry = inline_infos_.Get(j + entry.inline_infos_start_index);
          inline_info.SetMethodReferenceIndexAtDepth(j, inline_entry.method_index);
        }
      } else {
        stack_map.SetInlineDescriptorOffset(StackMap::kNoInlineInfo);
      }
    }
  }

 private:
  // The type of Dex register dictionary indices, based on the indices
  // used to reference entries in DexRegisterTable objects.
  typedef DexRegisterTable::EntryIndex DexRegisterMapIndex;

  // A dictionary of unique [location_kind, register_value] pairs (per method).
  GrowableArray<DexRegisterEntry> dex_register_dictionary_;
  GrowableArray<StackMapEntry> stack_maps_;
  // A set of concatenated maps of Dex register locations indices to
  // `dex_register_dictionary_`.
  GrowableArray<DexRegisterMapIndex> dex_register_maps_;
  GrowableArray<InlineInfoEntry> inline_infos_;
  int stack_mask_max_;
  size_t number_of_stack_maps_with_inline_info_;

  DISALLOW_COPY_AND_ASSIGN(StackMapStream);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_STACK_MAP_STREAM_H_
