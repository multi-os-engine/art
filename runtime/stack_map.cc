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

namespace art {

constexpr size_t DexRegisterLocationSet::kNoLocationEntryIndex;

DexRegisterLocation::Kind DexRegisterMap::GetLocationInternalKind(uint16_t dex_register_number,
                                                                  uint16_t number_of_dex_registers,
                                                                  const CodeInfo& code_info) const {
  DexRegisterLocationSet dex_register_location_set = code_info.GetDexRegisterLocationSet();
  size_t location_set_entry_index = GetLocationSetEntryIndex(
      dex_register_number,
      number_of_dex_registers,
      code_info.GetNumberOfDexRegisterLocationSetEntries());
  return dex_register_location_set.GetLocationInternalKind(location_set_entry_index);
}

DexRegisterLocation DexRegisterMap::GetDexRegisterLocation(uint16_t dex_register_number,
                                                            uint16_t number_of_dex_registers,
                                                            const CodeInfo& code_info) const {
  DexRegisterLocationSet dex_register_location_set = code_info.GetDexRegisterLocationSet();
  size_t location_set_entry_index = GetLocationSetEntryIndex(
      dex_register_number,
      number_of_dex_registers,
      code_info.GetNumberOfDexRegisterLocationSetEntries());
  return dex_register_location_set.GetDexRegisterLocation(location_set_entry_index);
}

}  // namespace art
