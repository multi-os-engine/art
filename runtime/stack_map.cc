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

namespace art {

constexpr size_t DexRegisterLocationCatalog::kNoLocationEntryIndex;
constexpr uint32_t StackMap::kNoDexRegisterMap;
constexpr uint32_t StackMap::kNoInlineInfo;

DexRegisterLocation::Kind DexRegisterMap::GetLocationInternalKind(uint16_t dex_register_number,
                                                                  uint16_t number_of_dex_registers,
                                                                  const CodeInfo& code_info,
                                                                  const StackMapEncoding& enc) const {
  DexRegisterLocationCatalog dex_register_location_catalog =
      code_info.GetDexRegisterLocationCatalog(enc);
  size_t location_catalog_entry_index = GetLocationCatalogEntryIndex(
      dex_register_number,
      number_of_dex_registers,
      code_info.GetNumberOfDexRegisterLocationCatalogEntries());
  return dex_register_location_catalog.GetLocationInternalKind(location_catalog_entry_index);
}

DexRegisterLocation DexRegisterMap::GetDexRegisterLocation(uint16_t dex_register_number,
                                                           uint16_t number_of_dex_registers,
                                                           const CodeInfo& code_info,
                                                           const StackMapEncoding& enc) const {
  DexRegisterLocationCatalog dex_register_location_catalog =
      code_info.GetDexRegisterLocationCatalog(enc);
  size_t location_catalog_entry_index = GetLocationCatalogEntryIndex(
      dex_register_number,
      number_of_dex_registers,
      code_info.GetNumberOfDexRegisterLocationCatalogEntries());
  return dex_register_location_catalog.GetDexRegisterLocation(location_catalog_entry_index);
}

static void DumpRegisterMapping(std::ostream& os,
                                size_t dex_register_num,
                                DexRegisterLocation location,
                                const std::string& prefix = "v",
                                const std::string& suffix = "") {
  Indenter indent_filter(os.rdbuf(), kIndentChar, kIndentBy1Count);
  std::ostream indented_os(&indent_filter);
  indented_os << prefix << dex_register_num << ": "
              << DexRegisterLocation::PrettyDescriptor(location.GetInternalKind())
              << " (" << location.GetValue() << ")" << suffix << '\n';
}

void CodeInfo::Dump(std::ostream& os,
                    uint32_t code_offset,
                    uint16_t number_of_dex_registers,
                    bool dump_stack_maps) const {
  StackMapEncoding enc = ExtractEncoding();
  uint32_t code_info_size = GetOverallSize();
  size_t number_of_stack_maps = GetNumberOfStackMaps();
  Indenter indent_filter(os.rdbuf(), kIndentChar, kIndentBy1Count);
  std::ostream indented_os(&indent_filter);
  indented_os << "Optimized CodeInfo (size=" << code_info_size
              << ", number_of_dex_registers=" << number_of_dex_registers
              << ", number_of_stack_maps=" << number_of_stack_maps
              << ", has_inline_info=" << enc.HasInlineInfo()
              << ", number_of_bytes_for_inline_info=" << enc.NumberOfBytesForInlineInfo()
              << ", number_of_bytes_for_dex_register_map=" << enc.NumberOfBytesForDexRegisterMap()
              << ", number_of_bytes_for_dex_pc=" << enc.NumberOfBytesForDexPc()
              << ", number_of_bytes_for_native_pc=" << enc.NumberOfBytesForNativePc()
              << ", number_of_bytes_for_register_mask=" << enc.NumberOfBytesForRegisterMask()
              << ")\n";
  // Display the Dex register location catalog.
  GetDexRegisterLocationCatalog(enc).Dump(indented_os, *this);
  // Display stack maps along with (live) Dex register maps.
  if (dump_stack_maps) {
    for (size_t i = 0; i < number_of_stack_maps; ++i) {
      StackMap stack_map = GetStackMapAt(i, enc);
      stack_map.Dump(
          indented_os, *this, code_offset, number_of_dex_registers, " " + std::to_string(i));
    }
  }
  // TODO: Dump the stack map's inline information? We need to know more from the caller:
  //       we need to know the number of dex registers for each inlined method.
}

void DexRegisterLocationCatalog::Dump(std::ostream& os, const CodeInfo& code_info) {
  StackMapEncoding encoding = code_info.ExtractEncoding();
  size_t number_of_location_catalog_entries =
      code_info.GetNumberOfDexRegisterLocationCatalogEntries();
  size_t location_catalog_size_in_bytes = code_info.GetDexRegisterLocationCatalogSize(encoding);
  Indenter indent_filter(os.rdbuf(), kIndentChar, kIndentBy1Count);
  std::ostream indented_os(&indent_filter);
  indented_os
      << "DexRegisterLocationCatalog (number_of_entries=" << number_of_location_catalog_entries
      << ", size_in_bytes=" << location_catalog_size_in_bytes << ")\n";
  for (size_t i = 0; i < number_of_location_catalog_entries; ++i) {
    DexRegisterLocation location = GetDexRegisterLocation(i);
    DumpRegisterMapping(indented_os, i, location, "entry ");
  }
}

void DexRegisterMap::Dump(std::ostream& os,
                          const CodeInfo& code_info,
                          uint16_t number_of_dex_registers) const {
  StackMapEncoding encoding = code_info.ExtractEncoding();
  size_t number_of_location_catalog_entries =
      code_info.GetNumberOfDexRegisterLocationCatalogEntries();
  Indenter indent_filter(os.rdbuf(), kIndentChar, kIndentBy1Count);
  std::ostream indented_os(&indent_filter);
  // TODO: Display the bit mask of live Dex registers.
  for (size_t j = 0; j < number_of_dex_registers; ++j) {
    if (IsDexRegisterLive(j)) {
      size_t location_catalog_entry_index = GetLocationCatalogEntryIndex(
          j, number_of_dex_registers, number_of_location_catalog_entries);
      DexRegisterLocation location = GetDexRegisterLocation(j,
                                                            number_of_dex_registers,
                                                            code_info,
                                                            encoding);
      DumpRegisterMapping(
          indented_os, j, location, "v",
          "\t[entry " + std::to_string(static_cast<int>(location_catalog_entry_index)) + "]");
    }
  }
}

void StackMap::Dump(std::ostream& os,
                    const CodeInfo& code_info,
                    uint32_t code_offset,
                    uint16_t number_of_dex_registers,
                    const std::string& header_suffix) const {
  Indenter indent_filter(os.rdbuf(), kIndentChar, kIndentBy1Count);
  std::ostream indented_os(&indent_filter);
  indented_os << "StackMap" << header_suffix
              << std::hex
              << " [native_pc=0x" << code_offset + GetNativePcOffset() << "]"
              << " (dex_pc=0x" << GetDexPc()
              << ", native_pc_offset=0x" << GetNativePcOffset()
              << ", dex_register_map_offset=0x" << GetDexRegisterMapOffset()
              << ", inline_info_offset=0x" << GetInlineDescriptorOffset()
              << ", register_mask=0x" << GetRegisterMask()
              << std::dec
              << ", stack_mask=0b";
  MemoryRegion stack_mask = GetStackMask();
  for (size_t i = 0, e = stack_mask.size_in_bits(); i < e; ++i) {
    indented_os << stack_mask.LoadBit(e - i - 1);
  }
  indented_os << ")\n";
  if (HasDexRegisterMap()) {
    DexRegisterMap dex_register_map = code_info.GetDexRegisterMapOf(*this, number_of_dex_registers);
    dex_register_map.Dump(os, code_info, number_of_dex_registers);
  }
}

void InlineInfo::Dump(std::ostream& os,
                      const CodeInfo& code_info,
                      uint16_t number_of_dex_registers[]) const {
  Indenter indent_filter(os.rdbuf(), kIndentChar, kIndentBy1Count);
  std::ostream indented_os(&indent_filter);
  indented_os << "InlineInfo with depth " << static_cast<uint32_t>(GetDepth()) << "\n";

  for (size_t i = 0; i < GetDepth(); ++i) {
    indented_os << " At depth " << i
                << std::hex
                << " (dex_pc=0x" << GetDexPcAtDepth(i)
                << ", method_index=0x" << GetMethodIndexAtDepth(i)
                << ")\n";
    if (HasDexRegisterMapAtDepth(i)) {
      StackMapEncoding encoding = code_info.ExtractEncoding();
      DexRegisterMap dex_register_map =
          code_info.GetDexRegisterMapAtDepth(i, *this, number_of_dex_registers[i], encoding);
      dex_register_map.Dump(indented_os, code_info, number_of_dex_registers[i]);
    }
  }
}

}  // namespace art
