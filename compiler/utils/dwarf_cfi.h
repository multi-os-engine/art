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

#ifndef ART_COMPILER_UTILS_DWARF_CFI_H_
#define ART_COMPILER_UTILS_DWARF_CFI_H_

#include <vector>

namespace art {

/**
 * @brief Convert non-volatile ART register id to DWARF register id
 * @param is_x86_64 If it's x86_64 register id.
 * @param art_reg_id ART register id.
 * @param dwarf_reg_id DWARF reg id (output).
 * @return If art_reg_id is a valid non-volatile register id.
 */
bool ARTRegIDToDWARFRegID(bool is_x86_64, int art_reg_id, int* dwarf_reg_id);

#if 0
/**
 * @brief Encode an unsigned integer into a buffer
 * @param buf buffer.
 * @param value Data value.
 */
void EncodeUnsignedLeb128(std::vector<uint8_t>& buf, uint32_t value);

/**
 * @brief Encode an integer into a buffer
 * @param buf buffer.
 * @param value Data value.
 */
void EncodeSignedLeb128(std::vector<uint8_t>& buf, int32_t value);
#endif

/**
 * @brief Enter a 32 bit quantity into a buffer
 * @param buf buffer.
 * @param data Data value.
 */
void PushWord(std::vector<uint8_t>& buf, int32_t data);

/**
 * @brief Enter a 'DW_CFA_advance_loc' into an FDE buffer
 * @param buf FDE buffer.
 * @param increment Amount by which to increase the current location.
 */
void DW_CFA_advance_loc(std::vector<uint8_t>& buf, uint32_t increment);

/**
 * @brief Enter a 'DW_CFA_offset_extended_sf' into an FDE buffer
 * @param buf FDE buffer.
 * @param reg Register number.
 * @param offset Offset of register address from CFA.
 */
void DW_CFA_offset_extended_sf(std::vector<uint8_t>& buf, int reg, int32_t offset);

/**
 * @brief Enter a 'DW_CFA_offset' into an FDE buffer
 * @param buf FDE buffer.
 * @param reg Register number.
 * @param offset Offset of register address from CFA.
 */
void DW_CFA_offset(std::vector<uint8_t>& buf, int reg, uint32_t offset);

/**
 * @brief Enter a 'DW_CFA_def_cfa_offset' into an FDE buffer
 * @param buf FDE buffer.
 * @param offset New offset of CFA.
 */
void DW_CFA_def_cfa_offset(std::vector<uint8_t>& buf, int32_t offset);

/**
 * @brief Enter a 'DW_CFA_remember_state' into an FDE buffer
 * @param buf FDE buffer.
 */
void DW_CFA_remember_state(std::vector<uint8_t>& buf);

/**
 * @brief Enter a 'DW_CFA_restore_state' into an FDE buffer
 * @param buf FDE buffer.
 */
void DW_CFA_restore_state(std::vector<uint8_t>& buf);

/**
 * @brief Construct CIE for x86/x86_64
 * @param buf CIE buffer.
 */
std::vector<uint8_t>* X86GetCIE(bool is_x86_64);

/**
 * @brief Write FDE header into an FDE buffer
 * @param buf FDE buffer.
 */
void WriteFDEHeader(std::vector<uint8_t>& buf);

/**
 * @brief Set 'address_range' field of an FDE buffer
 * @param buf FDE buffer.
 */
void WriteFDEAddressRange(std::vector<uint8_t>& buf, uint32_t data);

/**
 * @brief Set 'length' field of an FDE buffer
 * @param buf FDE buffer.
 */
void WriteCFILength(std::vector<uint8_t>& buf);

/**
 * @brief Pad an FDE buffer with 0 until its size is a multiple of 4
 * @param buf FDE buffer.
 */
void PadCFI(std::vector<uint8_t>& buf);
}  // namespace art

#endif  // ART_COMPILER_UTILS_DWARF_CFI_H_
