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

#include <map>
#include <ostream>

#include "base/value_object.h"
#include "arch/instruction_set.h"

namespace art {

class CodeGenerator;
class DexCompilationUnit;
class HGraph;
class HInstruction;

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
  void DumpGraphWithDisassembly() const;

  const CodeGenerator& GetCodegen() const { return codegen_; }

  struct GeneratedCodeInterval {
    size_t start_;
    size_t end_;
  };

  const std::map<const HInstruction*, GeneratedCodeInterval>& InstructionCodeOffsets() const {
    return instruction_code_offsets_;
  }

  void AddInstructionCodeOffsets(HInstruction* instr, size_t start, size_t end) {
    instruction_code_offsets_[instr] = {start, end};
  }

 private:
  std::ostream* const output_;
  HGraph* const graph_;
  const CodeGenerator& codegen_;

  // The offsets to the start and end of the code generated for instructions is
  // recorded here. The graph visualizer printer will use that information to
  // print the disassembly of the code generated.
  std::map<const HInstruction*, GeneratedCodeInterval> instruction_code_offsets_;

  DISALLOW_COPY_AND_ASSIGN(HGraphVisualizer);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_GRAPH_VISUALIZER_H_
