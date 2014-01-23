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

struct IFieldAnnotation {
  uint16_t field_idx;
  uint32_t fast_get : 1;
  uint32_t fast_put : 1;
  uint32_t is_volatile : 1;
  uint32_t reserved : 13;
  int field_offset;

  static constexpr IFieldAnnotation Unresolved(uint16_t field_idx) {
    // Slow path, volatile.
    return IFieldAnnotation { field_idx, 0u, 0u, 1u, 0u, -1 };  // NOLINT(readability/braces)
  }
};

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

  static constexpr SFieldAnnotation Unresolved(uint16_t field_idx) {
    // Slow path, volatile.
    return SFieldAnnotation { field_idx, 0u, 0u, 1u, 0u, 0u, 0u, -1, -1 };  // NOLINT(readability/braces)
  }
};

struct MethodAnnotation {
  // On entry to CompilerDriver::ComputeMethodAnnotations(), either called_dex_file
  // is nullptr, or together with called_method_idx it contains the MethodReference to
  // the verification-based devirtualized invoke target.
  const DexFile* called_dex_file;
  uint16_t called_method_idx;
  uint16_t method_idx;
  uint16_t invoke_type : 3;  // InvokeType;
  uint16_t sharp_type : 3;  // InvokeType
  uint16_t fast_path : 1;
  uint16_t reserved : 9;
  uint16_t vtable_idx;
  uintptr_t direct_code;
  uintptr_t direct_method;

  static constexpr MethodAnnotation Unresolved(uint16_t method_idx, uint16_t type) {
    // Slow path.
    return MethodAnnotation { nullptr, 0u, method_idx, type, type, 0u, 0u, 0u, 0u, 0u };  // NOLINT(readability/braces)
  }
};

}  // namespace art

#endif  // ART_COMPILER_DEX_MIR_ANNOTATIONS_H_
