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

class TimingLogger;

// Used for compile time and runtime for ElfFile access. Because of
// the need for use at runtime, cannot directly use LLVM classes such as
// ELFObjectFile.
template <typename Elf_Ehdr, typename Elf_Phdr, typename Elf_Shdr, typename Elf_Word,
          typename Elf_Sword, typename Elf_Addr, typename Elf_Sym, typename Elf_Rel,
          typename Elf_Rela, typename Elf_Dyn, typename Elf_Off>
class ElfFileImpl {
 public:
  static ElfFileImpl* Open(File* file, bool writable, bool program_header_only, std::string* error_msg);
  // Open with specific mmap flags, Always maps in the whole file, not just the
  // program header sections.
  static ElfFileImpl* Open(File* file, int mmap_prot, int mmap_flags, std::string* error_msg);
  ~ElfFileImpl();

  // Load segments into memory based on PT_LOAD program headers

  const File& GetFile() const {
    return *file_;
  }

  byte* Begin() const {
    return map_->Begin();
  }

  byte* End() const {
    return map_->End();
  }

  size_t Size() const {
    return map_->Size();
  }

  Elf_Ehdr& GetHeader() const;

  Elf_Word GetProgramHeaderNum() const;
  Elf_Phdr& GetProgramHeader(Elf_Word) const;
  Elf_Phdr* FindProgamHeaderByType(Elf_Word type) const;

  Elf_Word GetSectionHeaderNum() const;
  Elf_Shdr& GetSectionHeader(Elf_Word) const;
  Elf_Shdr* FindSectionByType(Elf_Word type) const;
  Elf_Shdr* FindSectionByName(const std::string& name) const;

  Elf_Shdr& GetSectionNameStringSection() const;

  // Find .dynsym using .hash for more efficient lookup than FindSymbolAddress.
  const byte* FindDynamicSymbolAddress(const std::string& symbol_name) const;
  const Elf_Sym* FindDynamicSymbol(const std::string& symbol_name) const;

  static bool IsSymbolSectionType(Elf_Word section_type);
  Elf_Word GetSymbolNum(Elf_Shdr&) const;
  Elf_Sym& GetSymbol(Elf_Word section_type, Elf_Word i) const;

  // Find symbol in specified table, returning nullptr if it is not found.
  //
  // If build_map is true, builds a map to speed repeated access. The
  // map does not included untyped symbol values (aka STT_NOTYPE)
  // since they can contain duplicates. If build_map is false, the map
  // will be used if it was already created. Typically build_map
  // should be set unless only a small number of symbols will be
  // looked up.
  Elf_Sym* FindSymbolByName(Elf_Word section_type,
                            const std::string& symbol_name,
                            bool build_map);

  // Find address of symbol in specified table, returning 0 if it is
  // not found. See FindSymbolByName for an explanation of build_map.
  Elf_Addr FindSymbolAddress(Elf_Word section_type,
                             const std::string& symbol_name,
                             bool build_map);

  // Lookup a string given string section and offset. Returns nullptr for
  // special 0 offset.
  const char* GetString(Elf_Shdr&, Elf_Word) const;

  // Lookup a string by section type. Returns nullptr for special 0 offset.
  const char* GetString(Elf_Word section_type, Elf_Word) const;

  Elf_Word GetDynamicNum() const;
  Elf_Dyn& GetDynamic(Elf_Word) const;
  Elf_Dyn* FindDynamicByType(Elf_Sword type) const;
  Elf_Word FindDynamicValueByType(Elf_Sword type) const;

  Elf_Word GetRelNum(Elf_Shdr&) const;
  Elf_Rel& GetRel(Elf_Shdr&, Elf_Word) const;

  Elf_Word GetRelaNum(Elf_Shdr&) const;
  Elf_Rela& GetRela(Elf_Shdr&, Elf_Word) const;

  // Returns the expected size when the file is loaded at runtime
  size_t GetLoadedSize() const;

  // Load segments into memory based on PT_LOAD program headers.
  // executable is true at run time, false at compile time.
  bool Load(bool executable, std::string* error_msg);

  bool FixupDebugSections(off_t base_address_delta);

  // Defined in patchoat.cc
  bool PatchElf(TimingLogger* timings, uintptr_t base_address_delta);
  bool PatchTextSection(uintptr_t base_address_delta);
  bool PatchTextSection(const Elf_Shdr& patches_sec, uintptr_t base_address_delta);
  bool CheckOatFile(const Elf_Shdr& patches_sec, uintptr_t base_address_delta);
  bool PatchOatHeader(uintptr_t base_address_delta);
  bool PatchSymbols(Elf_Shdr* section, uintptr_t base_address_delta);
  bool CheckOatFile(const Elf_Shdr& patches_sec);

