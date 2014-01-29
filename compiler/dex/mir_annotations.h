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

#ifndef ART_COMPILER_DEX_MIR_ANNOTATIONS_H_
#define ART_COMPILER_DEX_MIR_ANNOTATIONS_H_

#include "invoke_type.h"

namespace art {

/*
 * Annotations are calculated from the perspective of the compilation unit that
 * accesses the fields or methods. Since they are stored with that unit, they do not
 * need to reference the dex file or method for which they have been calculated.
 * However, we do store the dex file, declaring class index and field index of the
 * resolved field to help distinguish between fields.
 */

/**
 * @brief Instance field annotation.
 */
struct IFieldAnnotation {
  uint16_t field_idx;
  uint16_t fast_get : 1;
  uint16_t fast_put : 1;
  uint16_t is_volatile : 1;
  uint16_t reserved : 13;
  int field_offset;
  const DexFile* resolved_dex_file;
  uint16_t resolved_class_idx;
  uint16_t resolved_field_idx;

  static constexpr IFieldAnnotation Unresolved(uint16_t field_idx) {
    // Slow path, volatile.
    return IFieldAnnotation { field_idx, 0u, 0u, 1u, 0u, -1, nullptr, 0u, 0u };  // NOLINT(readability/braces)
  }
};

/**
 * @brief Static field annotation.
 */
struct SFieldAnnotation {
  uint16_t field_idx;
  uint16_t fast_get : 1;
  uint16_t fast_put : 1;
  uint16_t is_volatile : 1;
  uint16_t is_referrers_class : 1;
  uint16_t is_initialized : 1;
  uint16_t reserved : 11;
  int field_offset;
  int storage_index;
  const DexFile* resolved_dex_file;
  uint16_t resolved_class_idx;
  uint16_t resolved_field_idx;

  static constexpr SFieldAnnotation Unresolved(uint16_t field_idx) {
    // Slow path, volatile.
    return SFieldAnnotation { field_idx, 0u, 0u, 1u, 0u, 0u, 0u, -1, -1, nullptr, 0u, 0u };  // NOLINT(readability/braces)
  }
};

}  // namespace art

#endif  // ART_COMPILER_DEX_MIR_ANNOTATIONS_H_
