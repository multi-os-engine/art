/*
 * Copyright (C) 2013 The Android Open Source Project
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

#include "mir_graph.h"

namespace art {

static const char* kDumpCfgDirectory = "/sdard/";
static const bool kUseC1visualizerFormat = false;
// Filters what DumpC1visualizerCFG emits. The filter is on the method name.
static const char* kStringFilter = "";

const char* MIRGraph::extended_mir_op_names_[kMirOpLast - kMirOpFirst] = {
  "Phi",
  "Copy",
  "FusedCmplFloat",
  "FusedCmpgFloat",
  "FusedCmplDouble",
  "FusedCmpgDouble",
  "FusedCmpLong",
  "Nop",
  "OpNullCheck",
  "OpRangeCheck",
  "OpDivZeroCheck",
  "Check1",
  "Check2",
  "Select",
};

char* MIRGraph::GetDalvikDisassembly(const MIR* mir) {
  DecodedInstruction insn = mir->dalvikInsn;
  std::string str;
  int flags = 0;
  int opcode = insn.opcode;
  char* ret;
  bool nop = false;
  SSARepresentation* ssa_rep = mir->ssa_rep;
  Instruction::Format dalvik_format = Instruction::k10x;  // Default to no-operand format
  int defs = (ssa_rep != NULL) ? ssa_rep->num_defs : 0;
  int uses = (ssa_rep != NULL) ? ssa_rep->num_uses : 0;

  // Handle special cases.
  if ((opcode == kMirOpCheck) || (opcode == kMirOpCheckPart2)) {
    str.append(extended_mir_op_names_[opcode - kMirOpFirst]);
    str.append(": ");
    // Recover the original Dex instruction
    insn = mir->meta.throw_insn->dalvikInsn;
    ssa_rep = mir->meta.throw_insn->ssa_rep;
    defs = ssa_rep->num_defs;
    uses = ssa_rep->num_uses;
    opcode = insn.opcode;
  } else if (opcode == kMirOpNop) {
    str.append("[");
    // Recover original opcode.
    insn.opcode = Instruction::At(current_code_item_->insns_ + mir->offset)->Opcode();
    opcode = insn.opcode;
    nop = true;
  }

  if (opcode >= kMirOpFirst) {
    str.append(extended_mir_op_names_[opcode - kMirOpFirst]);
  } else {
    dalvik_format = Instruction::FormatOf(insn.opcode);
    flags = Instruction::FlagsOf(insn.opcode);
    str.append(Instruction::Name(insn.opcode));
  }

  if (opcode == kMirOpPhi) {
    BasicBlockId* incoming = mir->meta.phi_incoming;
    str.append(StringPrintf(" %s = (%s",
               GetSSANameWithConst(ssa_rep->defs[0], true).c_str(),
               GetSSANameWithConst(ssa_rep->uses[0], true).c_str()));
    str.append(StringPrintf(":%d", incoming[0]));
    int i;
    for (i = 1; i < uses; i++) {
      str.append(StringPrintf(", %s:%d",
                              GetSSANameWithConst(ssa_rep->uses[i], true).c_str(),
                              incoming[i]));
    }
    str.append(")");
  } else if ((flags & Instruction::kBranch) != 0) {
    // For branches, decode the instructions to print out the branch targets.
    int offset = 0;
    switch (dalvik_format) {
      case Instruction::k21t:
        str.append(StringPrintf(" %s,", GetSSANameWithConst(ssa_rep->uses[0], false).c_str()));
        offset = insn.vB;
        break;
      case Instruction::k22t:
        str.append(StringPrintf(" %s, %s,", GetSSANameWithConst(ssa_rep->uses[0], false).c_str(),
                   GetSSANameWithConst(ssa_rep->uses[1], false).c_str()));
        offset = insn.vC;
        break;
      case Instruction::k10t:
      case Instruction::k20t:
      case Instruction::k30t:
        offset = insn.vA;
        break;
      default:
        LOG(FATAL) << "Unexpected branch format " << dalvik_format << " from " << insn.opcode;
    }
    str.append(StringPrintf(" 0x%x (%c%x)", mir->offset + offset,
                            offset > 0 ? '+' : '-', offset > 0 ? offset : -offset));
  } else {
    // For invokes-style formats, treat wide regs as a pair of singles
    bool show_singles = ((dalvik_format == Instruction::k35c) ||
                         (dalvik_format == Instruction::k3rc));
    if (defs != 0) {
      str.append(StringPrintf(" %s", GetSSANameWithConst(ssa_rep->defs[0], false).c_str()));
      if (uses != 0) {
        str.append(", ");
      }
    }
    for (int i = 0; i < uses; i++) {
      str.append(
          StringPrintf(" %s", GetSSANameWithConst(ssa_rep->uses[i], show_singles).c_str()));
      if (!show_singles && (reg_location_ != NULL) && reg_location_[i].wide) {
        // For the listing, skip the high sreg.
        i++;
      }
      if (i != (uses -1)) {
        str.append(",");
      }
    }
    switch (dalvik_format) {
      case Instruction::k11n:  // Add one immediate from vB
      case Instruction::k21s:
      case Instruction::k31i:
      case Instruction::k21h:
        str.append(StringPrintf(", #%d", insn.vB));
        break;
      case Instruction::k51l:  // Add one wide immediate
        str.append(StringPrintf(", #%" PRId64, insn.vB_wide));
        break;
      case Instruction::k21c:  // One register, one string/type/method index
      case Instruction::k31c:
        str.append(StringPrintf(", index #%d", insn.vB));
        break;
      case Instruction::k22c:  // Two registers, one string/type/method index
        str.append(StringPrintf(", index #%d", insn.vC));
        break;
      case Instruction::k22s:  // Add one immediate from vC
      case Instruction::k22b:
        str.append(StringPrintf(", #%d", insn.vC));
        break;
      default: {
        // Nothing left to print
      }
    }
  }
  if (nop) {
    str.append("]--optimized away");
  }
  int length = str.length() + 1;
  ret = static_cast<char*>(arena_->Alloc(length, ArenaAllocator::kAllocDFInfo));
  strncpy(ret, str.c_str(), length);
  return ret;
}

/* Dump the CFG into a DOT graph */
void MIRGraph::DumpCFG(const char* pass_name, bool all_blocks, const char *suffix) {
  if (kUseC1visualizerFormat) {
    return DumpC1visualizerCFG(pass_name, all_blocks);
  }
  FILE* file;
  std::string fname(PrettyMethod(cu_->method_idx, *cu_->dex_file));
  ReplaceSpecialChars(fname);
  fname = StringPrintf("%s/%s/%s%x%s.dot", kDumpCfgDirectory, pass_name, fname.c_str(),
                      GetBasicBlock(GetEntryBlock()->fall_through)->start_offset,
                      suffix == nullptr ? "" : suffix);
  file = fopen(fname.c_str(), "w");
  if (file == NULL) {
    return;
  }
  fprintf(file, "digraph G {\n");

  fprintf(file, "  rankdir=TB\n");

  int num_blocks = all_blocks ? GetNumBlocks() : num_reachable_blocks_;
  int idx;

  for (idx = 0; idx < num_blocks; idx++) {
    int block_idx = all_blocks ? idx : dfs_order_->Get(idx);
    BasicBlock *bb = GetBasicBlock(block_idx);
    if (bb == NULL) break;
    if (bb->block_type == kDead) continue;
    if (bb->block_type == kEntryBlock) {
      fprintf(file, "  entry_%d [shape=Mdiamond];\n", bb->id);
    } else if (bb->block_type == kExitBlock) {
      fprintf(file, "  exit_%d [shape=Mdiamond];\n", bb->id);
    } else if (bb->block_type == kDalvikByteCode) {
      fprintf(file, "  block%04x_%d [shape=record,label = \"{ \\\n",
              bb->start_offset, bb->id);
      const MIR *mir;
        fprintf(file, "    {block id %d\\l}%s\\\n", bb->id,
                bb->first_mir_insn ? " | " : " ");
        for (mir = bb->first_mir_insn; mir; mir = mir->next) {
            int opcode = mir->dalvikInsn.opcode;
            fprintf(file, "    {%04x %s %s %s\\l}%s\\\n", mir->offset,
                    mir->ssa_rep ? GetDalvikDisassembly(mir) :
                    (opcode < kMirOpFirst) ?  Instruction::Name(mir->dalvikInsn.opcode) :
                    extended_mir_op_names_[opcode - kMirOpFirst],
                    (mir->optimization_flags & MIR_IGNORE_RANGE_CHECK) != 0 ? " no_rangecheck" : " ",
                    (mir->optimization_flags & MIR_IGNORE_NULL_CHECK) != 0 ? " no_nullcheck" : " ",
                    mir->next ? " | " : " ");
        }
        fprintf(file, "  }\"];\n\n");
    } else if (bb->block_type == kExceptionHandling) {
      char block_name[BLOCK_NAME_LEN];

      GetBlockName(bb, block_name);
      fprintf(file, "  %s [shape=invhouse];\n", block_name);
    }

    char block_name1[BLOCK_NAME_LEN], block_name2[BLOCK_NAME_LEN];

    if (bb->taken != NullBasicBlockId) {
      GetBlockName(bb, block_name1);
      GetBlockName(GetBasicBlock(bb->taken), block_name2);
      fprintf(file, "  %s:s -> %s:n [style=dotted]\n",
              block_name1, block_name2);
    }
    if (bb->fall_through != NullBasicBlockId) {
      GetBlockName(bb, block_name1);
      GetBlockName(GetBasicBlock(bb->fall_through), block_name2);
      fprintf(file, "  %s:s -> %s:n\n", block_name1, block_name2);
    }

    if (bb->successor_block_list_type != kNotUsed) {
      fprintf(file, "  succ%04x_%d [shape=%s,label = \"{ \\\n",
              bb->start_offset, bb->id,
              (bb->successor_block_list_type == kCatch) ?  "Mrecord" : "record");
      GrowableArray<SuccessorBlockInfo*>::Iterator iterator(bb->successor_blocks);
      SuccessorBlockInfo *successor_block_info = iterator.Next();

      int succ_id = 0;
      while (true) {
        if (successor_block_info == NULL) break;

        BasicBlock *dest_block = GetBasicBlock(successor_block_info->block);
        SuccessorBlockInfo *next_successor_block_info = iterator.Next();

        fprintf(file, "    {<f%d> %04x: %04x\\l}%s\\\n",
                succ_id++,
                successor_block_info->key,
                dest_block->start_offset,
                (next_successor_block_info != NULL) ? " | " : " ");

        successor_block_info = next_successor_block_info;
      }
      fprintf(file, "  }\"];\n\n");

      GetBlockName(bb, block_name1);
      fprintf(file, "  %s:s -> succ%04x_%d:n [style=dashed]\n",
              block_name1, bb->start_offset, bb->id);

      if (bb->successor_block_list_type == kPackedSwitch ||
          bb->successor_block_list_type == kSparseSwitch) {
        GrowableArray<SuccessorBlockInfo*>::Iterator iter(bb->successor_blocks);

        succ_id = 0;
        while (true) {
          SuccessorBlockInfo *successor_block_info = iter.Next();
          if (successor_block_info == NULL) break;

          BasicBlock* dest_block = GetBasicBlock(successor_block_info->block);

          GetBlockName(dest_block, block_name2);
          fprintf(file, "  succ%04x_%d:f%d:e -> %s:n\n", bb->start_offset,
                  bb->id, succ_id++, block_name2);
        }
      }
    }
    fprintf(file, "\n");

    if (cu_->verbose) {
      /* Display the dominator tree */
      GetBlockName(bb, block_name1);
      fprintf(file, "  cfg%s [label=\"%s\", shape=none];\n",
              block_name1, block_name1);
      if (bb->i_dom) {
        GetBlockName(GetBasicBlock(bb->i_dom), block_name2);
        fprintf(file, "  cfg%s:s -> cfg%s:n\n\n", block_name2, block_name1);
      }
    }
  }
  fprintf(file, "}\n");
  fclose(file);
}