  // Defined in elf_fixup.cc
  bool Fixup(uintptr_t base_address);
  bool FixupDynamic(uintptr_t base_address);
  bool FixupSectionHeaders(uintptr_t base_address);
  bool FixupProgramHeaders(uintptr_t base_address);
  bool FixupSymbols(uintptr_t base_address, bool dynamic);
  bool FixupRelocations(uintptr_t base_address);

  // Defined in elf_stripper.cc
  bool Strip(std::string* error_msg);

  // Defined in elf_patcher.cc
  bool WriteOutPatchData(const std::vector<uintptr_t>& patches, std::string* error_msg);

 private:
  ElfFileImpl(File* file, bool writable, bool program_header_only);

  bool Setup(int prot, int flags, std::string* error_msg);

  bool SetMap(MemMap* map, std::string* error_msg);

  byte* GetProgramHeadersStart() const;
  byte* GetSectionHeadersStart() const;
  Elf_Phdr& GetDynamicProgramHeader() const;
  Elf_Dyn* GetDynamicSectionStart() const;
  Elf_Sym* GetSymbolSectionStart(Elf_Word section_type) const;
  const char* GetStringSectionStart(Elf_Word section_type) const;
  Elf_Rel* GetRelSectionStart(Elf_Shdr&) const;
  Elf_Rela* GetRelaSectionStart(Elf_Shdr&) const;
  Elf_Word* GetHashSectionStart() const;
  Elf_Word GetHashBucketNum() const;
  Elf_Word GetHashChainNum() const;
  Elf_Word GetHashBucket(size_t i) const;
  Elf_Word GetHashChain(size_t i) const;

  typedef std::map<std::string, Elf_Sym*> SymbolTable;
  SymbolTable** GetSymbolTable(Elf_Word section_type);

  bool ValidPointer(const byte* start) const;

  const File* const file_;
  const bool writable_;
  const bool program_header_only_;

  // ELF header mapping. If program_header_only_ is false, will
  // actually point to the entire elf file.
  std::unique_ptr<MemMap> map_;
  Elf_Ehdr* header_;
  std::vector<MemMap*> segments_;

  // Pointer to start of first PT_LOAD program segment after Load()
  // when program_header_only_ is true.
  byte* base_address_;

  // The program header should always available but use GetProgramHeadersStart() to be sure.
  byte* program_headers_start_;

  // Conditionally available values. Use accessors to ensure they exist if they are required.
  byte* section_headers_start_;
  Elf_Phdr* dynamic_program_header_;
  Elf_Dyn* dynamic_section_start_;
  Elf_Sym* symtab_section_start_;
  Elf_Sym* dynsym_section_start_;
  char* strtab_section_start_;
  char* dynstr_section_start_;
  Elf_Word* hash_section_start_;

  SymbolTable* symtab_symbol_table_;
  SymbolTable* dynsym_symbol_table_;

  // Support for GDB JIT
  byte* jit_elf_image_;
  JITCodeEntry* jit_gdb_entry_;
  std::unique_ptr<ElfFileImpl<Elf_Ehdr, Elf_Phdr, Elf_Shdr, Elf_Word,
                  Elf_Sword, Elf_Addr, Elf_Sym, Elf_Rel,
                  Elf_Rela, Elf_Dyn, Elf_Off>> gdb_file_mapping_;
  void GdbJITSupport();
};

// Explicitly instantiated in elf_file.cc
typedef ElfFileImpl<Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr, Elf32_Word, Elf32_Sword,
                    Elf32_Addr, Elf32_Sym, Elf32_Rel, Elf32_Rela, Elf32_Dyn, Elf32_Off> ElfFileImpl32;
typedef ElfFileImpl<Elf64_Ehdr, Elf64_Phdr, Elf64_Shdr, Elf64_Word, Elf64_Sword,
                    Elf64_Addr, Elf64_Sym, Elf64_Rel, Elf64_Rela, Elf64_Dyn, Elf64_Off> ElfFileImpl64;

class ElfFile {
 public:
  static ElfFile* Open(File* file, bool writable, bool program_header_only, std::string* error_msg);
  static ElfFile* Open(File* file, int mmap_prot, int mmap_flags, std::string* error_msg);
  ~ElfFile();

