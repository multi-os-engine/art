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
 * If enabled, emits compilation information suitable for the c1visualizer tool
 * and IRHydra.
 * Currently only works if the compiler is single threaded.
 */
class HGraphDump {
 public:
  /**
   * If output is not null, and the method name of the dex compilation
   * unit contains `string_filter`, the compilation information will be
   * emitted.
   */
  HGraphDump(HGraph* graph, const DexCompilationUnit& cu);

  virtual ~HGraphDump() { }

  /**
   * If this visualizer is enabled, emit the compilation information
   * in `output_`.
   */
  virtual void DumpGraph(const char* name, const char* attr = nullptr) const;

 protected:
  HGraph* const graph_;
  std::string method_name_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HGraphDump);
};

class HGraphC1visualizerDump : public HGraphDump {
 public:
  HGraphC1visualizerDump(HGraph* graph,
                         std::ostream& output,
                         const char* method_filter,
                         const CodeGenerator& codegen,
                         const DexCompilationUnit& cu);

  virtual void DumpGraph(const char* pass_name, const char* pass_attr) const;

 private:
  std::ostream& output_;
  const CodeGenerator& codegen_;
  bool is_enabled_;
};

class HGraphTestDump : public HGraphDump {
 public:
  HGraphTestDump(HGraph* graph, const DexCompilationUnit& cu);
  virtual ~HGraphTestDump();

  virtual void DumpGraph(const char* pass_name, const char* pass_attr) const;
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_GRAPH_VISUALIZER_H_
