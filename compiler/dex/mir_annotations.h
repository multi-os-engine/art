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
#include "offsets.h"

namespace art {

/*
 * Annotations are calculated from the perspective of the compilation unit that
 * accesses the fields or methods. Since they are stored with that unit, they do not
 * need to reference the dex file or method for which they have been calculated.
 * However, we do store the dex file, declaring class index and field index of the
 * resolved field to help distinguish between fields.
 */

struct IFieldAnnotation {
  // The field index in the compiling method's dex file.
  uint16_t field_idx;
  // Can the compiling method fast-path IGET from this field?
  uint16_t fast_get : 1;
  // Can the compiling method fast-path IPUT from this field?
  uint16_t fast_put : 1;
  // Is the field volatile? Unknown if unresolved, so treated as volatile.
  uint16_t is_volatile : 1;
  // Reserved.
  uint16_t reserved : 13;
  // The member offset of the field, MemberOffset(static_cast<size_t>(-1)) if unresolved.
  MemberOffset field_offset;
  // The dex file that defines the class containing the field and the field, nullptr if unresolved.
  const DexFile* declaring_dex_file;
  // The type index of the class declaring the field, 0 if unresolved.
  uint16_t declaring_class_idx;
  // The field index in the dex file that defines field, 0 if unresolved.
  uint16_t declaring_field_idx;

  static IFieldAnnotation Unresolved(uint16_t field_idx) {
    // Slow path, volatile.
    IFieldAnnotation result = {
        field_idx,
        0u, 0u,  // fast_get, fast_put
        1u,      // is_volatile
        0u,      // reserved
        MemberOffset(static_cast<size_t>(-1)),  // field_offset
        nullptr, 0u, 0u  // declaring_dex_file, declaring_class_idx, declaring_field_idx
    };
    return result;
  }
};

struct SFieldAnnotation {
  // The field index in the compiling method's dex file.
  uint16_t field_idx;
  // Can the compiling method fast-path IGET from this field?
  uint16_t fast_get : 1;
  // Can the compiling method fast-path IPUT from this field?
  uint16_t fast_put : 1;
  // Is the field volatile? Unknown if unresolved, so treated as volatile (true).
  uint16_t is_volatile : 1;
  // Is the field in the referrer's class? false if unresolved.
  uint16_t is_referrers_class : 1;
  // Can we assume that the field's class is already initialized? false if unresolved.
  uint16_t is_initialized : 1;
  // Reserved.
  uint16_t reserved : 11;
  // The member offset of the field, static_cast<size_t>(-1) if unresolved.
  MemberOffset field_offset;
  // The type index of the declaring class in the compiling method's dex file,
  // -1 if the field is unresolved or there's no appropriate TypeId in that dex file.
  int storage_index;
  // The dex file that defines the class containing the field and the field, nullptr if unresolved.
  const DexFile* declaring_dex_file;
  // The type index of the class declaring the field, 0 if unresolved.
  uint16_t declaring_class_idx;
  // The field index in the dex file that defines field, 0 if unresolved.
  uint16_t declaring_field_idx;

  static SFieldAnnotation Unresolved(uint16_t field_idx) {
    // Slow path, volatile.
    SFieldAnnotation result = {
        field_idx,
        0u, 0u,  // fast_get, fast_put
        1u,      // is_volatile
        0u, 0u,  // is_referrers_class, is_initialized
        0u,      // reserved
        MemberOffset(static_cast<size_t>(-1)),  // field_offset
        -1,      // storage_index
        nullptr, 0u, 0u  // declaring_dex_file, declaring_class_idx, declaring_field_idx
    };
    return result;
  }
};

}  // namespace art

#endif  // ART_COMPILER_DEX_MIR_ANNOTATIONS_H_
