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

#include "base/value_object.h"

namespace art {

class CodeGenerator;
class DexCompilationUnit;
class HGraph;

// TODO: Create an analysis/optimization abstraction.
static const char* kLivenessPassName = "liveness";
static const char* kRegisterAllocatorPassName = "register";

/**
 * This class outputs the HGraph in the following formats:
 *  - PrettyPrint:
 *      Uses StringPrettyPrinter to dump the HGraph both before and after passes onto LOG(INFO).
 *      This format allows for easy regex matching suitable for writing pass tests.
 *  - C1visualizer:
 *      Dumps HGraph on a specified output stream in a format compatible with C1visualizer.
 *      It only dumps the graph after the individual passes.
 * If the method name contains the contents of the 'method_filter' argument, it will not be dumped
 * by the visualizer.
 *
 * Note: Currently only works if the compiler is single threaded.
 */
class HGraphVisualizer {
 public:
  HGraphVisualizer(HGraph* graph,
                   bool enable_prettyprint,
                   bool enable_c1visualizer,
                   std::ostream* c1_output,
                   const char* method_filter,
                   const CodeGenerator& codegen,
                   const DexCompilationUnit& cu);

  ~HGraphVisualizer();
  void DumpGraph(const char* pass_name, bool is_after_pass = true) const;

 private:
  HGraph* const graph_;
  std::string method_name_;
  const CodeGenerator& codegen_;
  bool is_enabled_prettyprint_;
  bool is_enabled_c1visualizer_;
  std::ostream* c1_output_;

  DISALLOW_COPY_AND_ASSIGN(HGraphVisualizer);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_GRAPH_VISUALIZER_H_
