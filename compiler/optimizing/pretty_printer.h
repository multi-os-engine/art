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

#ifndef ART_COMPILER_OPTIMIZING_PRETTY_PRINTER_H_
#define ART_COMPILER_OPTIMIZING_PRETTY_PRINTER_H_

#include "aart.h"

#include "base/stringprintf.h"
#include "nodes.h"

namespace art {

class HPrettyPrinter : public HGraphVisitor {
 public:
  explicit HPrettyPrinter(HGraph* graph) : HGraphVisitor(graph) { }

  void PrintPreInstruction(HInstruction* instruction) {
    PrintString("  ");
    PrintInt(instruction->GetId());
    PrintString(": ");
  }

  void VisitInstruction(HInstruction* instruction) OVERRIDE {
    PrintPreInstruction(instruction);
    PrintString(instruction->DebugName());
#ifdef AART
    if (instruction->IsInvoke()) {
      HInvoke* invoke = instruction->AsInvoke();
      PrintString(" ");
      PrintString(PrettyMethod(invoke->GetDexMethodIndex(), GetGraph()->GetDexFile(), true).c_str());
      if (invoke->IsIntrinsic()) {
        PrintString(" [INTRIN]");
      }
      PrintString(" ");
    }
#endif
    PrintPostInstruction(instruction);
  }

  void PrintPostInstruction(HInstruction* instruction) {
    if (instruction->InputCount() != 0) {
      PrintString("(");
      bool first = true;
      for (HInputIterator it(instruction); !it.Done(); it.Advance()) {
        if (first) {
          first = false;
        } else {
          PrintString(", ");
        }
        PrintInt(it.Current()->GetId());
      }
      PrintString(")");
    }
    if (instruction->HasUses()) {
      PrintString(" [");
      bool first = true;
      for (HUseIterator<HInstruction*> it(instruction->GetUses()); !it.Done(); it.Advance()) {
        if (first) {
          first = false;
        } else {
          PrintString(", ");
        }
        PrintInt(it.Current()->GetUser()->GetId());
      }
      PrintString("]");
    }
#ifdef AART
    if (instruction->HasEnvironmentUses()) {
      PrintString(" HAS-ENV-USES {");
#ifdef AART2
      for (HUseIterator<HEnvironment*> it(instruction->GetEnvUses()); !it.Done(); it.Advance()) {
        HEnvironment* env = it.Current()->GetUser();
        PrintString("holder=");
        PrintInt(env->GetHolder()->GetId());
        PrintString(", ");
      }
#endif
      PrintString("}");
    }
    if (instruction->NeedsEnvironment()) {
      PrintString(" NEEDS-ENV");
    }
    if (instruction->HasEnvironment()) {
      PrintString(" HAS-ENV [");
#ifdef AART2
      HEnvironment* env = instruction->GetEnvironment();
      PrintString("holder=");
      PrintInt(env->GetHolder()->GetId());
      PrintString("(me) {");
      // These are dex-registers.
      for (size_t i = 0; i < env->Size(); i++) {
        PrintString("v"); PrintInt(i); PrintString("=");
        if (env->GetInstructionAt(i)) {
          PrintInt(env->GetInstructionAt(i)->GetId());
          PrintString(", ");
        } else {
          PrintString("!, ");
        }
      }
      PrintString("}");
#endif
      PrintString("]");
    }
    if (instruction->CanBeMoved()) {
      PrintString(" CAN-MOVE");
    }
    if (instruction->CanThrow()) {
      PrintString(" CAN-THROW");
    }
    PrintString(" ");
    PrintString(instruction->GetSideEffects().ToString().c_str());
    PrintString(" PC=");
    PrintInt(instruction->GetDexPc());
#endif
    PrintNewLine();
  }

  void VisitBasicBlock(HBasicBlock* block) OVERRIDE {
    PrintString("BasicBlock ");
    PrintInt(block->GetBlockId());
    const ArenaVector<HBasicBlock*>& predecessors = block->GetPredecessors();
    if (!predecessors.empty()) {
      PrintString(", pred: ");
      for (size_t i = 0; i < predecessors.size() -1; i++) {
        PrintInt(predecessors[i]->GetBlockId());
        PrintString(", ");
      }
      PrintInt(predecessors.back()->GetBlockId());
    }
    const ArenaVector<HBasicBlock*>& successors = block->GetSuccessors();
    if (!successors.empty()) {
      PrintString(", succ: ");
      for (size_t i = 0; i < successors.size() - 1; i++) {
        PrintInt(successors[i]->GetBlockId());
        PrintString(", ");
      }
      PrintInt(successors.back()->GetBlockId());
    }
#ifdef AART
    if (block->IsTryBlock()) {
      PrintString(" IS-TRY");
    }
    if (block->IsCatchBlock()) {
      PrintString(" IS-CATCH");
    }
    if (block->GetTryCatchInformation()) {
      PrintString(" HAS-TRY/CATCH-INFO");
    }
#endif
    PrintNewLine();
    HGraphVisitor::VisitBasicBlock(block);
  }

  virtual void PrintNewLine() = 0;
  virtual void PrintInt(int value) = 0;
  virtual void PrintString(const char* value) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(HPrettyPrinter);
};

class StringPrettyPrinter : public HPrettyPrinter {
 public:
  explicit StringPrettyPrinter(HGraph* graph)
      : HPrettyPrinter(graph), str_(""), current_block_(nullptr) { }

  void PrintInt(int value) OVERRIDE {
    str_ += StringPrintf("%d", value);
  }

  void PrintString(const char* value) OVERRIDE {
    str_ += value;
  }

  void PrintNewLine() OVERRIDE {
    str_ += '\n';
  }

  void Clear() { str_.clear(); }

  std::string str() const { return str_; }

  void VisitBasicBlock(HBasicBlock* block) OVERRIDE {
    current_block_ = block;
    HPrettyPrinter::VisitBasicBlock(block);
  }

  void VisitGoto(HGoto* gota) OVERRIDE {
    PrintString("  ");
    PrintInt(gota->GetId());
    PrintString(": Goto ");
    PrintInt(current_block_->GetSuccessors()[0]->GetBlockId());
    PrintNewLine();
  }

 private:
  std::string str_;
  HBasicBlock* current_block_;

  DISALLOW_COPY_AND_ASSIGN(StringPrettyPrinter);
};

}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_PRETTY_PRINTER_H_
