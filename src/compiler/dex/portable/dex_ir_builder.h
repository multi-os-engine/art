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

#ifndef ART_SRC_COMPILER_DEX_PORTABLE_DEX_IR_BUILDER_H_
#define ART_SRC_COMPILER_DEX_PORTABLE_DEX_IR_BUILDER_H_

#include "compiler/llvm/art_ir_builder.h"

#include "mirror/array.h"
#include "thread.h"

namespace art {
namespace compiler {
namespace dex {
namespace portable {

class DexIRBuilder : public Llvm::ArtIRBuilder {
 public:
  // Creates a named basic block with dex pc and postfix in the name.
  llvm::BasicBlock* CreateBasicBlockWithDexPC(uint32_t dex_pc, const char* postfix) {
    if (kIsDebugBuild) {
      return CreateBasicBlock(art::StringPrintf("B%04x.%s", dex_pc, postfix));
    } else {
      return CreateBasicBlock();
    }
  }

  // Create a GetElementPtr instruction that is pointing into a mirror::Array at the given index.
  llvm::Value* MirrorArrayGEP(llvm::Value* array_addr, llvm::Value* index_value,
                              Primitive::Type elem_type) {
    int data_offset;
    if (elem_type == Primitive::kPrimLong || elem_type == Primitive::kPrimDouble ||
        (elem_type == Primitive::kPrimNot && sizeof(uint64_t) == sizeof(art::mirror::Object*))) {
      data_offset = art::mirror::Array::DataOffset(sizeof(int64_t)).Int32Value();
    } else {
      data_offset = art::mirror::Array::DataOffset(sizeof(int32_t)).Int32Value();
    }
    llvm::Constant* data_offset_value = getInt32(data_offset);
    llvm::Type* elem_jtype = GetJavaType(elem_type);
    // Convert the ptr to an int, add the offset, then convert back to the given type of ptr.
    llvm::Type* intptr_type = getInt32Ty();
    llvm::Value* base_addr = CreatePtrToInt(array_addr, intptr_type);
    // TODO: set no signed wrap (nsw) ?
    base_addr = CreateAdd(base_addr, data_offset_value);
    llvm::Value* array_data_addr = CreateIntToPtr(base_addr, elem_jtype->getPointerTo());
    return CreateGEP(array_data_addr, index_value);
  }

  void CreateNullCheck(uint32_t dex_pc, llvm::Value* object) {
    AssertShadowFrameIsPushedAndVRegsAreFlushed();
    llvm::Value* is_null = CreateICmpEQ(object, GetJavaNull());
    llvm::BasicBlock* bb_cont = CreateBasicBlockWithDexPC(dex_pc, "null_check_cont");
    llvm::BasicBlock* bb_throw = CreateBasicBlockWithDexPC(dex_pc, "null_check_throw");
    CreateCondBr(is_null, bb_throw, bb_cont, mdb_->BranchUnlikely());

    SetInsertPoint(bb_throw);
    rsb_->ThrowNullPointerException();
    CreateBr(GetTryHandlerBlockForDexPc(dex_pc));

    SetInsertPoint(bb_cont);
  }

  llvm::Value* CreateArrayLength(bool ignore_null_check, uint32_t dex_pc, llvm::Value* array) {
    if (!ignore_null_check) {
      CreateNullCheck(dex_pc, array);
    }
    return LoadFromObjectOffset(array,
                                art::mirror::Array::LengthOffset(),
                                getInt32Ty(),
                                mdb_->GetTbaaForArrayLength());
  }

