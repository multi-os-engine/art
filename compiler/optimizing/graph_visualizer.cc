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

#include "code_generator.h"
#include "driver/dex_compilation_unit.h"
#include "graph_visualizer.h"
#include "nodes.h"
#include "pretty_printer.h"
#include "ssa_liveness_analysis.h"

namespace art {

/**
 * HGraph visitor to generate a file suitable for the c1visualizer tool and IRHydra.
 */
class HGraphC1visualizerPrinter : public HGraphVisitor {
 public:
  HGraphC1visualizerPrinter(HGraph* graph,
                            std::ostream& output,
                            const char* pass_name,
                            const CodeGenerator& codegen)
      : HGraphVisitor(graph),
        output_(output),
        pass_name_(pass_name),
        codegen_(codegen),
        indent_(0) {}

  void StartTag(const char* name) {
    AddIndent();
    output_ << "begin_" << name << std::endl;
    indent_++;
  }

  void EndTag(const char* name) {
    indent_--;
    AddIndent();
    output_ << "end_" << name << std::endl;
  }

  void PrintProperty(const char* name, const char* property) {
    AddIndent();
    output_ << name << " \"" << property << "\"" << std::endl;
  }

  void PrintProperty(const char* name, const char* property, int id) {
    AddIndent();
    output_ << name << " \"" << property << id << "\"" << std::endl;
  }

  void PrintEmptyProperty(const char* name) {
    AddIndent();
    output_ << name << std::endl;
  }

  void PrintTime(const char* name) {
    AddIndent();
    output_ << name << " " << time(NULL) << std::endl;
  }

  void PrintInt(const char* name, int value) {
    AddIndent();
    output_ << name << " " << value << std::endl;
  }

  void AddIndent() {
    for (size_t i = 0; i < indent_; ++i) {
      output_ << "  ";
    }
  }

  char GetTypeId(Primitive::Type type) {
    // Note that Primitive::Descriptor would not work for us
    // because it does not handle reference types (that is kPrimNot).
    switch (type) {
      case Primitive::kPrimBoolean: return 'z';
      case Primitive::kPrimByte: return 'b';
      case Primitive::kPrimChar: return 'c';
      case Primitive::kPrimShort: return 's';
      case Primitive::kPrimInt: return 'i';
      case Primitive::kPrimLong: return 'j';
      case Primitive::kPrimFloat: return 'f';
      case Primitive::kPrimDouble: return 'd';
      case Primitive::kPrimNot: return 'l';
      case Primitive::kPrimVoid: return 'v';
    }
    LOG(FATAL) << "Unreachable";
    return 'v';
  }

  void PrintPredecessors(HBasicBlock* block) {
    AddIndent();
    output_ << "predecessors";
    for (size_t i = 0, e = block->GetPredecessors().Size(); i < e; ++i) {
      HBasicBlock* predecessor = block->GetPredecessors().Get(i);
      output_ << " \"B" << predecessor->GetBlockId() << "\" ";
    }
    output_<< std::endl;
  }

  void PrintSuccessors(HBasicBlock* block) {
    AddIndent();
    output_ << "successors";
    for (size_t i = 0, e = block->GetSuccessors().Size(); i < e; ++i) {
      HBasicBlock* successor = block->GetSuccessors().Get(i);
      output_ << " \"B" << successor->GetBlockId() << "\" ";
    }
    output_<< std::endl;
  }

  void DumpLocation(Location location) {
    if (location.IsRegister()) {
      codegen_.DumpCoreRegister(output_, location.reg());
    } else if (location.IsFpuRegister()) {
      codegen_.DumpFloatingPointRegister(output_, location.reg());
    } else if (location.IsConstant()) {
      output_ << "constant";
      HConstant* constant = location.GetConstant();
      if (constant->IsIntConstant()) {
        output_ << " " << constant->AsIntConstant()->GetValue();
      } else if (constant->IsLongConstant()) {
        output_ << " " << constant->AsLongConstant()->GetValue();
      }
    } else if (location.IsInvalid()) {
      output_ << "invalid";
    } else if (location.IsStackSlot()) {
      output_ << location.GetStackIndex() << "(sp)";
    } else {
      DCHECK(location.IsDoubleStackSlot());
      output_ << "2x" << location.GetStackIndex() << "(sp)";
    }
  }

  void VisitParallelMove(HParallelMove* instruction) {
    output_ << instruction->DebugName();
    output_ << " (";
    for (size_t i = 0, e = instruction->NumMoves(); i < e; ++i) {
      MoveOperands* move = instruction->MoveOperandsAt(i);
      DumpLocation(move->GetSource());
      output_ << " -> ";
      DumpLocation(move->GetDestination());
      if (i + 1 != e) {
        output_ << ", ";
      }
    }
    output_ << ")";
    output_ << " (liveness: " << instruction->GetLifetimePosition() << ")";
  }

  void VisitInstruction(HInstruction* instruction) {
    output_ << instruction->DebugName();
    if (instruction->InputCount() > 0) {
      output_ << " [ ";
      for (HInputIterator inputs(instruction); !inputs.Done(); inputs.Advance()) {
        output_ << GetTypeId(inputs.Current()->GetType()) << inputs.Current()->GetId() << " ";
      }
      output_ << "]";
    }
    if (pass_name_ == kLivenessPassName && instruction->GetLifetimePosition() != kNoLifetime) {
      output_ << " (liveness: " << instruction->GetLifetimePosition();
      if (instruction->HasLiveInterval()) {
        output_ << " ";
        const LiveInterval& interval = *instruction->GetLiveInterval();
        interval.Dump(output_);
      }
      output_ << ")";
    } else if (pass_name_ == kRegisterAllocatorPassName) {
      LocationSummary* locations = instruction->GetLocations();
      if (locations != nullptr) {
        output_ << " ( ";
        for (size_t i = 0; i < instruction->InputCount(); ++i) {
          DumpLocation(locations->InAt(i));
          output_ << " ";
        }
        output_ << ")";
        if (locations->Out().IsValid()) {
          output_ << " -> ";
          DumpLocation(locations->Out());
        }
      }
      output_ << " (liveness: " << instruction->GetLifetimePosition() << ")";
    }
  }