class Tracer {
 public:
  Tracer(MIRGraph* graph, FILE* file) : indent_(0), graph_(graph), c1_visualizer_file_(file) {}
  void StartTag(const char* name);
  void EndTag(const char* name);
  void PrintProperty(const char* name, const char* format, ...);
  void PrintInt(const char* name, int value);
  void PrintEmptyProperty(const char* name);
  void PrintTime(const char* name);
  void VisitAllBlocks();
  void PrintSuccessors(BasicBlock* bb);
  void PrintPredecessors(BasicBlock* bb);
  void AddIndent();

 private:
  int indent_;
  MIRGraph* graph_;
  FILE* c1_visualizer_file_;
};

void Tracer::StartTag(const char* name) {
  AddIndent();
  fprintf(c1_visualizer_file_, "begin_%s\n", name);
  indent_++;
}

void Tracer::EndTag(const char* name) {
  indent_--;
  AddIndent();
  fprintf(c1_visualizer_file_, "end_%s\n", name);
}

void Tracer::PrintProperty(const char* name, const char* format, ...) {
  AddIndent();
  va_list ap;
  va_start(ap, format);
  fprintf(c1_visualizer_file_, "%s \"", name);
  vfprintf(c1_visualizer_file_, format, ap);
  fprintf(c1_visualizer_file_, "\"\n");
  va_end(ap);
}