  const bool is_elf64_;
  
  bool Load(bool executable, std::string* error_msg) {
    if (is_elf64_) {
      CHECK_NE(elf_.elf64_, static_cast<ElfFileImpl64*>(nullptr));
      return elf_.elf64_->Load(executable, error_msg);
    } else {
      CHECK_NE(elf_.elf32_, static_cast<ElfFileImpl32*>(nullptr));
      return elf_.elf32_->Load(executable, error_msg);
    }
  }

  const byte* FindDynamicSymbolAddress(const std::string& symbol_name) const {
    if (is_elf64_) {
      CHECK_NE(elf_.elf64_, static_cast<ElfFileImpl64*>(nullptr));
      return elf_.elf64_->FindDynamicSymbolAddress(symbol_name);
    } else {
      CHECK_NE(elf_.elf32_, static_cast<ElfFileImpl32*>(nullptr));
      return elf_.elf32_->FindDynamicSymbolAddress(symbol_name);
    }
  }

  size_t Size() const {
    if (is_elf64_) {
      CHECK_NE(elf_.elf64_, static_cast<ElfFileImpl64*>(nullptr));
      return elf_.elf64_->Size();
    } else {
      CHECK_NE(elf_.elf32_, static_cast<ElfFileImpl32*>(nullptr));
      return elf_.elf32_->Size();
    }
  }

  byte* Begin() const {
    if (is_elf64_) {
      CHECK_NE(elf_.elf64_, static_cast<ElfFileImpl64*>(nullptr));
      return elf_.elf64_->Begin();
    } else {
      CHECK_NE(elf_.elf32_, static_cast<ElfFileImpl32*>(nullptr));
      return elf_.elf32_->Begin();
    }
  }

  byte* End() const {
    if (is_elf64_) {
      CHECK_NE(elf_.elf64_, static_cast<ElfFileImpl64*>(nullptr));
      return elf_.elf64_->End();
    } else {
      CHECK_NE(elf_.elf32_, static_cast<ElfFileImpl32*>(nullptr));
      return elf_.elf32_->End();
    }
  }

  bool GetSectionOffsetAndSize(const char* section_name, uintptr_t* offset, uintptr_t* size) {
    if (is_elf64_) {
      CHECK_NE(elf_.elf64_, static_cast<ElfFileImpl64*>(nullptr));

      Elf64_Shdr *shdr = elf_.elf64_->FindSectionByName(section_name);
      if (shdr == nullptr)
        return false;

      if (offset != nullptr)
        *offset = shdr->sh_offset;
      if (size != nullptr)
        *size = shdr->sh_size;
      return true;
    } else {
      CHECK_NE(elf_.elf32_, static_cast<ElfFileImpl32*>(nullptr));

      Elf32_Shdr *shdr = elf_.elf32_->FindSectionByName(section_name);
      if (shdr == nullptr)
        return false;

      if (offset != nullptr)
        *offset = shdr->sh_offset;
      if (size != nullptr)
        *size = shdr->sh_size;
      return true;
    }
  }

  uintptr_t FindSymbolAddress(unsigned section_type,
                              const std::string& symbol_name,
                              bool build_map) {
    if (is_elf64_) {
      CHECK_NE(elf_.elf64_, static_cast<ElfFileImpl64*>(nullptr));
      return elf_.elf64_->FindSymbolAddress(section_type, symbol_name, build_map);
    } else {
      CHECK_NE(elf_.elf32_, static_cast<ElfFileImpl32*>(nullptr));
      return elf_.elf32_->FindSymbolAddress(section_type, symbol_name, build_map);
    }
  }

  size_t GetLoadedSize() const {
    if (is_elf64_) {
      CHECK_NE(elf_.elf64_, static_cast<ElfFileImpl64*>(nullptr));
      return elf_.elf64_->GetLoadedSize();
    } else {
      CHECK_NE(elf_.elf32_, static_cast<ElfFileImpl32*>(nullptr));
      return elf_.elf32_->GetLoadedSize();
    }
  }

  friend class PatchOat;
  friend class ElfFixup;
  friend class ElfStripper;
  friend class ElfPatcher;

 private:
  ElfFile(ElfFileImpl32* elf32);
  ElfFile(ElfFileImpl64* elf64);

  union ElfFileContainer {
    ElfFileImpl32* elf32_;
    ElfFileImpl64* elf64_;
  } elf_;
};

}  // namespace art

#endif  // ART_RUNTIME_ELF_FILE_H_
