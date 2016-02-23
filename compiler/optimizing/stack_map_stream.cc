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
                                        BitVector* stack_mask,
                                        uint32_t num_dex_registers ATTRIBUTE_UNUSED,
                                        uint8_t inlining_depth ATTRIBUTE_UNUSED) {
  DCHECK(current_stack_map_ == nullptr);
  StackMapEntry entry;
  entry.dex_pc = dex_pc;
  entry.native_pc_offset = native_pc_offset;
  entry.register_mask = register_mask;
  entry.stack_mask = (stack_mask != nullptr ? stack_mask : &empty_stack_mask_);
  entry.num_dex_registers = 0;
  entry.num_dex_registers_including_inlined = 0;
  entry.inlining_depth = 0;
  entry.dex_register_locations_start_index = dex_register_locations_.size();
  entry.inline_infos_start_index = inline_infos_.size();
  stack_maps_.push_back(entry);
  current_stack_map_ = &stack_maps_.back();
}

void StackMapStream::EndStackMapEntry() {
  DCHECK(current_stack_map_ != nullptr);
  current_stack_map_ = nullptr;
}

void StackMapStream::AddDexRegisterEntry(DexRegisterLocation::Kind kind, int32_t value) {
  DCHECK(current_stack_map_ != nullptr);
  dex_register_locations_.push_back(DexRegisterLocation(kind, value));
  if (current_inline_info_ != nullptr) {
    current_inline_info_->num_dex_registers++;
  } else {
    // We can not add main method registers after we have started inlined registers.
    DCHECK_EQ(current_stack_map_->inlining_depth, 0u);
    current_stack_map_->num_dex_registers++;
  }
  current_stack_map_->num_dex_registers_including_inlined++;
}

void StackMapStream::BeginInlineInfoEntry(uint32_t method_index,
                                          uint32_t dex_pc,
                                          InvokeType invoke_type,
                                          uint32_t num_dex_registers ATTRIBUTE_UNUSED) {
  DCHECK(current_inline_info_ == nullptr);
  InlineInfoEntry entry;
  entry.method_index = method_index;
  entry.dex_pc = dex_pc;
  entry.invoke_type = invoke_type;
  entry.num_dex_registers = 0;
  entry.dex_register_locations_start_index = dex_register_locations_.size();
  inline_infos_.push_back(entry);
  current_inline_info_ = &inline_infos_.back();

  DCHECK(current_stack_map_ != nullptr);
  current_stack_map_->inlining_depth++;
}

void StackMapStream::EndInlineInfoEntry() {
  DCHECK(current_inline_info_ != nullptr);
  current_inline_info_ = nullptr;
}

size_t StackMapStream::Encoder::PrepareForFillIn() {
  // Prepare compressed dex register locations.
  // We need to do this first since it sets flags.
  size_t dex_register_maps_size = PrepareDexRegisterMaps();

  // Calculate how many bits we need for each field based on the maximal observed values.
  uint32_t max_dex_pc = 0;
  uint32_t max_native_pc_offset = 0;
  uint32_t max_register_mask = 0;  // Value, not bit count.
  size_t max_stack_mask_bits = 0;  // Bit count because value would not fit.
  uint32_t max_num_dex_registers = 0;
  uint32_t max_flags = 0;
  uint32_t max_inlined_method_index = 0;
  uint32_t max_inlined_dex_pc = 0;
  uint32_t max_inlined_invoke_type = 0;
  uint32_t max_inlined_num_dex_registers = 0;
  for (size_t s = 0; s < inputs_->stack_maps_.size(); ++s) {
    const StackMapEntry& entry = inputs_->stack_maps_[s];
    max_dex_pc = std::max(max_dex_pc, entry.dex_pc);
    max_native_pc_offset = std::max(max_native_pc_offset, entry.native_pc_offset);
    max_register_mask |= entry.register_mask;
    max_stack_mask_bits = std::max(max_stack_mask_bits, entry.stack_mask->GetNumberOfBits());
    max_num_dex_registers = std::max(max_num_dex_registers, entry.num_dex_registers);
    max_flags |= stack_map_flags_[s];
    uint32_t inline_info_index = entry.inline_infos_start_index;
    for (size_t j = 0; j < entry.inlining_depth; ++j) {
      InlineInfoEntry inline_entry = inputs_->inline_infos_[inline_info_index++];
      max_inlined_method_index = std::max(max_inlined_method_index, inline_entry.method_index);
      max_inlined_dex_pc = std::max(max_inlined_dex_pc, inline_entry.dex_pc);
      max_inlined_invoke_type = std::max(max_inlined_invoke_type,
                                         static_cast<uint32_t>(inline_entry.invoke_type));
      max_inlined_num_dex_registers = std::max(max_inlined_num_dex_registers,
                                               inline_entry.num_dex_registers);
    }
  }

  InlineInfoEncoding inline_info_encoding;
  inline_info_encoding.SetFromSizes(max_inlined_method_index,
                                    max_inlined_dex_pc,
                                    max_inlined_invoke_type,
                                    max_inlined_num_dex_registers);
  size_t inline_infos_size = inputs_->inline_infos_.size() * inline_info_encoding.GetEntrySize();

  StackMapEncoding stack_map_encoding;
  size_t stack_map_size = stack_map_encoding.SetFromSizes(max_native_pc_offset,
                                                          max_dex_pc,
                                                          max_flags,
                                                          inline_infos_size,
                                                          max_register_mask,
                                                          max_stack_mask_bits);
  size_t stack_maps_size = stack_map_size * inputs_->stack_maps_.size();

  // Sanity check - all stack maps should have the same number of dex registers (or zero).
  for (const StackMapEntry& entry : inputs_->stack_maps_) {
    if (entry.num_dex_registers != 0) {
      DCHECK_EQ(entry.num_dex_registers, max_num_dex_registers);
    }
  }

  // Prepare the CodeInfo variable-sized header.
  CodeInfoHeader header;
  header.number_of_stack_maps = inputs_->stack_maps_.size();
  header.stack_map_size = stack_map_size;
  header.number_of_dex_registers = max_num_dex_registers;
  header.dex_register_maps_size = dex_register_maps_size;
  header.inline_infos_size = inline_infos_size;
  header.stack_map_encoding = &stack_map_encoding;
  header.inline_info_encoding = &inline_info_encoding;
  header.Encode(&encoded_header_);
  size_t header_size = encoded_header_.size();

  return header_size + stack_maps_size + dex_register_maps_size + inline_infos_size;
}