void Tracer::PrintEmptyProperty(const char* name) {
  AddIndent();
  fprintf(c1_visualizer_file_, "%s\n", name);
}

void Tracer::PrintTime(const char* name) {
  AddIndent();
  fprintf(c1_visualizer_file_, "%s %lld\n", name, (long long) time(NULL));
}

void Tracer::PrintInt(const char* name, int value) {
  AddIndent();
  fprintf(c1_visualizer_file_, "%s %d\n", name, value);
}   

void Tracer::AddIndent() {
  for (int i = 0; i < indent_; i++) {
    fprintf(c1_visualizer_file_, "  ");
  }
}

void Tracer::PrintPredecessors(BasicBlock* bb) {
  AddIndent();
  fprintf(c1_visualizer_file_, "predecessors");
  GrowableArray<BasicBlockId>::Iterator iter(bb->predecessors);
  BasicBlock* pred_bb;
  while ((pred_bb = graph_->GetBasicBlock(iter.Next())) != NULL) {
    fprintf(c1_visualizer_file_, " \"B%d\" ", pred_bb->id);
  }
  fprintf(c1_visualizer_file_, "\n");
}

void Tracer::PrintSuccessors(BasicBlock* bb) {
  AddIndent();
  fprintf(c1_visualizer_file_, "successors");
  if (bb->fall_through != NullBasicBlockId) {
    fprintf(c1_visualizer_file_, " \"B%d\"", graph_->GetBasicBlock(bb->fall_through)->id);
  }
  if (bb->taken != NullBasicBlockId) {
    fprintf(c1_visualizer_file_, " \"B%d\"", graph_->GetBasicBlock(bb->taken)->id);
  }
  if (bb->successor_blocks != NULL) {
    GrowableArray<SuccessorBlockInfo*>::Iterator iter(bb->successor_blocks);
    SuccessorBlockInfo* info = iter.Next();
    while ((info = iter.Next()) != NULL) {
      fprintf(c1_visualizer_file_, " \"B%d\" ", graph_->GetBasicBlock(info->block)->id);
    }
  }
  fprintf(c1_visualizer_file_, "\n");
}


