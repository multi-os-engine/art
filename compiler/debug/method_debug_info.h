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
  bool deduped;
  uintptr_t low_pc;
  uintptr_t high_pc;
  const void* code_info;
  size_t frame_size;
  bool is_from_optimizing_compiler;
  bool is_compiled_as_native_debuggable;

  static MethodDebugInfo FormArtMethod(ArtMethod& method, const OatQuickMethodHeader* header)
      SHARED_REQUIRES(Locks::mutator_lock_) {
    MethodDebugInfo info;
    info.dex_file = method.GetDexFile();
    info.class_def_index = method.GetClassDefIndex();
    info.dex_method_index = method.GetDexMethodIndex();
    info.access_flags = method.GetAccessFlags();
    info.code_item = method.GetCodeItem();
    info.deduped = false;
    info.low_pc = reinterpret_cast<uintptr_t>(header->GetCode());
    info.high_pc = header->GetCodeSize();
    info.code_info;
    info.frame_size = header->GetFrameSizeInBytes();
    info.is_from_optimizing_compiler;
    info.is_compiled_as_native_debuggable = false;
    return info;
  }
};

}  // namespace debug
}  // namespace art

#endif  // ART_COMPILER_DEBUG_METHOD_DEBUG_INFO_H_
