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

#ifndef ART_COMPILER_DEBUG_METHOD_DEBUG_INFO_H_
#define ART_COMPILER_DEBUG_METHOD_DEBUG_INFO_H_

#include "art_method.h"
#include "compiled_method.h"
#include "dex_file.h"
#include "oat_quick_method_header.h"

namespace art {
namespace debug {

struct MethodDebugInfo {
  const DexFile* dex_file;
  size_t class_def_index;
  uint32_t dex_method_index;
  uint32_t access_flags;
  const DexFile::CodeItem* code_item;
  InstructionSet isa;
  bool deduped;
  bool is_debuggable;
  bool is_optimized;
  uint64_t code_address;  // Absolute address (i.e. not relative to .text).
  uint32_t code_size;
  uint32_t frame_size_in_bytes;
  const void* code_info;
  ArrayRef<const uint8_t> cfi;

  static MethodDebugInfo CreateFormArtMethod(ArtMethod& method, const OatQuickMethodHeader* header)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    MethodDebugInfo info;
    info.dex_file = method.GetDexFile();
    info.class_def_index = method.GetClassDefIndex();
    info.dex_method_index = method.GetDexMethodIndex();
    info.access_flags = method.GetAccessFlags();
    info.code_item = method.GetCodeItem();
    info.isa = kRuntimeISA;
    info.deduped = false;
    info.is_debuggable = false;
    info.is_optimized = header->IsOptimized() &&
                        header->GetCode() != nullptr &&
                        method.GetCodeItem() != nullptr;
    info.code_address = reinterpret_cast<uintptr_t>(header->GetCode());
    info.code_size = header->GetCodeSize();
    info.frame_size_in_bytes = header->GetFrameSizeInBytes();
    info.code_info = header->GetOptimizedCodeInfoPointer();
    info.cfi = ArrayRef<const uint8_t>();
    return info;
  }
};

}  // namespace debug
}  // namespace art

#endif  // ART_COMPILER_DEBUG_METHOD_DEBUG_INFO_H_
