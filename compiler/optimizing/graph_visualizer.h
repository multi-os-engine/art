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
 * The parent class for utility classes which output the HGraph. The default implementation does
 * nothing.
 * Currently only works if the compiler is single threaded.
 */
class HGraphVisualizer {
 public:
  HGraphVisualizer(HGraph* graph, const DexCompilationUnit& cu);
  virtual ~HGraphVisualizer() { }
  virtual void DumpGraph(const char* name, const char* attr = nullptr) const;

 protected:
  HGraph* const graph_;
  std::string method_name_;

 private:
  DISALLOW_COPY_AND_ASSIGN(HGraphVisualizer);
};

/**
 * Implementation of HGraphVisualizer which outputs the graph in the C1visualizer format.
 * The 'method_filter' argument can be used to dump only methods whose name contains the given
 * string.
 */
class HGraphC1Visualizer : public HGraphVisualizer {
 public:
  HGraphC1Visualizer(HGraph* graph,
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

/**
 * Implementation of HGraphVisualizer which dumps the graph using the HPrettyPrinter class. It is
 * intended to be used for testing purposes as it allows for easy regex matching of the actual
 * graph against the expected outcome. The graph is dumped onto LOG(INFO).
 */
class HGraphTestVisualizer : public HGraphVisualizer {
 public:
  HGraphTestVisualizer(HGraph* graph, const DexCompilationUnit& cu);
  virtual ~HGraphTestVisualizer();
  virtual void DumpGraph(const char* pass_name, const char* pass_attr) const;
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_GRAPH_VISUALIZER_H_
