/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_SRC_COMPILER_DEX_PORTABLE_MIRTOGBC_H_
#define ART_SRC_COMPILER_DEX_PORTABLE_MIRTOGBC_H_

#include "invoke_type.h"
#include "compiled_method.h"
#include "compiler/dex/compiler_enums.h"
#include "compiler/dex/compiler_ir.h"
#include "compiler/dex/backend.h"
#include "dex_ir_builder.h"
#include "safe_map.h"

namespace art {

class CompiledMethod;
class CompilationUnit;
struct BasicBlock;
struct CallInfo;
struct CompilationUnit;
struct MIR;
struct RegLocation;
struct RegisterInfo;
class MIRGraph;

// Target-specific initialization.
Backend* PortableCodeGenerator(CompilationUnit* const cu, MIRGraph* const mir_graph,
                               ArenaAllocator* const arena);

namespace compiler {
namespace dex {
namespace portable {

class MirConverter : public Backend {

 public:
  MirConverter(CompilationUnit* cu, MIRGraph* mir_graph, ArenaAllocator* arena);

  void Materialize();

  CompiledMethod* GetCompiledMethod() {
    return NULL;
  }

 private:

  void ConvertMIRNode(MIR* mir, BasicBlock* bb, llvm::BasicBlock* llvm_bb);

  // Return the LLVM value associated with the SSA register s_reg.
  llvm::Value* GetLLVMValue(int s_reg) const;

  // Associate value with the MIR SSA register s_reg.
  void DefineValue(llvm::Value* val, int s_reg);

  llvm::Type* LlvmTypeFromLocRec(RegLocation loc);

  // Return the LLVM basic block associated with the MIR basic block "id".
  llvm::BasicBlock* GetLLVMBlock(int id) const {
    return id_to_block_map_.Get(id);
  }

  llvm::BasicBlock* FindCaseTarget(uint32_t vaddr) const;

  void ConvertPackedSwitch(BasicBlock* bb, int32_t table_offset, RegLocation rl_src);

  void ConvertSparseSwitch(BasicBlock* bb, int32_t table_offset, RegLocation rl_src);

  void ConvertSget(int32_t field_idx, Primitive::Type type, uint32_t dex_pc, RegLocation rl_dest);

  void ConvertSput(int32_t field_index, Primitive::Type type, uint32_t dex_pc, RegLocation rl_src);

  void ConvertIget(MIR* mir, RegLocation rl_dest, RegLocation rl_obj, uint32_t field_idx,
                   Primitive::Type elem_type);
  void ConvertIput(MIR* mir, RegLocation rl_src, RegLocation rl_obj, uint32_t field_idx,
                   Primitive::Type elem_type);
  void ConvertAget(MIR* mir, RegLocation rl_dest, RegLocation rl_array, RegLocation rl_index,
                   Primitive::Type elem_type);
  void ConvertAput(MIR* mir, RegLocation rl_src, RegLocation rl_array, RegLocation rl_index,
                   Primitive::Type elem_type);

#if 0
  void ConvertFillArrayData(int32_t offset, RegLocation rl_array);
  llvm::Value* EmitConst(llvm::ArrayRef< llvm::Value*> src,
                         RegLocation loc);
  void EmitPopShadowFrame();
  llvm::Value* EmitCopy(llvm::ArrayRef< llvm::Value*> src,
                        RegLocation loc);
  void ConvertMoveException(RegLocation rl_dest);
  void ConvertThrow(RegLocation rl_src);
  void ConvertMonitorEnterExit(int opt_flags,
                               artllvm::IntrinsicHelper::IntrinsicId id, RegLocation rl_src);
#endif
  llvm::Value* ConvertCompare(ConditionCode cc,
                              llvm::Value* src1, llvm::Value* src2);
  void ConvertCompareAndBranch(BasicBlock* bb, MIR* mir, ConditionCode cc,
                               RegLocation rl_src1, RegLocation rl_src2);
  void ConvertCompareZeroAndBranch(BasicBlock* bb, MIR* mir, ConditionCode cc,
                                   RegLocation rl_src1);
#if 0
  void ConvertInvoke(BasicBlock* bb, MIR* mir, InvokeType invoke_type,
                     bool is_range, bool is_filled_new_array);
  void ConvertConstObject(uint32_t idx,
                          artllvm::IntrinsicHelper::IntrinsicId id, RegLocation rl_dest);
  void ConvertCheckCast(uint32_t type_idx, RegLocation rl_src);
  void ConvertNewInstance(uint32_t type_idx, RegLocation rl_dest);
  void ConvertNewArray(uint32_t type_idx, RegLocation rl_dest,
                       RegLocation rl_src);
  void ConvertInstanceOf(uint32_t type_idx, RegLocation rl_dest,
                         RegLocation rl_src);
  void SetDexOffset(int32_t offset);
  void SetMethodInfo();
  void HandlePhiNodes(BasicBlock* bb, llvm::BasicBlock* llvm_bb);
  void ConvertExtendedMIR(BasicBlock* bb, MIR* mir, llvm::BasicBlock* llvm_bb);
  bool BlockBitcodeConversion(BasicBlock* bb);
  llvm::FunctionType* GetFunctionType();
  bool CreateFunction();
  bool CreateLLVMBasicBlock(BasicBlock* bb);
#endif

  CompilationUnit* const cu_;
  MIRGraph* const mir_graph_;
  DexIRBuilder irb_;
  std::string symbol_;
  llvm::BasicBlock* placeholder_bb_;
  llvm::BasicBlock* entry_bb_;
  llvm::BasicBlock* entry_target_bb_;
  std::string bitcode_filename_;
  GrowableArray< llvm::Value*> llvm_values_;
  int32_t temp_name_;
  SafeMap<int32_t, llvm::BasicBlock*> id_to_block_map_;  // block id -> llvm bb.
  int current_dalvik_offset_;
};  // Class MirConverter

}  // namespace portable
}  // namespace dex
}  // namespace compiler
}  // namespace art

#endif // ART_SRC_COMPILER_DEX_PORTABLE_MIRTOGBC_H_