  void CreateBoundCheck(bool ignore_null_check, uint32_t dex_pc, llvm::Value* array,
                        llvm::Value* index) {
    llvm::Value* array_length = CreateArrayLength(ignore_null_check, dex_pc, array);

    AssertShadowFrameIsPushedAndVRegsAreFlushed();

    llvm::Value* is_oob = CreateICmpUGE(index, array_length);

    llvm::BasicBlock* bb_cont = CreateBasicBlockWithDexPC(dex_pc, "bound_check_cont");
    llvm::BasicBlock* bb_throw = CreateBasicBlockWithDexPC(dex_pc, "bound_check_throw");
    CreateCondBr(is_oob, bb_throw, bb_cont, mdb_->BranchUnlikely());

    SetInsertPoint(bb_throw);
    rsb_->ThrowArrayIndexOutOfBoundsException(index, array_length);
    CreateBr(GetTryHandlerBlockForDexPc(dex_pc));

    SetInsertPoint(bb_cont);
  }

  // Terminates the current basic block with a check whether Thread::exception_ is null. A non-null
  // value causes a branch to the try-block associated with the dex_pc or if there are no try
  // blocks a return to the caller. The IR builder continues in the block that has no associated
  // exception.
  void ExceptionCheck(uint32_t dex_pc) {
    AssertShadowFrameIsPushedAndVRegsAreFlushed();
    llvm::Value* exception = rsb_->LoadFromThreadOffset(Thread::ExceptionOffset(),
                                                        GetJavaObjectTy(),
                                                        mdb_->GetTbaaForThread());
    llvm::Value* is_pending = CreateIsNotNull(exception);
    llvm::BasicBlock* bb_cont = CreateBasicBlockWithDexPC(dex_pc, "exception_cont");
    llvm::BasicBlock* bb_exception = GetTryHandlerBlockForDexPc(dex_pc);
    CreateCondBr(is_pending, bb_exception, bb_cont, mdb_->BranchUnlikely());
    SetInsertPoint(bb_cont);
  }

  // Similar to exception check when the fact an exception is pending is passed as a non-zero value.
  void BranchToExceptionIfNonZero(llvm::Value* zero_if_success, uint32_t dex_pc) {
    llvm::Value* is_pending = CreateICmpNE(zero_if_success, getInt32(0));
    llvm::BasicBlock* bb_cont = CreateBasicBlockWithDexPC(dex_pc, "exception_cont");
    llvm::BasicBlock* bb_exception = GetTryHandlerBlockForDexPc(dex_pc);
    CreateCondBr(is_pending, bb_exception, bb_cont, mdb_->BranchUnlikely());
    SetInsertPoint(bb_cont);
  }

  llvm::Value* CreateArrayLoad(bool ignore_null_check, bool ignore_bound_check,
                               uint32_t dex_pc, llvm::Value* array, llvm::Value* index,
                               Primitive::Type elem_type) {
    if (!ignore_bound_check) {
      CreateBoundCheck(ignore_null_check, dex_pc, array, index);
    }
    llvm::Value* elem_ptr = MirrorArrayGEP(array, index, elem_type);
    return CreateLoad(elem_ptr,
                      mdb_->GetTbaaForArrayAccess(elem_type, Primitive::Descriptor(elem_type),
                                                  false));
  }

  void CreateArrayStore(bool ignore_null_check, bool ignore_bound_check,
                        uint32_t dex_pc, llvm::Value* array, llvm::Value* index,
                        llvm::Value* value, Primitive::Type elem_type) {
    if (!ignore_bound_check) {
      CreateBoundCheck(ignore_null_check, dex_pc, array, index);
    }
    llvm::Value* elem_ptr = MirrorArrayGEP(array, index, elem_type);
    CreateStore(elem_ptr, value,
                mdb_->GetTbaaForArrayAccess(elem_type, Primitive::Descriptor(elem_type), false));
  }

