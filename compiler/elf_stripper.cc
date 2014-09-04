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

#include "elf_stripper.h"

#include <unistd.h>
#include <sys/types.h>
#include <memory>
#include <vector>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "elf_file.h"
#include "elf_utils.h"
#include "utils.h"

namespace art {

bool ElfStripper::Strip(File* file, std::string* error_msg) {
  std::unique_ptr<ElfFile> elf_file(ElfFile::Open(file, true, false, error_msg));
  if (elf_file.get() == nullptr) {
    return false;
  }

  if (elf_file->is_elf64_)
    return elf_file->elf_.elf64_->Strip(error_msg);
  else
    return elf_file->elf_.elf32_->Strip(error_msg);
}

template <typename Elf_Ehdr, typename Elf_Phdr, typename Elf_Shdr, typename Elf_Word,
          typename Elf_Sword, typename Elf_Addr, typename Elf_Sym, typename Elf_Rel,
          typename Elf_Rela, typename Elf_Dyn, typename Elf_Off>
bool ElfFileImpl<Elf_Ehdr, Elf_Phdr, Elf_Shdr, Elf_Word,
    Elf_Sword, Elf_Addr, Elf_Sym, Elf_Rel, Elf_Rela, Elf_Dyn, Elf_Off>
    ::Strip(std::string* error_msg) {
  // ELF files produced by MCLinker look roughly like this
  //
  // +------------+
  // | Elf_Ehdr   | contains number of Elf_Shdr and offset to first
  // +------------+
  // | Elf_Phdr   | program headers
  // | Elf_Phdr   |
  // | ...        |
  // | Elf_Phdr   |
  // +------------+
  // | section    | mixture of needed and unneeded sections
  // +------------+
  // | section    |
  // +------------+
  // | ...        |
  // +------------+
  // | section    |
  // +------------+
  // | Elf_Shdr   | section headers
  // | Elf_Shdr   |
  // | ...        | contains offset to section start
  // | Elf_Shdr   |
  // +------------+
  //
  // To strip:
  // - leave the Elf_Ehdr and Elf_Phdr values in place.
  // - walk the sections making a new set of Elf_Shdr section headers for what we want to keep
  // - move the sections are keeping up to fill in gaps of sections we want to strip
  // - write new Elf_Shdr section headers to end of file, updating Elf_Ehdr
  // - truncate rest of file
  //

  std::vector<Elf_Shdr> section_headers;
  std::vector<Elf_Word> section_headers_original_indexes;
  section_headers.reserve(GetSectionHeaderNum());


  Elf_Shdr& string_section = GetSectionNameStringSection();
  for (Elf_Word i = 0; i < GetSectionHeaderNum(); i++) {
    Elf_Shdr& sh = GetSectionHeader(i);
    const char* name = GetString(string_section, sh.sh_name);
    if (name == NULL) {
      CHECK_EQ(0U, i);
      section_headers.push_back(sh);
      section_headers_original_indexes.push_back(0);
      continue;
    }
    if (StartsWith(name, ".debug")
        || (strcmp(name, ".strtab") == 0)
        || (strcmp(name, ".symtab") == 0)) {
      continue;
    }
    section_headers.push_back(sh);
    section_headers_original_indexes.push_back(i);
  }
  CHECK_NE(0U, section_headers.size());
  CHECK_EQ(section_headers.size(), section_headers_original_indexes.size());

  // section 0 is the NULL section, sections start at offset of first section
  Elf_Off offset = GetSectionHeader(1).sh_offset;
  for (size_t i = 1; i < section_headers.size(); i++) {
    Elf_Shdr& new_sh = section_headers[i];
    Elf_Shdr& old_sh = GetSectionHeader(section_headers_original_indexes[i]);
    CHECK_EQ(new_sh.sh_name, old_sh.sh_name);
    if (old_sh.sh_addralign > 1) {
      offset = RoundUp(offset, old_sh.sh_addralign);
    }
    if (old_sh.sh_offset == offset) {
      // already in place
      offset += old_sh.sh_size;
      continue;
    }
    // shift section earlier
    memmove(Begin() + offset,
            Begin() + old_sh.sh_offset,
            old_sh.sh_size);
    new_sh.sh_offset = offset;
    offset += old_sh.sh_size;
  }

  Elf_Off shoff = offset;
  size_t section_headers_size_in_bytes = section_headers.size() * sizeof(Elf_Shdr);
  memcpy(Begin() + offset, &section_headers[0], section_headers_size_in_bytes);
  offset += section_headers_size_in_bytes;

  GetHeader().e_shnum = section_headers.size();
  GetHeader().e_shoff = shoff;
  int result = ftruncate(file_->Fd(), offset);
  if (result != 0) {
    *error_msg = StringPrintf("Failed to truncate while stripping ELF file: '%s': %s",
                              file_->GetPath().c_str(), strerror(errno));
    return false;
  }
  return true;
}

}  // namespace art
