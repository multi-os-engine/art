/*
 * Copyright (C) 2014 The Android Open Source Project
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

#include <vector>
#include <algorithm>

#include "vtune_support.h"
#include "mapping_table.h"
#include "oat_file-inl.h"
#include "dex_file-inl.h"
#include "dex_instruction.h"
#include "utils.h"
#include "cutils/process_name.h"

#if defined(HAVE_PRCTL)
#include <sys/prctl.h>
#endif

#include "runtime.h"
#include "vtune/jitprofiling.h"

namespace art {

typedef std::vector<LineNumberInfo> LineInfoTable;

struct SortLineNumberInfoByOffset {
  bool operator()(LineNumberInfo const& lhs, LineNumberInfo const& rhs) {
    return lhs.Offset < rhs.Offset;
  }

  static void sort(LineInfoTable &table) {
    std::sort(table.begin(), table.end(), SortLineNumberInfoByOffset());
  }
};

struct SortLineNumberInfoByLine {
  static const unsigned NOT_FOUND = 0 - 1;

  bool operator()(LineNumberInfo const& lhs, LineNumberInfo const& rhs) {
    return lhs.LineNumber < rhs.LineNumber;
  }

  static void sort(LineInfoTable &table) {
    std::sort(table.begin(), table.end(), SortLineNumberInfoByLine());
  }

  static int find(LineInfoTable &table, unsigned line, bool strict = false) {
    LineInfoTable::iterator lower =
           std::lower_bound(table.begin(), table.end(), LineNumberInfo( {0, line}), SortLineNumberInfoByLine());
    return lower == table.end() || (strict && lower->LineNumber != line)
           ? NOT_FOUND
           : lower->Offset;
  }
};

// Reads debug information from dex file to get BC to Java line mapping
static void getLineInfoForJava(const uint8_t *dbgstream, LineInfoTable& table, LineInfoTable& pc2dex) {
  SortLineNumberInfoByLine::sort(pc2dex);

  int adjopcode;
  uint32_t address = 0;
  uint32_t line = DecodeUnsignedLeb128(&dbgstream);

  // skip parameters
  for(uint32_t param_count = DecodeUnsignedLeb128(&dbgstream); param_count != 0; --param_count) {
    DecodeUnsignedLeb128(&dbgstream);
  }

  for(bool is_end = false; is_end == false; ) {
    uint8_t opcode = *dbgstream++;
    switch (opcode) {
    case DexFile::DBG_END_SEQUENCE:
      is_end = true;
      break;

    case DexFile::DBG_ADVANCE_PC:
      address += DecodeUnsignedLeb128(&dbgstream);
      break;

    case DexFile::DBG_ADVANCE_LINE:
      line += DecodeSignedLeb128(&dbgstream);
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
      address += adjopcode / DexFile::DBG_LINE_RANGE;
      line += DexFile::DBG_LINE_BASE + (adjopcode % DexFile::DBG_LINE_RANGE);

      unsigned offset;
      offset = SortLineNumberInfoByLine::find(pc2dex, address);
      if (offset != SortLineNumberInfoByLine::NOT_FOUND) {
        table.push_back( {offset, line});
      }
      break;
    }
  }
}

// convert dex offsets to dex disassembly line numbers in pc2dex
static void getLineInfoForDex(const DexFile::CodeItem* code_item, LineInfoTable& pc2dex) {
  SortLineNumberInfoByLine::sort(pc2dex);

  size_t offset = 0; // in code units
  size_t end = code_item->insns_size_in_code_units_;
  int line_no = 1;
  auto li = pc2dex.begin();
  for ( ; offset < end && li != pc2dex.end(); ++line_no) {
    const Instruction* instr = Instruction::At(&code_item->insns_[offset]);
    for ( ; li != pc2dex.end() && li->LineNumber <= offset; li++) {
      // REVIEW: to be strict we have to remove li if li->lineNumber != offset
      li->LineNumber = line_no;
    }
    offset += instr->SizeInCodeUnits();
  }
}

// filtering options initialized once for every new process id
static std::string vtune_package;
static std::string vtune_map;
static bool process_listed;
static bool core_expected; // core classes can come from a forked parent process

static bool IsCoreOat(OatFile &oat_file) {
  return EndsWith(oat_file.GetLocation(), "/system@framework@boot.oat") ||
         EndsWith(oat_file.GetLocation(), "/system/framework/boot.oat");
}

// called once per process
static bool IsRejectedProcess() {
  vtune_package = Runtime::Current()->GetVTunePackage();
  vtune_map = Runtime::Current()->GetVTuneMap();

  if (vtune_package.size() == 0) {
    return true;
  }

  const char *proc_name = get_process_name();
#if defined(HAVE_PRCTL)
  char prctlbuf[17];
  if (strcmp(proc_name, "unknown") == 0 &&
      prctl(PR_GET_NAME, (unsigned long) prctlbuf, 0, 0, 0) == 0) {
    prctlbuf[16] = 0;
    proc_name = prctlbuf;
  }
#endif

  process_listed = vtune_package.find(std::string(":") + proc_name) != std::string::npos;
  core_expected = vtune_package.compare(0, 5, "core:") == 0;

  bool rejected = !process_listed && !core_expected;

  LOG(INFO) << "VTUNE: package=" << vtune_package
            << "; proc=" << proc_name
            << (rejected ? "; rejected" : "");

  return rejected;
}

void SendOatFileToVTune(OatFile &oat_file) {
  static bool rejected_process = true;
  static pid_t init_pid = 0;

  pid_t current_pid = getpid();
  if (current_pid != init_pid) {
    rejected_process = IsRejectedProcess();
    init_pid = current_pid;
  }

  if (rejected_process || (!process_listed && (!core_expected || !IsCoreOat(oat_file)))) {
    LOG(INFO) << "VTUNE: package=" << vtune_package
              << "; oat=" << oat_file.GetLocation()
              << "; rejected";
    return;
  }

  // accepted
  DCHECK(process_listed || core_expected);

  iJIT_Method_Load jit_method;
  memset(&jit_method, 0, sizeof(jit_method));

  // dump dex files
  std::vector<const OatFile::OatDexFile*> dex_files = oat_file.GetOatDexFiles();
  for(auto it = dex_files.begin(); it != dex_files.end(); ++it) {
    const OatFile::OatDexFile* oat_dex_file = *it;
    jit_method.class_file_name = const_cast<char *>(oat_dex_file->GetDexFileLocation().c_str());

    std::string error_msg;
    std::unique_ptr<const DexFile> dex_file(oat_dex_file->OpenDexFile(&error_msg));
    if (dex_file.get() == nullptr) {
      return;
    }

    // dump classes
    for (size_t class_def_index = 0; class_def_index < dex_file->NumClassDefs(); class_def_index++) {
      const DexFile::ClassDef& class_def = dex_file->GetClassDef(class_def_index);
      const OatFile::OatClass oat_class = oat_dex_file->GetOatClass(class_def_index);
      jit_method.source_file_name = const_cast<char *>(dex_file->GetSourceFile(class_def));

      const byte* class_data = dex_file->GetClassData(class_def);
      if (class_data == NULL) {  // empty class such as a marker interface?
        return;
      }
      ClassDataItemIterator it(*dex_file, class_data);

      // SkipAllFields
      while (it.HasNextStaticField() || it.HasNextInstanceField()) {
        it.Next();
      }

      // dump methods
      for (uint32_t class_method_idx = 0;
           it.HasNextDirectMethod() || it.HasNextVirtualMethod();
           class_method_idx++, it.Next()) {
        const OatFile::OatMethod oat_method = oat_class.GetOatMethod(class_method_idx);
        const DexFile::CodeItem* code_item = it.GetMethodCodeItem();

        const void* code = oat_method.GetQuickCode();
        uint32_t code_size = oat_method.GetQuickCodeSize();
        if (code == nullptr) {
          continue; // portable is currently not supported.
        }

        static unsigned next_method_id = 1000;
        next_method_id++;

        std::string method_name = PrettyMethod(it.GetMemberIndex(), *dex_file, true);
        const char *method_name_c = strchr(method_name.c_str(), ' '); // skip return type
        jit_method.method_name = const_cast<char*>(method_name_c == nullptr ? method_name.c_str() : (method_name_c + 1));
        jit_method.method_id = next_method_id;
        jit_method.method_load_address = const_cast<void*>(code);
        jit_method.method_size = code_size;

        // dump positions
        MappingTable table(oat_method.GetMappingTable());

        // prepare pc2src to point to either pc2java or pc2dex
        LineInfoTable *pc2src = nullptr;
        LineInfoTable pc2dex;
        LineInfoTable pc2java;

        if (vtune_map != "none" && table.TotalSize() != 0 && table.PcToDexSize() != 0) {
          for (MappingTable::PcToDexIterator cur = table.PcToDexBegin(), end = table.PcToDexEnd(); cur != end; ++cur) {
            pc2dex.push_back( {cur.NativePcOffset(), cur.DexPc()});
          }

          if (vtune_map == "dex") {
            pc2src = &pc2dex;
            getLineInfoForDex(code_item, pc2dex);
            // TODO: set method specific dexdump file name for this method
          } else { // default is pc -> java
            pc2src = &pc2java;
            const byte* dbgstream = dex_file->GetDebugInfoStream(it.GetMethodCodeItem());
            getLineInfoForJava(dbgstream, pc2java, pc2dex);
          }
        }

        if (pc2src == nullptr || pc2src->size() == 0) {
          jit_method.line_number_size = 0;
          jit_method.line_number_table = nullptr;
        } else {
          // shift offsets
          SortLineNumberInfoByOffset::sort(*pc2src);
          for (unsigned i = 1; i < pc2src->size(); ++i) {
            (*pc2src)[i - 1].Offset = (*pc2src)[i].Offset;
          }
          (*pc2src)[pc2src->size() - 1].Offset = code_size;

          jit_method.line_number_size = pc2src->size();
          jit_method.line_number_table = pc2src->size() == 0 ? nullptr : &((*pc2src)[0]);
        }

        int is_notified = iJIT_NotifyEvent(iJVM_EVENT_TYPE_METHOD_LOAD_FINISHED, (void*)&jit_method);
        if (is_notified) {
          LOG(DEBUG) << "VTUNE: method '" << jit_method.method_name
                     << "' is written successfully: id=" << jit_method.method_id
                     << ", address=" << jit_method.method_load_address
                     << ", size=" << jit_method.method_size;
        } else {
          LOG(WARNING) << "VTUNE: failed to write method '" << jit_method.method_name
                       << "': id=" << jit_method.method_id
                       << ", address=" << jit_method.method_load_address
                       << ", size=" << jit_method.method_size;
        }
      }
    }
  }
}

}  // namespace art
