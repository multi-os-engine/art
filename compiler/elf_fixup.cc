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

#include "elf_fixup.h"

#include <inttypes.h>
#include <memory>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "elf_file.h"
#include "elf_writer.h"

namespace art {

static const bool DEBUG_FIXUP = false;

bool ElfFixup::Fixup(File* file, uintptr_t oat_data_begin) {
  std::string error_msg;
  std::unique_ptr<ElfFile> elf_file(ElfFile::Open(file, true, false, &error_msg));
  CHECK(elf_file.get() != nullptr) << error_msg;

  // Lookup "oatdata" symbol address.
  Elf32_Addr oatdata_address = ElfWriter::GetOatDataAddress(elf_file.get());
  Elf32_Off base_address = oat_data_begin - oatdata_address;

  if (elf_file->is_elf64_)
    return elf_file->elf_.elf64_->Fixup(base_address);
  else
    return elf_file->elf_.elf32_->Fixup(base_address);
}

template <typename Elf_Ehdr, typename Elf_Phdr, typename Elf_Shdr, typename Elf_Word,
          typename Elf_Sword, typename Elf_Addr, typename Elf_Sym, typename Elf_Rel,
          typename Elf_Rela, typename Elf_Dyn, typename Elf_Off>
bool ElfFileImpl<Elf_Ehdr, Elf_Phdr, Elf_Shdr, Elf_Word,
    Elf_Sword, Elf_Addr, Elf_Sym, Elf_Rel, Elf_Rela, Elf_Dyn, Elf_Off>
    ::Fixup(uintptr_t base_address) {
  if (!FixupDynamic(base_address)) {
    LOG(WARNING) << "Failed to fixup .dynamic in " << file_->GetPath();
    return false;
  }
  if (!FixupSectionHeaders(base_address)) {
    LOG(WARNING) << "Failed to fixup section headers in " << file_->GetPath();
    return false;
  }
  if (!FixupProgramHeaders(base_address)) {
    LOG(WARNING) << "Failed to fixup program headers in " << file_->GetPath();
    return false;
  }
  if (!FixupSymbols(base_address, true)) {
    LOG(WARNING) << "Failed to fixup .dynsym in " << file_->GetPath();
    return false;
  }
  if (!FixupSymbols(base_address, false)) {
    LOG(WARNING) << "Failed to fixup .symtab in " << file_->GetPath();
    return false;
  }
  if (!FixupRelocations(base_address)) {
    LOG(WARNING) << "Failed to fixup .rel.dyn in " << file_->GetPath();
    return false;
  }
  if (!FixupDebugSections(base_address)) {
    LOG(WARNING) << "Failed to fixup debug sections in " << file_->GetPath();
    return false;
  }
  return true;
}

template <typename Elf_Ehdr, typename Elf_Phdr, typename Elf_Shdr, typename Elf_Word,
          typename Elf_Sword, typename Elf_Addr, typename Elf_Sym, typename Elf_Rel,
          typename Elf_Rela, typename Elf_Dyn, typename Elf_Off>
bool ElfFileImpl<Elf_Ehdr, Elf_Phdr, Elf_Shdr, Elf_Word,
    Elf_Sword, Elf_Addr, Elf_Sym, Elf_Rel, Elf_Rela, Elf_Dyn, Elf_Off>
    ::FixupDynamic(uintptr_t base_address) {
  for (Elf_Word i = 0; i < GetDynamicNum(); i++) {
    Elf_Dyn& elf_dyn = GetDynamic(i);
    Elf_Word d_tag = elf_dyn.d_tag;
    if (IsDynamicSectionPointer(d_tag, GetHeader().e_machine)) {
      Elf_Addr d_ptr = elf_dyn.d_un.d_ptr;
      if (DEBUG_FIXUP) {
        LOG(INFO) << StringPrintf("In %s moving Elf_Dyn[%d] from 0x%08x to 0x%08x" PRIxPTR,
                                  GetFile().GetPath().c_str(), i,
                                  static_cast<unsigned int>(d_ptr),
                                  static_cast<unsigned int>(d_ptr + base_address));
      }
      d_ptr += base_address;
      elf_dyn.d_un.d_ptr = d_ptr;
    }
  }
  return true;
}

template <typename Elf_Ehdr, typename Elf_Phdr, typename Elf_Shdr, typename Elf_Word,
          typename Elf_Sword, typename Elf_Addr, typename Elf_Sym, typename Elf_Rel,
          typename Elf_Rela, typename Elf_Dyn, typename Elf_Off>
bool ElfFileImpl<Elf_Ehdr, Elf_Phdr, Elf_Shdr, Elf_Word,
    Elf_Sword, Elf_Addr, Elf_Sym, Elf_Rel, Elf_Rela, Elf_Dyn, Elf_Off>
    ::FixupSectionHeaders(uintptr_t base_address) {
  for (Elf_Word i = 0; i < GetSectionHeaderNum(); i++) {
    Elf_Shdr& sh = GetSectionHeader(i);
    // 0 implies that the section will not exist in the memory of the process
    if (sh.sh_addr == 0) {
      continue;
    }
    if (DEBUG_FIXUP) {
      LOG(INFO) << StringPrintf("In %s moving Elf_Shdr[%d] from 0x%08x to 0x%08x" PRIxPTR,
                                GetFile().GetPath().c_str(), i,
                                static_cast<unsigned int>(sh.sh_addr),
                                static_cast<unsigned int>(sh.sh_addr + base_address));
    }
    sh.sh_addr += base_address;
  }
  return true;
}

template <typename Elf_Ehdr, typename Elf_Phdr, typename Elf_Shdr, typename Elf_Word,
          typename Elf_Sword, typename Elf_Addr, typename Elf_Sym, typename Elf_Rel,
          typename Elf_Rela, typename Elf_Dyn, typename Elf_Off>
bool ElfFileImpl<Elf_Ehdr, Elf_Phdr, Elf_Shdr, Elf_Word,
    Elf_Sword, Elf_Addr, Elf_Sym, Elf_Rel, Elf_Rela, Elf_Dyn, Elf_Off>
    ::FixupProgramHeaders(uintptr_t base_address) {
  // TODO: ELFObjectFile doesn't have give to Elf_Phdr, so we do that ourselves for now.
  for (Elf_Word i = 0; i < GetProgramHeaderNum(); i++) {
    Elf_Phdr& ph = GetProgramHeader(i);
    CHECK_EQ(ph.p_vaddr, ph.p_paddr) << GetFile().GetPath() << " i=" << i;
    CHECK((ph.p_align == 0) || (0 == ((ph.p_vaddr - ph.p_offset) & (ph.p_align - 1))))
            << GetFile().GetPath() << " i=" << i;
    if (DEBUG_FIXUP) {
      LOG(INFO) << StringPrintf("In %s moving Elf_Phdr[%d] from 0x%08x to 0x%08x" PRIxPTR,
                                GetFile().GetPath().c_str(), i,
                                static_cast<unsigned int>(ph.p_vaddr),
                                static_cast<unsigned int>(ph.p_vaddr + base_address));
    }
    ph.p_vaddr += base_address;
    ph.p_paddr += base_address;
    CHECK((ph.p_align == 0) || (0 == ((ph.p_vaddr - ph.p_offset) & (ph.p_align - 1))))
            << GetFile().GetPath() << " i=" << i;
  }
  return true;
}

template <typename Elf_Ehdr, typename Elf_Phdr, typename Elf_Shdr, typename Elf_Word,
          typename Elf_Sword, typename Elf_Addr, typename Elf_Sym, typename Elf_Rel,
          typename Elf_Rela, typename Elf_Dyn, typename Elf_Off>
bool ElfFileImpl<Elf_Ehdr, Elf_Phdr, Elf_Shdr, Elf_Word,
    Elf_Sword, Elf_Addr, Elf_Sym, Elf_Rel, Elf_Rela, Elf_Dyn, Elf_Off>
    ::FixupSymbols(uintptr_t base_address, bool dynamic) {
  Elf_Word section_type = dynamic ? SHT_DYNSYM : SHT_SYMTAB;
  // TODO: Unfortunate ELFObjectFile has protected symbol access, so use ElfFile
  Elf_Shdr* symbol_section = FindSectionByType(section_type);
  if (symbol_section == NULL) {
    // file is missing optional .symtab
    CHECK(!dynamic) << GetFile().GetPath();
    return true;
  }
  for (uint32_t i = 0; i < GetSymbolNum(*symbol_section); i++) {
    Elf_Sym& symbol = GetSymbol(section_type, i);
    if (symbol.st_value != 0) {
      if (DEBUG_FIXUP) {
        LOG(INFO) << StringPrintf("In %s moving Elf_Sym[%d] from 0x%08x to 0x%08x" PRIxPTR,
                                  GetFile().GetPath().c_str(), i,
                                  static_cast<unsigned int>(symbol.st_value),
                                  static_cast<unsigned int>(symbol.st_value + base_address));
      }
      symbol.st_value += base_address;
    }
  }
  return true;
}

template <typename Elf_Ehdr, typename Elf_Phdr, typename Elf_Shdr, typename Elf_Word,
          typename Elf_Sword, typename Elf_Addr, typename Elf_Sym, typename Elf_Rel,
          typename Elf_Rela, typename Elf_Dyn, typename Elf_Off>
bool ElfFileImpl<Elf_Ehdr, Elf_Phdr, Elf_Shdr, Elf_Word,
    Elf_Sword, Elf_Addr, Elf_Sym, Elf_Rel, Elf_Rela, Elf_Dyn, Elf_Off>
    ::FixupRelocations(uintptr_t base_address) {
  for (Elf_Word i = 0; i < GetSectionHeaderNum(); i++) {
    Elf_Shdr& sh = GetSectionHeader(i);
    if (sh.sh_type == SHT_REL) {
      for (uint32_t i = 0; i < GetRelNum(sh); i++) {
        Elf_Rel& rel = GetRel(sh, i);
        if (DEBUG_FIXUP) {
          LOG(INFO) << StringPrintf("In %s moving Elf_Rel[%d] from 0x%08x to 0x%08x" PRIxPTR,
                                    GetFile().GetPath().c_str(), i,
                                    static_cast<unsigned int>(rel.r_offset),
                                    static_cast<unsigned int>(rel.r_offset + base_address));
        }
        rel.r_offset += base_address;
      }
    } else if (sh.sh_type == SHT_RELA) {
      for (uint32_t i = 0; i < GetRelaNum(sh); i++) {
        Elf_Rela& rela = GetRela(sh, i);
        if (DEBUG_FIXUP) {
          LOG(INFO) << StringPrintf("In %s moving Elf_Rela[%d] from 0x%08x to 0x%08x" PRIxPTR,
                                    GetFile().GetPath().c_str(), i,
                                    static_cast<unsigned int>(rela.r_offset),
                                    static_cast<unsigned int>(rela.r_offset + base_address));
        }
        rela.r_offset += base_address;
      }
    }
  }
  return true;
}

}  // namespace art
