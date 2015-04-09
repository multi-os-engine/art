void WriteCIE(InstructionSet isa, std::vector<uint8_t>* eh_frame) {
  const bool is64bit = Is64BitInstructionSet(isa);
  // Scratch registers should be marked as undefined.  This tells the
  // debugger that its value in the previous frame is not recoverable.
  switch (isa) {
    case kArm:
    case kThumb2: {
      dwarf::DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(dwarf::Reg::ArmCore(13), 0);  // R13(SP).
      // core registers.
      for (int reg = 0; reg < 13; reg++) {
        if (reg < 4 || reg == 12) {
          opcodes.Undefined(dwarf::Reg::ArmCore(reg));
        } else {
          opcodes.SameValue(dwarf::Reg::ArmCore(reg));
        }
      }
      // fp registers.
      for (int reg = 0; reg < 32; reg++) {
        if (reg < 16) {
          opcodes.Undefined(dwarf::Reg::ArmFp(reg));
        } else {
          opcodes.SameValue(dwarf::Reg::ArmFp(reg));
        }
      }
      auto return_address_reg = dwarf::Reg::ArmCore(14);  // R14(LR).
      dwarf::WriteEhFrameCIE(is64bit, return_address_reg, opcodes, eh_frame);
      return;
    }
    case kArm64: {
      dwarf::DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(dwarf::Reg::Arm64Core(31), 0);  // R31(SP).
      // core registers.
      for (int reg = 0; reg < 30; reg++) {
        if (reg < 8 || reg == 16 || reg == 17) {
          opcodes.Undefined(dwarf::Reg::Arm64Core(reg));
        } else {
          opcodes.SameValue(dwarf::Reg::Arm64Core(reg));
        }
      }
      // fp registers.
      for (int reg = 0; reg < 32; reg++) {
        if (reg < 8 || reg >= 16) {
          opcodes.Undefined(dwarf::Reg::Arm64Fp(reg));
        } else {
          opcodes.SameValue(dwarf::Reg::Arm64Fp(reg));
        }
      }
      auto return_address_reg = dwarf::Reg::Arm64Core(30);  // R30(LR).
      dwarf::WriteEhFrameCIE(is64bit, return_address_reg, opcodes, eh_frame);
      return;
    }
    case kMips:
    case kMips64: {
      dwarf::DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(dwarf::Reg::MipsCore(29), 0);  // R29(SP).
      // core registers.
      for (int reg = 1; reg < 26; reg++) {
        if (reg < 16 || reg == 24 || reg == 25) {  // AT, V*, A*, T*.
          opcodes.Undefined(dwarf::Reg::MipsCore(reg));
        } else {
          opcodes.SameValue(dwarf::Reg::MipsCore(reg));
        }
      }
      auto return_address_reg = dwarf::Reg::MipsCore(31);  // R31(RA).
      dwarf::WriteEhFrameCIE(is64bit, return_address_reg, opcodes, eh_frame);
      return;
    }
    case kX86: {
      dwarf::DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(dwarf::Reg::X86Core(4), 4);   // R4(ESP).
      opcodes.Offset(dwarf::Reg::X86Core(8), -4);  // R8(EIP).
      // core registers.
      for (int reg = 0; reg < 8; reg++) {
        if (reg <= 3) {
          opcodes.Undefined(dwarf::Reg::X86Core(reg));
        } else if (reg == 4) {
          // Stack pointer.
        } else {
          opcodes.SameValue(dwarf::Reg::X86Core(reg));
        }
      }
      // fp registers.
      for (int reg = 0; reg < 8; reg++) {
        opcodes.Undefined(dwarf::Reg::X86Fp(reg));
      }
      auto return_address_reg = dwarf::Reg::X86Core(8);  // R8(EIP).
      dwarf::WriteEhFrameCIE(is64bit, return_address_reg, opcodes, eh_frame);
      return;
    }
    case kX86_64: {
      dwarf::DebugFrameOpCodeWriter<> opcodes;
      opcodes.DefCFA(dwarf::Reg::X86_64Core(4), 8);  // R4(RSP).
      opcodes.Offset(dwarf::Reg::X86_64Core(16), -8);  // R16(RIP).
      // core registers.
      for (int reg = 0; reg < 16; reg++) {
        if (reg == 4) {
          // Stack pointer.
        } else if (reg < 12 && reg != 3 && reg != 5) {  // except EBX and EBP.
          opcodes.Undefined(dwarf::Reg::X86_64Core(reg));
        } else {
          opcodes.SameValue(dwarf::Reg::X86_64Core(reg));
        }
      }
      // fp registers.
      for (int reg = 0; reg < 16; reg++) {
        if (reg < 12) {
          opcodes.Undefined(dwarf::Reg::X86_64Fp(reg));
        } else {
          opcodes.SameValue(dwarf::Reg::X86_64Fp(reg));
        }
      }
      auto return_address_reg = dwarf::Reg::X86_64Core(16);  // R16(RIP).
      dwarf::WriteEhFrameCIE(is64bit, return_address_reg, opcodes, eh_frame);
      return;
    }
    case kNone:
      break;
  }
  LOG(FATAL) << "Can not write CIE frame for ISA " << isa;
  UNREACHABLE();
}