  void PrintInstructions(const HInstructionList& list) {
    const char* kEndInstructionMarker = "<|@";
    for (HInstructionIterator it(list); !it.Done(); it.Advance()) {
      HInstruction* instruction = it.Current();
      AddIndent();
      int bci = 0;
      output_ << bci << " " << instruction->NumberOfUses()
              << " " << GetTypeId(instruction->GetType()) << instruction->GetId() << " ";
      instruction->Accept(this);
      output_ << kEndInstructionMarker << std::endl;
    }
  }

  void Run() {
    StartTag("cfg");
    PrintProperty("name", pass_name_);
    VisitInsertionOrder();
    EndTag("cfg");
  }

  void VisitBasicBlock(HBasicBlock* block) {
    StartTag("block");
    PrintProperty("name", "B", block->GetBlockId());
    if (block->GetLifetimeStart() != kNoLifetime) {
      // Piggy back on these fields to show the lifetime of the block.
      PrintInt("from_bci", block->GetLifetimeStart());
      PrintInt("to_bci", block->GetLifetimeEnd());
    } else {
      PrintInt("from_bci", -1);
      PrintInt("to_bci", -1);
    }
    PrintPredecessors(block);
    PrintSuccessors(block);
    PrintEmptyProperty("xhandlers");
    PrintEmptyProperty("flags");
    if (block->GetDominator() != nullptr) {
      PrintProperty("dominator", "B", block->GetDominator()->GetBlockId());
    }

    StartTag("states");
    StartTag("locals");
    PrintInt("size", 0);
    PrintProperty("method", "None");
    for (HInstructionIterator it(block->GetPhis()); !it.Done(); it.Advance()) {
      AddIndent();
      HInstruction* instruction = it.Current();
      output_ << instruction->GetId() << " " << GetTypeId(instruction->GetType())
              << instruction->GetId() << "[ ";
      for (HInputIterator inputs(instruction); !inputs.Done(); inputs.Advance()) {
        output_ << inputs.Current()->GetId() << " ";
      }
      output_ << "]" << std::endl;
    }
    EndTag("locals");
    EndTag("states");

    StartTag("HIR");
    PrintInstructions(block->GetPhis());
    PrintInstructions(block->GetInstructions());
    EndTag("HIR");
    EndTag("block");
  }

 private:
  std::ostream& output_;
  const char* pass_name_;
  const CodeGenerator& codegen_;
  size_t indent_;

  DISALLOW_COPY_AND_ASSIGN(HGraphC1visualizerPrinter);
};

HGraphVisualizer::HGraphVisualizer(HGraph* graph, const DexCompilationUnit& cu)
    : graph_(graph) {
  method_name_ = PrettyMethod(cu.GetDexMethodIndex(), *cu.GetDexFile(), false);
}

void HGraphVisualizer::DumpGraph(const char* pass_name ATTRIBUTE_UNUSED,
                           const char* pass_attr ATTRIBUTE_UNUSED) const {
}

HGraphC1Visualizer::HGraphC1Visualizer(HGraph* graph,
                                               std::ostream& output,
                                               const char* method_filter,
                                               const CodeGenerator& codegen,
                                               const DexCompilationUnit& cu)
    : HGraphVisualizer(graph, cu), output_(output), codegen_(codegen) {
  is_enabled_ = method_name_.find(method_filter) != std::string::npos;
  if (is_enabled_) {
    HGraphC1visualizerPrinter printer(graph_, output_, "", codegen_);
    printer.StartTag("compilation");
    printer.PrintProperty("name", method_name_.c_str());
    printer.PrintProperty("method", method_name_.c_str());
    printer.PrintTime("date");
    printer.EndTag("compilation");
  }
}

void HGraphC1Visualizer::DumpGraph(const char* pass_name,
                                       const char* pass_attr ATTRIBUTE_UNUSED)
                                       const {
  if (is_enabled_) {
    HGraphC1visualizerPrinter printer(graph_, output_, pass_name, codegen_);
    printer.Run();
  }
}

HGraphTestVisualizer::HGraphTestVisualizer(HGraph* graph,
                               const DexCompilationUnit& cu)
    : HGraphVisualizer(graph, cu) {
  LOG(INFO) << "------------------------------------";
  LOG(INFO) << "BEGIN_METHOD " << method_name_;
}

HGraphTestVisualizer::~HGraphTestVisualizer() {
  LOG(INFO) << "END_METHOD " << method_name_;
  LOG(INFO) << "------------------------------------";
  LOG(INFO) << std::endl;
}

void HGraphTestVisualizer::DumpGraph(const char* pass_name,
                               const char* pass_attr) const {
  if (pass_attr)
    LOG(INFO) << "BEGIN_GRAPH_DUMP " << pass_name << " [" << pass_attr << "]";
  else
    LOG(INFO) << "BEGIN_GRAPH_DUMP " << pass_name;

  StringPrettyPrinter printer(graph_);
  printer.VisitInsertionOrder();
  LOG(INFO) << printer.str();

  if (pass_attr)
    LOG(INFO) << "END_GRAPH_DUMP " << pass_name << " [" << pass_attr << "]";
  else
    LOG(INFO) << "END_GRAPH_DUMP " << pass_name;
  LOG(INFO) << std::endl;
}

}  // namespace art