  // Load the mirror::Class* at the type index in the dex cache's initialized_static_storage_.
  llvm::Value* LoadFromDexCacheInitializedStaticStorageBase(uint32_t type_idx) {
    llvm::Value* static_storage_dex_cache_addr =
      LoadFieldFromCurMethod(Primitive::kPrimNot, "Ljava/lang/Class;",
                             "dexCacheInitializedStaticStorage",
                             art::mirror::AbstractMethod::DexCacheInitializedStaticStorageOffset(),
                             true);
    llvm::Value* type_idx_value = getInt32(type_idx);
    llvm::Value* type_ptr = MirrorArrayGEP(static_storage_dex_cache_addr, type_idx_value,
                                           Primitive::kPrimNot);
    return CreateLoad(type_ptr,
                      mdb_->GetTbaaForArrayAccess(Primitive::kPrimNot, "Ljava/lang/Class;",
                                                  true));
  }

  // Load an initialized Class for type_index from the current method's dex cache.
  llvm::Value* LoadStaticStorageBase(uint32_t dex_pc, uint32_t type_index) {
    llvm::BasicBlock* check_bb = GetInsertBlock();
    llvm::BasicBlock* bb_cont = CreateBasicBlockWithDexPC(dex_pc, "load_ssb_cont");
    llvm::BasicBlock* init_ssb = CreateBasicBlockWithDexPC(dex_pc, "load_ssb_init_ssb");

    // Flush vregs to shadow frame in case of exception.
    // TODO: should we move the flush into the slow path?
    FlushShadowFrameVRegsAndSetDexPc(dex_pc);

    // Test whether the table's value is initialized.
    llvm::Value* possibly_init_ssb = LoadFromDexCacheInitializedStaticStorageBase(type_index);
    llvm::Value* is_initialized = CreateIsNotNull(possibly_init_ssb);
    CreateCondBr(is_initialized, bb_cont, init_ssb, mdb_->BranchLikely());

    // Do the initialization in the unlikely path.
    SetInsertPoint(init_ssb);
    llvm::Value* ssb_init_call_result = rsb_->InitializeStaticStorage(type_index);
    ExceptionCheck(dex_pc);
    CreateBr(bb_cont);

    // Merge the result of the original load with the result of initialization.
    SetInsertPoint(bb_cont);
    llvm::PHINode* phi = CreatePHI(GetJavaObjectTy(), 2);
    phi->addIncoming(possibly_init_ssb, check_bb);
    phi->addIncoming(ssb_init_call_result, init_ssb);
    return phi;
  }

  llvm::Value* LoadStaticField(Primitive::Type type,
                               const char* type_descriptor,
                               const char* class_name,
                               const char* field_name,
                               bool is_volatile,
                               bool is_const,
                               llvm::Value* static_storage_addr,
                               int field_offset) {
    llvm::Value* static_field_addr = CreateObjectFieldPtr(static_storage_addr,
                                                          MemberOffset(field_offset),
                                                          GetJavaType(type));
    llvm::LoadInst* loaded_value = CreateLoad(static_field_addr,
                                              mdb_->GetTbaaForStaticField(type, type_descriptor,
                                                                          class_name, field_name,
                                                                          is_const));
    llvm::Value* result = SignOrZeroExtendCat1Types(loaded_value, type);
    if (is_volatile) {
      CreateMemoryBarrier(art::kLoadLoad);
    }
    return result;
  }

  llvm::Value* LoadInstanceField(llvm::Value* object, Primitive::Type type,
                                 const char* type_descriptor,
                                 const char* declaring_class_descriptor, const char* field_name,
                                 art::MemberOffset offset,
                                 bool ignore_null_check, uint32_t dex_pc,
                                 bool is_volatile, bool is_const) {
    if (!ignore_null_check) {
      CreateNullCheck(dex_pc, object);
    }
    llvm::MDNode* md_node = mdb_->GetTbaaForInstanceField(type,
                                                          type_descriptor,
                                                          declaring_class_descriptor,
                                                          field_name,
                                                          is_const);
    llvm::Value* field_value = LoadFromObjectOffset(object, offset, GetJavaType(type), md_node);
    field_value = SignOrZeroExtendCat1Types(field_value, type);
    if (is_volatile) {
      CreateMemoryBarrier(art::kLoadLoad);
    }
    return field_value;
  }

