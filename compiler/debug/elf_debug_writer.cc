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

#include "elf_writer_debug.h"

#include <algorithm>
#include <unordered_set>
#include <vector>
#include <cstdio>

#include "base/casts.h"
#include "base/stl_util.h"
#include "compiled_method.h"
#include "debug_info/dwarf/expression.h"
#include "debug_info/dwarf/headers.h"
#include "debug_info/dwarf/register.h"
#include "debug_info/method_debug_info.h"
#include "dex_file-inl.h"
#include "driver/compiler_driver.h"
#include "elf_builder.h"
#include "linear_alloc.h"
#include "linker/vector_output_stream.h"
#include "mirror/array.h"
#include "mirror/class-inl.h"
#include "mirror/class.h"
#include "oat_writer.h"
#include "stack_map.h"
#include "utils.h"

// liblzma.
#include "XzEnc.h"
#include "7zCrc.h"
#include "XzCrc64.h"

namespace art {
namespace dwarf {

template <typename ElfTypes>
void WriteDebugInfo(ElfBuilder<ElfTypes>* builder,
                    const ArrayRef<const MethodDebugInfo>& method_infos,
                    CFIFormat cfi_format,
                    bool write_oat_patches) {
  // Add methods to .symtab.
  WriteDebugSymbols(builder, method_infos, true /* with_signature */);
  // Generate CFI (stack unwinding information).
  WriteCFISection(builder, method_infos, cfi_format, write_oat_patches);
  // Write DWARF .debug_* sections.
  WriteDebugSections(builder, method_infos, write_oat_patches);
}

template<typename ElfTypes>
static void WriteDebugSections(ElfBuilder<ElfTypes>* builder,
                               const ArrayRef<const MethodDebugInfo>& method_infos,
                               bool write_oat_patches) {
  // Group the methods into compilation units based on source file.
  std::vector<CompilationUnit> compilation_units;
  const char* last_source_file = nullptr;
  for (const MethodDebugInfo& mi : method_infos) {
    auto& dex_class_def = mi.dex_file_->GetClassDef(mi.class_def_index_);
    const char* source_file = mi.dex_file_->GetSourceFile(dex_class_def);
    if (compilation_units.empty() || source_file != last_source_file) {
      compilation_units.push_back(CompilationUnit());
    }
    CompilationUnit& cu = compilation_units.back();
    cu.methods_.push_back(&mi);
    cu.low_pc_ = std::min(cu.low_pc_, mi.low_pc_);
    cu.high_pc_ = std::max(cu.high_pc_, mi.high_pc_);
    last_source_file = source_file;
  }

  // Write .debug_line section.
  if (!compilation_units.empty()) {
    DebugLineWriter<ElfTypes> line_writer(builder);
    line_writer.Start();
    for (auto& compilation_unit : compilation_units) {
      line_writer.WriteCompilationUnit(compilation_unit);
    }
    line_writer.End(write_oat_patches);
  }

  // Write .debug_info section.
  if (!compilation_units.empty()) {
    DebugInfoWriter<ElfTypes> info_writer(builder);
    info_writer.Start();
    for (const auto& compilation_unit : compilation_units) {
      info_writer.WriteCompilationUnit(compilation_unit);
    }
    info_writer.End(write_oat_patches);
  }
}

std::vector<uint8_t> MakeMiniDebugInfo(
    InstructionSet isa,
    size_t rodata_size,
    size_t text_size,
    const ArrayRef<const MethodDebugInfo>& method_infos) {
  if (Is64BitInstructionSet(isa)) {
    return MakeMiniDebugInfoInternal<ElfTypes64>(isa, rodata_size, text_size, method_infos);
  } else {
    return MakeMiniDebugInfoInternal<ElfTypes32>(isa, rodata_size, text_size, method_infos);
  }
}

template <typename ElfTypes>
static ArrayRef<const uint8_t> WriteDebugElfFileForMethodInternal(
    const dwarf::MethodDebugInfo& method_info) {
  const InstructionSet isa = method_info.compiled_method_->GetInstructionSet();
  std::vector<uint8_t> buffer;
  buffer.reserve(KB);
  VectorOutputStream out("Debug ELF file", &buffer);
  std::unique_ptr<ElfBuilder<ElfTypes>> builder(new ElfBuilder<ElfTypes>(isa, &out));
  // No program headers since the ELF file is not linked and has no allocated sections.
  builder->Start(false /* write_program_headers */);
  WriteDebugInfo(builder.get(),
                 ArrayRef<const MethodDebugInfo>(&method_info, 1),
                 DW_DEBUG_FRAME_FORMAT,
                 false /* write_oat_patches */);
  builder->End();
  CHECK(builder->Good());
  // Make a copy of the buffer.  We want to shrink it anyway.
  uint8_t* result = new uint8_t[buffer.size()];
  CHECK(result != nullptr);
  memcpy(result, buffer.data(), buffer.size());
  return ArrayRef<const uint8_t>(result, buffer.size());
}

ArrayRef<const uint8_t> WriteDebugElfFileForMethod(const dwarf::MethodDebugInfo& method_info) {
  const InstructionSet isa = method_info.compiled_method_->GetInstructionSet();
  if (Is64BitInstructionSet(isa)) {
    return WriteDebugElfFileForMethodInternal<ElfTypes64>(method_info);
  } else {
    return WriteDebugElfFileForMethodInternal<ElfTypes32>(method_info);
  }
}

template <typename ElfTypes>
static ArrayRef<const uint8_t> WriteDebugElfFileForClassesInternal(
    const InstructionSet isa, const ArrayRef<mirror::Class*>& types)
    SHARED_REQUIRES(Locks::mutator_lock_) {
  std::vector<uint8_t> buffer;
  buffer.reserve(KB);
  VectorOutputStream out("Debug ELF file", &buffer);
  std::unique_ptr<ElfBuilder<ElfTypes>> builder(new ElfBuilder<ElfTypes>(isa, &out));
  // No program headers since the ELF file is not linked and has no allocated sections.
  builder->Start(false /* write_program_headers */);
  DebugInfoWriter<ElfTypes> info_writer(builder.get());
  info_writer.Start();
  info_writer.WriteTypes(types);
  info_writer.End(false /* write_oat_patches */);

  builder->End();
  CHECK(builder->Good());
  // Make a copy of the buffer.  We want to shrink it anyway.
  uint8_t* result = new uint8_t[buffer.size()];
  CHECK(result != nullptr);
  memcpy(result, buffer.data(), buffer.size());
  return ArrayRef<const uint8_t>(result, buffer.size());
}

ArrayRef<const uint8_t> WriteDebugElfFileForClasses(const InstructionSet isa,
                                                    const ArrayRef<mirror::Class*>& types) {
  if (Is64BitInstructionSet(isa)) {
    return WriteDebugElfFileForClassesInternal<ElfTypes64>(isa, types);
  } else {
    return WriteDebugElfFileForClassesInternal<ElfTypes32>(isa, types);
  }
}

// Explicit instantiations
template void WriteDebugInfo<ElfTypes32>(
    ElfBuilder<ElfTypes32>* builder,
    const ArrayRef<const MethodDebugInfo>& method_infos,
    CFIFormat cfi_format,
    bool write_oat_patches);
template void WriteDebugInfo<ElfTypes64>(
    ElfBuilder<ElfTypes64>* builder,
    const ArrayRef<const MethodDebugInfo>& method_infos,
    CFIFormat cfi_format,
    bool write_oat_patches);

}  // namespace dwarf
}  // namespace art
