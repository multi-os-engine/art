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
#include "stack_map_stream.h"

namespace art {

void StackMapStream::BeginStackMapEntry(uint32_t dex_pc,
                                        uint32_t native_pc_offset,
                                        uint32_t register_mask,
                                        BitVector* stack_mask) {
  DCHECK(!in_stack_map_);
  in_stack_map_ = true;
  StackMapEntry entry;
  entry.dex_pc = dex_pc;
  entry.native_pc_offset = native_pc_offset;
  entry.register_mask = register_mask;
  entry.stack_mask = (stack_mask != nullptr ? stack_mask : &empty_stack_mask);
  entry.num_dex_registers = 0;
  entry.inlining_depth = 0;
  entry.dex_register_locations_start_index = dex_register_locations_.size();
  entry.inline_infos_start_index = inline_infos_.size();
  entry.flags = 0;
  stack_maps_.push_back(entry);
}

void StackMapStream::EndStackMapEntry() {
  DCHECK(in_stack_map_);
  in_stack_map_ = false;
}

void StackMapStream::AddDexRegisterEntry(DexRegisterLocation::Kind kind, int32_t value) {
  DCHECK(in_stack_map_);
  dex_register_locations_.push_back(DexRegisterLocation(kind, value));
  if (in_inline_info_) {
    inline_infos_.back().num_dex_registers++;
  } else {
    // We can not add main method registers after we have started inlined registers.
    DCHECK_EQ(stack_maps_.back().inlining_depth, 0u);
    stack_maps_.back().num_dex_registers++;
  }
}

void StackMapStream::BeginInlineInfoEntry(uint32_t method_index,
                                          uint32_t dex_pc,
                                          InvokeType invoke_type) {
  DCHECK(in_stack_map_);
  DCHECK(!in_inline_info_);
  in_inline_info_ = true;
  InlineInfoEntry entry;
  entry.method_index = method_index;
  entry.dex_pc = dex_pc;
  entry.invoke_type = invoke_type;
  entry.num_dex_registers = 0;
  inline_infos_.push_back(entry);
  stack_maps_.back().inlining_depth++;
}

void StackMapStream::EndInlineInfoEntry() {
  DCHECK(in_inline_info_);
  in_inline_info_ = false;
}

size_t StackMapStream::PrepareForFillIn() {
  DCHECK(!in_stack_map_);
  DCHECK(!in_inline_info_);

  // Prepare compressed dex register locations.
  // We need to do this first since it can set extra flags.
  PrepareDexRegisterMap();

  // Calculate how many bits we need for each field based on the maximal observed values.
  uint32_t max_dex_pc = 0;
  uint32_t max_native_pc_offset = 0;
  uint32_t max_register_mask = 0;  // Value, not bit count.
  uint32_t max_stack_mask_bits = 0;  // Bit count because value would not fit.
  uint32_t max_num_dex_registers = 0;
  uint32_t inline_infos_size = 0;
  uint32_t max_flags = 0;
  for (const StackMapEntry& entry : stack_maps_) {
    max_dex_pc = std::max(max_dex_pc, entry.dex_pc);
    max_native_pc_offset = std::max(max_native_pc_offset, entry.native_pc_offset);
    max_register_mask |= entry.register_mask;
    uint32_t num_stack_mask_bits = entry.stack_mask->GetHighestBitSet() + 1;
    max_stack_mask_bits = std::max(max_stack_mask_bits, num_stack_mask_bits);
    max_num_dex_registers = std::max(max_num_dex_registers, entry.num_dex_registers);
    if (entry.inlining_depth != 0) {
      inline_infos_size += InlineInfo::kFixedSize;
      inline_infos_size += InlineInfo::SingleEntrySize() * entry.inlining_depth;
    }
    max_flags |= entry.flags;
  }
  StackMapEncoding encoding;
  size_t stack_map_size = stack_map_encoding_.SetFromSizes(max_native_pc_offset,
                                                           max_dex_pc,
                                                           max_flags,
                                                           inline_infos_size,
                                                           max_register_mask,
                                                           max_stack_mask_bits);

  // Sanity check - all stack maps should have the same number of dex registers.
  for (const StackMapEntry& entry : stack_maps_) {
    if (entry.num_dex_registers != 0) {
      DCHECK_EQ(entry.num_dex_registers, max_num_dex_registers);
    }
  }

  // Prepare the CodeInfo variable-sized header.
  CodeInfoHeader header;
  header.number_of_stack_maps = stack_maps_.size();
  header.stack_map_size = stack_map_size;
  header.inline_infos_size = inline_infos_size;
  header.number_of_dex_registers = max_num_dex_registers;
  header.dex_register_maps_size = encoded_dex_register_locations_.size();
  header.Encode(&encoded_header_);

  return encoded_header_.size() +
       sizeof(StackMapEncoding) +
       header.number_of_stack_maps * header.stack_map_size +
       header.inline_infos_size +
       header.dex_register_maps_size;
}

void StackMapStream::FillIn(MemoryRegion region) {
  DCHECK(!in_stack_map_);
  DCHECK(!in_inline_info_);
  DCHECK(!encoded_header_.empty()) << "PrepareForFillIn not called before FillIn";

  // Note that the memory region does not have to be zeroed.

  // Write the CodeInfo header.
  region.CopyFrom(0, MemoryRegion(encoded_header_.data(), encoded_header_.size()));

  // Create CodeInfo for writing. This also checks that region has the right size.
  CodeInfo code_info(region);

  code_info.SetStackMapEncoding(stack_map_encoding_);
  uintptr_t next_inline_info_offset = 0;
  for (size_t i = 0, e = stack_maps_.size(); i < e; ++i) {
    StackMap stack_map = code_info.GetStackMapAt(i);
    StackMapEntry entry = stack_maps_[i];
    stack_map.SetNativePcOffset(entry.native_pc_offset);
    stack_map.SetDexPc(entry.dex_pc);
    stack_map.SetFlags(entry.flags);
    stack_map.SetRegisterMask(entry.register_mask);
    size_t number_of_stack_mask_bits = stack_map.GetNumberOfStackMaskBits();
    for (size_t bit = 0; bit < number_of_stack_mask_bits; bit++) {
      stack_map.SetStackMaskBit(bit, entry.stack_mask->IsBitSet(bit));
    }

    // Set the inlining info.
    if (entry.inlining_depth != 0) {
      stack_map.SetInlineDescriptorOffset(next_inline_info_offset);
      MemoryRegion inline_region = code_info.inline_infos_region_.Subregion(
          next_inline_info_offset,
          InlineInfo::kFixedSize + entry.inlining_depth * InlineInfo::SingleEntrySize());
      next_inline_info_offset += inline_region.size();

      InlineInfo inline_info(inline_region);
      inline_info.SetDepth(entry.inlining_depth);
      DCHECK_LE(entry.inline_infos_start_index + entry.inlining_depth, inline_infos_.size());
      for (size_t depth = 0; depth < entry.inlining_depth; ++depth) {
        InlineInfoEntry inline_entry = inline_infos_[depth + entry.inline_infos_start_index];
        inline_info.SetMethodIndexAtDepth(depth, inline_entry.method_index);
        inline_info.SetDexPcAtDepth(depth, inline_entry.dex_pc);
        inline_info.SetInvokeTypeAtDepth(depth, inline_entry.invoke_type);
        inline_info.SetNumDexRegistersAtDepth(depth, inline_entry.num_dex_registers);
      }
    } else {
      stack_map.SetInlineDescriptorOffset(StackMap::kNoInlineInfo);
    }
  }

  code_info.dex_register_maps_region_.CopyFrom(
      0,
      MemoryRegion(encoded_dex_register_locations_.data(),
                   encoded_dex_register_locations_.size()));

  DCHECK(CheckCodeInfo(region));
}

// Write dex register locations for all stack maps (including inlined methods).
// The locations are written only when they are different from their last state.
// Each map starts with bitmask which marks the modified locations.
void StackMapStream::PrepareDexRegisterMap() {
  DCHECK(encoded_dex_register_locations_.empty());
  std::vector<DexRegisterLocation> locations;  // Last state. Never shrinks.
  for (StackMapEntry& entry : stack_maps_) {
    // Count the number of dex registers including the inlined ones.
    size_t num_locations = 0;
    if (entry.num_dex_registers > 0) {
      num_locations += entry.num_dex_registers;
      entry.flags |= StackMap::Flags::kHasDexRegisterMap;
    } else {
      DCHECK_EQ(entry.flags & StackMap::Flags::kHasDexRegisterMap, 0u);  // Flag not set.
    }
    for (size_t i = 0; i < entry.inlining_depth; ++i) {
      num_locations += inline_infos_[entry.inline_infos_start_index + i].num_dex_registers;
    }
    locations.resize(std::max(locations.size(), num_locations));

    // Allocate enough memory for encoded locations (overestimate).
    const size_t old_size = encoded_dex_register_locations_.size();
    const size_t bitmap_size = RoundUp(num_locations, kBitsPerByte) / kBitsPerByte;
    const size_t locations_size = num_locations * DexRegisterLocation::kMaximumEncodedSize;
    encoded_dex_register_locations_.resize(old_size + bitmap_size + locations_size);
    MemoryRegion region(encoded_dex_register_locations_.data() + old_size,
                        bitmap_size + locations_size);

    // Write the bitmap and encode register locations.
    size_t num_used_bytes = bitmap_size;
    bool any_bit_set = false;
    for (size_t i = 0; i < num_locations; i++) {
      const DexRegisterLocation& dex_register_location =
          dex_register_locations_[entry.dex_register_locations_start_index + i];
      if (locations[i] != dex_register_location) {
        locations[i] = dex_register_location;
        region.StoreBit(i, true);
        any_bit_set = true;
        dex_register_location.Encode(&region, &num_used_bytes);
      }
    }

    // Optimization - omit the bitmap if it has no bits set.
    if (!any_bit_set) {
      entry.flags |= StackMap::Flags::kSameDexRegisterMap;
      num_used_bytes = 0;
    }

    // Trim the buffer back to exclude any reserved space we did not use.
    encoded_dex_register_locations_.resize(old_size + num_used_bytes);
  }
}

// Read the encoded data back and check that it matches the StackMapStream inputs.
bool StackMapStream::CheckCodeInfo(MemoryRegion region) const {
  CodeInfo code_info(region);
  size_t decoded_offset = 0;
  dchecked_vector<DexRegisterLocation> decoded_locations;
  DCHECK_EQ(code_info.GetNumberOfStackMaps(), stack_maps_.size());
  for (size_t s = 0; s < stack_maps_.size(); ++s) {
    const StackMap stack_map = code_info.GetStackMapAt(s);
    StackMapEntry entry = stack_maps_[s];
    DCHECK_EQ(stack_map.GetNativePcOffset(), entry.native_pc_offset);
    DCHECK_EQ(stack_map.GetDexPc(), entry.dex_pc);
    DCHECK_EQ(stack_map.GetFlags(), entry.flags);
    DCHECK_EQ(stack_map.GetRegisterMask(), entry.register_mask);
    int number_of_stack_mask_bits = stack_map.GetNumberOfStackMaskBits();
    DCHECK_GE(number_of_stack_mask_bits, entry.stack_mask->GetHighestBitSet() + 1);
    for (int b = 0; b < number_of_stack_mask_bits; b++) {
      DCHECK_EQ(stack_map.GetStackMaskBit(b), entry.stack_mask->IsBitSet(b));
    }
    size_t num_locations = entry.num_dex_registers;
    DCHECK_EQ(code_info.GetNumberOfDexRegistersOf(stack_map), entry.num_dex_registers);
    DCHECK_EQ(stack_map.HasInlineInfo(), entry.inlining_depth != 0);
    if (entry.inlining_depth != 0) {
      InlineInfo inline_info = code_info.GetInlineInfoOf(stack_map);
      DCHECK_EQ(inline_info.GetDepth(), entry.inlining_depth);
      for (size_t d = 0; d < entry.inlining_depth; ++d) {
        InlineInfoEntry inline_entry = inline_infos_[entry.inline_infos_start_index + d];
        DCHECK_EQ(inline_info.GetMethodIndexAtDepth(d), inline_entry.method_index);
        DCHECK_EQ(inline_info.GetDexPcAtDepth(d), inline_entry.dex_pc);
        DCHECK_EQ(inline_info.GetInvokeTypeAtDepth(d), inline_entry.invoke_type);
        DCHECK_EQ(inline_info.GetNumDexRegistersAtDepth(d), inline_entry.num_dex_registers);
        num_locations += inline_entry.num_dex_registers;
      }
    }
    size_t num = code_info.DecodeNextDexRegisterMap(stack_map, &decoded_offset, &decoded_locations);
    DCHECK_EQ(num, num_locations);
    for (size_t r = 0; r < num_locations; ++r) {
      size_t index = entry.dex_register_locations_start_index + r;
      DCHECK_LT(index, dex_register_locations_.size());
      DCHECK_EQ(decoded_locations[r], dex_register_locations_[index]);
    }
  }
  DCHECK_EQ(decoded_offset, code_info.dex_register_maps_region_.size());
  return true;
}

}  // namespace art
