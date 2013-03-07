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

#include "mir_to_llvm.h"

#include "compiler/dex/compiler_internals.h"
#include "compiler/dex/dataflow_iterator.h"
#include "compiler/dex/frontend.h"
#include "dex_file-inl.h"
#include "object_utils.h"

#include <llvm/ADT/DepthFirstIterator.h>
#include <llvm/Analysis/Verifier.h>
#include <llvm/Bitcode/ReaderWriter.h>
#include <llvm/IR/Instruction.h>
#include <llvm/IR/Instructions.h>
#include <llvm/IR/Metadata.h>
#include <llvm/IR/Type.h>
#include <llvm/Support/Casting.h>
#include <llvm/Support/InstIterator.h>
#include <llvm/Support/ToolOutputFile.h>

const char* kLabelFormat = "%c0x%x_%d";
const char kInvalidBlock = 0xff;
const char kNormalBlock = 'L';
const char kCatchBlock = 'C';

namespace art {

Backend* PortableCodeGenerator(CompilationUnit* const cu, MIRGraph* const mir_graph,
                               ArenaAllocator* const arena) {
  return new compiler::dex::portable::MirConverter(cu, mir_graph, arena);
}

namespace compiler {
namespace dex {
namespace portable {

MirConverter::MirConverter(CompilationUnit* cu, MIRGraph* mir_graph, ArenaAllocator* arena) :
  Backend(arena),
  cu_(cu),
  mir_graph_(mir_graph),
  irb_(cu),
  placeholder_bb_(NULL),
  entry_bb_(NULL),
  entry_target_bb_(NULL),
  llvm_values_(arena, mir_graph->GetNumSSARegs()),
  temp_name_(0),
  current_dalvik_offset_(0) {
  // If we're a debug build force bitcode verification.
  if (kIsDebugBuild) {
    cu->enable_debug |= (1 << kDebugVerifyBitcode);
  }
}

void MirConverter::Materialize() {
  /*
   * Convert MIR to LLVM_IR
   *  o For each ssa name, create LLVM named value.  Type these
   *    appropriately, and ignore high half of wide and double operands.
   *  o For each MIR basic block, create an LLVM basic block.
   *  o Iterate through the MIR a basic block at a time, setting arguments
   *    to recovered ssa name.
   */
#if 0
  InitIR();

  // Create the function
  CreateFunction();

  // Create an LLVM basic block for each MIR block in dfs preorder
  PreOrderDfsIterator iter(mir_graph_, false /* not iterative */);
  for (BasicBlock* bb = iter.Next(); bb != NULL; bb = iter.Next()) {
    CreateLLVMBasicBlock(bb);
  }

  /*
   * Create an llvm named value for each MIR SSA name.  Note: we'll use
   * placeholders for all non-argument values (because we haven't seen
   * the definition yet).
   */
  irb_.SetInsertPoint(placeholder_bb_);
  llvm::Function::arg_iterator arg_iter(func_->arg_begin());
  arg_iter++;  /* Skip path method */
  for (int i = 0; i < mir_graph_->GetNumSSARegs(); i++) {
    llvm::Value* val;
    RegLocation rl_temp = mir_graph_->reg_location_[i];
    if ((mir_graph_->SRegToVReg(i) < 0) || rl_temp.high_word) {
      llvm_values_.Insert(0);
    } else if ((i < cu_->num_regs) ||
               (i >= (cu_->num_regs + cu_->num_ins))) {
      llvm::Constant* imm_value = mir_graph_->reg_location_[i].wide ?
         irb_.getJLong(0) : irb_.getJInt(0);
      val = EmitConst(imm_value, mir_graph_->reg_location_[i]);
      val->setName(mir_graph_->GetSSAString(i));
      llvm_values_.Insert(val);
    } else {
      // Recover previously-created argument values
      llvm::Value* arg_val = arg_iter++;
      llvm_values_.Insert(arg_val);
    }
  }

  PreOrderDfsIterator iter2(mir_graph_, false /* not iterative */);
  for (BasicBlock* bb = iter2.Next(); bb != NULL; bb = iter2.Next()) {
    BlockBitcodeConversion(bb);
  }

  /*
   * In a few rare cases of verification failure, the verifier will
   * replace one or more Dalvik opcodes with the special
   * throw-verification-failure opcode.  This can leave the SSA graph
   * in an invalid state, as definitions may be lost, while uses retained.
   * To work around this problem, we insert placeholder definitions for
   * all Dalvik SSA regs in the "placeholder" block.  Here, after
   * bitcode conversion is complete, we examine those placeholder definitions
   * and delete any with no references (which normally is all of them).
   *
   * If any definitions remain, we link the placeholder block into the
   * CFG.  Otherwise, it is deleted.
   */
  for (llvm::BasicBlock::iterator it = placeholder_bb_->begin(),
       it_end = placeholder_bb_->end(); it != it_end;) {
    llvm::Instruction* inst = llvm::dyn_cast< llvm::Instruction>(it++);
    DCHECK(inst != NULL);
    llvm::Value* val = llvm::dyn_cast< llvm::Value>(inst);
    DCHECK(val != NULL);
    if (val->getNumUses() == 0) {
      inst->eraseFromParent();
    }
  }
  SetDexOffset(0);
  if (placeholder_bb_->empty()) {
    placeholder_bb_->eraseFromParent();
  } else {
    irb_.SetInsertPoint(placeholder_bb_);
    irb_.CreateBr(entry_target_bb_);
    entry_target_bb_ = placeholder_bb_;
  }
  irb_.SetInsertPoint(entry_bb_);
  irb_.CreateBr(entry_target_bb_);

  if (cu_->enable_debug & (1 << kDebugVerifyBitcode)) {
     if (llvm::verifyFunction(*func_, llvm::PrintMessageAction)) {
       LOG(INFO) << "Bitcode verification FAILED for "
                 << PrettyMethod(cu_->method_idx, *cu_->dex_file)
                 << " of size " << cu_->code_item->insns_size_in_code_units_;
       cu_->enable_debug |= (1 << kDebugDumpBitcodeFile);
     }
  }

  if (cu_->enable_debug & (1 << kDebugDumpBitcodeFile)) {
    // Write bitcode to file
    std::string errmsg;
    std::string fname(PrettyMethod(cu_->method_idx, *cu_->dex_file));
    mir_graph_->ReplaceSpecialChars(fname);
    // TODO: make configurable change naming mechanism to avoid fname length issues.
    fname = StringPrintf("/sdcard/Bitcode/%s.bc", fname.c_str());

    if (fname.size() > 240) {
      LOG(INFO) << "Warning: bitcode filename too long. Truncated.";
      fname.resize(240);
    }

    llvm::OwningPtr< llvm::tool_output_file> out_file(
        new llvm::tool_output_file(fname.c_str(), errmsg,
                                   llvm::raw_fd_ostream::F_Binary));

    if (!errmsg.empty()) {
      LOG(ERROR) << "Failed to create bitcode output file: " << errmsg;
    }

    llvm::WriteBitcodeToFile(module_, out_file->os());
    out_file->keep();
  }
#endif
}

/*
 * Target-independent code generation.  Use only high-level
 * load/store utilities here, or target-dependent genXX() handlers
 * when necessary.
 */
void MirConverter::ConvertMIRNode(MIR* mir, BasicBlock* bb, llvm::BasicBlock* llvm_bb) {
  //bool res = false;   // Assume success
  RegLocation rl_src[3];
  RegLocation rl_dest = mir_graph_->GetBadLoc();
  Instruction::Code opcode = mir->dalvikInsn.opcode;
  int op_val = opcode;
  //uint32_t vB = mir->dalvikInsn.vB;
  //uint32_t vC = mir->dalvikInsn.vC;
  //int opt_flags = mir->optimization_flags;

  if (cu_->verbose) {
    if (op_val < kMirOpFirst) {
      LOG(INFO) << ".. " << Instruction::Name(opcode) << " 0x" << std::hex << op_val;
    } else {
      LOG(INFO) << mir_graph_->extended_mir_op_names_[op_val - kMirOpFirst] << " 0x" << std::hex << op_val;
    }
  }

  /* Prep Src and Dest locations */
  int next_sreg = 0;
  int next_loc = 0;
  int attrs = mir_graph_->oat_data_flow_attributes_[opcode];
  rl_src[0] = rl_src[1] = rl_src[2] = mir_graph_->GetBadLoc();
  if (attrs & DF_UA) {
    if (attrs & DF_A_WIDE) {
      rl_src[next_loc++] = mir_graph_->GetSrcWide(mir, next_sreg);
      next_sreg+= 2;
    } else {
      rl_src[next_loc++] = mir_graph_->GetSrc(mir, next_sreg);
      next_sreg++;
    }
  }
  if (attrs & DF_UB) {
    if (attrs & DF_B_WIDE) {
      rl_src[next_loc++] = mir_graph_->GetSrcWide(mir, next_sreg);
      next_sreg+= 2;
    } else {
      rl_src[next_loc++] = mir_graph_->GetSrc(mir, next_sreg);
      next_sreg++;
    }
  }
  if (attrs & DF_UC) {
    if (attrs & DF_C_WIDE) {
      rl_src[next_loc++] = mir_graph_->GetSrcWide(mir, next_sreg);
    } else {
      rl_src[next_loc++] = mir_graph_->GetSrc(mir, next_sreg);
    }
  }
  if (attrs & DF_DA) {
    if (attrs & DF_A_WIDE) {
      rl_dest = mir_graph_->GetDestWide(mir);
    } else {
      rl_dest = mir_graph_->GetDest(mir);
    }
  }

  switch (opcode) {
    case Instruction::NOP:
      break;

    case Instruction::MOVE:
    case Instruction::MOVE_OBJECT:
    case Instruction::MOVE_16:
    case Instruction::MOVE_OBJECT_16:
    case Instruction::MOVE_OBJECT_FROM16:
    case Instruction::MOVE_FROM16:
    case Instruction::MOVE_WIDE:
    case Instruction::MOVE_WIDE_16:
    case Instruction::MOVE_WIDE_FROM16:
      DefineValue(GetLLVMValue(rl_src[0].orig_sreg), rl_dest.orig_sreg);
      break;

    case Instruction::CONST:
    case Instruction::CONST_4:
    case Instruction::CONST_16: {
        llvm::Constant* imm_value = irb_.getInt32(mir->dalvikInsn.vB);
        DefineValue(imm_value, rl_dest.orig_sreg);
      }
      break;

    case Instruction::CONST_WIDE_16:
    case Instruction::CONST_WIDE_32: {
        // Sign extend to 64 bits
        int64_t imm = static_cast<int32_t>(mir->dalvikInsn.vB);
        llvm::Constant* imm_value = irb_.getInt64(imm);
        DefineValue(imm_value, rl_dest.orig_sreg);
      }
      break;

    case Instruction::CONST_HIGH16: {
        llvm::Constant* imm_value = irb_.getInt32(mir->dalvikInsn.vB << 16);
        DefineValue(imm_value, rl_dest.orig_sreg);
      }
      break;

    case Instruction::CONST_WIDE: {
        llvm::Constant* imm_value = irb_.getInt64(mir->dalvikInsn.vB_wide);
        DefineValue(imm_value, rl_dest.orig_sreg);
      }
      break;
    case Instruction::CONST_WIDE_HIGH16: {
        int64_t imm = static_cast<int64_t>(mir->dalvikInsn.vB) << 48;
        llvm::Constant* imm_value = irb_.getInt64(imm);
        DefineValue(imm_value, rl_dest.orig_sreg);
      }
      break;

    case Instruction::SPUT_OBJECT:
      ConvertSput(mir->dalvikInsn.vB, Primitive::kPrimNot, mir->GetDexPc(), rl_src[0]);
      break;
    case Instruction::SPUT:
      if (rl_src[0].fp) {
        ConvertSput(mir->dalvikInsn.vB, Primitive::kPrimFloat, mir->GetDexPc(), rl_src[0]);
      } else {
        ConvertSput(mir->dalvikInsn.vB, Primitive::kPrimInt, mir->GetDexPc(), rl_src[0]);
      }
      break;
    case Instruction::SPUT_BOOLEAN:
      ConvertSput(mir->dalvikInsn.vB, Primitive::kPrimBoolean, mir->GetDexPc(), rl_src[0]);
      break;
    case Instruction::SPUT_BYTE:
      ConvertSput(mir->dalvikInsn.vB, Primitive::kPrimByte, mir->GetDexPc(), rl_src[0]);
      break;
    case Instruction::SPUT_CHAR:
      ConvertSput(mir->dalvikInsn.vB, Primitive::kPrimChar, mir->GetDexPc(), rl_src[0]);
      break;
    case Instruction::SPUT_SHORT:
      ConvertSput(mir->dalvikInsn.vB, Primitive::kPrimShort, mir->GetDexPc(), rl_src[0]);
      break;
    case Instruction::SPUT_WIDE:
      if (rl_src[0].fp) {
        ConvertSput(mir->dalvikInsn.vB, Primitive::kPrimDouble, mir->GetDexPc(), rl_src[0]);
      } else {
        ConvertSput(mir->dalvikInsn.vB, Primitive::kPrimLong, mir->GetDexPc(), rl_src[0]);
      }
      break;

    case Instruction::SGET_OBJECT:
      ConvertSget(mir->dalvikInsn.vB, Primitive::kPrimNot, mir->GetDexPc(), rl_dest);
      break;
    case Instruction::SGET:
      if (rl_dest.fp) {
        ConvertSget(mir->dalvikInsn.vB, Primitive::kPrimFloat, mir->GetDexPc(), rl_dest);
      } else {
        ConvertSget(mir->dalvikInsn.vB, Primitive::kPrimInt, mir->GetDexPc(), rl_dest);
      }
      break;
    case Instruction::SGET_BOOLEAN:
      ConvertSget(mir->dalvikInsn.vB, Primitive::kPrimBoolean, mir->GetDexPc(), rl_dest);
      break;
    case Instruction::SGET_BYTE:
      ConvertSget(mir->dalvikInsn.vB, Primitive::kPrimByte, mir->GetDexPc(), rl_dest);
      break;
    case Instruction::SGET_CHAR:
      ConvertSget(mir->dalvikInsn.vB, Primitive::kPrimChar, mir->GetDexPc(), rl_dest);
      break;
    case Instruction::SGET_SHORT:
      ConvertSget(mir->dalvikInsn.vB, Primitive::kPrimShort, mir->GetDexPc(), rl_dest);
      break;
    case Instruction::SGET_WIDE:
      if (rl_dest.fp) {
        ConvertSget(mir->dalvikInsn.vB, Primitive::kPrimDouble, mir->GetDexPc(), rl_dest);
      } else {
        ConvertSget(mir->dalvikInsn.vB, Primitive::kPrimLong, mir->GetDexPc(), rl_dest);
      }
      break;
    case Instruction::RETURN_WIDE:
    case Instruction::RETURN:
    case Instruction::RETURN_OBJECT:
      if (!mir_graph_->MethodIsLeaf()) {
        irb_.SuspendCheck(mir->GetDexPc());
      }
      irb_.PopShadowFrame();
      irb_.CreateRet(GetLLVMValue(rl_src[0].orig_sreg));
      DCHECK(bb->terminated_by_return);
      break;

    case Instruction::RETURN_VOID:
      if (((cu_->access_flags & kAccConstructor) != 0) &&
          cu_->compiler_driver->RequiresConstructorBarrier(Thread::Current(),
                                                           cu_->dex_file,
                                                           cu_->class_def_idx)) {
        irb_.CreateMemoryBarrier(art::kStoreStore);
      }
      if (!mir_graph_->MethodIsLeaf()) {
        irb_.SuspendCheck(mir->GetDexPc());
      }
      irb_.PopShadowFrame();
      irb_.CreateRetVoid();
      DCHECK(bb->terminated_by_return);
      break;

    case Instruction::IF_EQ:
      ConvertCompareAndBranch(bb, mir, kCondEq, rl_src[0], rl_src[1]);
      break;
    case Instruction::IF_NE:
      ConvertCompareAndBranch(bb, mir, kCondNe, rl_src[0], rl_src[1]);
      break;
    case Instruction::IF_LT:
      ConvertCompareAndBranch(bb, mir, kCondLt, rl_src[0], rl_src[1]);
      break;
    case Instruction::IF_GE:
      ConvertCompareAndBranch(bb, mir, kCondGe, rl_src[0], rl_src[1]);
      break;
    case Instruction::IF_GT:
      ConvertCompareAndBranch(bb, mir, kCondGt, rl_src[0], rl_src[1]);
      break;
    case Instruction::IF_LE:
      ConvertCompareAndBranch(bb, mir, kCondLe, rl_src[0], rl_src[1]);
      break;
    case Instruction::IF_EQZ:
      ConvertCompareZeroAndBranch(bb, mir, kCondEq, rl_src[0]);
      break;
    case Instruction::IF_NEZ:
      ConvertCompareZeroAndBranch(bb, mir, kCondNe, rl_src[0]);
      break;
    case Instruction::IF_LTZ:
      ConvertCompareZeroAndBranch(bb, mir, kCondLt, rl_src[0]);
      break;
    case Instruction::IF_GEZ:
      ConvertCompareZeroAndBranch(bb, mir, kCondGe, rl_src[0]);
      break;
    case Instruction::IF_GTZ:
      ConvertCompareZeroAndBranch(bb, mir, kCondGt, rl_src[0]);
      break;
    case Instruction::IF_LEZ:
      ConvertCompareZeroAndBranch(bb, mir, kCondLe, rl_src[0]);
      break;

    case Instruction::GOTO:
    case Instruction::GOTO_16:
    case Instruction::GOTO_32:
      irb_.FlushShadowFrameVRegs();
      if (bb->taken->start_offset <= bb->start_offset) {
        irb_.SuspendCheck(mir->GetDexPc());
      }
      irb_.CreateBr(GetLLVMBlock(bb->taken->id));
      break;

    case Instruction::ADD_LONG:
    case Instruction::ADD_LONG_2ADDR:
    case Instruction::ADD_INT:
    case Instruction::ADD_INT_2ADDR:
      DefineValue(irb_.CreateAdd(GetLLVMValue(rl_src[0].orig_sreg),
                                 GetLLVMValue(rl_src[1].orig_sreg)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::SUB_LONG:
    case Instruction::SUB_LONG_2ADDR:
    case Instruction::SUB_INT:
    case Instruction::SUB_INT_2ADDR:
      DefineValue(irb_.CreateSub(GetLLVMValue(rl_src[0].orig_sreg),
                                 GetLLVMValue(rl_src[1].orig_sreg)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::MUL_LONG:
    case Instruction::MUL_LONG_2ADDR:
    case Instruction::MUL_INT:
    case Instruction::MUL_INT_2ADDR:
      DefineValue(irb_.CreateMul(GetLLVMValue(rl_src[0].orig_sreg),
                                 GetLLVMValue(rl_src[1].orig_sreg)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::DIV_LONG:
    case Instruction::DIV_LONG_2ADDR:
    case Instruction::DIV_INT:
    case Instruction::DIV_INT_2ADDR:
      DefineValue(irb_.CreateDivModOp(mir->GetDexPc(), true /* is_div */, rl_dest.wide,
                                      GetLLVMValue(rl_src[0].orig_sreg),
                                      GetLLVMValue(rl_src[1].orig_sreg)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::REM_LONG:
    case Instruction::REM_LONG_2ADDR:
    case Instruction::REM_INT:
    case Instruction::REM_INT_2ADDR:
      DefineValue(irb_.CreateDivModOp(mir->GetDexPc(), false /* is_div */, rl_dest.wide,
                                      GetLLVMValue(rl_src[0].orig_sreg),
                                      GetLLVMValue(rl_src[1].orig_sreg)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::AND_LONG:
    case Instruction::AND_LONG_2ADDR:
    case Instruction::AND_INT:
    case Instruction::AND_INT_2ADDR:
      DefineValue(irb_.CreateAnd(GetLLVMValue(rl_src[0].orig_sreg),
                                 GetLLVMValue(rl_src[1].orig_sreg)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::OR_LONG:
    case Instruction::OR_LONG_2ADDR:
    case Instruction::OR_INT:
    case Instruction::OR_INT_2ADDR:
      DefineValue(irb_.CreateOr(GetLLVMValue(rl_src[0].orig_sreg),
                                GetLLVMValue(rl_src[1].orig_sreg)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::XOR_LONG:
    case Instruction::XOR_LONG_2ADDR:
    case Instruction::XOR_INT:
    case Instruction::XOR_INT_2ADDR:
      DefineValue(irb_.CreateXor(GetLLVMValue(rl_src[0].orig_sreg),
                                 GetLLVMValue(rl_src[1].orig_sreg)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::SHL_LONG:
    case Instruction::SHL_LONG_2ADDR:
    case Instruction::SHL_INT:
    case Instruction::SHL_INT_2ADDR:
      DefineValue(irb_.CreateJavaShl(rl_dest.wide,
                                     GetLLVMValue(rl_src[0].orig_sreg),
                                     GetLLVMValue(rl_src[1].orig_sreg)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::SHR_LONG:
    case Instruction::SHR_LONG_2ADDR:
    case Instruction::SHR_INT:
    case Instruction::SHR_INT_2ADDR:
      DefineValue(irb_.CreateJavaAShr(rl_dest.wide,
                                      GetLLVMValue(rl_src[0].orig_sreg),
                                      GetLLVMValue(rl_src[1].orig_sreg)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::USHR_LONG:
    case Instruction::USHR_LONG_2ADDR:
    case Instruction::USHR_INT:
    case Instruction::USHR_INT_2ADDR:
      DefineValue(irb_.CreateJavaLShr(rl_dest.wide,
                                      GetLLVMValue(rl_src[0].orig_sreg),
                                      GetLLVMValue(rl_src[1].orig_sreg)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::ADD_INT_LIT16:
    case Instruction::ADD_INT_LIT8:
      DefineValue(irb_.CreateAdd(GetLLVMValue(rl_src[0].orig_sreg),
                                 irb_.getInt32(mir->dalvikInsn.vC)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::RSUB_INT:
    case Instruction::RSUB_INT_LIT8:
      DefineValue(irb_.CreateSub(irb_.getInt32(mir->dalvikInsn.vC),
                                 GetLLVMValue(rl_src[0].orig_sreg)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::MUL_INT_LIT16:
    case Instruction::MUL_INT_LIT8:
      DefineValue(irb_.CreateMul(GetLLVMValue(rl_src[0].orig_sreg),
                                 irb_.getInt32(mir->dalvikInsn.vC)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::DIV_INT_LIT16:
    case Instruction::DIV_INT_LIT8:
      DefineValue(irb_.CreateDivModOp(mir->GetDexPc(), true /* is_div */, false /* is_long */,
                                      GetLLVMValue(rl_src[0].orig_sreg),
                                      irb_.getInt32(mir->dalvikInsn.vC)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::REM_INT_LIT16:
    case Instruction::REM_INT_LIT8:
      DefineValue(irb_.CreateDivModOp(mir->GetDexPc(), false /* is_div */, false /* is_long */,
                                      GetLLVMValue(rl_src[0].orig_sreg),
                                      irb_.getInt32(mir->dalvikInsn.vC)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::AND_INT_LIT16:
    case Instruction::AND_INT_LIT8:
      DefineValue(irb_.CreateAnd(GetLLVMValue(rl_src[0].orig_sreg),
                                 irb_.getInt32(mir->dalvikInsn.vC)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::OR_INT_LIT16:
    case Instruction::OR_INT_LIT8:
      DefineValue(irb_.CreateOr(GetLLVMValue(rl_src[0].orig_sreg),
                                 irb_.getInt32(mir->dalvikInsn.vC)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::XOR_INT_LIT16:
    case Instruction::XOR_INT_LIT8:
      DefineValue(irb_.CreateXor(GetLLVMValue(rl_src[0].orig_sreg),
                                 irb_.getInt32(mir->dalvikInsn.vC)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::SHL_INT_LIT8:
      DefineValue(irb_.CreateShl(GetLLVMValue(rl_src[0].orig_sreg),
                                 irb_.getInt32(mir->dalvikInsn.vC & 0x1f)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::SHR_INT_LIT8:
      DefineValue(irb_.CreateAShr(GetLLVMValue(rl_src[0].orig_sreg),
                                  irb_.getInt32(mir->dalvikInsn.vC & 0x1f)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::USHR_INT_LIT8:
      DefineValue(irb_.CreateLShr(GetLLVMValue(rl_src[0].orig_sreg),
                                  irb_.getInt32(mir->dalvikInsn.vC & 0x1f)),
                  rl_dest.orig_sreg);
      break;

    case Instruction::ADD_FLOAT:
    case Instruction::ADD_FLOAT_2ADDR:
    case Instruction::ADD_DOUBLE:
    case Instruction::ADD_DOUBLE_2ADDR:
      DefineValue(irb_.CreateFAdd(GetLLVMValue(rl_src[0].orig_sreg),
                                  GetLLVMValue(rl_src[1].orig_sreg)),
                  rl_dest.orig_sreg);
      break;

    case Instruction::SUB_FLOAT:
    case Instruction::SUB_FLOAT_2ADDR:
    case Instruction::SUB_DOUBLE:
    case Instruction::SUB_DOUBLE_2ADDR:
      DefineValue(irb_.CreateFSub(GetLLVMValue(rl_src[0].orig_sreg),
                                  GetLLVMValue(rl_src[1].orig_sreg)),
                  rl_dest.orig_sreg);
      break;

    case Instruction::MUL_FLOAT:
    case Instruction::MUL_FLOAT_2ADDR:
    case Instruction::MUL_DOUBLE:
    case Instruction::MUL_DOUBLE_2ADDR:
      DefineValue(irb_.CreateFMul(GetLLVMValue(rl_src[0].orig_sreg),
                                  GetLLVMValue(rl_src[1].orig_sreg)),
                  rl_dest.orig_sreg);
      break;

    case Instruction::DIV_FLOAT:
    case Instruction::DIV_FLOAT_2ADDR:
    case Instruction::DIV_DOUBLE:
    case Instruction::DIV_DOUBLE_2ADDR:
      DefineValue(irb_.CreateFDiv(GetLLVMValue(rl_src[0].orig_sreg),
                                  GetLLVMValue(rl_src[1].orig_sreg)),
                  rl_dest.orig_sreg);
      break;

    case Instruction::REM_FLOAT:
    case Instruction::REM_FLOAT_2ADDR:
    case Instruction::REM_DOUBLE:
    case Instruction::REM_DOUBLE_2ADDR:
      DefineValue(irb_.CreateFRem(GetLLVMValue(rl_src[0].orig_sreg),
                                  GetLLVMValue(rl_src[1].orig_sreg)),
                  rl_dest.orig_sreg);
      break;

#if 0
    case Instruction::INVOKE_STATIC:
      ConvertInvoke(bb, mir, kStatic, false /*range*/,
                    false /* NewFilledArray */);
      break;
    case Instruction::INVOKE_STATIC_RANGE:
      ConvertInvoke(bb, mir, kStatic, true /*range*/,
                    false /* NewFilledArray */);
      break;

    case Instruction::INVOKE_DIRECT:
      ConvertInvoke(bb,  mir, kDirect, false /*range*/,
                    false /* NewFilledArray */);
      break;
    case Instruction::INVOKE_DIRECT_RANGE:
      ConvertInvoke(bb, mir, kDirect, true /*range*/,
                    false /* NewFilledArray */);
      break;

    case Instruction::INVOKE_VIRTUAL:
      ConvertInvoke(bb, mir, kVirtual, false /*range*/,
                    false /* NewFilledArray */);
      break;
    case Instruction::INVOKE_VIRTUAL_RANGE:
      ConvertInvoke(bb, mir, kVirtual, true /*range*/,
                    false /* NewFilledArray */);
      break;

    case Instruction::INVOKE_SUPER:
      ConvertInvoke(bb, mir, kSuper, false /*range*/,
                    false /* NewFilledArray */);
      break;
    case Instruction::INVOKE_SUPER_RANGE:
      ConvertInvoke(bb, mir, kSuper, true /*range*/,
                    false /* NewFilledArray */);
      break;

    case Instruction::INVOKE_INTERFACE:
      ConvertInvoke(bb, mir, kInterface, false /*range*/,
                    false /* NewFilledArray */);
      break;
    case Instruction::INVOKE_INTERFACE_RANGE:
      ConvertInvoke(bb, mir, kInterface, true /*range*/,
                    false /* NewFilledArray */);
      break;
    case Instruction::FILLED_NEW_ARRAY:
      ConvertInvoke(bb, mir, kInterface, false /*range*/,
                    true /* NewFilledArray */);
      break;
    case Instruction::FILLED_NEW_ARRAY_RANGE:
      ConvertInvoke(bb, mir, kInterface, true /*range*/,
                    true /* NewFilledArray */);
      break;

    case Instruction::CONST_STRING:
    case Instruction::CONST_STRING_JUMBO:
      ConvertConstObject(vB, artllvm::IntrinsicHelper::ConstString,
                         rl_dest);
      break;

    case Instruction::CONST_CLASS:
      ConvertConstObject(vB, artllvm::IntrinsicHelper::ConstClass,
                         rl_dest);
      break;

    case Instruction::CHECK_CAST:
      ConvertCheckCast(vB, rl_src[0]);
      break;

    case Instruction::NEW_INSTANCE:
      ConvertNewInstance(vB, rl_dest);
      break;

   case Instruction::MOVE_EXCEPTION:
      ConvertMoveException(rl_dest);
      break;

   case Instruction::THROW:
      ConvertThrow(rl_src[0]);
      /*
       * If this throw is standalone, terminate.
       * If it might rethrow, force termination
       * of the following block.
       */
      if (bb->fall_through == NULL) {
        irb_.CreateUnreachable();
      } else {
        bb->fall_through->fall_through = NULL;
        bb->fall_through->taken = NULL;
      }
      break;
#endif
    case Instruction::MOVE_RESULT_WIDE:
    case Instruction::MOVE_RESULT:
    case Instruction::MOVE_RESULT_OBJECT:
      /*
       * All move_results should have been folded into the preceeding invoke.
       */
      LOG(FATAL) << "Unexpected move_result";
      break;
    case Instruction::MONITOR_ENTER: {
      bool ignore_null_check = ((mir->optimization_flags & MIR_IGNORE_NULL_CHECK) != 0);
      llvm::Value* object = GetLLVMValue(rl_src[0].orig_sreg);
      irb_.CreateMonitorEnter(ignore_null_check, mir->GetDexPc(), object);
      break;
    }
    case Instruction::MONITOR_EXIT: {
      bool ignore_null_check = ((mir->optimization_flags & MIR_IGNORE_NULL_CHECK) != 0);
      llvm::Value* object = GetLLVMValue(rl_src[0].orig_sreg);
      irb_.CreateMonitorExit(ignore_null_check, mir->GetDexPc(), object);
      break;
    }
    case Instruction::ARRAY_LENGTH: {
      llvm::Value* array = GetLLVMValue(rl_src[0].orig_sreg);
      bool ignore_null_check = ((mir->optimization_flags & MIR_IGNORE_NULL_CHECK) != 0);
      if (!ignore_null_check) {
        // Flush vregs to shadow frame in case of exception.
        // TODO: should we move the flush into the slow path?
        irb_.FlushShadowFrameVRegsAndSetDexPc(mir->GetDexPc());
      }
      DefineValue(irb_.CreateArrayLength(ignore_null_check, mir->GetDexPc(), array),
                  rl_dest.orig_sreg);
      break;
    }
#if 0
    case Instruction::NEW_ARRAY:
      ConvertNewArray(vC, rl_dest, rl_src[0]);
      break;

    case Instruction::INSTANCE_OF:
      ConvertInstanceOf(vC, rl_dest, rl_src[0]);
      break;
#endif
    case Instruction::AGET:
      ConvertAget(mir, rl_dest, rl_src[0], rl_src[1],
                  rl_dest.fp ? Primitive::kPrimFloat : Primitive::kPrimInt);
      break;
    case Instruction::AGET_OBJECT:
      ConvertAget(mir, rl_dest, rl_src[0], rl_src[1], Primitive::kPrimNot);
      break;
    case Instruction::AGET_BOOLEAN:
      ConvertAget(mir, rl_dest, rl_src[0], rl_src[1], Primitive::kPrimBoolean);
      break;
    case Instruction::AGET_BYTE:
      ConvertAget(mir, rl_dest, rl_src[0], rl_src[1], Primitive::kPrimByte);
      break;
    case Instruction::AGET_CHAR:
      ConvertAget(mir, rl_dest, rl_src[0], rl_src[1], Primitive::kPrimChar);
      break;
    case Instruction::AGET_SHORT:
      ConvertAget(mir, rl_dest, rl_src[0], rl_src[1], Primitive::kPrimShort);
      break;
    case Instruction::AGET_WIDE:
      ConvertAget(mir, rl_dest, rl_src[0], rl_src[1],
                  rl_dest.fp ? Primitive::kPrimDouble : Primitive::kPrimLong);
      break;
    case Instruction::APUT:
      ConvertAput(mir, rl_src[0], rl_src[1], rl_src[2],
                  rl_dest.fp ? Primitive::kPrimFloat : Primitive::kPrimInt);
      break;
    case Instruction::APUT_OBJECT:
      ConvertAput(mir, rl_src[0], rl_src[1], rl_src[2], Primitive::kPrimNot);
      break;
    case Instruction::APUT_BOOLEAN:
      ConvertAput(mir, rl_src[0], rl_src[1], rl_src[2], Primitive::kPrimBoolean);
      break;
    case Instruction::APUT_BYTE:
      ConvertAput(mir, rl_src[0], rl_src[1], rl_src[2], Primitive::kPrimByte);
      break;
    case Instruction::APUT_CHAR:
      ConvertAput(mir, rl_src[0], rl_src[1], rl_src[2], Primitive::kPrimChar);
      break;
    case Instruction::APUT_SHORT:
      ConvertAput(mir, rl_src[0], rl_src[1], rl_src[2], Primitive::kPrimShort);
      break;
    case Instruction::APUT_WIDE:
      ConvertAput(mir, rl_src[0], rl_src[1], rl_src[2],
                  rl_dest.fp ? Primitive::kPrimDouble : Primitive::kPrimLong);
      break;

    case Instruction::IGET:
      ConvertIget(mir, rl_dest, rl_src[0], mir->dalvikInsn.vC,
                  rl_dest.fp ? Primitive::kPrimFloat : Primitive::kPrimInt);
      break;
    case Instruction::IGET_OBJECT:
      ConvertIget(mir, rl_dest, rl_src[0], mir->dalvikInsn.vC, Primitive::kPrimNot);
      break;
    case Instruction::IGET_BOOLEAN:
      ConvertIget(mir, rl_dest, rl_src[0], mir->dalvikInsn.vC, Primitive::kPrimBoolean);
      break;
    case Instruction::IGET_BYTE:
      ConvertIget(mir, rl_dest, rl_src[0], mir->dalvikInsn.vC, Primitive::kPrimByte);
      break;
    case Instruction::IGET_CHAR:
      ConvertIget(mir, rl_dest, rl_src[0], mir->dalvikInsn.vC, Primitive::kPrimChar);
      break;
    case Instruction::IGET_SHORT:
      ConvertIget(mir, rl_dest, rl_src[0], mir->dalvikInsn.vC, Primitive::kPrimShort);
      break;
    case Instruction::IGET_WIDE:
      ConvertIget(mir, rl_dest, rl_src[0], mir->dalvikInsn.vC,
                  rl_dest.fp ? Primitive::kPrimDouble : Primitive::kPrimLong);
      break;

    case Instruction::IPUT:
      ConvertIput(mir, rl_src[0], rl_src[1], mir->dalvikInsn.vC,
                  rl_src[0].fp ? Primitive::kPrimFloat : Primitive::kPrimInt);
      break;
    case Instruction::IPUT_OBJECT:
      ConvertIput(mir, rl_src[0], rl_src[1], mir->dalvikInsn.vC, Primitive::kPrimNot);
      break;
    case Instruction::IPUT_BOOLEAN:
      ConvertIput(mir, rl_src[0], rl_src[1], mir->dalvikInsn.vC, Primitive::kPrimBoolean);
      break;
    case Instruction::IPUT_BYTE:
      ConvertIput(mir, rl_src[0], rl_src[1], mir->dalvikInsn.vC, Primitive::kPrimByte);
      break;
    case Instruction::IPUT_CHAR:
      ConvertIput(mir, rl_src[0], rl_src[1], mir->dalvikInsn.vC, Primitive::kPrimChar);
      break;
    case Instruction::IPUT_SHORT:
      ConvertIput(mir, rl_src[0], rl_src[1], mir->dalvikInsn.vC, Primitive::kPrimShort);
      break;
    case Instruction::IPUT_WIDE:
      ConvertIput(mir, rl_src[0], rl_src[1], mir->dalvikInsn.vC,
                  rl_src[0].fp ? Primitive::kPrimDouble : Primitive::kPrimLong);
      break;

#if 0
    case Instruction::FILL_ARRAY_DATA:
      ConvertFillArrayData(vB, rl_src[0]);
      break;
#endif
    case Instruction::LONG_TO_INT:
      DefineValue(irb_.CreateTrunc(GetLLVMValue(rl_src[0].orig_sreg), irb_.getInt32Ty()),
                  rl_dest.orig_sreg);
      break;
    case Instruction::INT_TO_LONG:
      DefineValue(irb_.CreateSExt(GetLLVMValue(rl_src[0].orig_sreg), irb_.getInt64Ty()),
                  rl_dest.orig_sreg);
      break;
    case Instruction::INT_TO_CHAR:
      DefineValue(irb_.CreateZExt(irb_.CreateTrunc(GetLLVMValue(rl_src[0].orig_sreg),
                                                   irb_.getInt16Ty()),
                                  irb_.getInt32Ty()),
                  rl_dest.orig_sreg);
      break;
    case Instruction::INT_TO_BYTE:
      DefineValue(irb_.CreateSExt(irb_.CreateTrunc(GetLLVMValue(rl_src[0].orig_sreg),
                                                   irb_.getInt8Ty()),
                                  irb_.getInt32Ty()),
                  rl_dest.orig_sreg);
      break;
    case Instruction::INT_TO_SHORT:
      DefineValue(irb_.CreateSExt(irb_.CreateTrunc(GetLLVMValue(rl_src[0].orig_sreg),
                                                   irb_.getInt16Ty()),
                                  irb_.getInt32Ty()),
                  rl_dest.orig_sreg);
      break;
    case Instruction::INT_TO_FLOAT:
    case Instruction::LONG_TO_FLOAT:
      DefineValue(irb_.CreateSIToFP(GetLLVMValue(rl_src[0].orig_sreg), irb_.getFloatTy()),
                  rl_dest.orig_sreg);
      break;
    case Instruction::INT_TO_DOUBLE:
    case Instruction::LONG_TO_DOUBLE:
      DefineValue(irb_.CreateSIToFP(GetLLVMValue(rl_src[0].orig_sreg), irb_.getDoubleTy()),
                  rl_dest.orig_sreg);
      break;

    case Instruction::FLOAT_TO_DOUBLE:
      DefineValue(irb_.CreateFPExt(GetLLVMValue(rl_src[0].orig_sreg), irb_.getDoubleTy()),
                  rl_dest.orig_sreg);
      break;

    case Instruction::DOUBLE_TO_FLOAT:
      DefineValue(irb_.CreateFPTrunc(GetLLVMValue(rl_src[0].orig_sreg), irb_.getFloatTy()),
                  rl_dest.orig_sreg);
      break;

    case Instruction::NEG_LONG:
    case Instruction::NEG_INT:
      DefineValue(irb_.CreateNeg(GetLLVMValue(rl_src[0].orig_sreg)), rl_dest.orig_sreg);
      break;

    case Instruction::NEG_FLOAT:
    case Instruction::NEG_DOUBLE:
      DefineValue(irb_.CreateFNeg(GetLLVMValue(rl_src[0].orig_sreg)), rl_dest.orig_sreg);
      break;

    case Instruction::NOT_LONG:
    case Instruction::NOT_INT:
      DefineValue(irb_.CreateXor(GetLLVMValue(rl_src[0].orig_sreg), static_cast<uint64_t>(-1)),
                  rl_dest.orig_sreg);
      break;

    case Instruction::FLOAT_TO_INT:
      DefineValue(irb_.CreateFloatToInt(GetLLVMValue(rl_src[0].orig_sreg), false, false),
                  rl_dest.orig_sreg);
      break;
    case Instruction::DOUBLE_TO_INT:
      DefineValue(irb_.CreateFloatToInt(GetLLVMValue(rl_src[0].orig_sreg), true, false),
                  rl_dest.orig_sreg);
      break;
    case Instruction::FLOAT_TO_LONG:
      DefineValue(irb_.CreateFloatToInt(GetLLVMValue(rl_src[0].orig_sreg), false, true),
                  rl_dest.orig_sreg);
      break;
    case Instruction::DOUBLE_TO_LONG:
      DefineValue(irb_.CreateFloatToInt(GetLLVMValue(rl_src[0].orig_sreg), true, true),
                  rl_dest.orig_sreg);
      break;

    case Instruction::CMPL_FLOAT:
    case Instruction::CMPL_DOUBLE:
      DefineValue(irb_.CreateFpCompare(GetLLVMValue(rl_src[0].orig_sreg),
                                       GetLLVMValue(rl_src[1].orig_sreg),
                                       false /* gt_bias */),
                  rl_dest.orig_sreg);
      break;
    case Instruction::CMPG_FLOAT:
    case Instruction::CMPG_DOUBLE:
      DefineValue(irb_.CreateFpCompare(GetLLVMValue(rl_src[0].orig_sreg),
                                       GetLLVMValue(rl_src[1].orig_sreg),
                                       true /* gt_bias */),
                  rl_dest.orig_sreg);
      break;
    case Instruction::CMP_LONG:
      DefineValue(irb_.CreateLongCompare(GetLLVMValue(rl_src[0].orig_sreg),
                                         GetLLVMValue(rl_src[1].orig_sreg)),
                  rl_dest.orig_sreg);
      break;
    case Instruction::PACKED_SWITCH:
      ConvertPackedSwitch(bb, mir->dalvikInsn.vB, rl_src[0]);
      break;

    case Instruction::SPARSE_SWITCH:
      ConvertSparseSwitch(bb, mir->dalvikInsn.vB, rl_src[0]);
      break;

    default:
      UNIMPLEMENTED(FATAL) << "Unsupported Dex opcode 0x" << std::hex << opcode;
  }
}

llvm::Value* MirConverter::GetLLVMValue(int s_reg) const {
  llvm::Value* result = llvm_values_.Get(s_reg);
  DCHECK(result != NULL) << "Use before definition of: " << s_reg;
  return result;
}

void MirConverter::DefineValue(llvm::Value* value, int s_reg) {
  // Set vreg in the shadow from for GC and debugging.
  int v_reg = mir_graph_->SRegToVReg(s_reg);
  irb_.RememberShadowFrameVReg(v_reg, value);
  // Add MIR SSA name.
  if (kIsDebugBuild) {
    value->setName(mir_graph_->GetSSAString(s_reg));
  }
  // Place in LLVM values.
  DCHECK(llvm_values_.Get(s_reg) == NULL) << "Multiple definitions of SSA name: " << s_reg;
  llvm_values_.Put(s_reg, value);
}

llvm::Type* MirConverter::LlvmTypeFromLocRec(RegLocation loc) {
  if (loc.wide) {
    return loc.fp ? irb_.getDoubleTy() : irb_.getInt64Ty();
  } else if (loc.fp) {
    return irb_.getFloatTy();
  } else if (loc.ref) {
    return irb_.GetJavaObjectTy();
  } else {
    return irb_.getInt32Ty();
  }
}

llvm::BasicBlock* MirConverter::FindCaseTarget(uint32_t vaddr) const {
  BasicBlock* bb = mir_graph_->FindBlock(vaddr);
  DCHECK(bb != NULL);
  return GetLLVMBlock(bb->id);
}

void MirConverter::ConvertPackedSwitch(BasicBlock* bb, int32_t table_offset, RegLocation rl_src) {
  irb_.FlushShadowFrameVRegs();
  const Instruction::PackedSwitchPayload* payload =
      reinterpret_cast<const Instruction::PackedSwitchPayload*>(cu_->insns +
                                                                current_dalvik_offset_ +
                                                                table_offset);

  llvm::Value* value = GetLLVMValue(rl_src.orig_sreg);

  llvm::SwitchInst* sw = irb_.CreateSwitch(value, GetLLVMBlock(bb->fall_through->id),
                                           payload->case_count);

  for (uint16_t i = 0; i < payload->case_count; ++i) {
    llvm::BasicBlock* llvm_bb = FindCaseTarget(current_dalvik_offset_ + payload->targets[i]);
    sw->addCase(irb_.getInt32(payload->first_key + i), llvm_bb);
  }
  irb_.AddSwitchNodeMetaData(sw, table_offset);
  bb->taken = NULL;
  bb->fall_through = NULL;
}

void MirConverter::ConvertSparseSwitch(BasicBlock* bb, int32_t table_offset, RegLocation rl_src) {
  const Instruction::SparseSwitchPayload* payload =
      reinterpret_cast<const Instruction::SparseSwitchPayload*>(
      cu_->insns + current_dalvik_offset_ + table_offset);

  const int32_t* keys = payload->GetKeys();
  const int32_t* targets = payload->GetTargets();

  llvm::Value* value = GetLLVMValue(rl_src.orig_sreg);

  llvm::SwitchInst* sw = irb_.CreateSwitch(value, GetLLVMBlock(bb->fall_through->id),
                                           payload->case_count);

  for (size_t i = 0; i < payload->case_count; ++i) {
    llvm::BasicBlock* llvm_bb = FindCaseTarget(current_dalvik_offset_ + targets[i]);
    sw->addCase(irb_.getInt32(keys[i]), llvm_bb);
  }
  irb_.AddSwitchNodeMetaData(sw, table_offset);
  bb->taken = NULL;
  bb->fall_through = NULL;
}

void MirConverter::ConvertSget(int32_t field_idx, Primitive::Type type, uint32_t dex_pc,
                               RegLocation rl_dest) {
  int field_offset, ssb_index;
  bool is_referrers_class, is_volatile, is_const;
  compiler::driver::DexCompilationUnit m_unit(cu_);
  bool is_fast_path = cu_->compiler_driver->ComputeStaticFieldInfo(field_idx, &m_unit, field_offset,
                                                                   ssb_index, is_referrers_class,
                                                                   is_volatile, is_const,
                                                                   false /* is_put */);
  llvm::Value* static_field_value;
  if (!is_fast_path) {
    // Flush the vregs for the call that may throw an exception.
    irb_.FlushShadowFrameVRegsAndSetDexPc(dex_pc);
    // Do runtime support call.
    static_field_value = irb_.CallRuntimeGetStatic(type, field_idx);
    // Handle any pending exception.
    irb_.ExceptionCheck(dex_pc);
  } else {
    DCHECK_GE(field_offset, 0);
    llvm::Value* static_storage_addr;
    if (is_referrers_class) {
      // Fast path, static storage base is this method's class.
      static_storage_addr = irb_.LoadFieldFromCurMethod(Primitive::kPrimNot,
                                                        "Ljava/lang/Class;",
                                                        "declaringClass",
                                                        art::mirror::AbstractMethod::DeclaringClassOffset(),
                                                        true);
    } else {
      // Medium path, static storage base in a different class which requires checks that the other
      // class is initialized.
      // TODO: if we have an image we may know the class is already initialized and can elide the
      //       slow path.
      DCHECK_GE(ssb_index, 0);
      static_storage_addr = irb_.LoadStaticStorageBase(dex_pc, ssb_index);
    }
    const DexFile::FieldId& field_id = cu_->dex_file->GetFieldId(field_idx);
    const char* type_descriptor = cu_->dex_file->GetFieldTypeDescriptor(field_id);
    const char* class_descriptor = cu_->dex_file->GetFieldDeclaringClassDescriptor(field_id);
    const char* field_name = cu_->dex_file->GetFieldName(field_id);
    static_field_value = irb_.LoadStaticField(type, type_descriptor,
                                              class_descriptor, field_name, is_volatile, is_const,
                                              static_storage_addr, field_offset);
  }
  DefineValue(static_field_value, rl_dest.orig_sreg);
}

void MirConverter::ConvertSput(int32_t field_idx, Primitive::Type type, uint32_t dex_pc,
                               RegLocation rl_src) {
  int field_offset, ssb_index;
  bool is_referrers_class, is_volatile, is_const;
  compiler::driver::DexCompilationUnit m_unit(cu_);
  bool is_fast_path = cu_->compiler_driver->ComputeStaticFieldInfo(field_idx, &m_unit, field_offset,
                                                                   ssb_index, is_referrers_class,
                                                                   is_volatile, is_const,
                                                                   false /* is_put */);
  llvm::Value* value = GetLLVMValue(rl_src.orig_sreg);
  if (!is_fast_path) {
    // Flush the vregs for the call that may throw an exception.
    irb_.FlushShadowFrameVRegsAndSetDexPc(dex_pc);
    // Do runtime support call.
    llvm::Value* zero_if_success = irb_.CallRuntimeSetStatic(value, type, field_idx);
    // Handle any pending exception.
    irb_.BranchToExceptionIfNonZero(zero_if_success, dex_pc);
  } else {
    DCHECK_GE(field_offset, 0);
    llvm::Value* static_storage_addr;
    if (is_referrers_class) {
      // Fast path, static storage base is this method's class.
      static_storage_addr = irb_.LoadFieldFromCurMethod(Primitive::kPrimNot,
                                                        "Ljava/lang/Class;",
                                                        "declaringClass",
                                                        art::mirror::AbstractMethod::DeclaringClassOffset(),
                                                        true);
    } else {
      // Medium path, static storage base in a different class which requires checks that the other
      // class is initialized.
      // TODO: if we have an image we may know the class is already initialized and can elide the
      //       slow path.
      DCHECK_GE(ssb_index, 0);
      static_storage_addr = irb_.LoadStaticStorageBase(dex_pc, ssb_index);
    }
    const DexFile::FieldId& field_id = cu_->dex_file->GetFieldId(field_idx);
    const char* type_descriptor = cu_->dex_file->GetFieldTypeDescriptor(field_id);
    const char* class_descriptor = cu_->dex_file->GetFieldDeclaringClassDescriptor(field_id);
    const char* field_name = cu_->dex_file->GetFieldName(field_id);
    irb_.StoreStaticField(type, type_descriptor,
                          class_descriptor, field_name, is_volatile, is_const,
                          static_storage_addr, value, MemberOffset(field_offset));
  }
}

void MirConverter::ConvertIget(MIR* mir, RegLocation rl_dest, RegLocation rl_obj,
                               uint32_t field_idx, Primitive::Type type) {
  int field_offset, ssb_index;
  bool is_referrers_class, is_volatile, is_const;
  compiler::driver::DexCompilationUnit m_unit(cu_);
  bool is_fast_path = cu_->compiler_driver->ComputeStaticFieldInfo(field_idx, &m_unit, field_offset,
                                                                   ssb_index, is_referrers_class,
                                                                   is_volatile, is_const,
                                                                   false /* is_put */);
  llvm::Value* object = GetLLVMValue(rl_obj.orig_sreg);
  llvm::Value* field_value;
  if (!is_fast_path) {
    // Flush the vregs for the call that may throw an exception.
    irb_.FlushShadowFrameVRegsAndSetDexPc(mir->GetDexPc());
    // Do runtime support call.
    field_value = irb_.CallRuntimeGetInstance(type, field_idx, object);
    // Handle any pending exception.
    irb_.ExceptionCheck(mir->GetDexPc());
  } else {
    DCHECK_GE(field_offset, 0);
    bool ignore_null_check = ((mir->optimization_flags & MIR_IGNORE_NULL_CHECK) != 0);
    const DexFile::FieldId& field_id = cu_->dex_file->GetFieldId(field_idx);
    const char* type_descriptor = cu_->dex_file->GetFieldTypeDescriptor(field_id);
    const char* class_descriptor = cu_->dex_file->GetFieldDeclaringClassDescriptor(field_id);
    const char* field_name = cu_->dex_file->GetFieldName(field_id);

    field_value = irb_.LoadInstanceField(object, type, type_descriptor,
                                         class_descriptor, field_name,
                                         MemberOffset(field_offset),
                                         ignore_null_check, mir->GetDexPc(),
                                         is_volatile, is_const);
  }
  DefineValue(field_value, rl_dest.orig_sreg);
}

void MirConverter::ConvertIput(MIR* mir, RegLocation rl_src, RegLocation rl_obj,
                               uint32_t field_idx, Primitive::Type type) {
  int field_offset, ssb_index;
  bool is_referrers_class, is_volatile, is_const;
  compiler::driver::DexCompilationUnit m_unit(cu_);
  bool is_fast_path = cu_->compiler_driver->ComputeStaticFieldInfo(field_idx, &m_unit, field_offset,
                                                                   ssb_index, is_referrers_class,
                                                                   is_volatile, is_const,
                                                                   false /* is_put */);
  llvm::Value* value = GetLLVMValue(rl_src.orig_sreg);
  llvm::Value* object = GetLLVMValue(rl_obj.orig_sreg);
  if (!is_fast_path) {
    // Flush the vregs for the call that may throw an exception.
    irb_.FlushShadowFrameVRegsAndSetDexPc(mir->GetDexPc());
    // Do runtime support call.
    irb_.CallRuntimeSetInstance(object, value, type, field_idx);
    // Handle any pending exception.
    irb_.ExceptionCheck(mir->GetDexPc());
  } else {
    DCHECK_GE(field_offset, 0);
    bool ignore_null_check = ((mir->optimization_flags & MIR_IGNORE_NULL_CHECK) != 0);
    const DexFile::FieldId& field_id = cu_->dex_file->GetFieldId(field_idx);
    const char* type_descriptor = cu_->dex_file->GetFieldTypeDescriptor(field_id);
    const char* class_descriptor = cu_->dex_file->GetFieldDeclaringClassDescriptor(field_id);
    const char* field_name = cu_->dex_file->GetFieldName(field_id);

    irb_.StoreInstanceField(type, type_descriptor,
                            class_descriptor, field_name,
                            ignore_null_check, mir->GetDexPc(),
                            is_volatile, is_const,
                            object, value, MemberOffset(field_offset));
  }
}

void MirConverter::ConvertAget(MIR* mir, RegLocation rl_dest, RegLocation rl_array,
                               RegLocation rl_index, Primitive::Type elem_type) {
  bool ignore_null_check = ((mir->optimization_flags & MIR_IGNORE_NULL_CHECK) != 0);
  bool ignore_bound_check = ((mir->optimization_flags & MIR_IGNORE_RANGE_CHECK) != 0);
  if (!ignore_null_check || !ignore_bound_check) {
    // Flush vregs to shadow frame in case of exception.
    // TODO: should we move the flush into the slow path?
    irb_.FlushShadowFrameVRegsAndSetDexPc(mir->GetDexPc());
  }
  DefineValue(irb_.CreateArrayLoad(ignore_null_check, ignore_bound_check, mir->GetDexPc(),
                                   GetLLVMValue(rl_array.orig_sreg),
                                   GetLLVMValue(rl_index.orig_sreg), elem_type),
              rl_dest.orig_sreg);
}

void MirConverter::ConvertAput(MIR* mir, RegLocation rl_src, RegLocation rl_array,
                               RegLocation rl_index, Primitive::Type elem_type) {
  bool ignore_null_check = ((mir->optimization_flags & MIR_IGNORE_NULL_CHECK) != 0);
  bool ignore_bound_check = ((mir->optimization_flags & MIR_IGNORE_RANGE_CHECK) != 0);
  irb_.CreateArrayStore(ignore_null_check, ignore_bound_check, mir->GetDexPc(),
                        GetLLVMValue(rl_array.orig_sreg),
                        GetLLVMValue(rl_index.orig_sreg),
                        GetLLVMValue(rl_src.orig_sreg),
                        elem_type);
}

#if 0
void MirConverter::ConvertFillArrayData(int32_t offset, RegLocation rl_array)
{
  artllvm::IntrinsicHelper::IntrinsicId id;
  id = artllvm::IntrinsicHelper::HLFillArrayData;
  llvm::SmallVector< llvm::Value*, 2> args;
  args.push_back(irb_.getInt32(offset));
  args.push_back(GetLLVMValue(rl_array.orig_sreg));
  llvm::Function* intr = intrinsic_helper_->GetIntrinsicFunction(id);
  irb_.CreateCall(intr, args);
}

llvm::Value* MirConverter::EmitConst(llvm::ArrayRef< llvm::Value*> src,
                              RegLocation loc)
{
  artllvm::IntrinsicHelper::IntrinsicId id;
  if (loc.wide) {
    if (loc.fp) {
      id = artllvm::IntrinsicHelper::ConstDouble;
    } else {
      id = artllvm::IntrinsicHelper::ConstLong;
    }
  } else {
    if (loc.fp) {
      id = artllvm::IntrinsicHelper::ConstFloat;
    } else if (loc.ref) {
      id = artllvm::IntrinsicHelper::ConstObj;
    } else {
      id = artllvm::IntrinsicHelper::ConstInt;
    }
  }
  llvm::Function* intr = intrinsic_helper_->GetIntrinsicFunction(id);
  return irb_.CreateCall(intr, src);
}

void MirConverter::EmitPopShadowFrame()
{
  llvm::Function* intr = intrinsic_helper_->GetIntrinsicFunction(
      artllvm::IntrinsicHelper::PopShadowFrame);
  irb_.CreateCall(intr);
}

llvm::Value* MirConverter::EmitCopy(llvm::ArrayRef< llvm::Value*> src,
                             RegLocation loc)
{
  artllvm::IntrinsicHelper::IntrinsicId id;
  if (loc.wide) {
    if (loc.fp) {
      id = artllvm::IntrinsicHelper::CopyDouble;
    } else {
      id = artllvm::IntrinsicHelper::CopyLong;
    }
  } else {
    if (loc.fp) {
      id = artllvm::IntrinsicHelper::CopyFloat;
    } else if (loc.ref) {
      id = artllvm::IntrinsicHelper::CopyObj;
    } else {
      id = artllvm::IntrinsicHelper::CopyInt;
    }
  }
  llvm::Function* intr = intrinsic_helper_->GetIntrinsicFunction(id);
  return irb_.CreateCall(intr, src);
}

void MirConverter::ConvertMoveException(RegLocation rl_dest)
{
  llvm::Function* func = intrinsic_helper_->GetIntrinsicFunction(
      artllvm::IntrinsicHelper::GetException);
  llvm::Value* res = irb_.CreateCall(func);
  DefineValue(res, rl_dest.orig_sreg);
}

void MirConverter::ConvertThrow(RegLocation rl_src)
{
  llvm::Value* src = GetLLVMValue(rl_src.orig_sreg);
  llvm::Function* func = intrinsic_helper_->GetIntrinsicFunction(
      artllvm::IntrinsicHelper::HLThrowException);
  irb_.CreateCall(func, src);
}

void MirConverter::ConvertMonitorEnterExit(int opt_flags,
                                    artllvm::IntrinsicHelper::IntrinsicId id,
                                    RegLocation rl_src)
{
  llvm::SmallVector< llvm::Value*, 2> args;
  args.push_back(irb_.getInt32(opt_flags));
  args.push_back(GetLLVMValue(rl_src.orig_sreg));
  llvm::Function* func = intrinsic_helper_->GetIntrinsicFunction(id);
  irb_.CreateCall(func, args);
}

#endif

llvm::Value* MirConverter::ConvertCompare(ConditionCode cc, llvm::Value* src1, llvm::Value* src2) {
  llvm::Value* res = NULL;
  DCHECK_EQ(src1->getType(), src2->getType());
  switch(cc) {
    case kCondEq: res = irb_.CreateICmpEQ(src1, src2); break;
    case kCondNe: res = irb_.CreateICmpNE(src1, src2); break;
    case kCondLt: res = irb_.CreateICmpSLT(src1, src2); break;
    case kCondGe: res = irb_.CreateICmpSGE(src1, src2); break;
    case kCondGt: res = irb_.CreateICmpSGT(src1, src2); break;
    case kCondLe: res = irb_.CreateICmpSLE(src1, src2); break;
    default: LOG(FATAL) << "Unexpected cc value " << cc;
  }
  return res;
}

void MirConverter::ConvertCompareAndBranch(BasicBlock* bb, MIR* mir, ConditionCode cc,
                                           RegLocation rl_src1, RegLocation rl_src2) {
  irb_.FlushShadowFrameVRegs();
  if (bb->taken->start_offset <= mir->offset) {
    // TODO: move to back-edge.
    irb_.SuspendCheck(mir->GetDexPc());
  }
  llvm::Value* src1 = GetLLVMValue(rl_src1.orig_sreg);
  llvm::Value* src2 = GetLLVMValue(rl_src2.orig_sreg);
  llvm::Value* cond_value = ConvertCompare(cc, src1, src2);
  cond_value->setName(StringPrintf("t%d", temp_name_++));
  irb_.CreateCondBr(cond_value, GetLLVMBlock(bb->taken->id), GetLLVMBlock(bb->fall_through->id),
                    DexIRBuilder::kUnknown);
  // Don't redo the fall-through branch in the BB driver.
  bb->fall_through = NULL;
}

void MirConverter::ConvertCompareZeroAndBranch(BasicBlock* bb, MIR* mir, ConditionCode cc,
                                               RegLocation rl_src1) {
  irb_.FlushShadowFrameVRegs();
  if (bb->taken->start_offset <= mir->offset) {
    // TODO: move to back-edge.
    irb_.SuspendCheck(mir->GetDexPc());
  }
  llvm::Value* src1 = GetLLVMValue(rl_src1.orig_sreg);
  llvm::Value* src2;
  if (rl_src1.ref) {
    src2 = irb_.GetJavaNull();
  } else {
    src2 = irb_.getInt32(0);
  }
  llvm::Value* cond_value = ConvertCompare(cc, src1, src2);
  irb_.CreateCondBr(cond_value, GetLLVMBlock(bb->taken->id), GetLLVMBlock(bb->fall_through->id),
                    DexIRBuilder::kUnknown);
  // Don't redo the fall-through branch in the BB driver.
  bb->fall_through = NULL;
}

#if 0

/*
 * Process arguments for invoke.  Note: this code is also used to
 * collect and process arguments for NEW_FILLED_ARRAY and NEW_FILLED_ARRAY_RANGE.
 * The requirements are similar.
 */
void MirConverter::ConvertInvoke(BasicBlock* bb, MIR* mir,
                          InvokeType invoke_type, bool is_range, bool is_filled_new_array)
{
  CallInfo* info = mir_graph_->NewMemCallInfo(bb, mir, invoke_type, is_range);
  llvm::SmallVector< llvm::Value*, 10> args;
  // Insert the invoke_type
  args.push_back(irb_.getInt32(static_cast<int>(invoke_type)));
  // Insert the method_idx
  args.push_back(irb_.getInt32(info->index));
  // Insert the optimization flags
  args.push_back(irb_.getInt32(info->opt_flags));
  // Now, insert the actual arguments
  for (int i = 0; i < info->num_arg_words;) {
    llvm::Value* val = GetLLVMValue(info->args[i].orig_sreg);
    args.push_back(val);
    i += info->args[i].wide ? 2 : 1;
  }
  /*
   * Choose the invoke return type based on actual usage.  Note: may
   * be different than shorty.  For example, if a function return value
   * is not used, we'll treat this as a void invoke.
   */
  artllvm::IntrinsicHelper::IntrinsicId id;
  if (is_filled_new_array) {
    id = artllvm::IntrinsicHelper::HLFilledNewArray;
  } else if (info->result.location == kLocInvalid) {
    id = artllvm::IntrinsicHelper::HLInvokeVoid;
  } else {
    if (info->result.wide) {
      if (info->result.fp) {
        id = artllvm::IntrinsicHelper::HLInvokeDouble;
      } else {
        id = artllvm::IntrinsicHelper::HLInvokeLong;
      }
    } else if (info->result.ref) {
        id = artllvm::IntrinsicHelper::HLInvokeObj;
    } else if (info->result.fp) {
        id = artllvm::IntrinsicHelper::HLInvokeFloat;
    } else {
        id = artllvm::IntrinsicHelper::HLInvokeInt;
    }
  }
  llvm::Function* intr = cu->intrinsic_helper->GetIntrinsicFunction(id);
  llvm::Value* res = cu->irb->CreateCall(intr, args);
  if (info->result.location != kLocInvalid) {
    DefineValue(res, info->result.orig_sreg);
  }
}

void MirConverter::ConvertConstObject(uint32_t idx,
                               artllvm::IntrinsicHelper::IntrinsicId id, RegLocation rl_dest)
{
  llvm::Function* intr = intrinsic_helper_->GetIntrinsicFunction(id);
  llvm::Value* index = irb_.getInt32(idx);
  llvm::Value* res = irb_.CreateCall(intr, index);
  DefineValue(res, rl_dest.orig_sreg);
}

void MirConverter::ConvertCheckCast(uint32_t type_idx, RegLocation rl_src)
{
  artllvm::IntrinsicHelper::IntrinsicId id;
  id = artllvm::IntrinsicHelper::HLCheckCast;
  llvm::Function* intr = intrinsic_helper_->GetIntrinsicFunction(id);
  llvm::SmallVector< llvm::Value*, 2> args;
  args.push_back(irb_.getInt32(type_idx));
  args.push_back(GetLLVMValue(rl_src.orig_sreg));
  irb_.CreateCall(intr, args);
}

void MirConverter::ConvertNewInstance(uint32_t type_idx, RegLocation rl_dest)
{
  artllvm::IntrinsicHelper::IntrinsicId id;
  id = artllvm::IntrinsicHelper::NewInstance;
  llvm::Function* intr = intrinsic_helper_->GetIntrinsicFunction(id);
  llvm::Value* index = irb_.getInt32(type_idx);
  llvm::Value* res = irb_.CreateCall(intr, index);
  DefineValue(res, rl_dest.orig_sreg);
}

void MirConverter::ConvertNewArray(uint32_t type_idx,
                            RegLocation rl_dest, RegLocation rl_src)
{
  artllvm::IntrinsicHelper::IntrinsicId id;
  id = artllvm::IntrinsicHelper::NewArray;
  llvm::Function* intr = intrinsic_helper_->GetIntrinsicFunction(id);
  llvm::SmallVector< llvm::Value*, 2> args;
  args.push_back(irb_.getInt32(type_idx));
  args.push_back(GetLLVMValue(rl_src.orig_sreg));
  llvm::Value* res = irb_.CreateCall(intr, args);
  DefineValue(res, rl_dest.orig_sreg);
}

void MirConverter::ConvertIget(int opt_flags,
                        artllvm::IntrinsicHelper::IntrinsicId id,
                        RegLocation rl_dest, RegLocation rl_obj, int field_index)
{
  llvm::SmallVector< llvm::Value*, 3> args;
  args.push_back(irb_.getInt32(opt_flags));
  args.push_back(GetLLVMValue(rl_obj.orig_sreg));
  args.push_back(irb_.getInt32(field_index));
  llvm::Function* intr = intrinsic_helper_->GetIntrinsicFunction(id);
  llvm::Value* res = irb_.CreateCall(intr, args);
  DefineValue(res, rl_dest.orig_sreg);
}

void MirConverter::ConvertIput(int opt_flags,
                        artllvm::IntrinsicHelper::IntrinsicId id,
                        RegLocation rl_src, RegLocation rl_obj, int field_index)
{
  llvm::SmallVector< llvm::Value*, 4> args;
  args.push_back(irb_.getInt32(opt_flags));
  args.push_back(GetLLVMValue(rl_src.orig_sreg));
  args.push_back(GetLLVMValue(rl_obj.orig_sreg));
  args.push_back(irb_.getInt32(field_index));
  llvm::Function* intr = intrinsic_helper_->GetIntrinsicFunction(id);
  irb_.CreateCall(intr, args);
}

void MirConverter::ConvertInstanceOf(uint32_t type_idx,
                              RegLocation rl_dest, RegLocation rl_src)
{
  artllvm::IntrinsicHelper::IntrinsicId id;
  id = artllvm::IntrinsicHelper::InstanceOf;
  llvm::Function* intr = intrinsic_helper_->GetIntrinsicFunction(id);
  llvm::SmallVector< llvm::Value*, 2> args;
  args.push_back(irb_.getInt32(type_idx));
  args.push_back(GetLLVMValue(rl_src.orig_sreg));
  llvm::Value* res = irb_.CreateCall(intr, args);
  DefineValue(res, rl_dest.orig_sreg);
}

void MirConverter::HandlePhiNodes(BasicBlock* bb, llvm::BasicBlock* llvm_bb)
{
  SetDexOffset(bb->start_offset);
  for (MIR* mir = bb->first_mir_insn; mir != NULL; mir = mir->next) {
    int opcode = mir->dalvikInsn.opcode;
    if (opcode < kMirOpFirst) {
      // Stop after first non-pseudo MIR op.
      continue;
    }
    if (opcode != kMirOpPhi) {
      // Skip other mir Pseudos.
      continue;
    }
    RegLocation rl_dest = mir_graph_->reg_location_[mir->ssa_rep->defs[0]];
    /*
     * The Art compiler's Phi nodes only handle 32-bit operands,
     * representing wide values using a matched set of Phi nodes
     * for the lower and upper halves.  In the llvm world, we only
     * want a single Phi for wides.  Here we will simply discard
     * the Phi node representing the high word.
     */
    if (rl_dest.high_word) {
      continue;  // No Phi node - handled via low word
    }
    int* incoming = reinterpret_cast<int*>(mir->dalvikInsn.vB);
    llvm::Type* phi_type =
        LlvmTypeFromLocRec(rl_dest);
    llvm::PHINode* phi = irb_.CreatePHI(phi_type, mir->ssa_rep->num_uses);
    for (int i = 0; i < mir->ssa_rep->num_uses; i++) {
      RegLocation loc;
      // Don't check width here.
      loc = mir_graph_->GetRawSrc(mir, i);
      DCHECK_EQ(rl_dest.wide, loc.wide);
      DCHECK_EQ(rl_dest.wide & rl_dest.high_word, loc.wide & loc.high_word);
      DCHECK_EQ(rl_dest.fp, loc.fp);
      DCHECK_EQ(rl_dest.core, loc.core);
      DCHECK_EQ(rl_dest.ref, loc.ref);
      SafeMap<unsigned int, unsigned int>::iterator it;
      it = mir_graph_->block_id_map_.find(incoming[i]);
      DCHECK(it != mir_graph_->block_id_map_.end());
      DCHECK(GetLLVMValue(loc.orig_sreg) != NULL);
      DCHECK(GetLLVMBlock(it->second) != NULL);
      phi->addIncoming(GetLLVMValue(loc.orig_sreg),
                       GetLLVMBlock(it->second));
    }
    DefineValueOnly(phi, rl_dest.orig_sreg);
  }
}

/* Extended MIR instructions like PHI */
void MirConverter::ConvertExtendedMIR(BasicBlock* bb, MIR* mir,
                               llvm::BasicBlock* llvm_bb)
{

  switch (static_cast<ExtendedMIROpcode>(mir->dalvikInsn.opcode)) {
    case kMirOpPhi: {
      // The llvm Phi node already emitted - just DefineValue() here.
      RegLocation rl_dest = mir_graph_->reg_location_[mir->ssa_rep->defs[0]];
      if (!rl_dest.high_word) {
        // Only consider low word of pairs.
        DCHECK(GetLLVMValue(rl_dest.orig_sreg) != NULL);
        llvm::Value* phi = GetLLVMValue(rl_dest.orig_sreg);
        if (1) SetVregOnValue(phi, rl_dest.orig_sreg);
      }
      break;
    }
    case kMirOpCopy: {
      UNIMPLEMENTED(WARNING) << "unimp kMirOpPhi";
      break;
    }
    case kMirOpNop:
      if ((mir == bb->last_mir_insn) && (bb->taken == NULL) &&
          (bb->fall_through == NULL)) {
        irb_.CreateUnreachable();
      }
      break;

    // TODO: need GBC intrinsic to take advantage of fused operations
    case kMirOpFusedCmplFloat:
      UNIMPLEMENTED(FATAL) << "kMirOpFusedCmpFloat unsupported";
      break;
    case kMirOpFusedCmpgFloat:
      UNIMPLEMENTED(FATAL) << "kMirOpFusedCmgFloat unsupported";
      break;
    case kMirOpFusedCmplDouble:
      UNIMPLEMENTED(FATAL) << "kMirOpFusedCmplDouble unsupported";
      break;
    case kMirOpFusedCmpgDouble:
      UNIMPLEMENTED(FATAL) << "kMirOpFusedCmpgDouble unsupported";
      break;
    case kMirOpFusedCmpLong:
      UNIMPLEMENTED(FATAL) << "kMirOpLongCmpBranch unsupported";
      break;
    default:
      break;
  }
}

/* Handle the content in each basic block */
bool MirConverter::BlockBitcodeConversion(BasicBlock* bb)
{
  if (bb->block_type == kDead) return false;
  llvm::BasicBlock* llvm_bb = GetLLVMBlock(bb->id);
  if (llvm_bb == NULL) {
    CHECK(bb->block_type == kExitBlock);
  } else {
    irb_.SetInsertPoint(llvm_bb);
    SetDexOffset(bb->start_offset);
  }

  if (cu_->verbose) {
    LOG(INFO) << "................................";
    LOG(INFO) << "Block id " << bb->id;
    if (llvm_bb != NULL) {
      LOG(INFO) << "label " << llvm_bb->getName().str().c_str();
    } else {
      LOG(INFO) << "llvm_bb is NULL";
    }
  }

  if (bb->block_type == kEntryBlock) {
    SetMethodInfo();

    { // Allocate shadowframe.
      artllvm::IntrinsicHelper::IntrinsicId id =
              artllvm::IntrinsicHelper::AllocaShadowFrame;
      llvm::Function* func = intrinsic_helper_->GetIntrinsicFunction(id);
      llvm::Value* entries = irb_.getInt32(cu_->num_dalvik_registers);
      irb_.CreateCall(func, entries);
    }

    { // Store arguments to vregs.
      uint16_t arg_reg = cu_->num_regs;

      llvm::Function::arg_iterator arg_iter(func_->arg_begin());
      llvm::Function::arg_iterator arg_end(func_->arg_end());

      const char* shorty = cu_->shorty;
      uint32_t shorty_size = strlen(shorty);
      CHECK_GE(shorty_size, 1u);

      ++arg_iter; // skip method object

      if ((cu_->access_flags & kAccStatic) == 0) {
        SetVregOnValue(arg_iter, arg_reg);
        ++arg_iter;
        ++arg_reg;
      }

      for (uint32_t i = 1; i < shorty_size; ++i, ++arg_iter) {
        SetVregOnValue(arg_iter, arg_reg);

        ++arg_reg;
        if (shorty[i] == 'J' || shorty[i] == 'D') {
          // Wide types, such as long and double, are using a pair of registers
          // to store the value, so we have to increase arg_reg again.
          ++arg_reg;
        }
      }
    }
  } else if (bb->block_type == kExitBlock) {
    /*
     * Because of the differences between how MIR/LIR and llvm handle exit
     * blocks, we won't explicitly covert them.  On the llvm-to-lir
     * path, it will need to be regenereated.
     */
    return false;
  } else if (bb->block_type == kExceptionHandling) {
    /*
     * Because we're deferring null checking, delete the associated empty
     * exception block.
     */
    llvm_bb->eraseFromParent();
    return false;
  }

  HandlePhiNodes(bb, llvm_bb);



  for (MIR* mir = bb->first_mir_insn; mir != NULL; mir = mir->next) {

    SetDexOffset(mir->offset);

    int opcode = mir->dalvikInsn.opcode;
    Instruction::Format dalvik_format =
        Instruction::FormatOf(mir->dalvikInsn.opcode);

    if (opcode == kMirOpCheck) {
      // Combine check and work halves of throwing instruction.
      MIR* work_half = mir->meta.throw_insn;
      mir->dalvikInsn.opcode = work_half->dalvikInsn.opcode;
      opcode = mir->dalvikInsn.opcode;
      SSARepresentation* ssa_rep = work_half->ssa_rep;
      work_half->ssa_rep = mir->ssa_rep;
      mir->ssa_rep = ssa_rep;
      work_half->meta.original_opcode = work_half->dalvikInsn.opcode;
      work_half->dalvikInsn.opcode = static_cast<Instruction::Code>(kMirOpNop);
      if (bb->successor_block_list.block_list_type == kCatch) {
        llvm::Function* intr = intrinsic_helper_->GetIntrinsicFunction(
            artllvm::IntrinsicHelper::CatchTargets);
        llvm::Value* switch_key =
            irb_.CreateCall(intr, irb_.getInt32(mir->offset));
        GrowableArray<SuccessorBlockInfo*>::Iterator iter(bb->successor_block_list.blocks);
        // New basic block to use for work half
        llvm::BasicBlock* work_bb =
            llvm::BasicBlock::Create(*context_, "", func_);
        llvm::SwitchInst* sw =
            irb_.CreateSwitch(switch_key, work_bb,
                                     bb->successor_block_list.blocks->Size());
        while (true) {
          SuccessorBlockInfo *successor_block_info = iter.Next();
          if (successor_block_info == NULL) break;
          llvm::BasicBlock *target =
              GetLLVMBlock(successor_block_info->block->id);
          int type_index = successor_block_info->key;
          sw->addCase(irb_.getInt32(type_index), target);
        }
        llvm_bb = work_bb;
        irb_.SetInsertPoint(llvm_bb);
      }
    }

    if (opcode >= kMirOpFirst) {
      ConvertExtendedMIR(bb, mir, llvm_bb);
      continue;
    }

    bool not_handled = ConvertMIRNode(mir, bb, llvm_bb);
    if (not_handled) {
      Instruction::Code dalvik_opcode = static_cast<Instruction::Code>(opcode);
      LOG(WARNING) << StringPrintf("%#06x: Op %#x (%s) / Fmt %d not handled",
                                   mir->offset, opcode,
                                   Instruction::Name(dalvik_opcode),
                                   dalvik_format);
    }
  }

  if (bb->block_type == kEntryBlock) {
    entry_target_bb_ = GetLLVMBlock(bb->fall_through->id);
  } else if ((bb->fall_through != NULL) && !bb->terminated_by_return) {
    irb_.CreateBr(GetLLVMBlock(bb->fall_through->id));
  }

  return false;
}

char RemapShorty(char shorty_type) {
  /*
   * TODO: might want to revisit this.  Dalvik registers are 32-bits wide,
   * and longs/doubles are represented as a pair of registers.  When sub-word
   * arguments (and method results) are passed, they are extended to Dalvik
   * virtual register containers.  Because llvm is picky about type consistency,
   * we must either cast the "real" type to 32-bit container multiple Dalvik
   * register types, or always use the expanded values.
   * Here, we're doing the latter.  We map the shorty signature to container
   * types (which is valid so long as we always do a real expansion of passed
   * arguments and field loads).
   */
  switch(shorty_type) {
    case 'Z' : shorty_type = 'I'; break;
    case 'B' : shorty_type = 'I'; break;
    case 'S' : shorty_type = 'I'; break;
    case 'C' : shorty_type = 'I'; break;
    default: break;
  }
  return shorty_type;
}

llvm::FunctionType* MirConverter::GetFunctionType() {

  // Get return type
  llvm::Type* ret_type = irb_.getJType(RemapShorty(cu_->shorty[0]));

  // Get argument type
  std::vector< llvm::Type*> args_type;

  // method object
  args_type.push_back(irb_.getJMethodTy());

  // Do we have  a "this"?
  if ((cu_->access_flags & kAccStatic) == 0) {
    args_type.push_back(irb_.getJObjectTy());
  }

  for (uint32_t i = 1; i < strlen(cu_->shorty); ++i) {
    args_type.push_back(irb_.getJType(RemapShorty(cu_->shorty[i])));
  }

  return llvm::FunctionType::get(ret_type, args_type, false);
}

bool MirConverter::CreateFunction() {
  llvm::FunctionType* func_type = GetFunctionType();
  if (func_type == NULL) {
    return false;
  }

  func_ = llvm::Function::Create(func_type,
                                      llvm::Function::InternalLinkage,
                                      symbol_, module_);

  llvm::Function::arg_iterator arg_iter(func_->arg_begin());
  llvm::Function::arg_iterator arg_end(func_->arg_end());

  arg_iter->setName("method");
  ++arg_iter;

  int start_sreg = cu_->num_regs;

  for (unsigned i = 0; arg_iter != arg_end; ++i, ++arg_iter) {
    arg_iter->setName(StringPrintf("v%i_0", start_sreg));
    start_sreg += mir_graph_->reg_location_[start_sreg].wide ? 2 : 1;
  }

  return true;
}

bool MirConverter::CreateLLVMBasicBlock(BasicBlock* bb)
{
  // Skip the exit block
  if ((bb->block_type == kDead) ||(bb->block_type == kExitBlock)) {
    id_to_block_map_.Put(bb->id, NULL);
  } else {
    int offset = bb->start_offset;
    bool entry_block = (bb->block_type == kEntryBlock);
    llvm::BasicBlock* llvm_bb =
        llvm::BasicBlock::Create(*context_, entry_block ? "entry" :
                                 StringPrintf(kLabelFormat, bb->catch_entry ? kCatchBlock :
                                              kNormalBlock, offset, bb->id), func_);
    if (entry_block) {
        entry_bb_ = llvm_bb;
        placeholder_bb_ =
            llvm::BasicBlock::Create(*context_, "placeholder",
                                     func_);
    }
    id_to_block_map_.Put(bb->id, llvm_bb);
  }
  return false;
}
#endif

}  // namespace portable
}  // namespace dex
}  // namespace compiler
}  // namespace art
