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

#include "builder.h"

#include "art_field-inl.h"
#include "base/arena_bit_vector.h"
#include "base/bit_vector-inl.h"
#include "base/logging.h"
#include "bytecode_utils.h"
#include "class_linker.h"
#include "dex/verified_method.h"
#include "dex_file-inl.h"
#include "dex_instruction-inl.h"
#include "dex/verified_method.h"
#include "driver/compiler_driver-inl.h"
#include "driver/compiler_options.h"
#include "instruction_builder.h"
#include "mirror/class_loader.h"
#include "mirror/dex_cache.h"
#include "nodes.h"
#include "primitive.h"
#include "scoped_thread_state_change.h"
#include "ssa_builder.h"
#include "thread.h"
#include "utils/dex_cache_arrays_layout-inl.h"

namespace art {

void HGraphBuilder::InitializeLocals(uint16_t count) {
  graph_->SetNumberOfVRegs(count);
  locals_.resize(count);
  HBasicBlock* entry_block = graph_->GetEntryBlock();
  for (int i = 0; i < count; i++) {
    HLocal* local = new (arena_) HLocal(i);
    entry_block->AddInstruction(local);
    locals_[i] = local;
  }
}

void HGraphBuilder::InitializeParameters(uint16_t number_of_parameters) {
  // dex_compilation_unit_ is null only when unit testing.
  if (dex_compilation_unit_ == nullptr) {
    return;
  }

  HBasicBlock* entry_block = graph_->GetEntryBlock();

  graph_->SetNumberOfInVRegs(number_of_parameters);
  const char* shorty = dex_compilation_unit_->GetShorty();
  int locals_index = locals_.size() - number_of_parameters;
  int parameter_index = 0;

  const DexFile::MethodId& referrer_method_id =
      dex_file_->GetMethodId(dex_compilation_unit_->GetDexMethodIndex());
  if (!dex_compilation_unit_->IsStatic()) {
    // Add the implicit 'this' argument, not expressed in the signature.
    HParameterValue* parameter = new (arena_) HParameterValue(*dex_file_,
                                                              referrer_method_id.class_idx_,
                                                              parameter_index++,
                                                              Primitive::kPrimNot,
                                                              true);
    entry_block->AddInstruction(parameter);
    HLocal* local = GetLocalAt(locals_index++);
    entry_block->AddInstruction(new (arena_) HStoreLocal(local, parameter, local->GetDexPc()));
    number_of_parameters--;
  }

  const DexFile::ProtoId& proto = dex_file_->GetMethodPrototype(referrer_method_id);
  const DexFile::TypeList* arg_types = dex_file_->GetProtoParameters(proto);
  for (int i = 0, shorty_pos = 1; i < number_of_parameters; i++) {
    HParameterValue* parameter = new (arena_) HParameterValue(
        *dex_file_,
        arg_types->GetTypeItem(shorty_pos - 1).type_idx_,
        parameter_index++,
        Primitive::GetType(shorty[shorty_pos]),
        false);
    ++shorty_pos;
    entry_block->AddInstruction(parameter);
    HLocal* local = GetLocalAt(locals_index++);
    // Store the parameter value in the local that the dex code will use
    // to reference that parameter.
    entry_block->AddInstruction(new (arena_) HStoreLocal(local, parameter, local->GetDexPc()));
    bool is_wide = (parameter->GetType() == Primitive::kPrimLong)
        || (parameter->GetType() == Primitive::kPrimDouble);
    if (is_wide) {
      i++;
      locals_index++;
      parameter_index++;
    }
  }
}

void HGraphBuilder::MaybeRecordStat(MethodCompilationStat compilation_stat) {
  if (compilation_stats_ != nullptr) {
    compilation_stats_->RecordStat(compilation_stat);
  }
}

bool HGraphBuilder::SkipCompilation(size_t number_of_branches) {
  if (compiler_driver_ == nullptr) {
    // Note that the compiler driver is null when unit testing.
    return false;
  }

  const CompilerOptions& compiler_options = compiler_driver_->GetCompilerOptions();
  CompilerOptions::CompilerFilter compiler_filter = compiler_options.GetCompilerFilter();
  if (compiler_filter == CompilerOptions::kEverything) {
    return false;
  }

  if (compiler_options.IsHugeMethod(code_item_.insns_size_in_code_units_)) {
    VLOG(compiler) << "Skip compilation of huge method "
                   << PrettyMethod(dex_compilation_unit_->GetDexMethodIndex(), *dex_file_)
                   << ": " << code_item_.insns_size_in_code_units_ << " code units";
    MaybeRecordStat(MethodCompilationStat::kNotCompiledHugeMethod);
    return true;
  }

  // If it's large and contains no branches, it's likely to be machine generated initialization.
  if (compiler_options.IsLargeMethod(code_item_.insns_size_in_code_units_)
      && (number_of_branches == 0)) {
    VLOG(compiler) << "Skip compilation of large method with no branch "
                   << PrettyMethod(dex_compilation_unit_->GetDexMethodIndex(), *dex_file_)
                   << ": " << code_item_.insns_size_in_code_units_ << " code units";
    MaybeRecordStat(MethodCompilationStat::kNotCompiledLargeMethodNoBranches);
    return true;
  }

  return false;
}

static bool BlockIsNotPopulated(HBasicBlock* block) {
  if (!block->GetPhis().IsEmpty()) {
    return false;
  } else if (block->IsLoopHeader()) {
    return block->GetFirstInstruction()->IsSuspendCheck() &&
           (block->GetFirstInstruction() == block->GetLastInstruction());
  } else {
    return block->GetInstructions().IsEmpty();
  }
}

bool HGraphBuilder::GenerateInstructions() {
  // Find locations where we want to generate extra stackmaps for native debugging.
  // This allows us to generate the info only at interesting points (for example,
  // at start of java statement) rather than before every dex instruction.
  const bool native_debuggable = compiler_driver_ != nullptr &&
                                 compiler_driver_->GetCompilerOptions().GetNativeDebuggable();
  ArenaBitVector* native_debug_info_locations;
  if (native_debuggable) {
    const uint32_t num_instructions = code_item_.insns_size_in_code_units_;
    native_debug_info_locations = new (arena_) ArenaBitVector (arena_, num_instructions, false);
    FindNativeDebugInfoLocations(native_debug_info_locations);
  }

  InitializeLocals(code_item_.registers_size_);
  InitializeParameters(code_item_.ins_size_);

  // Add the suspend check to the entry block.
  current_block_ = graph_->GetEntryBlock();
  current_block_->AddInstruction(new (arena_) HSuspendCheck(0));

  for (CodeItemIterator it(code_item_); !it.Done(); it.Advance()) {
    uint32_t dex_pc = it.CurrentDexPc();

    HBasicBlock* next_block = FindBlockStartingAt(dex_pc);
    if (next_block != nullptr && next_block->GetGraph() != nullptr) {
      if (current_block_ != nullptr) {
        // Branching instructions clear current_block, so we know
        // the last instruction of the current block is not a branching
        // instruction. We add an unconditional goto to the found block.
        current_block_->AddInstruction(new (arena_) HGoto(dex_pc));
      }
      DCHECK(BlockIsNotPopulated(next_block));
      current_block_ = next_block;
    }

    if (current_block_ == nullptr) {
      // Unreachable code.
      continue;
    }

    if (native_debuggable && native_debug_info_locations->IsBitSet(dex_pc)) {
      current_block_->AddInstruction(new (arena_) HNativeDebugInfo(dex_pc));
    }

    if (!AnalyzeDexInstruction(it.CurrentInstruction(), dex_pc)) {
      return false;
    }
  }

  // Add Exit to the exit block.
  HBasicBlock* exit_block = graph_->GetExitBlock();
  if (exit_block == nullptr) {
    // Unreachable exit block was removed.
  } else {
    exit_block->AddInstruction(new (arena_) HExit());
  }

  return true;
}

GraphAnalysisResult HGraphBuilder::BuildGraph(StackHandleScopeCollection* handles) {
  DCHECK(graph_->GetBlocks().empty());
  graph_->SetMaximumNumberOfOutVRegs(code_item_.outs_size_);
  graph_->SetHasTryCatch(code_item_.tries_size_ != 0);
  graph_->InitializeInexactObjectRTI(handles);

  block_builder_ = new (arena_) HBasicBlockBuilder(graph_, dex_file_, code_item_);
  if (!block_builder_->Build()) {
    return kAnalysisInvalidBytecode;
  }

  if (SkipCompilation(block_builder_->GetNumberOfBranches())) {
    return kAnalysisSkipped;
  }

  GraphAnalysisResult result = graph_->BuildDominatorTree();
  if (result != kAnalysisSuccess) {
    return result;
  }

  if (!GenerateInstructions()) {
    return kAnalysisInvalidBytecode;
  }

  return SsaBuilder(graph_, code_item_, handles).BuildSsa();
}

void HGraphBuilder::FindNativeDebugInfoLocations(ArenaBitVector* locations) {
  // The callback gets called when the line number changes.
  // In other words, it marks the start of new java statement.
  struct Callback {
    static bool Position(void* ctx, const DexFile::PositionInfo& entry) {
      static_cast<ArenaBitVector*>(ctx)->SetBit(entry.address_);
      return false;
    }
  };
  dex_file_->DecodeDebugPositionInfo(&code_item_, Callback::Position, locations);
  // Instruction-specific tweaks.
  const Instruction* const begin = Instruction::At(code_item_.insns_);
  const Instruction* const end = begin->RelativeAt(code_item_.insns_size_in_code_units_);
  for (const Instruction* inst = begin; inst < end; inst = inst->Next()) {
    switch (inst->Opcode()) {
      case Instruction::MOVE_EXCEPTION: {
        // Stop in native debugger after the exception has been moved.
        // The compiler also expects the move at the start of basic block so
        // we do not want to interfere by inserting native-debug-info before it.
        locations->ClearBit(inst->GetDexPc(code_item_.insns_));
        const Instruction* next = inst->Next();
        if (next < end) {
          locations->SetBit(next->GetDexPc(code_item_.insns_));
        }
        break;
      }
      default:
        break;
    }
  }
}

}  // namespace art
