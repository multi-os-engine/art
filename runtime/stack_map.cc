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
  using Kind = DexRegisterLocation::Kind;
  switch (kind) {
    case Kind::kNone:
      return stream << "none";
    case Kind::kInStack:
      return stream << "in stack";
    case Kind::kInRegister:
      return stream << "in register";
    case Kind::kInRegisterHigh:
      return stream << "in register high";
    case Kind::kInFpuRegister:
      return stream << "in fpu register";
    case Kind::kInFpuRegisterHigh:
      return stream << "in fpu register high";
    case Kind::kConstant:
      return stream << "as constant";
  }
  return stream << "Kind<" << static_cast<uint32_t>(kind) << ">";
}

std::ostream& operator<<(std::ostream& stream, const DexRegisterLocation& location) {
  return stream << location.GetKind() << " (" << location.GetValue() << ")";
}

std::vector<DexRegisterMap> CodeInfo::GetDexRegisterMaps() const {
  std::vector<DexRegisterMap> result;
  size_t encoded_offset = 0;
  dchecked_vector<DexRegisterLocation> locations;
  for (size_t i = 0; i < GetNumberOfStackMaps(); ++i) {
    StackMap stack_map = GetStackMapAt(i);
    size_t count = DecodeNextDexRegisterMap(stack_map, &encoded_offset, &locations);
    result.push_back(DexRegisterMap(
        dchecked_vector<DexRegisterLocation>(locations.begin(), locations.begin() + count)));
  }
  return result;
}

DexRegisterMap CodeInfo::GetDexRegisterMapOf(StackMap for_stack_map,
                                             uint32_t num_dex_regs ATTRIBUTE_UNUSED) const {
  size_t encoded_offset = 0;
  dchecked_vector<DexRegisterLocation> locations;
  for (size_t i = 0; i < GetNumberOfStackMaps(); ++i) {
    StackMap stack_map = GetStackMapAt(i);
    DecodeNextDexRegisterMap(stack_map, &encoded_offset, &locations);
    if (stack_map.Equals(for_stack_map)) {
      locations.resize(GetNumberOfDexRegistersOf(stack_map, 0 /* max_depth */));  // Trim.
      return DexRegisterMap(std::move(locations));
    }
  }
  LOG(FATAL) << "Stack map not found";
  UNREACHABLE();
}

