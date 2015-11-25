/*
 * Copyright (C) 2012 The Android Open Source Project
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

#ifndef ART_COMPILER_ELF_WRITER_QUICK_H_
#define ART_COMPILER_ELF_WRITER_QUICK_H_

#include <memory>

#include "elf_utils.h"
#include "elf_writer.h"
#include "arch/instruction_set.h"

namespace art {

class BufferedOutputStream;
class CompilerOptions;
template <typename ElfTypes> class ElfBuilder;

std::unique_ptr<ElfWriter> CreateElfWriterQuick(InstructionSet instruction_set,
                                                const CompilerOptions* compiler_options,
                                                File* elf_file);

template <typename ElfTypes>
class ElfWriterQuick FINAL : public ElfWriter {
 public:
  ElfWriterQuick(InstructionSet instruction_set,
                 const CompilerOptions* compiler_options,
                 File* elf_file);
  ~ElfWriterQuick();

  void Start() OVERRIDE;
  OutputStream* StartRoData() OVERRIDE;
  void EndRoData(OutputStream* rodata) OVERRIDE;
  OutputStream* StartText() OVERRIDE;
  void EndText(OutputStream* text) OVERRIDE;
  void SetBssSize(size_t bss_size) OVERRIDE;
  void WriteDynamicSection() OVERRIDE;
  void WriteDebugInfo(const ArrayRef<const CompiledMethodDebugInfo>& debug_infos) OVERRIDE;
  void WritePatchLocations(const ArrayRef<const uintptr_t>& patch_locations) OVERRIDE;
  bool End() OVERRIDE;

  static void EncodeOatPatches(const std::vector<uintptr_t>& locations,
                               std::vector<uint8_t>* buffer);

 private:
  const CompilerOptions* const compiler_options_;
  File* const elf_file_;
  std::unique_ptr<BufferedOutputStream> output_stream_;
  std::unique_ptr<ElfBuilder<ElfTypes>> builder_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ElfWriterQuick);
};

// Explicitly instantiated in elf_writer_quick.cc
typedef ElfWriterQuick<ElfTypes32> ElfWriterQuick32;
typedef ElfWriterQuick<ElfTypes64> ElfWriterQuick64;

}  // namespace art

#endif  // ART_COMPILER_ELF_WRITER_QUICK_H_