  void StoreStaticField(Primitive::Type type,
                        const char* type_descriptor,
                        const char* class_name,
                        const char* field_name,
                        bool is_volatile,
                        bool is_const,
                        llvm::Value* static_storage_addr,
                        llvm::Value* value,
                        MemberOffset field_offset) {
    StoreToObjectOffset(static_storage_addr, field_offset, value, GetJavaType(type),
                        mdb_->GetTbaaForStaticField(type, type_descriptor,
                                            class_name, field_name,
                                            is_const));
    if (type == Primitive::kPrimNot) {  // If put an object, mark the GC card table.
      rsb_->EmitMarkGCCard(value, static_storage_addr);
    }
    if (is_volatile) {
      CreateMemoryBarrier(art::kStoreLoad);
    }
  }

  void StoreInstanceField(Primitive::Type type,
                          const char* type_descriptor,
                          const char* declaring_class_descriptor, const char* field_name,
                          bool ignore_null_check, uint32_t dex_pc,
                          bool is_volatile, bool is_const,
                          llvm::Value* object, llvm::Value* value, art::MemberOffset offset) {
    if (!ignore_null_check) {
      CreateNullCheck(dex_pc, object);
    }
    llvm::MDNode* md_node = mdb_->GetTbaaForInstanceField(type,
                                                          type_descriptor,
                                                          declaring_class_descriptor,
                                                          field_name,
                                                          is_const);
    StoreToObjectOffset(object, offset, value, GetJavaType(type), md_node);
    if (type == Primitive::kPrimNot) {  // If put an object, mark the GC card table.
      rsb_->EmitMarkGCCard(value, object);
    }
    if (is_volatile) {
      CreateMemoryBarrier(art::kStoreLoad);
    }
  }

  // Create a slow-path call to the runtime get static routine for the given type.
  llvm::Value* CallRuntimeGetStatic(Primitive::Type type, uint32_t field_idx) {
    llvm::Value* val = rsb_->GetStatic(type, field_idx);
    return SignOrZeroExtendCat1Types(val, type);
  }

  llvm::Value* CallRuntimeGetInstance(Primitive::Type type, uint32_t field_idx,
                                      llvm::Value* object) {
    llvm::Value* val = rsb_->GetInstance(type, field_idx, object);
    return SignOrZeroExtendCat1Types(val, type);
  }

  // Create a slow-path call to the runtime get static routine for the given type.
  llvm::Value* CallRuntimeSetStatic(llvm::Value* value, Primitive::Type type, uint32_t field_idx) {
    return rsb_->SetStatic(value, type, field_idx);
  }

  llvm::Value* CallRuntimeSetInstance(llvm::Value* object, llvm::Value* value,
                                      Primitive::Type type, uint32_t field_idx) {
    return rsb_->SetInstance(object, value, type, field_idx);
  }

  // Add description of where a switch instruction's dex PC is.
  void AddSwitchNodeMetaData(llvm::SwitchInst* sw, uint32_t table_offset) {
    if (kIsDebugBuild) {
      llvm::MDNode* switch_node = llvm::MDNode::get(getContext(), getInt32(table_offset));
      sw->setMetadata("SwitchTable", switch_node);
    }
  }

  void SuspendCheck(uint32_t dex_pc) {
    llvm::Value* thread_flags =
        rsb_->LoadFromThreadOffset(art::Thread::ThreadFlagsOffset(), getInt16Ty(),
                                   mdb_->GetTbaaForThread());
    llvm::Value* suspend_check = CreateICmpNE(thread_flags, getInt16(0));

    llvm::BasicBlock* bb_suspend = CreateBasicBlockWithDexPC(dex_pc, "suspend");
    llvm::BasicBlock* bb_cont = CreateBasicBlockWithDexPC(dex_pc, "suspend_cont");

    CreateCondBr(suspend_check, bb_suspend, bb_cont, kUnlikely);

    SetInsertPoint(bb_suspend);
    CHECK_NE(dex_pc, art::DexFile::kDexNoIndex);
    FlushShadowFrameVRegsAndSetDexPc(dex_pc);
    rsb_->TestSuspend(GetUnwindBasicBlock(), bb_cont);

    SetInsertPoint(bb_cont);
  }