void StackMapStream::Encoder::FillIn(MemoryRegion region) {
  DCHECK(!encoded_header_.empty()) << "PrepareForFillIn not called before FillIn";
  // Note that the memory region does not have to be zeroed.

  // Write the CodeInfo header.
  region.CopyFrom(0, MemoryRegion(encoded_header_.data(), encoded_header_.size()));

  // Create CodeInfo for writing. This also checks that region has the right size.
  CodeInfo code_info(region);

  uintptr_t next_inline_info_offset = 0;
  for (size_t i = 0, e = inputs_->stack_maps_.size(); i < e; ++i) {
    StackMap stack_map = code_info.GetStackMapAt(i);
    const StackMapEntry& entry = inputs_->stack_maps_[i];
    stack_map.SetDexPc(entry.dex_pc);
    stack_map.SetNativePcOffset(entry.native_pc_offset);
    stack_map.SetFlags(stack_map_flags_[i]);
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
          entry.inlining_depth * code_info.header_.inline_info_encoding->GetEntrySize());
      next_inline_info_offset += inline_region.size();
      InlineInfo inline_info(code_info.header_.inline_info_encoding, inline_region);

      inline_info.SetDepth(entry.inlining_depth);
      for (size_t depth = 0; depth < entry.inlining_depth; ++depth) {
        size_t inline_info_index = entry.inline_infos_start_index + depth;
        DCHECK_LE(inline_info_index, inputs_->inline_infos_.size());
        InlineInfoEntry inline_entry = inputs_->inline_infos_[inline_info_index];
        inline_info.SetMethodIndexAtDepth(depth, inline_entry.method_index);
        inline_info.SetDexPcAtDepth(depth, inline_entry.dex_pc);
        inline_info.SetInvokeTypeAtDepth(depth, inline_entry.invoke_type);
        inline_info.SetNumberOfDexRegistersAtDepth(depth, inline_entry.num_dex_registers);
      }
    } else {
      stack_map.SetInlineDescriptorOffset(StackMap::kNoInlineInfo);
    }
  }

  code_info.dex_register_maps_region_.CopyFrom(
      0,
      MemoryRegion(encoded_dex_register_maps_.data(),
                   encoded_dex_register_maps_.size()));
}

