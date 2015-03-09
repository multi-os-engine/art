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

#include "elf_writer_quick.h"

#include <unordered_map>

#include "base/logging.h"
#include "base/unix_file/fd_file.h"
#include "buffered_output_stream.h"
#include "driver/compiler_driver.h"
#include "dwarf.h"
#include "dwarf/debug_frame_writer.h"
#include "dwarf/debug_line_writer.h"
#include "elf_builder.h"
#include "elf_file.h"
#include "elf_utils.h"
#include "file_output_stream.h"
#include "globals.h"
#include "leb128.h"
#include "oat.h"
#include "oat_writer.h"
#include "utils.h"

namespace art {

static void PushByte(std::vector<uint8_t>* buf, int data) {
  buf->push_back(data & 0xff);
}

static uint32_t PushStr(std::vector<uint8_t>* buf, const char* str, const char* def = nullptr) {
  if (str == nullptr) {
    str = def;
  }

  uint32_t offset = buf->size();
  for (size_t i = 0; str[i] != '\0'; ++i) {
    buf->push_back(str[i]);
  }
  buf->push_back('\0');
  return offset;
}

static uint32_t PushStr(std::vector<uint8_t>* buf, const std::string &str) {
  uint32_t offset = buf->size();
  buf->insert(buf->end(), str.begin(), str.end());
  buf->push_back('\0');
  return offset;
}

static void UpdateWord(std::vector<uint8_t>* buf, int offset, int data) {
  (*buf)[offset+0] = data;
  (*buf)[offset+1] = data >> 8;
  (*buf)[offset+2] = data >> 16;
  (*buf)[offset+3] = data >> 24;
}

static void PushHalf(std::vector<uint8_t>* buf, int data) {
  buf->push_back(data & 0xff);
  buf->push_back((data >> 8) & 0xff);
}

template <typename Elf_Word, typename Elf_Sword, typename Elf_Addr,
          typename Elf_Dyn, typename Elf_Sym, typename Elf_Ehdr,
          typename Elf_Phdr, typename Elf_Shdr>
bool ElfWriterQuick<Elf_Word, Elf_Sword, Elf_Addr, Elf_Dyn,
  Elf_Sym, Elf_Ehdr, Elf_Phdr, Elf_Shdr>::Create(File* elf_file,
                            OatWriter* oat_writer,
                            const std::vector<const DexFile*>& dex_files,
                            const std::string& android_root,
                            bool is_host,
                            const CompilerDriver& driver) {
  ElfWriterQuick elf_writer(driver, elf_file);
  return elf_writer.Write(oat_writer, dex_files, android_root, is_host);
}

void WriteCIE(dwarf::DebugFrameWriter<>* cfi, InstructionSet isa) {
  // Scratch registers should be marked as undefined.  This tells the
  // debugger that its value in the previous frame is not recoverable.
  switch (isa) {
    case kArm:
    case kThumb2: {
      dwarf::DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(0, 13, 0);  // R13(SP).
      // core registers.
      for (int reg = 0; reg < 13; reg++) {
        if (reg < 4 || reg == 12) {
          opcodes.Undefined(0, reg);
        } else {
          opcodes.SameValue(0, reg);
        }
      }
      // fp registers.
      for (int reg = 0; reg < 32; reg++) {
        int dwarf_fp_base = 64;
        if (reg < 16) {
          opcodes.Undefined(0, dwarf_fp_base + reg);
        } else {
          opcodes.SameValue(0, dwarf_fp_base + reg);
        }
      }
      int return_address_reg = 14;  // R14(LR).
      cfi->WriteCIE(return_address_reg, opcodes);
      return;
    }
    case kArm64: {
      dwarf::DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(0, 31, 0);  // R31(SP).
      // core registers.
      for (int reg = 0; reg < 30; reg++) {
        if (reg < 8 || reg == 16 || reg == 17) {
          opcodes.Undefined(0, reg);
        } else {
          opcodes.SameValue(0, reg);
        }
      }
      // fp registers.
      for (int reg = 0; reg < 32; reg++) {
        int dwarf_fp_base = 64;
        if (reg < 8 || reg >= 16) {
          opcodes.Undefined(0, dwarf_fp_base + reg);
        } else {
          opcodes.SameValue(0, dwarf_fp_base + reg);
        }
      }
      int return_address_reg = 30;  // R30(LR).
      cfi->WriteCIE(return_address_reg, opcodes);
      return;
    }
    case kMips:
    case kMips64: {
      dwarf::DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(0, 29, 0);  // R29(SP).
      // core registers.
      for (int reg = 1; reg < 26; reg++) {
        if (reg < 16 || reg == 24 || reg == 25) {  // AT, V*, A*, T*.
          opcodes.Undefined(0, reg);
        } else {
          opcodes.SameValue(0, reg);
        }
      }
      int return_address_reg = 31;  // R31(RA).
      cfi->WriteCIE(return_address_reg, opcodes);
      return;
    }
    case kX86: {
      dwarf::DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(0, 4, 4);   // R4(ESP).
      opcodes.Offset(0, 8, -4);  // R8(EIP).
      // core registers.
      for (int reg = 0; reg < 8; reg++) {
        if (reg <= 3) {
          opcodes.Undefined(0, reg);
        } else if (reg != 4) {  // R4(ESP).
          opcodes.SameValue(0, reg);
        }
      }
      // fp registers.
      for (int reg = 0; reg < 8; reg++) {
        int dwarf_fp_base = 21;
        opcodes.Undefined(0, dwarf_fp_base + reg);
      }
      int return_address_reg = 8;  // R8(EIP).
      cfi->WriteCIE(return_address_reg, opcodes);
      return;
    }
    case kX86_64: {
      dwarf::DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(0, 7, 8);  // R7(RSP).
      opcodes.Offset(0, 16, -8);  // R16(RIP).
      // core registers.
      for (int reg = 0; reg < 16; reg++) {
        if (reg < 12 && reg != 3 && reg != 6) {  // except EBX and EBP.
          opcodes.Undefined(0, reg);
        } else if (reg != 7) {  // ESP.
          opcodes.SameValue(0, reg);
        }
      }
      // fp registers.
      for (int reg = 0; reg < 16; reg++) {
        int dwarf_fp_base = 17;
        if (reg < 12) {
          opcodes.Undefined(0, dwarf_fp_base + reg);
        } else {
          opcodes.SameValue(0, dwarf_fp_base + reg);
        }
      }
      int return_address_reg = 16;  // R16(RIP).
      cfi->WriteCIE(return_address_reg, opcodes);
      return;
    }
    case kNone:
      break;
  }
  LOG(FATAL) << "Can not write CIE frame for ISA " << isa;
  UNREACHABLE();
}

class OatWriterWrapper FINAL : public CodeOutput {
 public:
  explicit OatWriterWrapper(OatWriter* oat_writer) : oat_writer_(oat_writer) {}