  llvm::Value* CreateDivModOp(uint32_t dex_pc, bool is_div, bool is_long,
                           llvm::Value* dividend, llvm::Value* divisor) {
    bool need_zero_check = true;
    bool need_neg_min1_check = true;
    if (llvm::isa<llvm::ConstantInt>(divisor)) {
      need_zero_check = down_cast<llvm::ConstantInt*>(divisor)->isZero();
      need_neg_min1_check = down_cast<llvm::ConstantInt*>(divisor)->isMinusOne();
    }
    llvm::Type* type = NULL;
    llvm::Value* zero = NULL;
    if (need_zero_check || need_neg_min1_check) {
      type = is_long ? getInt32Ty() : getInt64Ty();
      zero = llvm::ConstantInt::get(type, 0);
    }
    if (need_zero_check) {
      // Flush vregs to shadow frame in case of exception.
      // TODO: should we move the flush into the slow path?
      FlushShadowFrameVRegsAndSetDexPc(dex_pc);

      // Check for divide by zero.
      llvm::BasicBlock* div0_exception_bb = CreateBasicBlockWithDexPC(dex_pc, "divmod0_throw");
      llvm::BasicBlock* cont_bb = CreateBasicBlockWithDexPC(dex_pc, "divmod0_cont");

      llvm::Value* equal_zero = CreateICmpEQ(divisor, zero);
      CreateCondBr(equal_zero, div0_exception_bb, cont_bb, kUnlikely);

      SetInsertPoint(div0_exception_bb);
      rsb_->ThrowDivZeroArithmeticException();
      CreateBr(GetTryHandlerBlockForDexPc(dex_pc));

      SetInsertPoint(cont_bb);
    }
    llvm::Value* min1_result = NULL;
    llvm::BasicBlock* min1_bb = NULL;
    llvm::BasicBlock* divmod_bb = NULL;
    llvm::BasicBlock* divmod_phi_bb = NULL;
    if (need_neg_min1_check) {
      llvm::Value* neg_one = llvm::ConstantInt::getSigned(type, -1ll);

      min1_bb = CreateBasicBlockWithDexPC(dex_pc, "divmod_min1");
      divmod_bb = CreateBasicBlockWithDexPC(dex_pc, "divmod");
      divmod_phi_bb = CreateBasicBlockWithDexPC(dex_pc, "divmod_phi");

      llvm::Value* is_equal_min1 = CreateICmpEQ(divisor, neg_one);
      CreateCondBr(is_equal_min1, min1_bb, divmod_bb, kUnlikely);

      SetInsertPoint(min1_bb);
      if (is_div) {
        // We can just change from "dividend div -1" to "neg dividend". The sub
        // don't care the sign/unsigned because of two's complement representation.
        // And the behavior is what we want:
        //  -(2^n)        (2^n)-1
        //  MININT  < k <= MAXINT    ->     mul k -1  =  -k
        //  MININT == k              ->     mul k -1  =   k
        //
        // LLVM use sub to represent 'neg'
        min1_result = CreateSub(zero, dividend);
      } else {
        // Everything modulo -1 will be 0.
        min1_result = zero;
      }
      CreateBr(divmod_phi_bb);

      SetInsertPoint(divmod_bb);
    }
    llvm::Value* divmod_result;
    if (is_div) {
      divmod_result = CreateSDiv(dividend, divisor);
    } else {
      divmod_result = CreateSRem(dividend, divisor);
    }
    if (!need_neg_min1_check) {
      return divmod_result;
    } else {
      CreateBr(divmod_phi_bb);

      SetInsertPoint(divmod_phi_bb);
      llvm::PHINode* result = CreatePHI(type, 2);
      result->addIncoming(min1_result, min1_bb);
      result->addIncoming(divmod_result, divmod_bb);
      return result;
    }
  }

