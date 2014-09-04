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

#ifndef ART_RUNTIME_ELF_FILE_H_
#define ART_RUNTIME_ELF_FILE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/timing_logger.h"
#include "base/unix_file/fd_file.h"
#include "globals.h"
#include "elf_utils.h"
#include "mem_map.h"
#include "os.h"

namespace art {

// Interface to GDB JIT for backtrace information.
extern "C" {
  struct JITCodeEntry;
}

template <typename Elf_Ehdr, typename Elf_Phdr, typename Elf_Shdr, typename Elf_Word,
          typename Elf_Sword, typename Elf_Addr, typename Elf_Sym, typename Elf_Rel,
          typename Elf_Rela, typename Elf_Dyn, typename Elf_Off>
class ElfFileImpl;
// Explicitly instantiated in elf_file.cc
typedef ElfFileImpl<Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr, Elf32_Word, Elf32_Sword,
                    Elf32_Addr, Elf32_Sym, Elf32_Rel, Elf32_Rela, Elf32_Dyn, Elf32_Off> ElfFileImpl32;
typedef ElfFileImpl<Elf64_Ehdr, Elf64_Phdr, Elf64_Shdr, Elf64_Word, Elf64_Sword,
                    Elf64_Addr, Elf64_Sym, Elf64_Rel, Elf64_Rela, Elf64_Dyn, Elf64_Off> ElfFileImpl64;

// Used for compile time and runtime for ElfFile access. Because of
// the need for use at runtime, cannot directly use LLVM classes such as
// ELFObjectFile.
class ElfFile {
 public:
  static ElfFile* Open(File* file, bool writable, bool program_header_only, std::string* error_msg);
  // Open with specific mmap flags, Always maps in the whole file, not just the
  // program header sections.
  static ElfFile* Open(File* file, int mmap_prot, int mmap_flags, std::string* error_msg);
  ~ElfFile();

  const bool is_elf64_;

  // Load segments into memory based on PT_LOAD program headers
  bool Load(bool executable, std::string* error_msg);

  const byte* FindDynamicSymbolAddress(const std::string& symbol_name) const;

  size_t Size() const;

  byte* Begin() const;

  byte* End() const;

  const File& GetFile() const;

  bool GetSectionOffsetAndSize(const char* section_name, uint64_t* offset, uint64_t* size);

  uint64_t FindSymbolAddress(unsigned section_type,
                             const std::string& symbol_name,
                             bool build_map);

  size_t GetLoadedSize() const;

  // Strip an ELF file of unneeded debugging information.
  // Returns true on success, false on failure.
  static bool Strip(File* file, std::string* error_msg);

  // Fixup an ELF file so that that oat header will be loaded at oat_begin.
  // Returns true on success, false on failure.
  static bool Fixup(File* file, uintptr_t oat_data_begin);

  // Patches oat in place, modifying the oat_file given to the constructor.
  bool PatchElf(TimingLogger* timings, off_t delta);

  bool WriteOutPatchData(std::vector<uintptr_t>& patches, std::string* error_msg);

  bool Fixup(uintptr_t base_address);

 private:
  explicit ElfFile(ElfFileImpl32* elf32);
  explicit ElfFile(ElfFileImpl64* elf64);

  union ElfFileContainer {
    ElfFileImpl32* elf32_;
    ElfFileImpl64* elf64_;
  } elf_;
};

}  // namespace art

#endif  // ART_RUNTIME_ELF_FILE_H_