  void SetCodeOffset(size_t offset) {
    oat_writer_->SetOatDataOffset(offset);
  }
  bool Write(OutputStream* out) OVERRIDE {
    return oat_writer_->Write(out);
  }
 private:
  OatWriter* const oat_writer_;
};

template <typename Elf_Word, typename Elf_Sword, typename Elf_Addr,
          typename Elf_Dyn, typename Elf_Sym, typename Elf_Ehdr,
          typename Elf_Phdr, typename Elf_Shdr>
static void WriteDebugSymbols(const CompilerDriver* compiler_driver,
                              ElfBuilder<Elf_Word, Elf_Sword, Elf_Addr, Elf_Dyn,
                                         Elf_Sym, Elf_Ehdr, Elf_Phdr, Elf_Shdr>* builder,
                              OatWriter* oat_writer);

template <typename Elf_Word, typename Elf_Sword, typename Elf_Addr,
          typename Elf_Dyn, typename Elf_Sym, typename Elf_Ehdr,
          typename Elf_Phdr, typename Elf_Shdr>
bool ElfWriterQuick<Elf_Word, Elf_Sword, Elf_Addr, Elf_Dyn,
  Elf_Sym, Elf_Ehdr, Elf_Phdr, Elf_Shdr>::Write(OatWriter* oat_writer,
                           const std::vector<const DexFile*>& dex_files_unused ATTRIBUTE_UNUSED,
                           const std::string& android_root_unused ATTRIBUTE_UNUSED,
                           bool is_host_unused ATTRIBUTE_UNUSED) {
  constexpr bool debug = false;
  const OatHeader& oat_header = oat_writer->GetOatHeader();
  Elf_Word oat_data_size = oat_header.GetExecutableOffset();
  uint32_t oat_exec_size = oat_writer->GetSize() - oat_data_size;
  uint32_t oat_bss_size = oat_writer->GetBssSize();

  OatWriterWrapper wrapper(oat_writer);

  std::unique_ptr<ElfBuilder<Elf_Word, Elf_Sword, Elf_Addr, Elf_Dyn,
                             Elf_Sym, Elf_Ehdr, Elf_Phdr, Elf_Shdr> > builder(
      new ElfBuilder<Elf_Word, Elf_Sword, Elf_Addr, Elf_Dyn,
                     Elf_Sym, Elf_Ehdr, Elf_Phdr, Elf_Shdr>(
          &wrapper,
          elf_file_,
          compiler_driver_->GetInstructionSet(),
          0,
          oat_data_size,
          oat_data_size,
          oat_exec_size,
          RoundUp(oat_data_size + oat_exec_size, kPageSize),
          oat_bss_size,
          compiler_driver_->GetCompilerOptions().GetIncludeDebugSymbols(),
          debug));

  if (!builder->Init()) {
    return false;
  }

  if (compiler_driver_->GetCompilerOptions().GetIncludeDebugSymbols()) {
    WriteDebugSymbols(compiler_driver_, builder.get(), oat_writer);
  }

  if (compiler_driver_->GetCompilerOptions().GetIncludePatchInformation()) {
    ElfRawSectionBuilder<Elf_Word, Elf_Sword, Elf_Shdr> oat_patches(
        ".oat_patches", SHT_OAT_PATCH, 0, NULL, 0, sizeof(uintptr_t), sizeof(uintptr_t));
    const std::vector<uintptr_t>& locations = oat_writer->GetAbsolutePatchLocations();
    const uint8_t* begin = reinterpret_cast<const uint8_t*>(&locations[0]);
    const uint8_t* end = begin + locations.size() * sizeof(locations[0]);
    oat_patches.GetBuffer()->assign(begin, end);
    if (debug) {
      LOG(INFO) << "Prepared .oat_patches for " << locations.size() << " patches.";
    }
    builder->RegisterRawSection(oat_patches);
  }

  return builder->Write();
}

// TODO: rewriting it using DexFile::DecodeDebugInfo needs unneeded stuff.
static void GetLineInfoForJava(const uint8_t* dbgstream, DefaultSrcMap* dex2line) {
  if (dbgstream == nullptr) {
    return;
  }

  int adjopcode;
  uint32_t dex_offset = 0;
  uint32_t java_line = DecodeUnsignedLeb128(&dbgstream);

  // skip parameters
  for (uint32_t param_count = DecodeUnsignedLeb128(&dbgstream); param_count != 0; --param_count) {
    DecodeUnsignedLeb128(&dbgstream);
  }

  for (bool is_end = false; is_end == false; ) {
    uint8_t opcode = *dbgstream;
    dbgstream++;
    switch (opcode) {
    case DexFile::DBG_END_SEQUENCE:
      is_end = true;
      break;

    case DexFile::DBG_ADVANCE_PC:
      dex_offset += DecodeUnsignedLeb128(&dbgstream);
      break;

    case DexFile::DBG_ADVANCE_LINE:
      java_line += DecodeSignedLeb128(&dbgstream);
      break;

    case DexFile::DBG_START_LOCAL:
    case DexFile::DBG_START_LOCAL_EXTENDED:
      DecodeUnsignedLeb128(&dbgstream);
      DecodeUnsignedLeb128(&dbgstream);
      DecodeUnsignedLeb128(&dbgstream);

      if (opcode == DexFile::DBG_START_LOCAL_EXTENDED) {
        DecodeUnsignedLeb128(&dbgstream);
      }
      break;

    case DexFile::DBG_END_LOCAL:
    case DexFile::DBG_RESTART_LOCAL:
      DecodeUnsignedLeb128(&dbgstream);
      break;

    case DexFile::DBG_SET_PROLOGUE_END:
    case DexFile::DBG_SET_EPILOGUE_BEGIN:
    case DexFile::DBG_SET_FILE:
      break;

    default:
      adjopcode = opcode - DexFile::DBG_FIRST_SPECIAL;
      dex_offset += adjopcode / DexFile::DBG_LINE_RANGE;
      java_line += DexFile::DBG_LINE_BASE + (adjopcode % DexFile::DBG_LINE_RANGE);
      dex2line->push_back({dex_offset, static_cast<int32_t>(java_line)});
      break;
    }
  }
}

/*
 * @brief Generate the DWARF debug_info and debug_abbrev sections
 * @param oat_writer The Oat file Writer.
 * @param dbg_info Compilation unit information.
 * @param dbg_abbrev Abbreviations used to generate dbg_info.
 * @param dbg_str Debug strings.
 */
static void FillInCFIInformation(OatWriter* oat_writer,
                                 std::vector<uint8_t>* dbg_info,
                                 std::vector<uint8_t>* dbg_abbrev,
                                 std::vector<uint8_t>* dbg_str,
                                 std::vector<uint8_t>* dbg_line,
                                 uint32_t text_section_offset) {
  // DWARF requires that we write line numbers in strictly increasing order of addresses.
  // This also means that we can not have duplicate copies of a same method (due to DEDUP).
  std::vector<const OatWriter::DebugInfo*> methods;
  methods.reserve(oat_writer->GetCFIMethodInfo().size());
  for (size_t i = 0; i < oat_writer->GetCFIMethodInfo().size(); i++) {
    methods.push_back(&oat_writer->GetCFIMethodInfo()[i]);
  }
  std::sort(methods.begin(), methods.end(), [] (const OatWriter::DebugInfo* lhs, const OatWriter::DebugInfo* rhs) -> bool {
    return lhs->low_pc_ < rhs->low_pc_;
  });
  methods.erase(std::unique(methods.begin(), methods.end(), [] (const OatWriter::DebugInfo* lhs, const OatWriter::DebugInfo* rhs) -> bool {
    return lhs->low_pc_ == rhs->low_pc_;
  }), methods.end());

  uint32_t cunit_low_pc = methods.empty() ? 0 : methods.front()->low_pc_;
  uint32_t cunit_high_pc = methods.empty() ? 0 : methods.back()->high_pc_;

  uint32_t producer_str_offset = PushStr(dbg_str, "Android dex2oat");

  bool use_64bit_addresses = false;

  // Create the debug_abbrev section with boilerplate information.
  // We only care about low_pc and high_pc right now for the compilation
  // unit and methods.

  // Tag 1: Compilation unit: DW_TAG_compile_unit.
  PushByte(dbg_abbrev, 1);
  PushByte(dbg_abbrev, dwarf::DW_TAG_compile_unit);

  // There are children (the methods).
  PushByte(dbg_abbrev, dwarf::DW_CHILDREN_yes);

  // DW_AT_producer DW_FORM_data1.
  // REVIEW: we can get rid of dbg_str section if
  // DW_FORM_string (immediate string) was used everywhere instead of
  // DW_FORM_strp (ref to string from .debug_str section).
  // DW_FORM_strp makes sense only if we reuse the strings.
  PushByte(dbg_abbrev, dwarf::DW_AT_producer);
  PushByte(dbg_abbrev, dwarf::DW_FORM_strp);

  // DW_LANG_Java DW_FORM_data1.
  PushByte(dbg_abbrev, dwarf::DW_AT_language);
  PushByte(dbg_abbrev, dwarf::DW_FORM_data1);

  // DW_AT_low_pc DW_FORM_addr.
  PushByte(dbg_abbrev, dwarf::DW_AT_low_pc);
  PushByte(dbg_abbrev, dwarf::DW_FORM_addr);

  // DW_AT_high_pc DW_FORM_addr.
  PushByte(dbg_abbrev, dwarf::DW_AT_high_pc);
  PushByte(dbg_abbrev, dwarf::DW_FORM_addr);

  if (dbg_line != nullptr) {
    // DW_AT_stmt_list DW_FORM_sec_offset.
    PushByte(dbg_abbrev, dwarf::DW_AT_stmt_list);
    PushByte(dbg_abbrev, dwarf::DW_FORM_sec_offset);
  }

  // End of DW_TAG_compile_unit.
  PushByte(dbg_abbrev, 0);  // DW_AT.
  PushByte(dbg_abbrev, 0);  // DW_FORM.

  // Tag 2: Compilation unit: DW_TAG_subprogram.
  PushByte(dbg_abbrev, 2);
  PushByte(dbg_abbrev, dwarf::DW_TAG_subprogram);

  // There are no children.
  PushByte(dbg_abbrev, dwarf::DW_CHILDREN_no);

  // Name of the method.
  PushByte(dbg_abbrev, dwarf::DW_AT_name);
  PushByte(dbg_abbrev, dwarf::DW_FORM_strp);

  // DW_AT_low_pc DW_FORM_addr.
  PushByte(dbg_abbrev, dwarf::DW_AT_low_pc);
  PushByte(dbg_abbrev, dwarf::DW_FORM_addr);

  // DW_AT_high_pc DW_FORM_addr.
  PushByte(dbg_abbrev, dwarf::DW_AT_high_pc);
  PushByte(dbg_abbrev, dwarf::DW_FORM_addr);

  // End of DW_TAG_subprogram.
  PushByte(dbg_abbrev, 0);  // DW_AT.
  PushByte(dbg_abbrev, 0);  // DW_FORM.

  // End of abbrevs for compilation unit
  PushByte(dbg_abbrev, 0);

  // Start the debug_info section with the header information
  // 'unit_length' will be filled in later.
  int cunit_length = dbg_info->size();
  Push32(dbg_info, 0);

  // 'version' - 3.
  PushHalf(dbg_info, 3);

  // Offset into .debug_abbrev section (always 0).
  Push32(dbg_info, 0);

  // Address size: 4.
  PushByte(dbg_info, use_64bit_addresses ? 8 : 4);

  // Start the description for the compilation unit.
  // This uses tag 1.
  PushByte(dbg_info, 1);

  // The producer is Android dex2oat.
  Push32(dbg_info, producer_str_offset);

  // The language is Java.
  PushByte(dbg_info, dwarf::DW_LANG_Java);

  // low_pc and high_pc.
  Push32(dbg_info, cunit_low_pc + text_section_offset);
  Push32(dbg_info, cunit_high_pc + text_section_offset);

  if (dbg_line != nullptr) {
    // Line number table offset.
    Push32(dbg_info, dbg_line->size());
  }

  for (auto method_info : methods) {
    // Start a new TAG: subroutine (2).
    PushByte(dbg_info, 2);

    // Enter name, low_pc, high_pc.
    Push32(dbg_info, PushStr(dbg_str, method_info->method_name_));
    Push32(dbg_info, method_info->low_pc_ + text_section_offset);
    Push32(dbg_info, method_info->high_pc_ + text_section_offset);
  }

  if (dbg_line != nullptr) {
    // TODO: in gdb info functions <regexp> - reports Java functions, but
    // source file is <unknown> because .debug_line is formed as one
    // compilation unit. To fix this it is possible to generate
    // a separate compilation unit for every distinct Java source.
    // Each of the these compilation units can have several non-adjacent
    // method ranges.

    std::vector<const char*> include_directories;

    // File_names (sequence of file entries).
    std::vector<dwarf::DebugLineWriter<>::FileEntry> file_entries;
    std::unordered_map<const char*, size_t> files;
    for (auto method_info : methods) {
      // TODO: add package directory to the file name.
      const char* file_name = method_info->src_file_name_ == nullptr ? "null" : method_info->src_file_name_;
      auto found = files.find(file_name);
      if (found == files.end()) {
        size_t file_index = 1 + files.size();
        files[file_name] = file_index;
        dwarf::DebugLineWriter<>::FileEntry entry = {
          file_name,
          0,  // include directory index - no directory.
          0,  // modification time - NA.
          0   // file length - NA.
        };
        file_entries.push_back(entry);
      }
    }

    InstructionSet isa = oat_writer->GetOatHeader().GetInstructionSet();
    int code_factor_bits_ = (isa == kThumb2 ? 1 : 0);
    dwarf::DebugLineOpCodeWriter<> opcodes(use_64bit_addresses, code_factor_bits_);
    opcodes.SetAddress(cunit_low_pc + text_section_offset);

    switch (isa) {
      case kThumb2:
        opcodes.SetISA(1);  // DW_ISA_ARM_thumb.
        break;
      case kArm:
        opcodes.SetISA(2);  // DW_ISA_ARM_arm.
        break;
      default:
        break;
    }

    DefaultSrcMap dex2line_map;
    for (auto method_info : methods) {
      const char* file_name = (method_info->src_file_name_ == nullptr) ? "null" : method_info->src_file_name_;
      size_t file_index = files[file_name];
      DCHECK_NE(file_index, 0U) << file_name;

      dex2line_map.clear();
      GetLineInfoForJava(method_info->dbgstream_, &dex2line_map);
      if (!dex2line_map.empty()) {
        bool first = true;
        for (SrcMapElem pc2dex : method_info->compiled_method_->GetSrcMappingTable()) {
          uint32_t pc = pc2dex.from_;
          int dex = pc2dex.to_;
          auto dex2line = dex2line_map.Find(static_cast<uint32_t>(dex));
          if (dex2line != dex2line_map.end()) {
            int line = dex2line->to_;
            if (first) {
              first = false;
              opcodes.SetFile(file_index);
              if (pc > 0) {
                // Assume that any preceding code is prologue.
                int first_line = dex2line_map.front().to_;
                opcodes.AddRow(text_section_offset + method_info->low_pc_, first_line);
                opcodes.SetPrologueEnd();
              }
              opcodes.AddRow(text_section_offset + method_info->low_pc_ + pc, line);
            } else if (line != opcodes.current_line()) {
              opcodes.AddRow(text_section_offset + method_info->low_pc_ + pc, line);
            }
          }
        }
      }
    }

    // First byte after the end of a sequence of target machine instructions.
    opcodes.SetAddress(cunit_high_pc + text_section_offset);
    opcodes.EndSequence();

    dwarf::DebugLineWriter<> dbg_line_writer(dbg_line);
    dbg_line_writer.WriteTable(include_directories, file_entries, opcodes);
  }

  // One byte terminator.
  PushByte(dbg_info, 0);

  // We have now walked all the methods.  Fill in lengths.
  UpdateWord(dbg_info, cunit_length, dbg_info->size() - cunit_length - 4);
}

template <typename Elf_Word, typename Elf_Sword, typename Elf_Addr,
          typename Elf_Dyn, typename Elf_Sym, typename Elf_Ehdr,
          typename Elf_Phdr, typename Elf_Shdr>
// Do not inline to avoid Clang stack frame problems. b/18738594
NO_INLINE
static void WriteDebugSymbols(const CompilerDriver* compiler_driver,
                              ElfBuilder<Elf_Word, Elf_Sword, Elf_Addr, Elf_Dyn,
                                         Elf_Sym, Elf_Ehdr, Elf_Phdr, Elf_Shdr>* builder,
                              OatWriter* oat_writer) {
  std::vector<uint8_t> cfi_data;
  bool is_64bit = GetInstructionSetPointerSize(compiler_driver->GetInstructionSet()) == 8u;
  dwarf::DebugFrameWriter<> cfi(&cfi_data, is_64bit);
  WriteCIE(&cfi, compiler_driver->GetInstructionSet());

  Elf_Addr text_section_address = builder->GetTextBuilder().GetSection()->sh_addr;

  // Iterate over the compiled methods.
  const std::vector<OatWriter::DebugInfo>& method_info = oat_writer->GetCFIMethodInfo();
  ElfSymtabBuilder<Elf_Word, Elf_Sword, Elf_Addr, Elf_Sym, Elf_Shdr>* symtab =
      builder->GetSymtabBuilder();
  for (auto it = method_info.begin(); it != method_info.end(); ++it) {
    symtab->AddSymbol(it->method_name_, &builder->GetTextBuilder(), it->low_pc_, true,
                      it->high_pc_ - it->low_pc_, STB_GLOBAL, STT_FUNC);

    // Conforming to aaelf, add $t mapping symbol to indicate start of a sequence of thumb2
    // instructions, so that disassembler tools can correctly disassemble.
    if (it->compiled_method_->GetInstructionSet() == kThumb2) {
      symtab->AddSymbol("$t", &builder->GetTextBuilder(), it->low_pc_ & ~1, true,
                        0, STB_LOCAL, STT_NOTYPE);
    }

    // Include FDE for compiled method, if possible.
    DCHECK(it->compiled_method_ != nullptr);
    const SwapVector<uint8_t>* unwind_opcodes = it->compiled_method_->GetCFIInfo();
    if (unwind_opcodes != nullptr) {
      cfi.WriteFDE(text_section_address + it->low_pc_, it->high_pc_ - it->low_pc_,
                   unwind_opcodes->data(), unwind_opcodes->size());
    }
  }

  bool hasLineInfo = false;
  for (auto& dbg_info : oat_writer->GetCFIMethodInfo()) {
    if (dbg_info.dbgstream_ != nullptr &&
        !dbg_info.compiled_method_->GetSrcMappingTable().empty()) {
      hasLineInfo = true;
      break;
    }
  }

  if (compiler_driver->GetCompilerOptions().GetGenerateGDBInformation()) {
    ElfRawSectionBuilder<Elf_Word, Elf_Sword, Elf_Shdr> debug_info(".debug_info",
                                                                   SHT_PROGBITS,
                                                                   0, nullptr, 0, 1, 0);
    ElfRawSectionBuilder<Elf_Word, Elf_Sword, Elf_Shdr> debug_abbrev(".debug_abbrev",
                                                                     SHT_PROGBITS,
                                                                     0, nullptr, 0, 1, 0);
    ElfRawSectionBuilder<Elf_Word, Elf_Sword, Elf_Shdr> debug_str(".debug_str",
                                                                  SHT_PROGBITS,
                                                                  0, nullptr, 0, 1, 0);
    ElfRawSectionBuilder<Elf_Word, Elf_Sword, Elf_Shdr> debug_line(".debug_line",
                                                                   SHT_PROGBITS,
                                                                   0, nullptr, 0, 1, 0);

    FillInCFIInformation(oat_writer, debug_info.GetBuffer(),
                         debug_abbrev.GetBuffer(), debug_str.GetBuffer(),
                         hasLineInfo ? debug_line.GetBuffer() : nullptr,
                         text_section_address);

    builder->RegisterRawSection(debug_info);
    builder->RegisterRawSection(debug_abbrev);

    ElfRawSectionBuilder<Elf_Word, Elf_Sword, Elf_Shdr> eh_frame(".eh_frame",
                                                                 SHT_PROGBITS,
                                                                 SHF_ALLOC,
                                                                 nullptr, 0, 4, 0);
    eh_frame.SetBuffer(std::move(cfi_data));
    builder->RegisterRawSection(eh_frame);

    if (hasLineInfo) {
      builder->RegisterRawSection(debug_line);
    }

    builder->RegisterRawSection(debug_str);
  }
}

// Explicit instantiations
template class ElfWriterQuick<Elf32_Word, Elf32_Sword, Elf32_Addr, Elf32_Dyn,
                              Elf32_Sym, Elf32_Ehdr, Elf32_Phdr, Elf32_Shdr>;
template class ElfWriterQuick<Elf64_Word, Elf64_Sword, Elf64_Addr, Elf64_Dyn,
                              Elf64_Sym, Elf64_Ehdr, Elf64_Phdr, Elf64_Shdr>;

}  // namespace art