  llvm::Value* CreateJavaShl(bool is_long, llvm::Value* src1, llvm::Value* src2) {
    return CreateShl(src1, FixShiftOperand(is_long, src2));
  }

  llvm::Value* CreateJavaLShr(bool is_long, llvm::Value* src1, llvm::Value* src2) {
    return CreateLShr(src1, FixShiftOperand(is_long, src2));
  }

  llvm::Value* CreateJavaAShr(bool is_long, llvm::Value* src1, llvm::Value* src2) {
    return CreateAShr(src1, FixShiftOperand(is_long, src2));
  }

  llvm::Value* CreateFpCompare(llvm::Value* src1, llvm::Value* src2, bool gt_bias) {
    llvm::Value* cmp_eq = CreateFCmpOEQ(src1, src2);
    llvm::Value* cmp_lt = gt_bias ? CreateFCmpOLT(src1, src2) : CreateFCmpULT(src1, src2);
    return CreateTernaryCompareResult(cmp_eq, cmp_lt);
  }

  llvm::Value* CreateLongCompare(llvm::Value* src1, llvm::Value* src2) {
    llvm::Value* cmp_eq = CreateICmpEQ(src1, src2);
    llvm::Value* cmp_lt = CreateICmpSLT(src1, src2);
    return CreateTernaryCompareResult(cmp_eq, cmp_lt);
  }

  llvm::Value* CreateFloatToInt(llvm::Value* src, bool is_double, bool is_long) {
    int64_t min_int = is_long ? 0x8000000000000000LL : 0x80000000L;
    int64_t max_int = is_long ? 0x7fffffffffffffffLL : 0x7fffffffL;
    double min_int_as_float = static_cast<double>(min_int);
    double max_int_as_float = static_cast<double>(max_int);
    llvm::Type* src_type = is_double ? getDoubleTy() : getFloatTy();
    llvm::Type* dst_type = is_long ? getInt64Ty() : getInt32Ty();
    //  if (LIKELY(x > min_int_as_float)) {
    //    if (LIKELY(x < max_int_as_float)) {
    //      return (int32_t)x;
    //    } else {
    //      return max_int;
    //    }
    //  } else {
    //    return (x != x) ? 0 : min_int;
    //  }
    llvm::BasicBlock* ordered_bb = CreateBasicBlock("f2i_ordered");
    llvm::BasicBlock* unordered_or_min_bb = CreateBasicBlock("f2i_unordered_or_min");

    llvm::Constant* lmin_int_as_float = llvm::ConstantFP::get(src_type, min_int_as_float);

    llvm::Value* is_gt_min_int_as_float = CreateFCmpOGT(src, lmin_int_as_float);
    CreateCondBr(is_gt_min_int_as_float, ordered_bb, unordered_or_min_bb, kLikely);

    SetInsertPoint(ordered_bb);
    llvm::BasicBlock* in_range_bb = CreateBasicBlock("f2i_in_range");
    llvm::BasicBlock* f2i_phi_bb = CreateBasicBlock("f2i_phi_bb");

    llvm::Constant* lmax_int_as_float = llvm::ConstantFP::get(src_type, max_int_as_float);

    llvm::Value* is_lt_max_int_as_float = CreateFCmpOLT(src, lmax_int_as_float);
    CreateCondBr(is_lt_max_int_as_float, in_range_bb, f2i_phi_bb, kLikely);

    SetInsertPoint(in_range_bb);
    llvm::Value* in_range_value = CreateFPToSI(src, src_type);
    CreateBr(f2i_phi_bb);

    SetInsertPoint(unordered_or_min_bb);
    llvm::Constant* lmin_int = llvm::ConstantInt::get(dst_type, min_int);
    llvm::Value* is_nan = CreateFCmpUNO(src, src);
    llvm::Value* unordered_or_min = CreateSelect(is_nan, getInt32(0), lmin_int);
    CreateBr(f2i_phi_bb);

    SetInsertPoint(f2i_phi_bb);
    llvm::Constant* lmax_int = llvm::ConstantInt::get(dst_type, max_int);
    llvm::PHINode* result = CreatePHI(dst_type, 3);
    result->addIncoming(lmax_int, ordered_bb);
    result->addIncoming(in_range_value, in_range_bb);
    result->addIncoming(unordered_or_min, unordered_or_min_bb);

    return result;
  }

