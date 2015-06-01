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

#ifndef ART_COMPILER_OPTIMIZING_GRAPH_VISUALIZER_H_
#define ART_COMPILER_OPTIMIZING_GRAPH_VISUALIZER_H_

#include <ostream>
#include <vector>

#include "arch/instruction_set.h"
#include "base/arena_containers.h"
#include "base/value_object.h"

namespace art {

class CodeGenerator;
class DexCompilationUnit;
class DisassemblyInformation;
class HGraph;
class HInstruction;
class SlowPathCode;

/**
 * This class outputs the HGraph in the C1visualizer format.
 * Note: Currently only works if the compiler is single threaded.
 */
class HGraphVisualizer : public ValueObject {
 public:
  HGraphVisualizer(std::ostream* output,
                   HGraph* graph,
                   const CodeGenerator& codegen);

  void PrintHeader(const char* method_name) const;
  void DumpGraph(const char* pass_name, bool is_after_pass = true) const;
  void DumpGraphWithDisassembly(DisassemblyInformation* disasm_info) const;

  const CodeGenerator& GetCodegen() const { return codegen_; }

 private:
  std::ostream* const output_;
  HGraph* const graph_;
  const CodeGenerator& codegen_;

  DISALLOW_COPY_AND_ASSIGN(HGraphVisualizer);
};

struct GeneratedCodeInterval {
  size_t start;
  size_t end;
};

struct SlowPathCodeInfo {
  const SlowPathCode* slow_path;
  GeneratedCodeInterval code_interval;
};

// This information is filled by the code generator. It will be used by the
// graph visualizer to associate disassembly of the generated code with the
// instructions and slow paths. We assume that the generated code follows the
// following structure:
//   - frame entry
//   - instructions
//   - slow paths
class DisassemblyInformation {
 public:
  explicit DisassemblyInformation(ArenaAllocator* allocator)
      : function_frame_entry_code_info_({0, 0}),
        instruction_code_offsets_(std::less<const HInstruction*>(), allocator->Adapter()),
        slow_paths_(allocator->Adapter()) {}

  void SetEndOfFrameEntry(size_t end_of_frame) {
    function_frame_entry_code_info_ = {0, end_of_frame};
  }

  void AddInstructionCodeOffsets(HInstruction* instr, size_t start, size_t end) {
    instruction_code_offsets_.Put(instr, {start, end});
  }

  void AddSlowPathCodeInfo(SlowPathCode* slow_path, size_t start, size_t end) {
    slow_paths_.push_back({slow_path, {start, end}});
  }

  GeneratedCodeInterval GetFunctionFrameEntryCodeInfo() const {
    return function_frame_entry_code_info_;
  }

  const ArenaSafeMap<const HInstruction*, GeneratedCodeInterval>& InstructionCodeOffsets() const {
    return instruction_code_offsets_;
  }

  const ArenaVector<SlowPathCodeInfo>& GetSlowPaths() const { return slow_paths_; }

 private:
  GeneratedCodeInterval function_frame_entry_code_info_;
  ArenaSafeMap<const HInstruction*, GeneratedCodeInterval> instruction_code_offsets_;
  ArenaVector<SlowPathCodeInfo> slow_paths_;
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_GRAPH_VISUALIZER_H_