static const char* GetName(const MIR* mir) {
  int opcode = mir->dalvikInsn.opcode;
  return (opcode < kMirOpFirst)
      ? Instruction::Name(mir->dalvikInsn.opcode)
      : MIRGraph::extended_mir_op_names_[opcode - kMirOpFirst];
}

/* Dump the CFG into a C1visualizer graph */
void MIRGraph::DumpC1visualizerCFG(const char* pass_name, bool all_blocks) {
  std::string pretty_name = PrettyMethod(cu_->method_idx, *cu_->dex_file);
  if (pretty_name.find(kStringFilter) == std::string::npos) {
    return;
  }

  static FILE* c1VisualizerFile = NULL;
  if (c1VisualizerFile == NULL) {
    if (cu_->compiler_driver->GetThreadCount() != 1) {
      LOG(ERROR) << "Tracer currently only works single threaded.";
      return;
    }
    std::string fname = StringPrintf("%s/dex.cfg", kDumpCfgDirectory);
    c1VisualizerFile = fopen(fname.c_str(), "w+");
  }

  Tracer tracer(this, c1VisualizerFile);
  if (pass_name[0] == '1') {
    tracer.StartTag("compilation");
    tracer.PrintProperty("name", pretty_name.c_str());
    tracer.PrintProperty("method", pretty_name.c_str());
    tracer.PrintTime("date");
    tracer.EndTag("compilation");
  }

  tracer.StartTag("cfg");
  tracer.PrintProperty("name", pass_name);

  bool use_dfs = dfs_order_ != NULL;
  int num_blocks = use_dfs ? num_reachable_blocks_ : GetNumBlocks();
  // Unique instruction id for instructions that are not used.
  int unused_instruction = 0;
  for (int idx = 0; idx < num_blocks; idx++) {
    int block_idx = use_dfs ? dfs_order_->Get(idx) : idx;
    BasicBlock *bb = GetBasicBlock(block_idx);
    if (bb == NULL) continue;
    if (bb->block_type == kDead) continue;

    tracer.StartTag("block");
    tracer.PrintProperty("name", "B%d", bb->id);
    tracer.PrintInt("from_bci", -1);
    tracer.PrintInt("to_bci", -1);
    tracer.PrintPredecessors(bb);
    tracer.PrintSuccessors(bb);
    tracer.PrintEmptyProperty("xhandlers");
    tracer.PrintEmptyProperty("flags");
    if (bb->i_dom != NullBasicBlockId) {
      tracer.PrintProperty("dominator", "B%d", bb->i_dom);
    }

    tracer.StartTag("states");
    tracer.StartTag("locals");
    tracer.PrintInt("size", 0);
    tracer.PrintProperty("method", "None");
    tracer.EndTag("locals");
    tracer.EndTag("states");

    tracer.StartTag("HIR");
    const char* kEndInstructionMarker = "<|@";
    if (bb->block_type == kEntryBlock) {
      int num_regs = cu_->num_dalvik_registers;
      int num_ins = cu_->num_ins;
      if (num_ins > 0) {
        int s_reg = num_regs - num_ins;
        if ((cu_->access_flags & kAccStatic) == 0) {
          tracer.AddIndent();
          fprintf(c1VisualizerFile, "0 %d v%d this %s\n",
                  GetRawUseCount(s_reg), s_reg, kEndInstructionMarker);
          s_reg++;
        }
        const char* shorty = cu_->shorty;
        int shorty_len = strlen(shorty);
        for (int i = 1; i < shorty_len; i++) {
          tracer.AddIndent();
          fprintf(c1VisualizerFile, "0 %d v%d param%i %s\n",
                  GetRawUseCount(s_reg), s_reg, i, kEndInstructionMarker);
          s_reg++;
        }
      }
      tracer.AddIndent();
      fprintf(c1VisualizerFile, "0 0 u%d goto B%d %s\n",
              unused_instruction++, GetBasicBlock(bb->fall_through)->id, kEndInstructionMarker);
    } else if (bb->block_type == kExitBlock) {
      tracer.AddIndent();
      fprintf(c1VisualizerFile, "0 0 u%d exit %s\n", unused_instruction++, kEndInstructionMarker);
    } else if (bb->block_type == kDalvikByteCode) {
      for (const MIR* mir = bb->first_mir_insn; mir; mir = mir->next) {
        tracer.AddIndent();
        if (mir->ssa_rep != NULL) {
          if (mir->ssa_rep->num_defs > 0) {
            int def_name = mir->ssa_rep->defs[0];
            int raw_use_count = GetRawUseCount(mir->ssa_rep->defs[0]);
            fprintf(c1VisualizerFile, "0 %d v%d %s", raw_use_count, def_name, GetName(mir));
          } else {
            fprintf(c1VisualizerFile, "0 0 u%d %s", unused_instruction++, GetName(mir));
          }
          int uses = mir->ssa_rep->num_uses;
          for (int i = 0; i < uses; i++) {
            fprintf(c1VisualizerFile, " v%d", mir->ssa_rep->uses[i]);
          }
        } else {
          fprintf(c1VisualizerFile, "0 0 %d %s", unused_instruction++, GetName(mir));
        }
        fprintf(c1VisualizerFile, " %s\n", kEndInstructionMarker);
      }
    } else if (bb->block_type == kExceptionHandling) {
      tracer.AddIndent();
      fprintf(c1VisualizerFile, "0 0 exception handling block %s\n", kEndInstructionMarker);
    }
    tracer.EndTag("HIR");

    tracer.EndTag("block");
  }

  tracer.EndTag("cfg");
}

} // namespace art
