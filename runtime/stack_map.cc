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

#include "stack_map.h"

#include <stdint.h>

#include "indenter.h"
#include "invoke_type.h"

namespace art {

std::ostream& operator<<(std::ostream& stream, const DexRegisterLocation::Kind& kind) {
  stream << DexRegisterLocation::PrettyDescriptor(kind);
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const DexRegisterLocation& location) {
  return stream << location.GetKind() << "(" << location.GetValue() << ")";
}

void DexRegisterLocation::Encode(const MemoryRegion* region, size_t* offset) const {
  uint32_t value;
  if (kind_ == Kind::kInStack) {
    // Instead of storing stack offsets expressed in bytes for short stack locations,
    // store slot offsets. This allows us to encode more values using the short form.
    DCHECK_EQ(value_ % kFrameSlotSize, 0);
    value = value_ / kFrameSlotSize;  // Signed arithmetic and then cast to uint32_t.
  } else {
    value = value_;  // Remove sign.
  }
  if ((kind_ == Kind::kInStack || kind_ == Kind::kConstant) && (value >= kValueMask)) {
    // The value is too large.  Encode it as five bytes.
    uint32_t encoded = (static_cast<uint32_t>(kind_) << kValueBits) | kValueMask;
    region->StoreUnaligned<uint8_t>((*offset)++, dchecked_integral_cast<uint8_t>(encoded));
    region->StoreUnaligned<uint32_t>(*offset, value);
    *offset += 4;
  } else {
    // Encode as single byte.
    DCHECK_EQ(value >> kValueBits, 0u);
    uint32_t encoded = (static_cast<uint32_t>(kind_) << kValueBits) | value;
    region->StoreUnaligned<uint8_t>((*offset)++, dchecked_integral_cast<uint8_t>(encoded));
  }
}

DexRegisterLocation DexRegisterLocation::Decode(const MemoryRegion* region, size_t* offset) {
  uint8_t encoded = region->LoadUnaligned<uint8_t>((*offset)++);
  Kind kind = static_cast<Kind>(encoded >> kValueBits);
  int32_t value = encoded & kValueMask;
  if ((kind == Kind::kInStack || kind == Kind::kConstant) && (value == kValueMask)) {
    value = region->LoadUnaligned<uint32_t>(*offset);
    *offset += 4;
  }
  if (kind == Kind::kInStack) {
    value *= kFrameSlotSize;
  }
  return DexRegisterLocation(kind, value);
}

uint32_t StackMap::LoadAt(size_t bit_count, size_t bit_offset) const {
  DCHECK_LE(bit_count, 32u);
  DCHECK_LE(bit_offset + bit_count, region_.size_in_bits());
  if (bit_count == 0) {
    return 0;
  }
  // Ensure that the bit offset is in range 0-7.
  uint8_t* address_bytes = region_.start() + bit_offset / kBitsPerByte;
  uint32_t address_bits = bit_offset & (kBitsPerByte - 1);
  // Load the value (reading only the required bytes).
  uint32_t value = *address_bytes++ >> address_bits;
  for (size_t loaded_bits = 8; loaded_bits < address_bits + bit_count; loaded_bits += 8) {
    value |= *address_bytes++ << (loaded_bits - address_bits);
  }
  // Clear unwanted most significant bits.
  const uint32_t mask = MaxInt<uint32_t>(bit_count);
  value &= mask;
  // Compare with the trivial bit-for-bit implementation.
  DCHECK_EQ(value, region_.LoadBits(bit_offset, bit_count));
  return value;
}

void StackMap::StoreAt(size_t bit_count, size_t bit_offset, uint32_t value) {
  region_.StoreBits(bit_offset, value, bit_count);
}

std::vector<DexRegisterMap> CodeInfo::GetDexRegisterMaps() const {
  std::vector<DexRegisterMap> result;
  size_t encoded_offset = 0;
  dchecked_vector<DexRegisterLocation> locations;
  StackMapEncoding encoding = ExtractEncoding();
  for (size_t i = 0; i < GetNumberOfStackMaps(); ++i) {
    StackMap stack_map = GetStackMapAt(i, encoding);
    DecodeNextDexRegisterMap(stack_map, &encoded_offset, &locations);
    dchecked_vector<DexRegisterLocation> copy(
        locations.begin(), locations.begin() + GetNumberOfDexRegistersOf(stack_map));
    result.push_back(DexRegisterMap(std::move(copy)));
  }
  return result;
}

DexRegisterMap CodeInfo::GetDexRegisterMapOf(StackMap for_stack_map,
                                             const StackMapEncoding& encoding,
                                             uint32_t num_dex_regs ATTRIBUTE_UNUSED) const {
  size_t encoded_offset = 0;
  dchecked_vector<DexRegisterLocation> locations;
  for (size_t i = 0; i < GetNumberOfStackMaps(); ++i) {
    StackMap stack_map = GetStackMapAt(i, encoding);
    DecodeNextDexRegisterMap(stack_map, &encoded_offset, &locations);
    if (stack_map.Equals(for_stack_map)) {
      locations.resize(GetNumberOfDexRegistersOf(stack_map));  // Trim.
      return DexRegisterMap(std::move(locations));
    }
  }
  LOG(FATAL) << "Stack map not found";
  UNREACHABLE();
}

DexRegisterMap CodeInfo::GetDexRegisterMapAtDepth(size_t depth,
                                                  InlineInfo for_inline_info,
                                                  const StackMapEncoding& encoding,
                                                  uint32_t num_dex_regs ATTRIBUTE_UNUSED) const {
  size_t encoded_offset = 0;
  dchecked_vector<DexRegisterLocation> locations;
  for (size_t i = 0; i < GetNumberOfStackMaps(); ++i) {
    StackMap stack_map = GetStackMapAt(i, encoding);
    DecodeNextDexRegisterMap(stack_map, &encoded_offset, &locations);
    if (stack_map.HasInlineInfo(encoding)) {
      InlineInfo inline_info = GetInlineInfoOf(stack_map, encoding);
      if (inline_info.Equals(for_inline_info)) {
        // Trim the result to just the registers of the inlined method.
        size_t first_dex_register = GetNumberOfDexRegistersOf(stack_map);
        for (size_t d = 0; d < depth; ++d) {
          first_dex_register += inline_info.GetNumDexRegistersAtDepth(d);
        }
        locations.erase(locations.begin(), locations.begin() + first_dex_register);
        locations.resize(inline_info.GetNumDexRegistersAtDepth(depth));
        return DexRegisterMap(std::move(locations));
      }
    }
  }
  LOG(FATAL) << "Stack map not found";
  UNREACHABLE();
}

size_t CodeInfo::DecodeNextDexRegisterMap(const StackMap& stack_map,
                                          size_t* encoded_offset,
                                          dchecked_vector<DexRegisterLocation>* locations) const {
  StackMapEncoding encoding = ExtractEncoding();

  // Count the total number of dex registers including inlined ones.
  size_t num_locations = GetNumberOfDexRegistersOf(stack_map);
  if (stack_map.HasInlineInfo(encoding)) {
    InlineInfo inline_info = GetInlineInfoOf(stack_map, encoding);
    size_t depth = inline_info.GetDepth();
    for (size_t d = 0; d < depth; ++d) {
      num_locations += inline_info.GetNumDexRegistersAtDepth(d);
    }
  }
  locations->resize(std::max(locations->size(), num_locations));

  // Decode register locations which changed since last stack map.
  if ((stack_map.GetFlags(encoding) & StackMap::Flags::kSameDexRegisterMap) == 0) {
    size_t bitmap_size = RoundUp(num_locations, kBitsPerByte) / kBitsPerByte;
    MemoryRegion bitmap = dex_register_maps_region_.Subregion(*encoded_offset, bitmap_size);
    *encoded_offset += bitmap_size;
    for (size_t r = 0; r < num_locations; ++r) {
      if (bitmap.LoadBit(r)) {
        (*locations)[r] = DexRegisterLocation::Decode(&dex_register_maps_region_, encoded_offset);
      }
    }
  }

  return num_locations;
}

void StackMapEncoding::Dump(VariableIndentationOutputStream* vios) const {
  vios->Stream()
      << "StackMapEncoding"
      << " (native_pc_bit_offset=" << NativePcBitOffset()
      << ", dex_pc_bit_offset=" << DexPcBitOffset()
      << ", flags_bit_offset=" << FlagsBitOffset()
      << ", inline_info_bit_offset=" << InlineInfoBitOffset()
      << ", register_mask_bit_offset=" << RegisterMaskBitOffset()
      << ", stack_mask_bit_offset=" << StackMaskBitOffset()
      << ")\n";
}

void CodeInfoHeader::Dump(VariableIndentationOutputStream* vios) const {
  vios->Stream()
      << "CodeInfoHeader"
      << " (number_of_stack_maps=" << number_of_stack_maps
      << ", stack_map_size=" << stack_map_size
      << ", inline_infos_size=" << inline_infos_size
      << ", number_of_dex_registers=" << number_of_dex_registers
      << ", dex_register_maps_size=" << dex_register_maps_size
      << ")\n";
}

void CodeInfo::Dump(VariableIndentationOutputStream* vios,
                    uint32_t code_offset,
                    uint16_t number_of_dex_registers,
                    bool dump_stack_maps) const {
  StackMapEncoding encoding = ExtractEncoding();
  header_.Dump(vios);
  encoding.Dump(vios);
  ScopedIndentation indent1(vios);
  // Display stack maps along with (live) Dex register maps.
  if (dump_stack_maps) {
    for (size_t i = 0; i < GetNumberOfStackMaps(); ++i) {
      StackMap stack_map = GetStackMapAt(i, encoding);
      stack_map.Dump(vios,
                     *this,
                     encoding,
                     code_offset,
                     number_of_dex_registers,
                     " " + std::to_string(i));
    }
  }
}

void DexRegisterMap::Dump(VariableIndentationOutputStream* vios) const {
  for (size_t j = 0; j < locations_.size(); ++j) {
    const DexRegisterLocation& location = locations_[j];
    if (location.GetKind() != DexRegisterLocation::Kind::kNone) {
      ScopedIndentation indent1(vios);
      vios->Stream() << "v" << j << ": " << location << '\n';
    }
  }
}

void StackMap::Dump(VariableIndentationOutputStream* vios,
                    const CodeInfo& code_info,
                    const StackMapEncoding& encoding,
                    uint32_t code_offset,
                    uint16_t number_of_dex_registers,
                    const std::string& header_suffix) const {
  vios->Stream()
      << "StackMap" << header_suffix
      << std::hex
      << " [native_pc=0x" << code_offset + GetNativePcOffset(encoding) << "]"
      << " (native_pc_offset=0x" << GetNativePcOffset(encoding)
      << ", dex_pc=0x" << GetDexPc(encoding)
      << ", flags=0x" << GetFlags(encoding)
      << ", inline_descriptor_offset=0x" << GetInlineDescriptorOffset(encoding)
      << ", register_mask=0x" << GetRegisterMask(encoding)
      << std::dec
      << ", stack_mask=0b";
  for (size_t i = 0, e = GetNumberOfStackMaskBits(encoding); i < e; ++i) {
    vios->Stream() << (GetStackMaskBit(encoding, e - i - 1) ? 1 : 0);
  }
  vios->Stream() << ")\n";
  if (HasDexRegisterMap(encoding)) {
    DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(
        *this, encoding, number_of_dex_registers);
    dex_register_map.Dump(vios);
  }
  if (HasInlineInfo(encoding)) {
    InlineInfo inline_info = code_info.GetInlineInfoOf(*this, encoding);
    inline_info.Dump(vios, code_info);
  }
}

void InlineInfo::Dump(VariableIndentationOutputStream* vios, const CodeInfo& code_info) const {
  vios->Stream() << "InlineInfo with depth " << static_cast<uint32_t>(GetDepth()) << "\n";
  for (size_t i = 0; i < GetDepth(); ++i) {
    vios->Stream()
        << " At depth " << i
        << std::hex
        << " (dex_pc=0x" << GetDexPcAtDepth(i)
        << std::dec
        << ", method_index=" << GetMethodIndexAtDepth(i)
        << ", invoke_type=" << static_cast<InvokeType>(GetInvokeTypeAtDepth(i))
        << ")\n";
    if (HasDexRegisterMapAtDepth(i)) {
      StackMapEncoding encoding = code_info.ExtractEncoding();
      DexRegisterMap dex_register_map =
          code_info.GetDexRegisterMapAtDepth(i, *this, encoding, GetNumDexRegistersAtDepth(i));
      ScopedIndentation indent1(vios);
      dex_register_map.Dump(vios);
    }
  }
}

}  // namespace art
