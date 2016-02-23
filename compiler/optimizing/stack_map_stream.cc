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

#include "base/stl_util.h"
#include "utils/array_ref.h"

#include <unordered_map>

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
  entry.dex_register_map_offset = StackMap::kNoDexRegisterMap;
  entry.inline_info_index = StackMap::kNoInlineInfo;
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
  DCHECK(current_stack_map_ != nullptr);
  DCHECK(current_inline_info_ == nullptr);
  InlineInfoEntry entry;
  entry.method_index = method_index;
  entry.dex_pc = dex_pc;
  entry.invoke_type = invoke_type;
  entry.num_dex_registers = 0;
  entry.dex_register_locations_start_index = dex_register_locations_.size();
  inline_infos_.push_back(entry);
  current_inline_info_ = &inline_infos_.back();
  current_stack_map_->inlining_depth++;
}

void StackMapStream::EndInlineInfoEntry() {
  DCHECK(current_inline_info_ != nullptr);
  current_inline_info_ = nullptr;
}

size_t StackMapStream::Encoder::PrepareForFillIn() {
  // Calculate how many bits we need for each field based on the maximum observed values.
  uint32_t max_dex_pc = 0;
  uint32_t max_native_pc_offset = 0;
  uint32_t max_register_mask = 0;  // Value, not bit count.
  size_t max_stack_mask_bits = 0;  // Bit count because value would not fit.
  uint32_t max_num_dex_registers = 0;
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
    for (size_t d = 0; d < entry.inlining_depth; ++d) {
      uint32_t inline_info_index = entry.inline_infos_start_index + d;
      const InlineInfoEntry& inline_entry = inputs_->inline_infos_[inline_info_index];
      max_inlined_method_index = std::max(max_inlined_method_index, inline_entry.method_index);
      max_inlined_dex_pc = std::max(max_inlined_dex_pc, inline_entry.dex_pc);
      max_inlined_invoke_type = std::max(max_inlined_invoke_type,
                                         static_cast<uint32_t>(inline_entry.invoke_type));
      max_inlined_num_dex_registers = std::max(max_inlined_num_dex_registers,
                                               inline_entry.num_dex_registers);
    }
  }

  // Decide on inline info encoding.
  InlineInfoEncoding inline_info_encoding;
  inline_info_encoding.SetFromSizes(max_inlined_method_index,
                                    max_inlined_dex_pc,
                                    max_inlined_invoke_type,
                                    max_inlined_num_dex_registers);

  // Encode and deduplicate inline info entries - depends on the inline info encoding.
  size_t inline_infos_count = PrepareInlineInfos(inline_info_encoding);
  size_t inline_infos_size = inline_infos_count * inline_info_encoding.GetEntrySize();

  // Encode dex register locations.
  size_t dex_register_maps_size = PrepareDexRegisterMaps();

  // Decide on stack map encoding - depends on the two encoded sections above.
  StackMapEncoding stack_map_encoding;
  size_t stack_map_size = stack_map_encoding.SetFromSizes(max_native_pc_offset,
                                                          max_dex_pc,
                                                          dex_register_maps_size,
                                                          inline_infos_count,
                                                          max_register_mask,
                                                          max_stack_mask_bits);
  size_t stack_maps_size = stack_map_size * inputs_->stack_maps_.size();

  // Sanity check - all stack maps should have the same number of dex registers (or zero).
  for (const StackMapEntry& entry : inputs_->stack_maps_) {
    if (entry.dex_register_map_offset != StackMap::kNoDexRegisterMap) {
      DCHECK_EQ(entry.num_dex_registers, max_num_dex_registers);
    } else {
      DCHECK_EQ(entry.num_dex_registers, 0u);
      DCHECK_EQ(entry.num_dex_registers_including_inlined, 0u);
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

  for (size_t i = 0, e = inputs_->stack_maps_.size(); i < e; ++i) {
    StackMap stack_map = code_info.GetStackMapAt(i);
    const StackMapEntry& entry = inputs_->stack_maps_[i];
    stack_map.SetNativePcOffset(entry.native_pc_offset);
    stack_map.SetDexPc(entry.dex_pc);
    stack_map.SetDexRegisterMapOffset(entry.dex_register_map_offset);
    stack_map.SetInlineInfoIndex(entry.inline_info_index);
    stack_map.SetRegisterMask(entry.register_mask);
    size_t number_of_stack_mask_bits = stack_map.GetNumberOfStackMaskBits();
    for (size_t bit = 0; bit < number_of_stack_mask_bits; bit++) {
      stack_map.SetStackMaskBit(bit, entry.stack_mask->IsBitSet(bit));
    }
  }

  if (encoded_dex_register_maps_.size() > 0) {
    code_info.dex_register_maps_region_.CopyFrom(0, MemoryRegion(
        encoded_dex_register_maps_.data(), encoded_dex_register_maps_.size()));
  }
  if (encoded_inline_infos_.size() > 0) {
    code_info.inline_infos_region_.CopyFrom(0, MemoryRegion(
        encoded_inline_infos_.data(), encoded_inline_infos_.size()));
  }
}

size_t StackMapStream::Encoder::PrepareInlineInfos(const InlineInfoEncoding& encoding) {
  // Preallocate memory since we do not want it to move (the dedup map will point into it).
  encoded_inline_infos_.resize(inputs_->inline_infos_.size() * encoding.GetEntrySize());
  std::unordered_map<ArrayRef<uint8_t>, size_t, FNVHash<ArrayRef<uint8_t>>> dedup_map(
      inputs_->inline_infos_.size(), FNVHash<ArrayRef<uint8_t>>());

  // Encode and deduplicate all inline info entries.
  const size_t entry_size = encoding.GetEntrySize();
  size_t inline_info_count = 0;
  for (size_t s = 0; s < inputs_->stack_maps_.size(); ++s) {
    const StackMapEntry& entry = inputs_->stack_maps_[s];
    if (entry.inlining_depth != 0) {
      ArrayRef<uint8_t> encoded(&encoded_inline_infos_[inline_info_count * entry_size],
                                entry.inlining_depth * entry_size);
      DCHECK_LE(&*encoded.end(), &*encoded_inline_infos_.end());
      InlineInfo inline_info(s, &encoding, MemoryRegion(encoded.data(), encoded.size()));
      inline_info.SetDepth(entry.inlining_depth);
      for (size_t depth = 0; depth < entry.inlining_depth; ++depth) {
        size_t inline_info_index = entry.inline_infos_start_index + depth;
        const InlineInfoEntry& inline_entry = inputs_->inline_infos_[inline_info_index];
        inline_info.SetMethodIndexAtDepth(depth, inline_entry.method_index);
        inline_info.SetDexPcAtDepth(depth, inline_entry.dex_pc);
        inline_info.SetInvokeTypeAtDepth(depth, inline_entry.invoke_type);
        inline_info.SetNumberOfDexRegistersAtDepth(depth, inline_entry.num_dex_registers);
      }
      const auto& it = dedup_map.emplace(encoded, inline_info_count);
      if (it.second /* inserted */) {
        entry.inline_info_index = inline_info_count;
        inline_info_count += entry.inlining_depth;
      } else {
        entry.inline_info_index = it.first->second;
      }
    } else {
      DCHECK_EQ(entry.inline_info_index, StackMap::kNoInlineInfo);
    }
  }
  encoded_inline_infos_.resize(inline_info_count * entry_size);  // Trim.
  return inline_info_count;
}

// Write dex register locations for all stack maps (including inlined registers).
// The locations are written only when they are different from their last state.
// Each map starts with bitmask which marks the modified locations.
size_t StackMapStream::Encoder::PrepareDexRegisterMaps() {
  DCHECK(encoded_dex_register_maps_.empty());
  std::vector<DexRegisterLocation> locations;  // Last state. Never shrinks.
  std::vector<size_t> last_update;  // Stack map index of last location update.
  for (size_t s = 0; s < inputs_->stack_maps_.size(); ++s) {
    const StackMapEntry& entry = inputs_->stack_maps_[s];
    size_t num_locations = entry.num_dex_registers_including_inlined;
    locations.resize(std::max(locations.size(), num_locations));
    last_update.resize(std::max(last_update.size(), num_locations));

    // Allocate enough memory for encoded locations (overestimate).
    const size_t old_size = encoded_dex_register_maps_.size();
    const size_t bitmap_size = RoundUp(num_locations, kBitsPerByte) / kBitsPerByte;
    const size_t locations_size = num_locations * DexRegisterLocation::kMaximumEncodedSize;
    encoded_dex_register_maps_.resize(old_size + bitmap_size + locations_size);
    MemoryRegion region(encoded_dex_register_maps_.data() + old_size, bitmap_size + locations_size);

    // Write the bitmap and encode register locations (if modified since last time).
    size_t num_used_bytes = bitmap_size;
    bool any_bit_set = false;
    for (size_t r = 0; r < num_locations; r++) {
      const DexRegisterLocation& dex_register_location =
          inputs_->dex_register_locations_[entry.dex_register_locations_start_index + r];
      // We also need to need to refresh live registers on regular basis to put an upper bound
      // on the look up time in the runtime. We do not refresh dead ones since that is the default.
      bool is_old = s - last_update[r] >= StackMap::kMaxNumOfDexRegisterMapToSearch;
      if (locations[r] != dex_register_location || (is_old && dex_register_location.IsLive())) {
        locations[r] = dex_register_location;
        last_update[r] = s;
        region.StoreBit(r, true);
        any_bit_set = true;
        dex_register_location.Encode(&region, &num_used_bytes);
      }
    }

    if (num_locations == 0) {
      entry.dex_register_map_offset = StackMap::kNoDexRegisterMap;
    } else if (!any_bit_set) {
      // Optimization which allows us to remove the bitmap as well.
      entry.dex_register_map_offset = StackMap::kSameDexRegisterMap;
      num_used_bytes = 0;
    } else {
      entry.dex_register_map_offset = old_size;
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
    const StackMap& stack_map = code_info.GetStackMapAt(s);
    const StackMapEntry& entry = stack_maps_[s];

    // Check main stack map fields.
    DCHECK_EQ(stack_map.GetNativePcOffset(), entry.native_pc_offset);
    DCHECK_EQ(stack_map.GetDexPc(), entry.dex_pc);
    DCHECK_EQ(stack_map.GetDexRegisterMapOffset(), entry.dex_register_map_offset);
    DCHECK_EQ(stack_map.GetInlineInfoIndex(), entry.inline_info_index);
    DCHECK_EQ(stack_map.GetRegisterMask(), entry.register_mask);
    size_t number_of_stack_mask_bits = stack_map.GetNumberOfStackMaskBits();
    DCHECK_GE(number_of_stack_mask_bits, entry.stack_mask->GetNumberOfBits());
    for (size_t b = 0; b < number_of_stack_mask_bits; b++) {
      DCHECK_EQ(stack_map.GetStackMaskBit(b), entry.stack_mask->IsBitSet(b));
    }

    // Check dex register map.
    DCHECK_EQ(code_info.GetNumberOfDexRegistersOf(stack_map, false /* inlude_inline */),
              entry.num_dex_registers);
    DCHECK_EQ(code_info.GetNumberOfDexRegistersOf(stack_map, true /* inlude_inline  */),
              entry.num_dex_registers_including_inlined);
    DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(
        stack_map, entry.num_dex_registers);
    DCHECK_EQ(dex_register_map.size(), entry.num_dex_registers);
    for (size_t r = 0; r < entry.num_dex_registers; r++) {
      size_t index = entry.dex_register_locations_start_index + r;
      DCHECK_EQ(dex_register_map.GetLocation(r), dex_register_locations_[index]);
    }

    // Check inline info.
    DCHECK_EQ(stack_map.HasInlineInfo(), (entry.inlining_depth != 0));
    if (entry.inlining_depth != 0) {
      InlineInfo inline_info = code_info.GetInlineInfoOf(stack_map);
      DCHECK_EQ(inline_info.GetDepth(), entry.inlining_depth);
      for (size_t d = 0; d < entry.inlining_depth; ++d) {
        size_t inline_info_index = entry.inline_infos_start_index + d;
        const InlineInfoEntry& inline_entry = inline_infos_[inline_info_index];
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
          DCHECK_EQ(inlined_dex_register_map.GetLocation(r), dex_register_locations_[index]);
        }
      }
    }
  }
}

}  // namespace art