  void CreateMonitorEnter(bool ignore_null_check, uint32_t dex_pc, llvm::Value* object) {
    // Flush vregs to shadow frame because of blocking due to contention and potential exceptions.
    // TODO: should we move the flush into the slow path?
    FlushShadowFrameVRegsAndSetDexPc(dex_pc);
    if (!ignore_null_check) {
      CreateNullCheck(dex_pc, object);
    }
    rsb_->LockObject(object);
  }

  void CreateMonitorExit(bool ignore_null_check, uint32_t dex_pc, llvm::Value* object) {
    // Flush vregs to shadow frame because of blocking due to contention and potential exceptions.
    // TODO: should we move the flush into the slow path?
    FlushShadowFrameVRegsAndSetDexPc(dex_pc);
    if (!ignore_null_check) {
      CreateNullCheck(dex_pc, object);
    }
    rsb_->UnlockObject(object);
  }

  DexIRBuilder(CompilationUnit* cu)
      : Llvm::ArtIRBuilder(cu->compiler_driver->GetLlvmModuleAtStartOfCompile(),
                           cu->compiler_driver->GetLlvmMdBuilder(),
                           cu->num_dalvik_registers,
                           cu->instruction_set), throw_to_caller_bb_(NULL) {}

 private:
  llvm::Value* FixShiftOperand(bool is_long, llvm::Value* opr) {
    if (is_long) {
      llvm::Value* masked_opr = CreateAnd(opr, 0x3f);
      return CreateZExt(masked_opr, getInt64Ty());
    } else {
      return CreateAnd(opr, 0x1f);
    }
  }

  llvm::Value* CreateTernaryCompareResult(llvm::Value* cmp_eq, llvm::Value* cmp_lt) {
    llvm::Value* pos_or_neg = CreateSelect(cmp_lt, getInt32(-1), getInt32(1));
    return CreateSelect(cmp_eq, getInt32(0), pos_or_neg);
  }

  llvm::BasicBlock* GetTryHandlerBlockForDexPc(uint32_t dex_pc) {
    UNIMPLEMENTED(FATAL) << dex_pc;
    return NULL;
  }


  // Returns the unique basic block within the LLVM function that will return to the calling method
  // to handle the pending exception.
  llvm::BasicBlock* GetUnwindBasicBlock() {
    // Check for an existing unwind basic block.
    if (throw_to_caller_bb_ != NULL) {
      return throw_to_caller_bb_;
    }
    // Create new basic block for unwinding and set as insert point.
    throw_to_caller_bb_ = CreateBasicBlock("exception_unwind");
    llvm::IRBuilderBase::InsertPoint irb_ip_original = saveIP();
    SetInsertPoint(throw_to_caller_bb_);
    // Pop the shadow frame and return.
    PopShadowFrame();
    CreateExceptionReturn();
    // Restore the original insert point.
    restoreIP(irb_ip_original);
    return throw_to_caller_bb_;
  }

  // A basic block that is branched to when an exception should be handled in the caller method.
  llvm::BasicBlock* throw_to_caller_bb_;
};

}  // namespace portable
}  // namespace dex
}  // namespace compiler
}  // namespace art

#endif  // ART_SRC_COMPILER_DEX_PORTABLE_DEX_IR_BUILDER_H_
