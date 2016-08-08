/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright 2014-2016 Intel Corporation
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

#ifndef llvm_dex_builder_hpp
#define llvm_dex_builder_hpp

#include "nodes.h"
#include "optimizing_compiler_stats.h"
#include "driver/compiler_options.h"

#include "llvm/IR/IRBuilder.h"

class LLVMCompiler;
class LLVMShadowFrameBuilder;

class LLVMDexBuilder : public art::HGraphVisitor {
    LLVMCompiler* compiler_;
    LLVMShadowFrameBuilder* shadow_frame_builder_;

    llvm::LLVMContext* context_;
    llvm::Module* module_;
    llvm::IRBuilder<>* builder_;
    llvm::Function* function_;

public:
    LLVMDexBuilder(LLVMCompiler* compiler,
                   art::HGraph* graph,
                   const art::CompilerOptions& compiler_options,
                   art::OptimizingCompilerStats* stats);
    
    ~LLVMDexBuilder();
    
    llvm::Function* GetFunction() {
        return function_;
    }
    
#define DECLARE_VISIT_INSTRUCTION(name, super)                                        \
    void Visit##name(art::H##name* instr);

    FOR_EACH_INSTRUCTION(DECLARE_VISIT_INSTRUCTION)
#undef DECLARE_VISIT_INSTRUCTION

private:
    llvm::Function* GetTestSuspendMethod();
    llvm::Function* GetUnresolvedInvokeTrampoline(art::InvokeType invoke_type);
    llvm::Function* GetResolutionMethodGetterMethod();
    llvm::Function* GetDeliverExceptionMethod();
    llvm::Function* GetThrowDivZeroExceptionMethod();
    llvm::Function* GetFrameAddressMethod();
    llvm::Function* GetThrowStackOverflowExceptionMethod();
    llvm::Function* GetUnresolvedFieldAccessMethod(art::Primitive::Type field_type, bool is_instance, bool is_get);
    llvm::Function* GetThrowNullPointerExceptionMethod();
    llvm::Function* GetIsAssignableMethod();
    llvm::Function* GetThrowArrayStoreExceptionMethod();
    llvm::Function* GetThrowArrayBoundsExceptionMethod();
    llvm::Function* GetInitializeTypeAndVerifyAccessMethod();
    llvm::Function* GetInitializeTypeMethod(bool do_clinit);
    llvm::Function* GetResolveStringMethod();
    llvm::Function* GetThrowClassCastExceptionMethod();
    llvm::Function* GetMonitorOperationMethod(bool is_enter);

    void HandleGoto(art::HInstruction* got, art::HBasicBlock* successor);
    void HandleVirtualOrInterface(art::HInvoke* invoke, uint32_t method_offset);
    void HandleFieldSet(art::HInstruction* instruction, const art::FieldInfo& field_info, bool value_can_be_null);
    void HandleFieldGet(art::HInstruction* instruction, const art::FieldInfo& field_info);
    void HandleBinaryOperation(art::HBinaryOperation* instruction, llvm::Instruction::BinaryOps op);
    
    void GenerateFrame();
    void GenerateShadowMapUpdate(art::HInstruction* instruction);
    void GenerateSuspendCheck(art::HSuspendCheck* instruction, art::HBasicBlock* successor);
    void GenerateUnresolvedFieldAccess(art::HInstruction* field_access, art::Primitive::Type field_type, uint32_t field_index);
    llvm::Value* GenerateMemoryBarrier(art::MemBarrierKind kind);
    void GenerateShadowMapPop();
    void GenerateMarkGCCard(llvm::Value* object, llvm::Value* value, bool value_can_be_null);
    llvm::Value* GenerateGcRootFieldLoad(llvm::Value* ptr);
    void GenerateClassInitializationCheck(llvm::Value* ptr, llvm::BasicBlock* init, llvm::BasicBlock* dont_init);
    llvm::Value* GenerateInitializeType(uint32_t type_idx, bool check_clinit);
    llvm::Value* GenerateReferenceLoad(llvm::Value* ptr);
};

#endif /* llvm_dex_builder_hpp */
