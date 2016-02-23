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

DexRegisterMap CodeInfo::GetDexRegisterMapOf(StackMap for_stack_map,
                                             uint32_t num_dex_regs ATTRIBUTE_UNUSED,
                                             bool include_inlined) const {
  // Allocate buffer to hold the decoded register locations.
  // Inlined registers are appended at the end of the buffer.
  size_t num_locations = GetNumberOfDexRegistersOf(for_stack_map, include_inlined);
  dchecked_vector<DexRegisterLocation> locations;
  locations.resize(num_locations);

  // We need to note whether we have already decoded the most recent location.
  size_t num_finished = 0;
  std::vector<bool> finished;
  finished.resize(num_locations);

  // Search through the maps backward to find the most recently modified location.
  // We enforce an upper bound on the number of stack maps that need to be searched.
  // If register is not mentioned in this period then it may be assumed to be dead.
  int32_t stack_map_index = GetIndexOfStackMap(for_stack_map);
  int32_t end = std::max(0, stack_map_index - StackMap::kMaxNumOfDexRegisterMapToSearch);
  for (; num_finished < num_locations && stack_map_index >= end; --stack_map_index) {
    StackMap stack_map = GetStackMapAt(stack_map_index);
    uint32_t offset = stack_map.GetDexRegisterMapOffset();
    if (offset != StackMap::kNoDexRegisterMap && offset != StackMap::kSameDexRegisterMap) {
      uint32_t bitmap_size_in_bits = GetNumberOfDexRegistersOf(stack_map, true /* inlined */);
      uint32_t bitmap_size_in_bytes = RoundUp(bitmap_size_in_bits, kBitsPerByte) / kBitsPerByte;
      MemoryRegion bitmap = dex_register_maps_region_.Subregion(offset, bitmap_size_in_bytes);
      size_t decode_offset = offset + bitmap_size_in_bytes;
      for (size_t r = 0; r < bitmap_size_in_bits && r < num_locations; ++r) {
        if (bitmap.LoadBit(r)) {
          if (!finished[r]) {
            locations[r] = DexRegisterLocation::Decode(&dex_register_maps_region_, &decode_offset);
            finished[r] = true;
            num_finished++;
          } else {
            // Skip the encoded register. Separate branch so the compiler can inline it.
            DexRegisterLocation::Decode(&dex_register_maps_region_, &decode_offset);
          }
        }
      }
    }
  }

  return DexRegisterMap(std::move(locations));
}

// TODO: Consider removing this method and make DexRegisterMap aware of inlined registers instead.
DexRegisterMap CodeInfo::GetDexRegisterMapAtDepth(size_t depth,
                                                  InlineInfo inline_info,
                                                  uint32_t num_dex_regs ATTRIBUTE_UNUSED) const {
  StackMap stack_map = GetStackMapAt(inline_info.stack_map_index_);
  DexRegisterMap dex_register_map = GetDexRegisterMapOf(stack_map,
                                                        header_.number_of_dex_registers,
                                                        true /* inlude_inlined */);

  dchecked_vector<DexRegisterLocation> locations;
  locations.resize(inline_info.GetNumberOfDexRegistersAtDepth(depth));
  if (stack_map.HasDexRegisterMap()) {
    uint32_t first_location = header_.number_of_dex_registers;
    for (uint32_t d = 0; d < depth; ++d) {
      first_location += inline_info.GetNumberOfDexRegistersAtDepth(d);
    }
    for (size_t i = 0; i < locations.size(); ++i) {
      locations[i] = dex_register_map.GetLocation(first_location + i);
    }
  }
  return DexRegisterMap(std::move(locations));
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
      << " (native_pc_bit_offset=" << static_cast<uint32_t>(kNativePcBitOffset)
      << ", dex_pc_bit_offset=" << static_cast<uint32_t>(dex_pc_bit_offset_)
      << ", dex_register_map_bit_offset=" << static_cast<uint32_t>(dex_register_map_bit_offset_)
      << ", inline_info_bit_offset=" << static_cast<uint32_t>(inline_info_bit_offset_)
      << ", register_mask_bit_offset=" << static_cast<uint32_t>(register_mask_bit_offset_)
      << ", stack_mask_bit_offset=" << static_cast<uint32_t>(stack_mask_bit_offset_)
      << ")\n";
}

void InlineInfoEncoding::Dump(VariableIndentationOutputStream* vios) const {
  vios->Stream()
      << "InlineInfoEncoding"
      << " (method_index_bit_offset=" << static_cast<uint32_t>(kMethodIndexBitOffset)
      << ", dex_pc_bit_offset=" << static_cast<uint32_t>(dex_pc_bit_offset_)
      << ", invoke_type_bit_offset=" << static_cast<uint32_t>(invoke_type_bit_offset_)
      << ", number_of_dex_registers_bit_offset=" <<
          static_cast<uint32_t>(number_of_dex_registers_bit_offset_)
      << ", total_bit_size=" << static_cast<uint32_t>(total_bit_size_)
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
      << ", dex_register_map_offset=0x" << GetDexRegisterMapOffset()
      << ", inline_indo_index=0x" << GetInlineInfoIndex()
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
        << ", num_dex_registers=" << GetNumberOfDexRegistersAtDepth(i)
        << ")\n";
    DexRegisterMap dex_register_map =
        code_info.GetDexRegisterMapAtDepth(i, *this, GetNumberOfDexRegistersAtDepth(i));
    ScopedIndentation indent1(vios);
    dex_register_map.Dump(vios);
  }
}

}  // namespace art