DexRegisterMap CodeInfo::GetDexRegisterMapAtDepth(size_t depth,
                                                  InlineInfo for_inline_info,
                                                  uint32_t num_dex_regs ATTRIBUTE_UNUSED) const {
  size_t encoded_offset = 0;
  dchecked_vector<DexRegisterLocation> locations;
  for (size_t i = 0; i < GetNumberOfStackMaps(); ++i) {
    StackMap stack_map = GetStackMapAt(i);
    DecodeNextDexRegisterMap(stack_map, &encoded_offset, &locations);
    if (stack_map.HasInlineInfo()) {
      InlineInfo inline_info = GetInlineInfoOf(stack_map);
      if (inline_info.Equals(for_inline_info)) {
        // Trim the result to just the registers of the inlined method.
        size_t start = GetNumberOfDexRegistersOf(stack_map, depth /* max_depth */);
        locations.resize(start + inline_info.GetNumberOfDexRegistersAtDepth(depth));  // Trim end.
        locations.erase(locations.begin(), locations.begin() + start);  // Trim start.
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
  // Grow the locations vector to the needed size (but never shrink it).
  // Inlined registers (if any) are encoded together with the main registers.
  size_t num_locations = GetNumberOfDexRegistersOf(stack_map, 0xFFFFFFFF /* max_depth */);
  locations->resize(std::max(locations->size(), num_locations));

  // Decode register locations which changed since last stack map.
  if ((stack_map.GetFlags() & StackMap::Flags::kSameDexRegisterMap) == 0) {
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

void CodeInfoHeader::Dump(VariableIndentationOutputStream* vios) const {
  vios->Stream()
      << "CodeInfoHeader"
      << " (number_of_stack_maps=" << number_of_stack_maps
      << ", stack_map_size=" << stack_map_size
      << ", number_of_dex_registers=" << number_of_dex_registers
      << ", dex_register_maps_size=" << dex_register_maps_size
      << ", inline_infos_size=" << inline_infos_size
      << ")\n";
  ScopedIndentation indent(vios);
  if (stack_map_encoding != nullptr) {
    stack_map_encoding->Dump(vios);
  }
  if (inline_info_encoding != nullptr) {
    inline_info_encoding->Dump(vios);
  }
}

void StackMapEncoding::Dump(VariableIndentationOutputStream* vios) const {
  vios->Stream()
      << "StackMapEncoding"
      << " (native_pc_bit_offset=" << kNativePcBitOffset
      << ", dex_pc_bit_offset=" << dex_pc_bit_offset_
      << ", flags_bit_offset=" << flags_bit_offset_
      << ", inline_info_bit_offset=" << inline_info_bit_offset_
      << ", register_mask_bit_offset=" << register_mask_bit_offset_
      << ", stack_mask_bit_offset=" << stack_mask_bit_offset_
      << ")\n";
}

void InlineInfoEncoding::Dump(VariableIndentationOutputStream* vios) const {
  vios->Stream()
      << "InlineInfoEncoding"
      << " (method_index_bit_offset=" << kMethodIndexBitOffset
      << ", dex_pc_bit_offset=" << dex_pc_bit_offset_
      << ", invoke_type_bit_offset=" << invoke_type_bit_offset_
      << ", number_of_dex_registers_bit_offset=" << number_of_dex_registers_bit_offset_
      << ", total_bit_size=" << total_bit_size_
      << ")\n";
}

void CodeInfo::Dump(VariableIndentationOutputStream* vios,
                    uint32_t code_offset,
                    uint16_t number_of_dex_registers,
                    bool dump_stack_maps) const {
  size_t number_of_stack_maps = GetNumberOfStackMaps();
  vios->Stream() << "Optimized CodeInfo\n";
  ScopedIndentation indent1(vios);
  header_.Dump(vios);
  // Display stack maps along with (live) Dex register maps.
  if (dump_stack_maps) {
    for (size_t i = 0; i < number_of_stack_maps; ++i) {
      StackMap stack_map = GetStackMapAt(i);
      stack_map.Dump(vios,
                     *this,
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
                    uint32_t code_offset,
                    uint16_t number_of_dex_registers,
                    const std::string& header_suffix) const {
  vios->Stream()
      << "StackMap" << header_suffix
      << std::hex
      << " [native_pc=0x" << code_offset + GetNativePcOffset() << "]"
      << " (native_pc_offset=0x" << GetNativePcOffset()
      << ", dex_pc=0x" << GetDexPc()
      << ", flags=0x" << GetFlags()
      << ", inline_descriptor_offset=0x" << GetInlineDescriptorOffset()
      << ", register_mask=0x" << GetRegisterMask()
      << std::dec
      << ", stack_mask=0b";
  for (size_t i = 0, e = GetNumberOfStackMaskBits(); i < e; ++i) {
    vios->Stream() << (GetStackMaskBit(e - i - 1) ? 1 : 0);
  }
  vios->Stream() << ")\n";
  if (HasDexRegisterMap()) {
    DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(
        *this, number_of_dex_registers);
    dex_register_map.Dump(vios);
  }
  if (HasInlineInfo()) {
    InlineInfo inline_info = code_info.GetInlineInfoOf(*this);
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
    DexRegisterMap dex_register_map =
        code_info.GetDexRegisterMapAtDepth(i, *this, GetNumberOfDexRegistersAtDepth(i));
    ScopedIndentation indent1(vios);
    dex_register_map.Dump(vios);
  }
}

}  // namespace art