// Write dex register locations for all stack maps (including inlined registers).
// The locations are written only when they are different from their last state.
// Each map starts with bitmask which marks the modified locations.
size_t StackMapStream::Encoder::PrepareDexRegisterMaps() {
  stack_map_flags_.resize(inputs_->stack_maps_.size());
  DCHECK(encoded_dex_register_maps_.empty());
  std::vector<DexRegisterLocation> locations;  // Last state. Never shrinks.
  for (size_t s = 0; s < inputs_->stack_maps_.size(); ++s) {
    StackMapEntry entry = inputs_->stack_maps_[s];

    if (entry.num_dex_registers > 0) {
      // The runtime uses this flag to determine whether the register count for this stack map
      // is zero or the shared per-method number. Inlined registers do not affect this flag.
      stack_map_flags_[s] |= StackMap::Flags::kHasAnyDexRegisters;
    }
    size_t num_locations = entry.num_dex_registers_including_inlined;
    locations.resize(std::max(locations.size(), num_locations));

    // Allocate enough memory for encoded locations (overestimate).
    const size_t old_size = encoded_dex_register_maps_.size();
    const size_t bitmap_size = RoundUp(num_locations, kBitsPerByte) / kBitsPerByte;
    const size_t locations_size = num_locations * DexRegisterLocation::kMaximumEncodedSize;
    encoded_dex_register_maps_.resize(old_size + bitmap_size + locations_size);
    MemoryRegion region(encoded_dex_register_maps_.data() + old_size,
                        bitmap_size + locations_size);

    // Write the bitmap and encode register locations.
    size_t num_used_bytes = bitmap_size;
    bool any_bit_set = false;
    for (size_t i = 0; i < num_locations; i++) {
      const DexRegisterLocation& dex_register_location =
          inputs_->dex_register_locations_[entry.dex_register_locations_start_index + i];
      if (locations[i] != dex_register_location) {
        locations[i] = dex_register_location;
        region.StoreBit(i, true);
        any_bit_set = true;
        dex_register_location.Encode(&region, &num_used_bytes);
      }
    }

    // Optimization - omit the bitmap if it has no bits set.
    if (!any_bit_set) {
      stack_map_flags_[s] |= StackMap::Flags::kSameDexRegisterMap;
      num_used_bytes = 0;
    }

    // Trim the buffer back to exclude any reserved space we did not use.
    encoded_dex_register_maps_.resize(old_size + num_used_bytes);
  }
  return encoded_dex_register_maps_.size();
}

// Check that all StackMapStream inputs are correctly encoded by trying to read them back.
void StackMapStream::CheckCodeInfo(MemoryRegion region) const {
  CodeInfo code_info(region);
  DCHECK_EQ(code_info.GetNumberOfStackMaps(), stack_maps_.size());
  for (size_t s = 0; s < stack_maps_.size(); ++s) {
    const StackMap stack_map = code_info.GetStackMapAt(s);
    StackMapEntry entry = stack_maps_[s];

    // Check main stack map fields.
    DCHECK_EQ(stack_map.GetNativePcOffset(), entry.native_pc_offset);
    DCHECK_EQ(stack_map.GetDexPc(), entry.dex_pc);
    DCHECK_EQ(stack_map.GetRegisterMask(), entry.register_mask);
    size_t number_of_stack_mask_bits = stack_map.GetNumberOfStackMaskBits();
    DCHECK_GE(number_of_stack_mask_bits, entry.stack_mask->GetNumberOfBits());
    for (size_t b = 0; b < number_of_stack_mask_bits; b++) {
      DCHECK_EQ(stack_map.GetStackMaskBit(b), entry.stack_mask->IsBitSet(b));
    }

    // Check dex register map.
    DCHECK_EQ(code_info.GetNumberOfDexRegistersOf(stack_map, 0 /* max_depth*/),
              entry.num_dex_registers);
    DCHECK_EQ(code_info.GetNumberOfDexRegistersOf(stack_map, 0xFFFFFFFF /* max_depth */),
              entry.num_dex_registers_including_inlined);
    DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(
              stack_map, entry.num_dex_registers);
    DCHECK_EQ(dex_register_map.size(), entry.num_dex_registers);
    for (size_t r = 0; r < entry.num_dex_registers; r++) {
      size_t index = entry.dex_register_locations_start_index + r;
      DCHECK_EQ(dex_register_map[r], dex_register_locations_[index]);
    }

    // Check inline info.
    DCHECK_EQ(stack_map.HasInlineInfo(), (entry.inlining_depth != 0));
    if (entry.inlining_depth != 0) {
      InlineInfo inline_info = code_info.GetInlineInfoOf(stack_map);
      DCHECK_EQ(inline_info.GetDepth(), entry.inlining_depth);
      for (size_t d = 0; d < entry.inlining_depth; ++d) {
        size_t inline_info_index = entry.inline_infos_start_index + d;
        DCHECK_LT(inline_info_index, inline_infos_.size());
        InlineInfoEntry inline_entry = inline_infos_[inline_info_index];
        DCHECK_EQ(inline_info.GetDexPcAtDepth(d), inline_entry.dex_pc);
        DCHECK_EQ(inline_info.GetMethodIndexAtDepth(d), inline_entry.method_index);
        DCHECK_EQ(inline_info.GetInvokeTypeAtDepth(d), inline_entry.invoke_type);
        DCHECK_EQ(inline_info.GetNumberOfDexRegistersAtDepth(d), inline_entry.num_dex_registers);

        // Check inlined dex register map.
        DexRegisterMap inlined_dex_register_map = code_info.GetDexRegisterMapAtDepth(
            d, inline_info, inline_entry.num_dex_registers);
        DCHECK_EQ(inlined_dex_register_map.size(), inline_entry.num_dex_registers);
        for (size_t r = 0; r < inline_entry.num_dex_registers; r++) {
          size_t index = inline_entry.dex_register_locations_start_index + r;
          DCHECK_EQ(inlined_dex_register_map[r], dex_register_locations_[index]);
        }
      }
    }
  }
}

}  // namespace art
