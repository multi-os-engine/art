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

#ifndef ART_COMPILER_UTILS_DEX_CACHE_ARRAYS_LAYOUT_H_
#define ART_COMPILER_UTILS_DEX_CACHE_ARRAYS_LAYOUT_H_

#include "base/logging.h"
#include "globals.h"
#include "mirror/array-inl.h"
#include "primitive.h"
#include "utils.h"

namespace mirror {
class ArtField;
class ArtMethod;
class Class;
class String;
}  // namespace mirror

namespace art {

/**
 * @class DexCacheArraysLayout
 * @details This class provides the layout information for the type, method, field and
 * string arrays for a DexCache with a fixed arrays' layout (such as in the boot image),
 */
class DexCacheArraysLayout {
 public:
  DexCacheArraysLayout(size_t start_offset, const DexFile* dex_file)
      : start_offset_(start_offset),
        types_offset_(start_offset_),
        methods_offset_(types_offset_ + ArraySize<mirror::Class>(dex_file->NumTypeIds())),
        strings_offset_(methods_offset_ + ArraySize<mirror::ArtMethod>(dex_file->NumMethodIds())),
        fields_offset_(strings_offset_ + ArraySize<mirror::String>(dex_file->NumStringIds())),
        end_offset_(fields_offset_ + ArraySize<mirror::ArtField>(dex_file->NumFieldIds())) {
  }

  size_t StartOffset() const {
    return start_offset_;
  }

  size_t EndOffset() const {
    return end_offset_;
  }

  size_t Size() const {
    return end_offset_ - start_offset_;
  }

  size_t TypesOffset() const {
    return types_offset_;
  }

  size_t TypeOffset(uint32_t type_idx) const {
    return types_offset_ + ElementOffset<mirror::Class>(type_idx);
  }

  size_t MethodsOffset() const {
    return methods_offset_;
  }

  size_t MethodOffset(uint32_t method_idx) const {
    return methods_offset_ + ElementOffset<mirror::ArtMethod>(method_idx);
  }

  size_t StringsOffset() const {
    return strings_offset_;
  }

  size_t StringOffset(uint32_t string_idx) const {
    return strings_offset_ + ElementOffset<mirror::String>(string_idx);
  }

  size_t FieldsOffset() const {
    return fields_offset_;
  }

  size_t FieldOffset(uint32_t field_idx) const {
    return fields_offset_ + ElementOffset<mirror::ArtField>(field_idx);
  }

 private:
  const size_t start_offset_;
  const size_t types_offset_;
  const size_t methods_offset_;
  const size_t strings_offset_;
  const size_t fields_offset_;
  const size_t end_offset_;

  template <typename MirrorType>
  static size_t ElementOffset(uint32_t idx) {
    return mirror::Array::DataOffset(sizeof(mirror::HeapReference<MirrorType>)).Uint32Value() +
        sizeof(mirror::HeapReference<MirrorType>) * idx;
  }

  template <typename MirrorType>
  static size_t ArraySize(uint32_t num_elements) {
    size_t array_size = mirror::ComputeArraySize(
        num_elements, ComponentSizeShiftWidth<sizeof(mirror::HeapReference<MirrorType>)>());
    DCHECK_NE(array_size, 0u);  // No overflow expected for dex cache arrays.
    return RoundUp(array_size, kObjectAlignment);
  }
};

}  // namespace art

#endif  // ART_COMPILER_UTILS_DEX_CACHE_ARRAYS_LAYOUT_H_