/*
 * @brief Generate the DWARF debug_info and debug_abbrev sections
 * @param oat_writer The Oat file Writer.
 * @param dbg_info Compilation unit information.
 * @param dbg_abbrev Abbreviations used to generate dbg_info.
 * @param dbg_str Debug strings.
 */
static void FillInCFIInformation(OatWriter* oat_writer,
                                 std::vector<uint8_t>* debug_info_data,
                                 std::vector<uint8_t>* debug_abbrev_data,
                                 std::vector<uint8_t>* debug_str_data,
                                 std::vector<uint8_t>* debug_line_data,
                                 uint32_t text_section_offset) {
  const std::vector<OatWriter::DebugInfo>& method_infos = oat_writer->GetCFIMethodInfo();

  uint32_t cunit_low_pc = static_cast<uint32_t>(-1);
  uint32_t cunit_high_pc = 0;
  for (auto method_info : method_infos) {
    cunit_low_pc = std::min(cunit_low_pc, method_info.low_pc_);
    cunit_high_pc = std::max(cunit_high_pc, method_info.high_pc_);
  }

  dwarf::DebugInfoEntryWriter<> info(false /* 32 bit */, debug_abbrev_data);
  info.StartTag(dwarf::DW_TAG_compile_unit, dwarf::DW_CHILDREN_yes);
  info.WriteStrp(dwarf::DW_AT_producer, "Android dex2oat", debug_str_data);
  info.WriteData1(dwarf::DW_AT_language, dwarf::DW_LANG_Java);
  info.WriteAddr(dwarf::DW_AT_low_pc, cunit_low_pc + text_section_offset);
  info.WriteAddr(dwarf::DW_AT_high_pc, cunit_high_pc + text_section_offset);
  if (debug_line_data != nullptr) {
    info.WriteData4(dwarf::DW_AT_stmt_list, debug_line_data->size());
  }
  for (auto method_info : method_infos) {
    std::string method_name = PrettyMethod(method_info.dex_method_index_,
                                           *method_info.dex_file_, true);
    if (method_info.deduped_) {
      // TODO We should place the DEDUPED tag on the first instance of a deduplicated symbol
      // so that it will show up in a debuggerd crash report.
      method_name += " [ DEDUPED ]";
    }
    info.StartTag(dwarf::DW_TAG_subprogram, dwarf::DW_CHILDREN_no);
    info.WriteStrp(dwarf::DW_AT_name, method_name.data(), debug_str_data);
    info.WriteAddr(dwarf::DW_AT_low_pc, method_info.low_pc_ + text_section_offset);
    info.WriteAddr(dwarf::DW_AT_high_pc, method_info.high_pc_ + text_section_offset);
    info.EndTag();  // DW_TAG_subprogram
  }
  info.EndTag();  // DW_TAG_compile_unit
  dwarf::WriteDebugInfoCU(0 /* debug_abbrev_offset */, info, debug_info_data);

  if (debug_line_data != nullptr) {
    // TODO: in gdb info functions <regexp> - reports Java functions, but
    // source file is <unknown> because .debug_line is formed as one
    // compilation unit. To fix this it is possible to generate
    // a separate compilation unit for every distinct Java source.
    // Each of the these compilation units can have several non-adjacent
    // method ranges.

    std::vector<dwarf::FileEntry> files;
    std::unordered_map<std::string, size_t> files_map;
    std::vector<std::string> directories;
    std::unordered_map<std::string, size_t> directories_map;

    int code_factor_bits_ = 0;
    int isa = -1;
    switch (oat_writer->GetOatHeader().GetInstructionSet()) {
      case kArm:  // arm actually means thumb2.
      case kThumb2:
        code_factor_bits_ = 1;  // 16-bit instuctions
        isa = 1;  // DW_ISA_ARM_thumb.
        break;
      case kArm64:
      case kMips:
      case kMips64:
        code_factor_bits_ = 2;  // 32-bit instructions
        break;
      case kNone:
      case kX86:
      case kX86_64:
        break;
    }

    dwarf::DebugLineOpCodeWriter<> opcodes(false /* 32bit */, code_factor_bits_);
    opcodes.SetAddress(text_section_offset + cunit_low_pc);
    if (isa != -1) {
      opcodes.SetISA(isa);
    }
    for (const OatWriter::DebugInfo& mi : method_infos) {
      // Addresses in the line table should be unique and increasing.
      if (mi.deduped_) {
        continue;
      }

      struct DebugInfoCallbacks {
        static bool NewPosition(void* ctx, uint32_t address, uint32_t line) {
          auto* context = reinterpret_cast<DebugInfoCallbacks*>(ctx);
          context->dex2line_.push_back({address, static_cast<int32_t>(line)});
          return false;
        }
        DefaultSrcMap dex2line_;
      } debug_info_callbacks;

      const DexFile* dex = mi.dex_file_;
      if (mi.code_item_ != nullptr) {
        dex->DecodeDebugInfo(mi.code_item_,
                             (mi.access_flags_ & kAccStatic) != 0,
                             mi.dex_method_index_,
                             DebugInfoCallbacks::NewPosition,
                             nullptr,
                             &debug_info_callbacks);
      }


      // Get and deduplicate directory and filename.
      int file_index = 0;  // 0 - primary source file of the compilation.
      auto& dex_class_def = dex->GetClassDef(mi.class_def_index_);
      const char* source_file = dex->GetSourceFile(dex_class_def);
      if (source_file != nullptr) {
        std::string file_name(source_file);
        size_t file_name_slash = file_name.find_last_of('/');
        std::string class_name(dex->GetClassDescriptor(dex_class_def));
        size_t class_name_slash = class_name.find_last_of('/');
        std::string full_path(file_name);

        // Guess directory from package name.
        int directory_index = 0;  // 0 - current directory of the compilation.
        if (file_name_slash == std::string::npos &&  // Just filename.
            class_name.front() == 'L' &&  // Type descriptor for a class.
            class_name_slash != std::string::npos) {  // Has package name.
          std::string package_name = class_name.substr(1, class_name_slash - 1);
          auto it = directories_map.find(package_name);
          if (it == directories_map.end()) {
            directory_index = 1 + directories.size();
            directories_map.emplace(package_name, directory_index);
            directories.push_back(package_name);
          } else {
            directory_index = it->second;
          }
          full_path = package_name + "/" + file_name;
        }

        // Add file entry.
        auto it2 = files_map.find(full_path);
        if (it2 == files_map.end()) {
          file_index = 1 + files.size();
          files_map.emplace(full_path, file_index);
          files.push_back(dwarf::FileEntry {
            file_name,
            directory_index,
            0,  // Modification time - NA.
            0,  // File size - NA.
          });
        } else {
          file_index = it2->second;
        }
      }
      opcodes.SetFile(file_index);

      // Generate mapping opcodes from PC to Java lines.
      const DefaultSrcMap& dex2line_map = debug_info_callbacks.dex2line_;
      uint32_t low_pc = text_section_offset + mi.low_pc_;
      if (file_index != 0 && !dex2line_map.empty()) {
        bool first = true;
        for (SrcMapElem pc2dex : mi.compiled_method_->GetSrcMappingTable()) {
          uint32_t pc = pc2dex.from_;
          int dex_pc = pc2dex.to_;
          auto dex2line = dex2line_map.Find(static_cast<uint32_t>(dex_pc));
          if (dex2line.first) {
            int line = dex2line.second;
            if (first) {
              first = false;
              if (pc > 0) {
                // Assume that any preceding code is prologue.
                int first_line = dex2line_map.front().to_;
                // Prologue is not a sensible place for a breakpoint.
                opcodes.NegateStmt();
                opcodes.AddRow(low_pc, first_line);
                opcodes.NegateStmt();
                opcodes.SetPrologueEnd();
              }
              opcodes.AddRow(low_pc + pc, line);
            } else if (line != opcodes.CurrentLine()) {
              opcodes.AddRow(low_pc + pc, line);
            }
          }
        }
      } else {
        // line 0 - instruction cannot be attributed to any source line.
        opcodes.AddRow(low_pc, 0);
      }
    }
    opcodes.AdvancePC(text_section_offset + cunit_high_pc);
    opcodes.EndSequence();
    dwarf::WriteDebugLineTable(directories, files, opcodes, debug_line_data);
  }
}
