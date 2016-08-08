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

#include "llvm_dex_builder.hpp"

#include "art_method-inl.h"
#include "code_generator.h"

#include "llvm/IR/Module.h"
#include "llvm/ADT/TinyPtrVector.h"

#include "llvm_compiler.hpp"
#include "llvm_shadow_frame_builder.hpp"

#define QUICK_ENTRY_POINT_STATIC(pointer_size, x)                                                    \
(pointer_size == 8 ?                                                                                 \
    art::Thread::QuickEntryPointOffset<8>(OFFSETOF_MEMBER(art::QuickEntryPoints, x)).Uint32Value() : \
    art::Thread::QuickEntryPointOffset<4>(OFFSETOF_MEMBER(art::QuickEntryPoints, x)).Uint32Value())

#define QUICK_ENTRY_POINT_DYNAMIC(pointer_size, x)                                                   \
(pointer_size == 8 ?                                                                                 \
    art::GetThreadOffset<8>(x).Uint32Value() :                                                       \
    art::GetThreadOffset<4>(x).Uint32Value())

LLVMDexBuilder::LLVMDexBuilder(
                   LLVMCompiler* compiler,
                   art::HGraph* graph,
                   const art::CompilerOptions& compiler_options,
                   art::OptimizingCompilerStats* stats) :
                   art::HGraphVisitor(graph), compiler_(compiler) {
    auto cc = LLVMCompiler::RetainClassContext();
    context_ = std::get<0>(cc);
    module_ = std::get<1>(cc);
    builder_ = new llvm::IRBuilder<>(*context_);
    
    GenerateFrame();
}

LLVMDexBuilder::~LLVMDexBuilder() {
    delete builder_;
    delete shadow_frame_builder_;
}

void LLVMDexBuilder::GenerateFrame() {
    bool is_64bit = art::Is64BitInstructionSet(compiler_->GetInstructionSet());
    uint32_t stack_end_offset = is_64bit ? art::Thread::StackEndOffset<8>().Uint32Value() :
                                           art::Thread::StackEndOffset<4>().Uint32Value();

    art::ArtMethod* m = GetGraph()->GetArtMethod();
    const char* shorty = m->GetShorty();
    
    // Get return type.
    llvm::Type* return_type = LLVMCompiler::GetLLVMType(context_, shorty[0]);
    
    // Get argument types.
    llvm::SmallVector<llvm::Type*, 4> arg_types {
        llvm::Type::getInt8PtrTy(*context_), // art::Thread* self
        llvm::Type::getInt8PtrTy(*context_), // art::ArtMethod* method
    };
    if (!m->IsStatic()) {
        arg_types.push_back(llvm::Type::getInt8PtrTy(*context_)); // art::Object/art::Class this
    }
    for (size_t i = 1; shorty[i] != 0; i++) {
        arg_types.push_back(LLVMCompiler::GetLLVMType(context_, shorty[i]));
    }
    
    // Create function type.
    llvm::FunctionType* function_type = llvm::FunctionType::get(return_type, arg_types, false);
    
    // Compute funciton name.
    std::string name = JniLongNameWithPrefix(m, "MOE_");
    
    // Create the function itself.
    function_ = llvm::Function::Create(function_type, compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage, name, module_);
    if (compiler_->IsWindows()) {
        function_->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
    }
    
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(*context_, "entry", function_);
    builder_->SetInsertPoint(entry);
    
    auto arg_itr = function_->args().begin();
    auto arg_itr_end = function_->args().end();
    llvm::Value* self = &*arg_itr++;
    llvm::Value* method = &*arg_itr++;
    
    // Build shadow frame.
    llvm::SmallVector<llvm::Value*, 3> args;
    for (; arg_itr != arg_itr_end; arg_itr++) {
        args.push_back(&*arg_itr);
    }
    shadow_frame_builder_ = new LLVMShadowFrameBuilder(context_, builder_, self, method, is_64bit);
    shadow_frame_builder_->buildFromVirtualRegisters(args);
    
    // MOE TODO: Explicit checking of stack overflow has some weak points:
    // - It is not very cheap CPU usage wise.
    // - Calling the builtin llvm.frameaddress forces the function to have a frame in the stack.
    // - Comparing the stack end of the current Thread object with the current frame stack pointer
    // does not guarantee anything, because while the frame may start above of the allowed stack
    // end, the frame can grow over this boundary.
    
    // Do en explicit stack overflow check.
    llvm::Value* stack_end = builder_->CreateConstGEP1_32(self, stack_end_offset);
    stack_end = builder_->CreatePointerCast(stack_end, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
    stack_end = builder_->CreateLoad(stack_end);
    llvm::Function* frame_address_function = GetFrameAddressMethod();
    llvm::Value* frame_address = builder_->CreateCall(frame_address_function, llvm::ConstantInt::get(*context_, llvm::APInt(32, 0, false)));
    llvm::Value* is_overflow = builder_->CreateICmpULT(frame_address, stack_end);
    llvm::BasicBlock* overflow = llvm::BasicBlock::Create(*context_, "overflow", function_);
    llvm::BasicBlock* not_overflow = llvm::BasicBlock::Create(*context_, "not overflow", function_);
    builder_->CreateCondBr(is_overflow, overflow, not_overflow);
    
    // If stack pointer is below stack end, then throw a stack overflow exception.
    builder_->SetInsertPoint(overflow);
    llvm::Function* throw_soe_function = GetThrowStackOverflowExceptionMethod();
    builder_->CreateCall(throw_soe_function, self);
    builder_->CreateBr(not_overflow);
    
    builder_->SetInsertPoint(not_overflow);
}

llvm::Function* LLVMDexBuilder::GetTestSuspendMethod() {
    // The test suspend method might be already constucted.
    const char* const test_suspend_name = "artTestSuspendFromCode";
    llvm::Function* test_suspend = module_->getFunction(test_suspend_name);
    
    // If not, then construct a new one.
    if (test_suspend == nullptr) {
        // Get LLVM type for return type.
        llvm::Type* ret_type = llvm::Type::getVoidTy(*context_);
        
        // Build argument type array.
        llvm::SmallVector<llvm::Type*, 1> arg_types {
          llvm::Type::getInt8PtrTy(*context_)
        };
        
        // Create function type.
        llvm::FunctionType* test_suspend_ty = llvm::FunctionType::get(ret_type, arg_types, false);
    
        // Create function.
        test_suspend =
            llvm::Function::Create(test_suspend_ty,
                                   compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                                   test_suspend_name, module_);
        if (compiler_->IsWindows()) {
          test_suspend->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
        }
    }
    
    return test_suspend;
}

void LLVMDexBuilder::GenerateShadowMapUpdate(art::HInstruction* instruction) {
    art::HEnvironment* env = instruction->GetEnvironment();
    
    uint32_t dex_pc = env->GetDexPc();
    
    llvm::SmallVector<llvm::Value*, 5> vregs;
    for (size_t i = 0, n = env->Size(); i < n; i++) {
        art::HInstruction* vreg_inst = env->GetInstructionAt(i);
        llvm::Value* vreg;
        if (vreg_inst == nullptr) {
            vreg = llvm::ConstantPointerNull::get(llvm::Type::getInt8PtrTy(*context_));
        } else {
            vreg = vreg_inst->GetLLVMValue();
        }
        vregs.push_back(vreg);
    }
    shadow_frame_builder_->update(vregs, dex_pc);
}

void LLVMDexBuilder::GenerateSuspendCheck(art::HSuspendCheck* instruction, art::HBasicBlock* successor) {
    bool is_64bit = art::Is64BitInstructionSet(compiler_->GetInstructionSet());
    int32_t flag_offset = is_64bit ? art::Thread::ThreadFlagsOffset<8>().Int32Value() :
                                     art::Thread::ThreadFlagsOffset<4>().Int32Value();

    llvm::Value* self = &*function_->args().begin();
    
    llvm::Value* flag = builder_->CreateConstGEP1_32(self, flag_offset);
    flag = builder_->CreatePointerCast(flag, llvm::Type::getInt16Ty(*context_)->getPointerTo());
    flag = builder_->CreateLoad(flag);
    
    llvm::Value* cond = builder_->CreateIsNull(flag);
    llvm::BasicBlock* cont = llvm::BasicBlock::Create(*context_, "continue", function_);
    llvm::BasicBlock* susp = llvm::BasicBlock::Create(*context_, "suspend", function_);
    builder_->CreateCondBr(cond, cont, susp);
    
    builder_->SetInsertPoint(susp);
    llvm::Function* test_suspend = GetTestSuspendMethod();
    builder_->CreateCall(test_suspend, llvm::SmallVector<llvm::Value*, 1> { self });
    builder_->CreateBr(cont);
    
    builder_->SetInsertPoint(cont);
    
    instruction->SetLLVMValue(cond);
}

void LLVMDexBuilder::HandleGoto(art::HInstruction* got, art::HBasicBlock* successor) {
    DCHECK(!successor->IsExitBlock());

    art::HBasicBlock* block = got->GetBlock();
    art::HInstruction* previous = got->GetPrevious();

    art::HLoopInformation* info = block->GetLoopInformation();
    if (info != nullptr && info->IsBackEdge(*block) && info->HasSuspendCheck()) {
        GenerateShadowMapUpdate(info->GetSuspendCheck());
        GenerateSuspendCheck(info->GetSuspendCheck(), successor);
        return;
    }

    if (block->IsEntryBlock() && (previous != nullptr) && previous->IsSuspendCheck()) {
        GenerateShadowMapUpdate(previous->AsSuspendCheck());
        GenerateSuspendCheck(previous->AsSuspendCheck(), nullptr);
    }

    llvm::BasicBlock* target = successor->GetLLVMBlock();
    if (target == nullptr) {
        target = llvm::BasicBlock::Create(*context_, "goto target", function_);
        successor->SetLLVMBlock(target);
    }
    got->SetLLVMValue(builder_->CreateBr(target));
    builder_->SetInsertPoint(target);
}

void LLVMDexBuilder::VisitGoto(art::HGoto* got) {
    HandleGoto(got, got->GetSuccessor());
}

void LLVMDexBuilder::VisitTryBoundary(art::HTryBoundary* try_boundary) {
    LOG(art::FATAL) << "Unimplemented!";
    UNREACHABLE();
}

void LLVMDexBuilder::VisitExit(art::HExit* exit ATTRIBUTE_UNUSED) {
    // LLVM exits function flow with return instructions.
}

void LLVMDexBuilder::VisitIf(art::HIf* if_instr) {
    llvm::Value* cond = if_instr->InputAt(0)->GetLLVMValue();
    llvm::BasicBlock* bb_true = if_instr->IfTrueSuccessor()->GetLLVMBlock();
    if (bb_true == nullptr) {
        bb_true = llvm::BasicBlock::Create(*context_, "if true", function_);
        if_instr->IfTrueSuccessor()->SetLLVMBlock(bb_true);
    }
    
    llvm::BasicBlock* bb_false = if_instr->IfFalseSuccessor()->GetLLVMBlock();
    if (bb_false == nullptr) {
        bb_false = llvm::BasicBlock::Create(*context_, "if false", function_);
        if_instr->IfFalseSuccessor()->SetLLVMBlock(bb_true);
    }
    
    if_instr->SetLLVMValue(builder_->CreateCondBr(cond, bb_true, bb_false));
}

void LLVMDexBuilder::VisitDeoptimize(art::HDeoptimize* deoptimize) {
    // We currently don't support deoptimization and probably we never will.
    // Ignoring this instruction may result in having some other instructions
    // (like condition ot other inputs) getting processed needlessly.
}

void LLVMDexBuilder::VisitSelect(art::HSelect* select) {
    llvm::Value* cond = select->GetCondition()->GetLLVMValue();
    llvm::Value* true_value = select->GetTrueValue()->GetLLVMValue();
    llvm::Value* false_value = select->GetFalseValue()->GetLLVMValue();
    select->SetLLVMValue(builder_->CreateSelect(cond, true_value, false_value));
}

void LLVMDexBuilder::VisitNativeDebugInfo(art::HNativeDebugInfo*) {
    LOG(art::FATAL) << "Unimplemented!";
    UNREACHABLE();
}

void LLVMDexBuilder::VisitEqual(art::HEqual* comp) {
    llvm::Value* lhs = comp->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = comp->GetRight()->GetLLVMValue();
    llvm::Value* result;
    if (art::Primitive::IsFloatingPointType(comp->GetLeft()->GetType())) {
        result = builder_->CreateFCmpUEQ(lhs, rhs);
    } else {
        result = builder_->CreateICmpEQ(lhs, rhs);
    }
    comp->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitNotEqual(art::HNotEqual* comp) {
    llvm::Value* lhs = comp->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = comp->GetRight()->GetLLVMValue();
    llvm::Value* result;
    if (art::Primitive::IsFloatingPointType(comp->GetLeft()->GetType())) {
        result = builder_->CreateFCmpUNE(lhs, rhs);
    } else {
        result = builder_->CreateICmpNE(lhs, rhs);
    }
    comp->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitLessThan(art::HLessThan* comp) {
    llvm::Value* lhs = comp->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = comp->GetRight()->GetLLVMValue();
    llvm::Value* result;
    if (art::Primitive::IsFloatingPointType(comp->GetLeft()->GetType())) {
        result = builder_->CreateFCmpULT(lhs, rhs);
    } else {
        result = builder_->CreateICmpSLT(lhs, rhs);
    }
    comp->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitLessThanOrEqual(art::HLessThanOrEqual* comp) {
    llvm::Value* lhs = comp->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = comp->GetRight()->GetLLVMValue();
    llvm::Value* result;
    if (art::Primitive::IsFloatingPointType(comp->GetLeft()->GetType())) {
        result = builder_->CreateFCmpULE(lhs, rhs);
    } else {
        result = builder_->CreateICmpSLE(lhs, rhs);
    }
    comp->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitGreaterThan(art::HGreaterThan* comp) {
    llvm::Value* lhs = comp->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = comp->GetRight()->GetLLVMValue();
    llvm::Value* result;
    if (art::Primitive::IsFloatingPointType(comp->GetLeft()->GetType())) {
        result = builder_->CreateFCmpUGT(lhs, rhs);
    } else {
        result = builder_->CreateICmpSGT(lhs, rhs);
    }
    comp->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitGreaterThanOrEqual(art::HGreaterThanOrEqual* comp) {
    llvm::Value* lhs = comp->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = comp->GetRight()->GetLLVMValue();
    llvm::Value* result;
    if (art::Primitive::IsFloatingPointType(comp->GetLeft()->GetType())) {
        result = builder_->CreateFCmpUGE(lhs, rhs);
    } else {
        result = builder_->CreateICmpSGE(lhs, rhs);
    }
    comp->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitBelow(art::HBelow* comp) {
    llvm::Value* lhs = comp->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = comp->GetRight()->GetLLVMValue();
    comp->SetLLVMValue(builder_->CreateICmpULT(lhs, rhs));
}

void LLVMDexBuilder::VisitBelowOrEqual(art::HBelowOrEqual* comp) {
    llvm::Value* lhs = comp->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = comp->GetRight()->GetLLVMValue();
    comp->SetLLVMValue(builder_->CreateICmpULE(lhs, rhs));
}

void LLVMDexBuilder::VisitAbove(art::HAbove* comp) {
    llvm::Value* lhs = comp->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = comp->GetRight()->GetLLVMValue();
    comp->SetLLVMValue(builder_->CreateICmpUGT(lhs, rhs));
}

void LLVMDexBuilder::VisitAboveOrEqual(art::HAboveOrEqual* comp) {
    llvm::Value* lhs = comp->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = comp->GetRight()->GetLLVMValue();
    comp->SetLLVMValue(builder_->CreateICmpUGE(lhs, rhs));
}

void LLVMDexBuilder::VisitIntConstant(art::HIntConstant* constant ATTRIBUTE_UNUSED) {
    llvm::Value* value = llvm::ConstantInt::get(*context_, llvm::APInt(32, constant->GetValue(), true));
    constant->SetLLVMValue(value);
}

void LLVMDexBuilder::VisitNullConstant(art::HNullConstant* constant ATTRIBUTE_UNUSED) {
    llvm::Value* value = llvm::ConstantPointerNull::get(llvm::Type::getInt8PtrTy(*context_));
    constant->SetLLVMValue(value);
}

void LLVMDexBuilder::VisitLongConstant(art::HLongConstant* constant ATTRIBUTE_UNUSED) {
    llvm::Value* value = llvm::ConstantInt::get(*context_, llvm::APInt(64, constant->GetValue(), true));
    constant->SetLLVMValue(value);
}

void LLVMDexBuilder::VisitFloatConstant(art::HFloatConstant* constant ATTRIBUTE_UNUSED) {
    llvm::Value* value = llvm::ConstantFP::get(*context_, llvm::APFloat(constant->GetValue()));
    constant->SetLLVMValue(value);
}

void LLVMDexBuilder::VisitDoubleConstant(art::HDoubleConstant* constant ATTRIBUTE_UNUSED) {
    llvm::Value* value = llvm::ConstantFP::get(*context_, llvm::APFloat(constant->GetValue()));
    constant->SetLLVMValue(value);
}

llvm::Value* LLVMDexBuilder::GenerateMemoryBarrier(art::MemBarrierKind kind ATTRIBUTE_UNUSED) {
    // TODO: Optimize this!
    return builder_->CreateFence(llvm::SequentiallyConsistent);
}

void LLVMDexBuilder::VisitMemoryBarrier(art::HMemoryBarrier* memory_barrier) {
    memory_barrier->SetLLVMValue(GenerateMemoryBarrier(memory_barrier->GetBarrierKind()));
}

void LLVMDexBuilder::GenerateShadowMapPop() {
    shadow_frame_builder_->relink();
}

void LLVMDexBuilder::VisitReturnVoid(art::HReturnVoid* ret ATTRIBUTE_UNUSED) {
    GenerateShadowMapPop();
    ret->SetLLVMValue(builder_->CreateRetVoid());
}

void LLVMDexBuilder::VisitReturn(art::HReturn* ret ATTRIBUTE_UNUSED) {
    llvm::Value* value = ret->InputAt(0)->GetLLVMValue();
    GenerateShadowMapPop();
    ret->SetLLVMValue(builder_->CreateRet(value));
}

llvm::Function* LLVMDexBuilder::GetUnresolvedInvokeTrampoline(art::InvokeType invoke_type) {
    // Get symbol name for the trampoline.
    const char* trampoline_name;
    switch (invoke_type) {
        case art::kStatic:
            trampoline_name = "artInvokeStaticTrampolineWithAccessCheck";
            break;
        case art::kDirect:
            trampoline_name = "artInvokeDirectTrampolineWithAccessCheck";
            break;
        case art::kVirtual:
            trampoline_name = "artInvokeVirtualTrampolineWithAccessCheck";
            break;
        case art::kSuper:
            trampoline_name = "artInvokeSuperTrampolineWithAccessCheck";
            break;
        case art::kInterface:
            trampoline_name = "artInvokeInterfaceTrampolineWithAccessCheck";
            break;
    }
    
    // The trampoline might be already constructed.
    llvm::Function* trampoline_function = module_->getFunction(trampoline_name);
    
    // If not, then construct a new one.
    if (trampoline_function == nullptr) {
        // Get LLVM type for return type.
        llvm::Type* ret_type = llvm::StructType::get(*context_, llvm::SmallVector<llvm::Type*, 2> {
            llvm::Type::getInt8PtrTy(*context_),
            llvm::Type::getInt8PtrTy(*context_)
        }, false);
    
        // Build argument type array.
        llvm::SmallVector<llvm::Type*, 3> arg_types {
            llvm::Type::getInt32Ty(*context_), // uint32_t method_idx
            llvm::Type::getInt8PtrTy(*context_), // art::Object this
            llvm::Type::getInt8PtrTy(*context_) // art::Thread* self
        };
        
        // Create function type.
        llvm::FunctionType* function_type = llvm::FunctionType::get(ret_type, arg_types, false);
        
        // Create function.
        trampoline_function = llvm::Function::Create(function_type,
                               compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               trampoline_name, module_);
        if (compiler_->IsWindows()) {
            trampoline_function->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
        }
    }
    
    return trampoline_function;
}

llvm::Function* LLVMDexBuilder::GetResolutionMethodGetterMethod() {
    // The resolution method getter might be already constucted.
    const char* const resolution_method_getter_name = "GetResolutionMethod";
    llvm::Function* resolution_method_getter = module_->getFunction(resolution_method_getter_name);
    
    // If not, then construct a new one.
    if (resolution_method_getter == nullptr) {
        // Get LLVM type for return type.
        llvm::Type* ret_type = llvm::Type::getInt8PtrTy(*context_);
    
        // Create function type.
        llvm::FunctionType* function_type = llvm::FunctionType::get(ret_type, false);
        
        // Create function.
        resolution_method_getter = llvm::Function::Create(function_type,
                               compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               resolution_method_getter_name, module_);
        if (compiler_->IsWindows()) {
            resolution_method_getter->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
        }
    }
    
    return resolution_method_getter;
}

llvm::Function* LLVMDexBuilder::GetDeliverExceptionMethod() {
    // The exception deliverer method might be already constucted.
    const char* const deliver_exc_name = "artDeliverPendingExceptionFromCode";
    llvm::Function* deliver_exc = module_->getFunction(deliver_exc_name);
    
    // If not, then construct a new one.
    if (deliver_exc == nullptr) {
        // Get LLVM type for return type.
        llvm::Type* ret_type = llvm::Type::getVoidTy(*context_);
        
        // Build argument type array.
        llvm::SmallVector<llvm::Type*, 1> arg_types {
          llvm::Type::getInt8PtrTy(*context_)
        };
        
        // Create function type.
        llvm::FunctionType* deliver_exc_ty = llvm::FunctionType::get(ret_type, arg_types, false);
    
        // Create function.
        deliver_exc =
            llvm::Function::Create(deliver_exc_ty,
                                   compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                                   deliver_exc_name, module_);
        deliver_exc->setDoesNotReturn();
        if (compiler_->IsWindows()) {
          deliver_exc->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
        }
    }
    
    return deliver_exc;
}

void LLVMDexBuilder::VisitInvokeUnresolved(art::HInvokeUnresolved* invoke) {
    bool is_64bit = art::Is64BitInstructionSet(compiler_->GetInstructionSet());
    bool is_static = invoke->GetOriginalInvokeType() == art::kStatic;

    llvm::Function* trampoline_function = GetUnresolvedInvokeTrampoline(invoke->GetOriginalInvokeType());
    
    llvm::Function* resolution_method_getter = GetResolutionMethodGetterMethod();
    
    llvm::Function* deliver_exc = GetDeliverExceptionMethod();
    
    GenerateShadowMapUpdate(invoke);
    
    // Create the callee function type.
    // First two implicit arguments are Thread* and ArtMethod*.
    llvm::Type* ret_type = LLVMCompiler::GetLLVMType(context_, invoke->GetType());
    llvm::TinyPtrVector<llvm::Type*> arg_types;
    arg_types.push_back(llvm::Type::getInt8PtrTy(*context_));
    arg_types.push_back(llvm::Type::getInt8PtrTy(*context_));
    for (uint8_t i = 0, n = invoke->GetNumberOfArguments(); i < n; i++) {
        arg_types.push_back(LLVMCompiler::GetLLVMType(context_, invoke->InputAt(i)->GetType()));
    }
    llvm::FunctionType *CalleeFT = llvm::FunctionType::get(ret_type, arg_types, false);

    auto arg_itr = function_->args().begin();
    auto arg_itr_end = function_->args().end();
    llvm::Value* self = &*arg_itr++++;

    // Use resolution runtime method for shadow frame.
    llvm::Value* method = builder_->CreateCall(resolution_method_getter);

    // Build temporal shadow frame.
    LLVMShadowFrameBuilder shadow_frame_builder(context_, builder_, self, method, is_64bit);
    llvm::TinyPtrVector<llvm::Value*> references;
    for (; arg_itr != arg_itr_end; arg_itr++) {
        llvm::Value* arg = &*arg_itr;
        if (arg->getType()->isPointerTy()) {
          references.push_back(&*arg_itr);
        }
    }
    shadow_frame_builder.buildFromReferences(references);

    // Fill up the trampoline arguments.
    llvm::SmallVector<llvm::Value*, 3> tramp_args;
    tramp_args.push_back(llvm::ConstantInt::get(*context_, llvm::APInt(32, invoke->GetDexMethodIndex(), false)));
    if (is_static) {
        tramp_args.push_back(llvm::ConstantPointerNull::get(llvm::Type::getInt8PtrTy(*context_)));
    } else {
        tramp_args.push_back(&*arg_itr);
    }
    tramp_args.push_back(self);

    // Call the resolution trampoline.
    llvm::Value* resolution = builder_->CreateCall(trampoline_function, tramp_args);

    // If the resolution failed then it was probably because of an exception.
    llvm::Value* code = builder_->CreateExtractValue(resolution, 1);
    llvm::BasicBlock* fail = llvm::BasicBlock::Create(*context_, "resolution failed", function_);
    llvm::BasicBlock* succ = llvm::BasicBlock::Create(*context_, "resolution succeeded", function_);
    builder_->CreateCondBr(builder_->CreateIsNull(code), fail, succ);
    builder_->SetInsertPoint(fail);
    builder_->CreateCall(deliver_exc, llvm::SmallVector<llvm::Value*, 1>{ self });
    builder_->CreateBr(succ);
    builder_->SetInsertPoint(succ);

    // Use method from the resolution.
    method = builder_->CreateExtractValue(resolution, 0);

    // Fill up the callee arguments.
    // Copy reference arguments from the temporal shadow frame.
    llvm::TinyPtrVector<llvm::Value*> target_args;
    target_args.push_back(self);
    target_args.push_back(method);
    arg_itr = ++++function_->args().begin();
    for (uint32_t ref = 0; arg_itr != arg_itr_end; arg_itr++) {
        llvm::Value* arg = &*arg_itr;
        if (arg_itr->getType()->isPointerTy()) {
            arg = shadow_frame_builder.getVReg(ref);
            ref++;
        }
        target_args.push_back(arg);
    }

    // Re-link the previous shadow frame.
    shadow_frame_builder.relink();

    // Finally, do the actual call.
    llvm::Value* value =
      builder_->CreateCall(builder_->CreatePointerCast(code, CalleeFT->getPointerTo()), target_args);

    invoke->SetLLVMValue(value);
}

void LLVMDexBuilder::VisitInvokeStaticOrDirect(art::HInvokeStaticOrDirect* invoke) {
    art::HInvokeStaticOrDirect::MethodLoadKind load_kind = invoke->GetMethodLoadKind();
    DCHECK(load_kind == art::HInvokeStaticOrDirect::MethodLoadKind::kStringInit ||
           load_kind == art::HInvokeStaticOrDirect::MethodLoadKind::kRecursive ||
           load_kind == art::HInvokeStaticOrDirect::MethodLoadKind::kDexCacheViaMethod);

    size_t pointer_size = art::GetInstructionSetPointerSize(compiler_->GetInstructionSet());

    auto arg_itr = function_->args().begin();
    llvm::Value* self = &*arg_itr++;
    llvm::Value* current_method = &*arg_itr;
    
    GenerateShadowMapUpdate(invoke);
    
    // Get the method we want to call.
    llvm::Value* method;
    switch (load_kind) {
        case art::HInvokeStaticOrDirect::MethodLoadKind::kStringInit: {
            uint32_t method_offset = invoke->GetStringInitOffset();
            
            // Get the string init method from the Thread object.
            method = builder_->CreateConstGEP1_32(self, method_offset);
            method = builder_->CreatePointerCast(method, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
            method = builder_->CreateLoad(method);
            break;
        }
        case art::HInvokeStaticOrDirect::MethodLoadKind::kRecursive: {
            // Use the current method.
            method = current_method;
            break;
        }
        default: {
            uint32_t method_cache_offset = art::ArtMethod::DexCacheResolvedMethodsOffset(pointer_size).Int32Value();
            uint32_t index_in_cache = invoke->GetTargetMethod().dex_method_index;
            uint32_t method_offset = pointer_size * index_in_cache;
            
            // Get the method cache.
            llvm::Value* method_cache = builder_->CreateConstGEP1_32(current_method, method_cache_offset);
            method_cache = builder_->CreatePointerCast(method_cache, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
            method_cache = builder_->CreateLoad(method_cache);

            // Get the method from the cache.
            method = builder_->CreateConstGEP1_32(method_cache, method_offset);
            method = builder_->CreatePointerCast(method, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
            method = builder_->CreateLoad(method);
            break;
        }
    }

    // Get the function pointer.
    llvm::Value* function_pointer;
    switch (invoke->GetCodePtrLocation()) {
        case art::HInvokeStaticOrDirect::CodePtrLocation::kCallSelf: {
            // We call the same method we are building.
            function_pointer = function_;
            break;
        }
        default: {
            uint32_t entry_offset = art::ArtMethod::EntryPointFromQuickCompiledCodeOffset(pointer_size).Int32Value();
            
            // Get LLVM type for return type.
            llvm::Type* ret_type = LLVMCompiler::GetLLVMType(context_, invoke->GetType());
            
            // Build argument type array.
            llvm::TinyPtrVector<llvm::Type*> arg_types;
            arg_types.push_back(llvm::Type::getInt8PtrTy(*context_)); // art::Thread* self
            arg_types.push_back(llvm::Type::getInt8PtrTy(*context_)); // art::ArtMethod* method
            for (uint8_t i = 0, n = invoke->GetNumberOfArguments(); i < n; i++) {
                arg_types.push_back(LLVMCompiler::GetLLVMType(context_, invoke->InputAt(i)->GetType()));
            }
            
            // Create function type.
            llvm::FunctionType* function_type = llvm::FunctionType::get(ret_type, arg_types, false);
            
            // Get the code pointer from the method.
            llvm::Value* entry = builder_->CreateConstGEP1_32(method, entry_offset);
            entry = builder_->CreatePointerCast(entry, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
            entry = builder_->CreateLoad(entry);
            
            // Cast entry point to function pointer.
            function_pointer = builder_->CreatePointerCast(entry, function_type->getPointerTo());
            break;
        }
    }
    
    // Build argument value array.
    llvm::TinyPtrVector<llvm::Value*> arg_values;
    arg_values.push_back(self);
    arg_values.push_back(method);
    for (uint8_t i = 0, n = invoke->GetNumberOfArguments(); i < n; i++) {
        arg_values.push_back(invoke->InputAt(i)->GetLLVMValue());
    }
    
    // And at last, call it.
    llvm::Value* result = builder_->CreateCall(function_pointer, arg_values);
    
    invoke->SetLLVMValue(result);
}

void LLVMDexBuilder::HandleVirtualOrInterface(art::HInvoke* invoke, uint32_t method_offset) {
    size_t pointer_size = art::GetInstructionSetPointerSize(compiler_->GetInstructionSet());
    uint32_t class_offset = art::mirror::Object::ClassOffset().Uint32Value();
    uint32_t entry_offset = art::ArtMethod::EntryPointFromQuickCompiledCodeOffset(pointer_size).Uint32Value();
    
    llvm::Value* self = &*function_->args().begin();
    
    GenerateShadowMapUpdate(invoke);
    
    // Get the receiver object which is always the first input.
    llvm::Value* receiver = invoke->InputAt(0)->GetLLVMValue();

    // Get the class from the receiver.
    llvm::Value* clazz = builder_->CreateConstGEP1_32(receiver, class_offset);
    clazz = builder_->CreatePointerCast(clazz, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
    clazz = builder_->CreateLoad(clazz);

    // From the class get the method to call.
    llvm::Value* method = builder_->CreateConstGEP1_32(clazz, method_offset);
    method = builder_->CreatePointerCast(method, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
    method = builder_->CreateLoad(method);

    // From the method get the entry point.
    llvm::Value* entry = builder_->CreateConstGEP1_32(method, entry_offset);
    entry = builder_->CreatePointerCast(entry, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
    entry = builder_->CreateLoad(entry);
    
    // Get LLVM type for return type.
    llvm::Type* ret_type = LLVMCompiler::GetLLVMType(context_, invoke->GetType());
    
    // Build argument type array.
    llvm::TinyPtrVector<llvm::Type*> arg_types;
    arg_types.push_back(llvm::Type::getInt8PtrTy(*context_)); // art::Thread* self
    arg_types.push_back(llvm::Type::getInt8PtrTy(*context_)); // art::ArtMethod* method
    for (uint8_t i = 0, n = invoke->GetNumberOfArguments(); i < n; i++) {
        arg_types.push_back(LLVMCompiler::GetLLVMType(context_, invoke->InputAt(i)->GetType()));
    }
    
    // Create function type.
    llvm::FunctionType* function_type = llvm::FunctionType::get(ret_type, arg_types, false);
    
    // Build argument value array.
    llvm::TinyPtrVector<llvm::Value*> arg_values;
    arg_values.push_back(self);
    arg_values.push_back(method);
    for (uint8_t i = 0, n = invoke->GetNumberOfArguments(); i < n; i++) {
        arg_values.push_back(invoke->InputAt(i)->GetLLVMValue());
    }
     
    // Cast entry point to function pointer.
    llvm::Value* function_pointer = builder_->CreatePointerCast(entry, function_type->getPointerTo());
    
    // And at last, call it.
    llvm::Value* result = builder_->CreateCall(function_pointer, arg_values);
    
    invoke->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitInvokeVirtual(art::HInvokeVirtual* invoke) {
    size_t pointer_size = art::GetInstructionSetPointerSize(compiler_->GetInstructionSet());
    uint32_t method_offset = art::mirror::Class::EmbeddedVTableEntryOffset(
      invoke->GetVTableIndex(), pointer_size).Uint32Value();
    HandleVirtualOrInterface(invoke, method_offset);
}

void LLVMDexBuilder::VisitInvokeInterface(art::HInvokeInterface* invoke) {
    size_t pointer_size = art::GetInstructionSetPointerSize(compiler_->GetInstructionSet());
    uint32_t method_offset = art::mirror::Class::EmbeddedImTableEntryOffset(
      invoke->GetImtIndex() % art::mirror::Class::kImtSize, pointer_size).Uint32Value();
    HandleVirtualOrInterface(invoke, method_offset);
}

void LLVMDexBuilder::VisitNeg(art::HNeg* neg) {
    llvm::Value* value = neg->GetInput()->GetLLVMValue();
    llvm::Value* result;
    if (art::Primitive::IsFloatingPointType(neg->GetResultType())) {
        result = builder_->CreateFNeg(value);
    } else {
        result = builder_->CreateNeg(value);
    }
    neg->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitTypeConversion(art::HTypeConversion* conversion) {
    art::Primitive::Type result_type = conversion->GetResultType();
    art::Primitive::Type input_type = conversion->GetInputType();

    DCHECK_NE(input_type, result_type);

    llvm::Value* value = conversion->GetInput()->GetLLVMValue();
    llvm::Type* llvm_result_type = LLVMCompiler::GetLLVMType(context_, result_type);
    llvm::Value* result;
    
    if (art::Primitive::IsIntegralType(result_type) && art::Primitive::IsIntegralType(input_type)) {
        int result_size = art::Primitive::ComponentSize(result_type);
        int input_size = art::Primitive::ComponentSize(input_type);
        int min_size = std::min(result_size, input_size);
        if (result_size < input_size) {
            result = builder_->CreateTrunc(value, llvm_result_type);
        } else {
            result = builder_->CreateSExt(value, llvm_result_type);
        }
    } else if (art::Primitive::IsFloatingPointType(result_type) && art::Primitive::IsIntegralType(input_type)) {
        result = builder_->CreateSIToFP(value, llvm_result_type);
    } else if (art::Primitive::IsIntegralType(result_type) && art::Primitive::IsFloatingPointType(input_type)) {
        result = builder_->CreateFPToSI(value, llvm_result_type);
    } else if (art::Primitive::IsFloatingPointType(result_type) && art::Primitive::IsFloatingPointType(input_type)) {
        if (result_type == art::Primitive::kPrimFloat) {
            result = builder_->CreateFPTrunc(value, llvm_result_type);
        } else {
            result = builder_->CreateFPExt(value, llvm_result_type);
        }
    } else {
        LOG(art::FATAL) << "Unexpected or unimplemented type conversion from " << input_type
                        << " to " << result_type;
        UNREACHABLE();
    }
    
    conversion->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitAdd(art::HAdd* add) {
    llvm::Value* lhs = add->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = add->GetRight()->GetLLVMValue();
    llvm::Value* result;
    if (art::Primitive::IsFloatingPointType(add->GetResultType())) {
        result = builder_->CreateFAdd(lhs, rhs);
    } else {
        result = builder_->CreateAdd(lhs, rhs);
    }
    add->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitSub(art::HSub* sub) {
    llvm::Value* lhs = sub->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = sub->GetRight()->GetLLVMValue();
    llvm::Value* result;
    if (art::Primitive::IsFloatingPointType(sub->GetResultType())) {
        result = builder_->CreateFSub(lhs, rhs);
    } else {
        result = builder_->CreateSub(lhs, rhs);
    }
    sub->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitMul(art::HMul* mul) {
    llvm::Value* lhs = mul->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = mul->GetRight()->GetLLVMValue();
    llvm::Value* result;
    if (art::Primitive::IsFloatingPointType(mul->GetResultType())) {
        result = builder_->CreateFMul(lhs, rhs);
    } else {
        result = builder_->CreateMul(lhs, rhs);
    }
    mul->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitDiv(art::HDiv* div) {
    llvm::Value* lhs = div->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = div->GetRight()->GetLLVMValue();
    llvm::Value* result;
    if (art::Primitive::IsFloatingPointType(div->GetResultType())) {
        result = builder_->CreateFDiv(lhs, rhs);
    } else {
        result = builder_->CreateSDiv(lhs, rhs);
    }
    div->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitRem(art::HRem* rem) {
    llvm::Value* lhs = rem->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = rem->GetRight()->GetLLVMValue();
    llvm::Value* result;
    if (art::Primitive::IsFloatingPointType(rem->GetResultType())) {
        result = builder_->CreateFRem(lhs, rhs);
    } else {
        result = builder_->CreateSRem(lhs, rhs);
    }
    rem->SetLLVMValue(result);
}

llvm::Function* LLVMDexBuilder::GetThrowDivZeroExceptionMethod() {
    // The throw div zero method might be already constucted.
    const char* const throw_div_zero_name = "artThrowDivZeroFromCode";
    llvm::Function* throw_div_zero = module_->getFunction(throw_div_zero_name);
    
    // If not, then construct a new one.
    if (throw_div_zero == nullptr) {
        // Get LLVM type for return type.
        llvm::Type* ret_type = llvm::Type::getVoidTy(*context_);
        
        // Build argument type array.
        llvm::SmallVector<llvm::Type*, 1> arg_types {
          llvm::Type::getInt8PtrTy(*context_)
        };
        
        // Create function type.
        llvm::FunctionType* throw_div_zero_ty = llvm::FunctionType::get(ret_type, arg_types, false);
    
        // Create function.
        throw_div_zero =
            llvm::Function::Create(throw_div_zero_ty,
                                   compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                                   throw_div_zero_name, module_);
        throw_div_zero->setDoesNotReturn();
        if (compiler_->IsWindows()) {
          throw_div_zero->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
        }
    }
    
    return throw_div_zero;
}

void LLVMDexBuilder::VisitDivZeroCheck(art::HDivZeroCheck* instruction) {
    llvm::Value* divisor = instruction->InputAt(0)->GetLLVMValue();
    
    llvm::Function* throw_div_zero = GetThrowDivZeroExceptionMethod();
    
    llvm::Value* self = &*function_->args().begin();
    
    llvm::Value* cond = builder_->CreateIsNull(divisor);
    llvm::BasicBlock* fail = llvm::BasicBlock::Create(*context_, "division failed", function_);
    llvm::BasicBlock* succ = llvm::BasicBlock::Create(*context_, "division succeeded", function_);
    llvm::Value* result = builder_->CreateCondBr(cond, fail, succ);
    builder_->SetInsertPoint(fail);
    GenerateShadowMapUpdate(instruction);
    builder_->CreateCall(throw_div_zero, llvm::SmallVector<llvm::Value*, 1>{ self });
    builder_->CreateBr(succ);
    builder_->SetInsertPoint(succ);
    
    instruction->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitRor(art::HRor* ror) {
    // Ror is not exposed in LLVM IR as it is not present in every supported architeture, but
    // fortunately, optimizers will optimize this to a ror instruction where it is possible.
    llvm::Value* lhs = ror->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = ror->GetRight()->GetLLVMValue();
    uint64_t bit_width = ror->GetResultType() == art::Primitive::kPrimLong ? 64 : 32;
    llvm::Value* mask = llvm::ConstantInt::get(*context_, llvm::APInt(bit_width, bit_width - 1, false));
    llvm::Value* a = builder_->CreateAnd(rhs, mask);
    a = builder_->CreateLShr(lhs, a);
    llvm::Value* b = builder_->CreateNeg(rhs);
    b = builder_->CreateAnd(b, mask);
    b = builder_->CreateShl(lhs, b);
    llvm::Value* result = builder_->CreateOr(a, b);
    ror->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitShl(art::HShl* shl) {
    llvm::Value* lhs = shl->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = shl->GetRight()->GetLLVMValue();
    llvm::Value* result = builder_->CreateShl(lhs, rhs);
    shl->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitShr(art::HShr* shr) {
    llvm::Value* lhs = shr->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = shr->GetRight()->GetLLVMValue();
    llvm::Value* result = builder_->CreateAShr(lhs, rhs);
    shr->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitUShr(art::HUShr* ushr) {
    llvm::Value* lhs = ushr->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = ushr->GetRight()->GetLLVMValue();
    llvm::Value* result = builder_->CreateLShr(lhs, rhs);
    ushr->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitNewInstance(art::HNewInstance* instruction) {
    art::QuickEntrypointEnum entry_point = instruction->GetEntrypoint();
    DCHECK(entry_point != art::kQuickAllocObject && entry_point != art::kQuickAllocObjectInitialized);

    size_t pointer_size = art::GetInstructionSetPointerSize(compiler_->GetInstructionSet());
    uint32_t entry_offset = QUICK_ENTRY_POINT_DYNAMIC(pointer_size, entry_point);
   
    // Get the second and third arguments.
    auto arg_itr = function_->args().begin();
    llvm::Value* self = &*arg_itr;
    llvm::Value* method = instruction->InputAt(1)->GetLLVMValue();
   
    GenerateShadowMapUpdate(instruction);
   
    // For the initialized variant we have to pass an art::mirror::Class* as first argument
    // and for the other a type index constant.
    bool initialized = entry_point == art::kQuickAllocObjectInitialized;
   
    // Get return type.
    llvm::Type* return_type = llvm::Type::getInt8PtrTy(*context_);
   
    // Get argument types.
    llvm::SmallVector<llvm::Type*, 3> arg_types {
        initialized ? (llvm::Type*)llvm::Type::getInt8PtrTy(*context_) :
                      (llvm::Type*)llvm::Type::getInt32Ty(*context_), // art::mirror::Class* class / uint32_t type_idx
        llvm::Type::getInt8PtrTy(*context_), // art::ArtMethod* method
        llvm::Type::getInt8PtrTy(*context_) // art::Thread* self
    };
   
    // Create function type.
    llvm::FunctionType* function_type = llvm::FunctionType::get(return_type, arg_types, false);
    
    // Get the entry point from the Thread object.
    llvm::Value* entry = builder_->CreateConstGEP1_32(self, entry_offset);
    entry = builder_->CreatePointerCast(entry, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
    entry = builder_->CreateLoad(entry);
    
    // Cast entry point to function pointer.
    llvm::Value* function_pointer = builder_->CreatePointerCast(entry, function_type->getPointerTo());
    
    // Build argument value array.
    llvm::SmallVector<llvm::Value*, 3> arg_values;
    if (initialized) {
        arg_values.push_back(instruction->InputAt(0)->GetLLVMValue());
    } else {
        arg_values.push_back(llvm::ConstantInt::get(*context_, llvm::APInt(32, instruction->GetTypeIndex(), true)));
    }
    arg_values.push_back(method);
    arg_values.push_back(self);
    
    // And at last, call it.
    llvm::Value* result = builder_->CreateCall(function_pointer, arg_values);
    
    llvm::BasicBlock* error = llvm::BasicBlock::Create(*context_, "error", function_);
    llvm::BasicBlock* done = llvm::BasicBlock::Create(*context_, "done", function_);
    llvm::Value* is_error = builder_->CreateIsNull(result);
    builder_->CreateCondBr(is_error, error, done);
    
    // Deliver pending exception.
    builder_->SetInsertPoint(error);
    llvm::Function* deliver_exc = GetDeliverExceptionMethod();
    builder_->CreateCall(deliver_exc, llvm::SmallVector<llvm::Value*, 1>{ self });
    builder_->CreateBr(done);
    
    builder_->SetInsertPoint(done);
    
    instruction->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitNewArray(art::HNewArray* instruction) {
    art::QuickEntrypointEnum entry_point = instruction->GetEntrypoint();
    DCHECK(entry_point != art::kQuickAllocArray && entry_point != art::kQuickAllocArrayWithAccessCheck);
    
    size_t pointer_size = art::GetInstructionSetPointerSize(compiler_->GetInstructionSet());
    uint32_t entry_offset = QUICK_ENTRY_POINT_DYNAMIC(pointer_size, entry_point);
   
    // Get length.
    llvm::Value* length = instruction->InputAt(0)->GetLLVMValue();
   
    // Get the second and third arguments.
    auto arg_itr = function_->args().begin();
    llvm::Value* self = &*arg_itr;
    llvm::Value* method = instruction->InputAt(1)->GetLLVMValue();
   
    GenerateShadowMapUpdate(instruction);
   
    // Get return type.
    llvm::Type* return_type = llvm::Type::getInt8PtrTy(*context_);
   
    // Get argument types.
    llvm::SmallVector<llvm::Type*, 4> arg_types {
        llvm::Type::getInt32Ty(*context_), // uint32_t type_idx
        llvm::Type::getInt32Ty(*context_), // int32_t component_count
        llvm::Type::getInt8PtrTy(*context_), // art::ArtMethod* method
        llvm::Type::getInt8PtrTy(*context_) // art::Thread* self
    };
   
    // Create function type.
    llvm::FunctionType* function_type = llvm::FunctionType::get(return_type, arg_types, false);
    
    // Get the entry point from the Thread object.
    llvm::Value* entry = builder_->CreateConstGEP1_32(self, entry_offset);
    entry = builder_->CreatePointerCast(entry, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
    entry = builder_->CreateLoad(entry);
    
    // Cast entry point to function pointer.
    llvm::Value* function_pointer = builder_->CreatePointerCast(entry, function_type->getPointerTo());
    
    // Build argument value array.
    llvm::SmallVector<llvm::Value*, 4> arg_values;
    arg_values.push_back(llvm::ConstantInt::get(*context_, llvm::APInt(32, instruction->GetTypeIndex(), true)));
    arg_values.push_back(length);
    arg_values.push_back(method);
    arg_values.push_back(self);
    
    // And at last, call it.
    llvm::Value* result = builder_->CreateCall(function_pointer, arg_values);
    
    llvm::BasicBlock* error = llvm::BasicBlock::Create(*context_, "error", function_);
    llvm::BasicBlock* done = llvm::BasicBlock::Create(*context_, "done", function_);
    llvm::Value* is_error = builder_->CreateIsNull(result);
    builder_->CreateCondBr(is_error, error, done);
    
    // Deliver pending exception.
    builder_->SetInsertPoint(error);
    llvm::Function* deliver_exc = GetDeliverExceptionMethod();
    builder_->CreateCall(deliver_exc, llvm::SmallVector<llvm::Value*, 1>{ self });
    builder_->CreateBr(done);
    
    builder_->SetInsertPoint(done);
    
    instruction->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitParameterValue(art::HParameterValue* instruction ATTRIBUTE_UNUSED) {
    auto arg_itr = function_->args().begin();
    std::advance(arg_itr, instruction->GetIndex() + 2); // Skip self and method arguments.
    instruction->SetLLVMValue(&*arg_itr);
}

llvm::Function* LLVMDexBuilder::GetFrameAddressMethod() {
    // The frame address getter might be already constucted.
    const char* const frame_address_name = "llvm.frameaddress";
    llvm::Function* frame_address = module_->getFunction(frame_address_name);
    
    // If not, then construct a new one.
    if (frame_address == nullptr) {
        // Get LLVM type for return type.
        llvm::Type* ret_type = llvm::Type::getInt8PtrTy(*context_);
    
        // Build argument type array.
        llvm::SmallVector<llvm::Type*, 1> arg_types {
          llvm::Type::getInt32Ty(*context_)
        };
    
        // Create function type.
        llvm::FunctionType* function_type = llvm::FunctionType::get(ret_type, arg_types, false);
        
        // Create function.
        frame_address = llvm::Function::Create(function_type,
                               compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               frame_address_name, module_);
        if (compiler_->IsWindows()) {
            frame_address->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
        }
    }
    
    return frame_address;
}

llvm::Function* LLVMDexBuilder::GetThrowStackOverflowExceptionMethod() {
    // The throw SOE might be already constucted.
    const char* const stack_overflow_name = "artThrowStackOverflowFromCode";
    llvm::Function* stack_overflow = module_->getFunction(stack_overflow_name);
    
    // If not, then construct a new one.
    if (stack_overflow == nullptr) {
        // Get LLVM type for return type.
        llvm::Type* ret_type = llvm::Type::getVoidTy(*context_);
    
        // Build argument type array.
        llvm::SmallVector<llvm::Type*, 1> arg_types {
          llvm::Type::getInt8PtrTy(*context_)
        };
    
        // Create function type.
        llvm::FunctionType* function_type = llvm::FunctionType::get(ret_type, arg_types, false);
        
        // Create function.
        stack_overflow = llvm::Function::Create(function_type,
                               compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               stack_overflow_name, module_);
        stack_overflow->setDoesNotReturn();
        if (compiler_->IsWindows()) {
            stack_overflow->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
        }
    }
    
    return stack_overflow;
}

void LLVMDexBuilder::VisitCurrentMethod(art::HCurrentMethod* instruction ATTRIBUTE_UNUSED) {
    instruction->SetLLVMValue(&*++function_->args().begin());
}

void LLVMDexBuilder::VisitNot(art::HNot* not_) {
    llvm::Value* value = not_->GetInput()->GetLLVMValue();
    llvm::Value* result = builder_->CreateNot(value);
    not_->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitBooleanNot(art::HBooleanNot* bool_not) {
    llvm::Value* value = bool_not->GetInput()->GetLLVMValue();
    llvm::Value* result = builder_->CreateXor(value, llvm::ConstantInt::getTrue(*context_));
    bool_not->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitCompare(art::HCompare* compare) {
    llvm::Value* lhs = compare->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = compare->GetRight()->GetLLVMValue();
    llvm::Value* result;
    if (art::Primitive::IsFloatingPointType(compare->GetResultType())) {
        llvm::Value* is_nan = builder_->CreateFCmpUNO(lhs, rhs);
        llvm::Value* is_gt = builder_->CreateFCmpOGT(lhs, rhs);
        llvm::Value* is_lt = builder_->CreateFCmpOLT(lhs, rhs);
        if (compare->IsGtBias()) {
            llvm::Value* is_one = builder_->CreateOr(is_nan, is_gt);
            llvm::Value* otherwise = builder_->CreateSExt(is_lt, llvm::Type::getInt32Ty(*context_));
            result = builder_->CreateSelect(is_one, llvm::ConstantInt::get(*context_, llvm::APInt(32, 1, true)), otherwise);
        } else {
            llvm::Value* is_one = builder_->CreateOr(is_nan, is_lt);
            llvm::Value* otherwise = builder_->CreateZExt(is_gt, llvm::Type::getInt32Ty(*context_));
            result = builder_->CreateSelect(is_one, llvm::ConstantInt::get(*context_, llvm::APInt(32, -1, true)), otherwise);
        }
    } else {
        llvm::Value* is_lt = builder_->CreateICmpSLT(lhs, rhs);
        llvm::Value* is_gt = builder_->CreateICmpSGT(lhs, rhs);
        llvm::Value* otherwise = builder_->CreateZExt(is_gt, llvm::Type::getInt32Ty(*context_));
        result = builder_->CreateSelect(is_lt, llvm::ConstantInt::get(*context_, llvm::APInt(32, -1, true)), otherwise);
    }
    compare->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitPhi(art::HPhi* instruction ATTRIBUTE_UNUSED) {
    LOG(art::FATAL) << "Unreachable";
    UNREACHABLE();
}

void LLVMDexBuilder::GenerateMarkGCCard(llvm::Value* object, llvm::Value* value, bool value_can_be_null) {
    bool is_64bit = art::Is64BitInstructionSet(compiler_->GetInstructionSet());
    llvm::Type* pointer_int = is_64bit ? llvm::Type::getInt64Ty(*context_) :
                                         llvm::Type::getInt32Ty(*context_);
    int32_t card_table_offset = is_64bit ? art::Thread::CardTableOffset<8>().Int32Value() :
                                           art::Thread::CardTableOffset<4>().Int32Value();

    llvm::BasicBlock* null;
    llvm::BasicBlock* not_null;
    if (value_can_be_null) {
        null = llvm::BasicBlock::Create(*context_, "null", function_);
        not_null = llvm::BasicBlock::Create(*context_, "not null", function_);
    
        llvm::Value* cond = builder_->CreateIsNull(value);
        builder_->CreateCondBr(cond, null, not_null);
        
        builder_->SetInsertPoint(not_null);
    }

    llvm::Value* self = &*function_->args().begin();

    llvm::Value* card = builder_->CreateConstGEP1_32(self, card_table_offset);
    card = builder_->CreatePointerCast(card, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
    card = builder_->CreateLoad(card);

    llvm::Value* object_int = builder_->CreatePtrToInt(object, pointer_int);
    llvm::Value* shifted = builder_->CreateAShr(object_int, art::gc::accounting::CardTable::kCardShift);
    
    llvm::Value* byte = builder_->CreatePtrToInt(card, pointer_int);
    byte = builder_->CreateTrunc(byte, llvm::Type::getInt8Ty(*context_));

    llvm::Value* ptr =  builder_->CreateGEP(card, shifted);
    builder_->CreateStore(byte, ptr);

    if (value_can_be_null) {
        builder_->SetInsertPoint(null);
    }
}

void LLVMDexBuilder::HandleFieldSet(art::HInstruction* instruction, const art::FieldInfo& field_info, bool value_can_be_null) {
    DCHECK(instruction->IsInstanceFieldSet() || instruction->IsStaticFieldSet());
    bool is_volatile = field_info.IsVolatile();
    art::Primitive::Type field_type = field_info.GetFieldType();
    uint32_t offset = field_info.GetFieldOffset().Uint32Value();
    
    if (is_volatile) {
        GenerateMemoryBarrier(art::MemBarrierKind::kAnyStore);
    }
    
    llvm::Value* base = instruction->InputAt(0)->GetLLVMValue();
    llvm::Value* value = instruction->InputAt(1)->GetLLVMValue();
    
    llvm::Value* ptr = builder_->CreateConstGEP1_32(base, offset);
    ptr = builder_->CreatePointerCast(ptr, LLVMCompiler::GetLLVMType(context_, field_type)->getPointerTo());
    builder_->CreateStore(value, ptr);
    
    if (art::CodeGenerator::StoreNeedsWriteBarrier(field_type, instruction->InputAt(1))) {
        GenerateMarkGCCard(base, value, value_can_be_null);
    }

    if (is_volatile) {
        GenerateMemoryBarrier(art::MemBarrierKind::kAnyAny);
    }
    
    // In case a field set used as a right value, then we should propagate the value properly.
    instruction->SetLLVMValue(value);
}

void LLVMDexBuilder::HandleFieldGet(art::HInstruction* instruction, const art::FieldInfo& field_info) {
    DCHECK(instruction->IsInstanceFieldGet() || instruction->IsStaticFieldGet());
    bool is_volatile = field_info.IsVolatile();
    art::Primitive::Type field_type = field_info.GetFieldType();
    uint32_t offset = field_info.GetFieldOffset().Uint32Value();

    llvm::Value* base = instruction->InputAt(0)->GetLLVMValue();

    llvm::Value* ptr = builder_->CreateConstGEP1_32(base, offset);
    ptr = builder_->CreatePointerCast(ptr, LLVMCompiler::GetLLVMType(context_, field_type)->getPointerTo());
    llvm::Value* value = builder_->CreateLoad(ptr);

    if (is_volatile) {
        GenerateMemoryBarrier(art::MemBarrierKind::kLoadAny);
    }

    instruction->SetLLVMValue(value);
}

llvm::Function* LLVMDexBuilder::GetUnresolvedFieldAccessMethod(art::Primitive::Type field_type, bool is_instance, bool is_get) {
    // Get symbol name for the handler method.
    const char* handler_name;
    switch (field_type) {
        case art::Primitive::kPrimBoolean:
            handler_name = is_instance
                ? (is_get ? "artGetBooleanInstanceFromCode" : "artSet8InstanceFromCode")
                : (is_get ? "artGetBooleanStaticFromCode" : "artSet8StaticFromCode");
            break;
        case art::Primitive::kPrimByte:
            handler_name = is_instance
                ? (is_get ? "artGetByteInstanceFromCode" : "artSet8InstanceFromCode")
                : (is_get ? "artGetByteStaticFromCode" : "artSet8StaticFromCode");
            break;
        case art::Primitive::kPrimShort:
            handler_name = is_instance
                  ? (is_get ? "artGetShortInstanceFromCode" : "artSet16InstanceFromCode")
                  : (is_get ? "artGetShortStaticFromCode" : "artSet16StaticFromCode");
            break;
        case art::Primitive::kPrimChar:
            handler_name = is_instance
                ? (is_get ? "artGetCharInstanceFromCode" : "artSet16InstanceFromCode")
                : (is_get ? "artGetCharStaticFromCode" : "artSet16StaticFromCode");
            break;
        case art::Primitive::kPrimInt:
        case art::Primitive::kPrimFloat:
            handler_name = is_instance
                ? (is_get ? "artGet32InstanceFromCode" : "artSet32InstanceFromCode")
                : (is_get ? "artGet32StaticFromCode" : "artSet32StaticFromCode");
            break;
        case art::Primitive::kPrimNot:
            handler_name = is_instance
                ? (is_get ? "artGetObjInstanceFromCode" : "artSetObjInstanceFromCode")
                : (is_get ? "artGetObjStaticFromCode" : "artSetObjStaticFromCode");
            break;
        case art::Primitive::kPrimLong:
        case art::Primitive::kPrimDouble:
            handler_name = is_instance
                ? (is_get ? "artGet64InstanceFromCode" : "artSet64InstanceFromCode")
                : (is_get ? "artGet64StaticFromCode" : "artSet64StaticFromCode");
            break;
        default:
            LOG(art::FATAL) << "Invalid type " << field_type;
            UNREACHABLE();
            break;
    }
    
    // The handler_name method might be already constucted.
    llvm::Function* handler = module_->getFunction(handler_name);
    
    // If not, then construct a new one.
    if (handler == nullptr) {
        // Get type for field.
        // For floating point field types integer types are used.
        llvm::Type* llvm_field_type;
        if (field_type == art::Primitive::kPrimFloat) {
            llvm_field_type = llvm::Type::getInt32Ty(*context_);
        } else if (field_type == art::Primitive::kPrimDouble) {
            llvm_field_type = llvm::Type::getInt64Ty(*context_);
        } else {
            llvm_field_type = LLVMCompiler::GetLLVMType(context_, field_type);
        }
    
        // Get return type.
        llvm::Type* return_type = is_get ? llvm_field_type : llvm::Type::getInt32Ty(*context_);
       
        // Get argument types.
        llvm::SmallVector<llvm::Type*, 5> arg_types;
        arg_types.push_back(llvm::Type::getInt32Ty(*context_)); // uint32_t field_index
        if (is_instance) {
            arg_types.push_back(llvm::Type::getInt8PtrTy(*context_)); // art::Object* obj
        }
        if (!is_get) {
            arg_types.push_back(llvm_field_type); // <llvm_field_type> new_value
        }
        arg_types.push_back(llvm::Type::getInt8PtrTy(*context_)); // art::ArtMethod* referrer
        arg_types.push_back(llvm::Type::getInt8PtrTy(*context_)); // art::Thread* self
       
        // Create function type.
        llvm::FunctionType* function_type = llvm::FunctionType::get(return_type, arg_types, false);
        
        // Create function.
        handler =
            llvm::Function::Create(function_type,
                                   compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                                   handler_name, module_);
        if (compiler_->IsWindows()) {
          handler->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
        }
    }
    
    return handler;
}

void LLVMDexBuilder::GenerateUnresolvedFieldAccess(art::HInstruction* field_access, art::Primitive::Type field_type, uint32_t field_index) {
    bool is_64bit = art::Is64BitInstructionSet(compiler_->GetInstructionSet());
    int32_t exc_offset = is_64bit ? art::Thread::ExceptionOffset<8>().Int32Value() :
                                    art::Thread::ExceptionOffset<4>().Int32Value();
    bool is_instance = field_access->IsUnresolvedInstanceFieldGet() || field_access->IsUnresolvedInstanceFieldSet();
    bool is_get = field_access->IsUnresolvedInstanceFieldGet() || field_access->IsUnresolvedStaticFieldGet();

    llvm::Function* handler = GetUnresolvedFieldAccessMethod(field_type, is_instance, is_get);
   
    // Get the last two arguments.
    auto arg_itr = function_->args().begin();
    llvm::Value* self = &*arg_itr++;
    llvm::Value* method = &*arg_itr;
    
    GenerateShadowMapUpdate(field_access);
    
    // Build argument value array.
    llvm::SmallVector<llvm::Value*, 5> arg_values;
    arg_values.push_back(llvm::ConstantInt::get(*context_, llvm::APInt(32, field_index, true)));
    if (is_instance) {
        arg_values.push_back(field_access->InputAt(0)->GetLLVMValue());
    }
    if (!is_get) {
        llvm::Value* value = field_access->InputAt(1)->GetLLVMValue();
        // For floating point field types we have to bitcast to integer.
        if (field_type == art::Primitive::kPrimFloat) {
            value = builder_->CreateBitCast(value, llvm::Type::getInt32Ty(*context_));
        } else if (field_type == art::Primitive::kPrimDouble) {
            value = builder_->CreateBitCast(value, llvm::Type::getInt64Ty(*context_));
        }
        arg_values.push_back(value);
    }
    arg_values.push_back(method);
    arg_values.push_back(self);
    
    // And at last, call it.
    llvm::Value* result = builder_->CreateCall(handler, arg_values);
    
    llvm::BasicBlock* end = llvm::BasicBlock::Create(*context_, "end", function_);
    llvm::BasicBlock* error = llvm::BasicBlock::Create(*context_, "error", function_);
    if (is_get) {
        // For floating point field types we have to bitcast back to floating point.
        if (field_type == art::Primitive::kPrimFloat) {
            result = builder_->CreateBitCast(result, llvm::Type::getFloatTy(*context_));
        } else if (field_type == art::Primitive::kPrimDouble) {
            result = builder_->CreateBitCast(result, llvm::Type::getDoubleTy(*context_));
        }
    
        llvm::Value* exc = builder_->CreateConstGEP1_32(self, exc_offset);
        exc = builder_->CreatePointerCast(exc, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
        exc = builder_->CreateLoad(exc);
        llvm::Value* is_error = builder_->CreateIsNull(exc);
        builder_->CreateCondBr(is_error, end, error);
        
        field_access->SetLLVMValue(result);
    } else {
        llvm::Value* is_error = builder_->CreateIsNull(result);
        builder_->CreateCondBr(is_error, error, end);
        
        builder_->SetInsertPoint(end);
        
        field_access->SetLLVMValue(field_access->InputAt(1)->GetLLVMValue());
    }
    
    // Deliver pending exception.
    builder_->SetInsertPoint(error);
    llvm::Function* deliver_exc = GetDeliverExceptionMethod();
    builder_->CreateCall(deliver_exc, llvm::SmallVector<llvm::Value*, 1>{ self });
    builder_->CreateBr(end);
    
    builder_->SetInsertPoint(end);
}

void LLVMDexBuilder::VisitInstanceFieldSet(art::HInstanceFieldSet* instruction) {
    HandleFieldSet(instruction, instruction->GetFieldInfo(), instruction->GetValueCanBeNull());
}

void LLVMDexBuilder::VisitStaticFieldSet(art::HStaticFieldSet* instruction) {
    HandleFieldSet(instruction, instruction->GetFieldInfo(), instruction->GetValueCanBeNull());
}

void LLVMDexBuilder::VisitInstanceFieldGet(art::HInstanceFieldGet* instruction) {
    HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void LLVMDexBuilder::VisitStaticFieldGet(art::HStaticFieldGet* instruction) {
    HandleFieldGet(instruction, instruction->GetFieldInfo());
}

void LLVMDexBuilder::VisitUnresolvedInstanceFieldGet(art::HUnresolvedInstanceFieldGet* instruction) {
    GenerateUnresolvedFieldAccess(instruction, instruction->GetFieldType(), instruction->GetFieldIndex());
}

void LLVMDexBuilder::VisitUnresolvedStaticFieldGet(art::HUnresolvedStaticFieldGet* instruction) {
    GenerateUnresolvedFieldAccess(instruction, instruction->GetFieldType(), instruction->GetFieldIndex());
}

void LLVMDexBuilder::VisitUnresolvedInstanceFieldSet(art::HUnresolvedInstanceFieldSet* instruction) {
    GenerateUnresolvedFieldAccess(instruction, instruction->GetFieldType(), instruction->GetFieldIndex());
}

void LLVMDexBuilder::VisitUnresolvedStaticFieldSet(art::HUnresolvedStaticFieldSet* instruction) {
    GenerateUnresolvedFieldAccess(instruction, instruction->GetFieldType(), instruction->GetFieldIndex());
}


llvm::Function* LLVMDexBuilder::GetThrowNullPointerExceptionMethod() {
    // The throw npe method might be already constucted.
    const char* const throw_npe_name = "artThrowNullPointerExceptionFromCode";
    llvm::Function* throw_npe = module_->getFunction(throw_npe_name);
    
    // If not, then construct a new one.
    if (throw_npe == nullptr) {
        // Get LLVM type for return type.
        llvm::Type* ret_type = llvm::Type::getVoidTy(*context_);
        
        // Build argument type array.
        llvm::SmallVector<llvm::Type*, 1> arg_types {
          llvm::Type::getInt8PtrTy(*context_)
        };
        
        // Create function type.
        llvm::FunctionType* throw_npe_ty = llvm::FunctionType::get(ret_type, arg_types, false);
    
        // Create function.
        throw_npe =
            llvm::Function::Create(throw_npe_ty,
                                   compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                                   throw_npe_name, module_);
        throw_npe->setDoesNotReturn();
        if (compiler_->IsWindows()) {
          throw_npe->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
        }
    }
    
    return throw_npe;
}

void LLVMDexBuilder::VisitNullCheck(art::HNullCheck* instruction) {
    llvm::Value* value = instruction->InputAt(0)->GetLLVMValue();
    llvm::Value* cond = builder_->CreateIsNull(value);
    
    llvm::BasicBlock* null = llvm::BasicBlock::Create(*context_, "null", function_);
    llvm::BasicBlock* not_null = llvm::BasicBlock::Create(*context_, "not null", function_);
    builder_->CreateCondBr(cond, null, not_null);
    
    builder_->SetInsertPoint(null);
    llvm::Value* self = &*function_->args().begin();
    llvm::Function* throw_npe = GetThrowNullPointerExceptionMethod();
    GenerateShadowMapUpdate(instruction);
    builder_->CreateCall(throw_npe, llvm::SmallVector<llvm::Value*, 1> { self });
    builder_->CreateBr(not_null);
    
    builder_->SetInsertPoint(not_null);
    
    instruction->SetLLVMValue(value);
}

void LLVMDexBuilder::VisitArrayGet(art::HArrayGet* instruction) {
    bool is_64bit = art::Is64BitInstructionSet(compiler_->GetInstructionSet());
    art::Primitive::Type type = instruction->GetType();
    uint32_t component_size = type == art::Primitive::kPrimNot ? (is_64bit ? 8 : 4) :
                                      art::Primitive::ComponentSize(type);
    uint32_t data_offset = art::mirror::Array::DataOffset(component_size).Uint32Value();
    
    llvm::Value* array = instruction->GetArray()->GetLLVMValue();
    llvm::Value* index = instruction->GetIndex()->GetLLVMValue();
    
    llvm::Value* offset = llvm::ConstantInt::get(*context_, llvm::APInt(64, data_offset, false));
    offset = builder_->CreateAdd(offset, builder_->CreateMul(
        builder_->CreateZExt(index, llvm::Type::getInt64Ty(*context_)),
        llvm::ConstantInt::get(*context_, llvm::APInt(64, component_size, false))
    ));
    
    llvm::Value* ptr = builder_->CreateGEP(array, offset);
    ptr = builder_->CreatePointerCast(ptr, LLVMCompiler::GetLLVMType(context_, type)->getPointerTo());
    
    llvm::Value* result = builder_->CreateLoad(ptr);
    
    instruction->SetLLVMValue(result);
}

llvm::Function* LLVMDexBuilder::GetIsAssignableMethod() {
    // The assignable checker might be already constucted.
    const char* const assignable_checker_name = "artIsAssignableFromCode";
    llvm::Function* assignable_checker = module_->getFunction(assignable_checker_name);
    
    // If not, then construct a new one.
    if (assignable_checker == nullptr) {
        // Get LLVM type for return type.
        llvm::Type* ret_type = llvm::Type::getInt32Ty(*context_);
    
        // Build argument type array.
        llvm::SmallVector<llvm::Type*, 2> arg_types {
          llvm::Type::getInt8PtrTy(*context_),
          llvm::Type::getInt8PtrTy(*context_)
        };
    
        // Create function type.
        llvm::FunctionType* function_type = llvm::FunctionType::get(ret_type, arg_types, false);
        
        // Create function.
        assignable_checker = llvm::Function::Create(function_type,
                               compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               assignable_checker_name, module_);
        if (compiler_->IsWindows()) {
            assignable_checker->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
        }
    }
    
    return assignable_checker;
}

llvm::Function* LLVMDexBuilder::GetThrowArrayStoreExceptionMethod() {
    // The array store exception thrower might be already constucted.
    const char* const throw_array_store_exception_name = "artThrowArrayStoreException";
    llvm::Function* throw_array_store_exception = module_->getFunction(throw_array_store_exception_name);
    
    // If not, then construct a new one.
    if (throw_array_store_exception == nullptr) {
        // Get LLVM type for return type.
        llvm::Type* ret_type = llvm::Type::getVoidTy(*context_);
    
        // Build argument type array.
        llvm::SmallVector<llvm::Type*, 3> arg_types {
          llvm::Type::getInt8PtrTy(*context_),
          llvm::Type::getInt8PtrTy(*context_),
          llvm::Type::getInt8PtrTy(*context_)
        };
    
        // Create function type.
        llvm::FunctionType* function_type = llvm::FunctionType::get(ret_type, arg_types, false);
        
        // Create function.
        throw_array_store_exception = llvm::Function::Create(function_type,
                               compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                               throw_array_store_exception_name, module_);
        throw_array_store_exception->setDoesNotReturn();
        if (compiler_->IsWindows()) {
            throw_array_store_exception->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
        }
    }
    
    return throw_array_store_exception;
}

void LLVMDexBuilder::VisitArraySet(art::HArraySet* instruction) {
    bool is_64bit = art::Is64BitInstructionSet(compiler_->GetInstructionSet());
    art::Primitive::Type type = instruction->GetComponentType();
    uint32_t component_size = type == art::Primitive::kPrimNot ? (is_64bit ? 8 : 4) :
                                      art::Primitive::ComponentSize(type);
    uint32_t data_offset = art::mirror::Array::DataOffset(component_size).Uint32Value();
    bool needs_type_check = instruction->NeedsTypeCheck();
    bool needs_write_barrier = art::CodeGenerator::StoreNeedsWriteBarrier(type, instruction->GetValue());
    bool is_object = type == art::Primitive::kPrimNot;
    
    llvm::Value* array = instruction->GetArray()->GetLLVMValue();
    llvm::Value* index = instruction->GetIndex()->GetLLVMValue();
    llvm::Value* value = instruction->GetIndex()->GetLLVMValue();
    
    llvm::Value* offset = llvm::ConstantInt::get(*context_, llvm::APInt(64, data_offset, false));
    offset = builder_->CreateAdd(offset, builder_->CreateMul(
        builder_->CreateZExt(index, llvm::Type::getInt64Ty(*context_)),
        llvm::ConstantInt::get(*context_, llvm::APInt(64, component_size, false))
    ));
    
    llvm::Value* ptr = builder_->CreateGEP(array, offset);
    ptr = builder_->CreatePointerCast(ptr, LLVMCompiler::GetLLVMType(context_, type)->getPointerTo());
    
    llvm::BasicBlock* done;
    if (is_object) {
        done = llvm::BasicBlock::Create(*context_, "done", function_);
    
        // If the value is null, then do store only and we are done.
        if (instruction->GetValueCanBeNull()) {
            llvm::BasicBlock* null = llvm::BasicBlock::Create(*context_, "null", function_);
            llvm::BasicBlock* not_null = llvm::BasicBlock::Create(*context_, "not null", function_);
        
            llvm::Value* is_null = builder_->CreateIsNull(value);
            builder_->CreateCondBr(is_null, null, not_null);
            
            builder_->SetInsertPoint(null);
            builder_->CreateStore(value, ptr);
            builder_->CreateBr(done);
          
            builder_->SetInsertPoint(not_null);
        }
        
        if (needs_type_check) {
            uint32_t class_offset = art::mirror::Object::ClassOffset().Int32Value();
            uint32_t super_offset = art::mirror::Class::SuperClassOffset().Int32Value();
            uint32_t component_offset = art::mirror::Class::ComponentTypeOffset().Int32Value();
        
            // Get the class of the array.
            llvm::Value* array_class = builder_->CreateConstGEP1_32(array, class_offset);
            array_class = builder_->CreatePointerCast(array_class, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
            array_class = builder_->CreateLoad(array_class);
            
            // From the class get the component class.
            llvm::Value* component_class = builder_->CreateConstGEP1_32(array_class, component_offset);
            component_class = builder_->CreatePointerCast(component_class, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
            component_class = builder_->CreateLoad(component_class);
       
            // Get the class of the value.
            llvm::Value* value_class = builder_->CreateConstGEP1_32(value, class_offset);
            value_class = builder_->CreatePointerCast(value_class, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
            value_class = builder_->CreateLoad(value_class);
       
            // Compare the class of the value and the array component.
            llvm::Value* is_match = builder_->CreateICmpEQ(component_class, value_class);
            llvm::BasicBlock* match = llvm::BasicBlock::Create(*context_, "match", function_);
            llvm::BasicBlock* not_match = llvm::BasicBlock::Create(*context_, "not match", function_);
            llvm::BasicBlock* slow_check = llvm::BasicBlock::Create(*context_, "slow check", function_);
        
            // Do additionaly type checks and for type mismatch do a slow assignability check.
            if (instruction->StaticTypeOfArrayIsObjectArray()) {
                // If the array is an object array and the types are a match, then type checking is done.
                builder_->CreateCondBr(is_match, match, not_match);
                
                // Otherwise we can optimize by allowing every kind of value for Object arrays.
                builder_->SetInsertPoint(not_match);
                llvm::Value* super_class = builder_->CreateConstGEP1_32(component_class, super_offset);
                super_class = builder_->CreatePointerCast(super_class, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
                super_class = builder_->CreateLoad(super_class);
                
                llvm::Value* is_root_class = builder_->CreateIsNull(super_class);
                builder_->CreateCondBr(is_root_class, match, slow_check);
            } else {
                builder_->CreateCondBr(is_match, match, slow_check);
            }
            
            // Types are not matching, do the assignability check.
            builder_->SetInsertPoint(slow_check);
            GenerateShadowMapUpdate(instruction);
            llvm::Function* assignable_function = GetIsAssignableMethod();
            llvm::SmallVector<llvm::Value*, 2> checker_args {
                value_class,
                component_class
            };
            llvm::Value* is_assignable = builder_->CreateCall(assignable_function, checker_args);
            is_assignable = builder_->CreateIsNotNull(is_assignable);
            
            // In case of a mismatch throw an error.
            llvm::BasicBlock* exception = llvm::BasicBlock::Create(*context_, "exception", function_);
            builder_->CreateCondBr(is_assignable, match, exception);
            builder_->SetInsertPoint(exception);
            llvm::Function* throw_function = GetThrowArrayStoreExceptionMethod();
            llvm::Value* self = &*function_->args().begin();
            llvm::SmallVector<llvm::Value*, 2> throw_args {
                array,
                value,
                self
            };
            builder_->CreateCall(throw_function, throw_args);
            
            builder_->SetInsertPoint(match);
        }
    }
    
    // And at last, store the value.
    builder_->CreateStore(value, ptr);
    
    if (is_object) {
        GenerateMarkGCCard(array, value, instruction->GetValueCanBeNull());
        builder_->SetInsertPoint(done);
    }
    
    // In case an array set used as a right value, then we should propagate the value properly.
    instruction->SetLLVMValue(value);
}

void LLVMDexBuilder::VisitArrayLength(art::HArrayLength* instruction) {
    uint32_t length_offset = art::CodeGenerator::GetArrayLengthOffset(instruction);
    
    llvm::Value* array = instruction->InputAt(0)->GetLLVMValue();
    
    llvm::Value* length = builder_->CreateConstGEP1_32(array, length_offset);
    length = builder_->CreatePointerCast(length, llvm::Type::getInt32Ty(*context_)->getPointerTo());
    length = builder_->CreateLoad(length);
    
    instruction->SetLLVMValue(length);
}

llvm::Function* LLVMDexBuilder::GetThrowArrayBoundsExceptionMethod() {
    // The throw array bounds method might be already constucted.
    const char* const throw_array_bounds_exception_name = "artThrowArrayBoundsFromCode";
    llvm::Function* throw_array_bounds_exception = module_->getFunction(throw_array_bounds_exception_name);
    
    // If not, then construct a new one.
    if (throw_array_bounds_exception == nullptr) {
        // Get LLVM type for return type.
        llvm::Type* ret_type = llvm::Type::getVoidTy(*context_);
        
        // Build argument type array.
        llvm::SmallVector<llvm::Type*, 3> arg_types {
          llvm::Type::getInt32Ty(*context_),
          llvm::Type::getInt32Ty(*context_),
          llvm::Type::getInt8PtrTy(*context_)
        };
        
        // Create function type.
        llvm::FunctionType* throw_array_bounds_exception_ty = llvm::FunctionType::get(ret_type, arg_types, false);
    
        // Create function.
        throw_array_bounds_exception =
            llvm::Function::Create(throw_array_bounds_exception_ty,
                                   compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                                   throw_array_bounds_exception_name, module_);
        throw_array_bounds_exception->setDoesNotReturn();
        if (compiler_->IsWindows()) {
          throw_array_bounds_exception->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
        }
    }
    
    return throw_array_bounds_exception;
}

void LLVMDexBuilder::VisitBoundsCheck(art::HBoundsCheck* instruction) {
    llvm::Value* index = instruction->InputAt(0)->GetLLVMValue();
    llvm::Value* length = instruction->InputAt(1)->GetLLVMValue();
    
    llvm::Value* cond = builder_->CreateICmpUGT(length, index);
    llvm::BasicBlock* success = llvm::BasicBlock::Create(*context_, "success", function_);
    llvm::BasicBlock* fail = llvm::BasicBlock::Create(*context_, "fail", function_);
    builder_->CreateCondBr(cond, success, fail);
    
    builder_->SetInsertPoint(fail);
    llvm::Function* throw_array_bounds = GetThrowArrayBoundsExceptionMethod();
    llvm::Value* self = &*function_->args().begin();
    GenerateShadowMapUpdate(instruction);
    builder_->CreateCall(throw_array_bounds, llvm::SmallVector<llvm::Value*, 3> {
        index,
        length,
        self
    });
    builder_->CreateBr(success);
    
    builder_->SetInsertPoint(success);
    
    instruction->SetLLVMValue(cond);
}

void LLVMDexBuilder::VisitParallelMove(art::HParallelMove* instruction) {
    LOG(art::FATAL) << "Unreachable";
    UNREACHABLE();
}

void LLVMDexBuilder::VisitSuspendCheck(art::HSuspendCheck* instruction) {
    art::HBasicBlock* block = instruction->GetBlock();
    if (block->GetLoopInformation() != nullptr) {
        DCHECK(block->GetLoopInformation()->GetSuspendCheck() == instruction);
        // The back edge will generate the suspend check.
        return;
    }
    if (block->IsEntryBlock() && instruction->GetNext()->IsGoto()) {
        // The goto will generate the suspend check.
        return;
    }
    GenerateShadowMapUpdate(instruction);
    GenerateSuspendCheck(instruction, nullptr);
}

llvm::Function* LLVMDexBuilder::GetInitializeTypeAndVerifyAccessMethod() {
    // The init type method might be already constucted.
    const char* const init_type_name = "artInitializeTypeAndVerifyAccessFromCode";
    llvm::Function* init_type = module_->getFunction(init_type_name);
    
    // If not, then construct a new one.
    if (init_type == nullptr) {
        // Get LLVM type for return type.
        llvm::Type* ret_type = llvm::Type::getInt8PtrTy(*context_);
        
        // Build argument type array.
        llvm::SmallVector<llvm::Type*, 2> arg_types {
          llvm::Type::getInt32Ty(*context_),
          llvm::Type::getInt8PtrTy(*context_)
        };
        
        // Create function type.
        llvm::FunctionType* init_type_ty = llvm::FunctionType::get(ret_type, arg_types, false);
    
        // Create function.
        init_type =
            llvm::Function::Create(init_type_ty,
                                   compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                                   init_type_name, module_);
        if (compiler_->IsWindows()) {
          init_type->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
        }
    }
    
    return init_type;
}

llvm::Value* LLVMDexBuilder::GenerateGcRootFieldLoad(llvm::Value* ptr) {
    ptr = builder_->CreatePointerCast(ptr, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
    return builder_->CreateLoad(ptr);
}

llvm::Function* LLVMDexBuilder::GetInitializeTypeMethod(bool do_clinit) {
    // Get symbol name for the initializer method.
    const char* init_type_name = do_clinit ? "artInitializeStaticStorageFromCode" : "artInitializeTypeFromCode";

    // The initializer method might be already constucted.
    llvm::Function* init_type = module_->getFunction(init_type_name);
    
    // If not, then construct a new one.
    if (init_type == nullptr) {
        // Get LLVM type for return type.
        llvm::Type* ret_type = llvm::Type::getInt8PtrTy(*context_);
        
        // Build argument type array.
        llvm::SmallVector<llvm::Type*, 2> arg_types {
          llvm::Type::getInt32Ty(*context_),
          llvm::Type::getInt8PtrTy(*context_)
        };
        
        // Create function type.
        llvm::FunctionType* init_type_ty = llvm::FunctionType::get(ret_type, arg_types, false);
    
        // Create function.
        init_type =
            llvm::Function::Create(init_type_ty,
                                   compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                                   init_type_name, module_);
        if (compiler_->IsWindows()) {
          init_type->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
        }
    }
    
    return init_type;
}

void LLVMDexBuilder::GenerateClassInitializationCheck(llvm::Value* ptr, llvm::BasicBlock* init, llvm::BasicBlock* dont_init) {
    int32_t status_offset = art::mirror::Class::StatusOffset().Int32Value();
    
    llvm::Value* status = builder_->CreateConstGEP1_32(ptr, status_offset);
    status = builder_->CreatePointerCast(status, llvm::Type::getInt32PtrTy(*context_));
    status = builder_->CreateLoad(ptr);
    
    llvm::Value* initialized_flag = llvm::ConstantInt::get(*context_, llvm::APInt(32, art::mirror::Class::kStatusInitialized, false));
    llvm::Value* cond = builder_->CreateICmpULT(status, initialized_flag);
    builder_->CreateCondBr(cond, init, dont_init);
}

llvm::Value* LLVMDexBuilder::GenerateInitializeType(uint32_t type_idx, bool check_clinit) {
    llvm::Function* init_type = GetInitializeTypeMethod(check_clinit);
    llvm::Value* self = &*function_->args().begin();
    llvm::Value* result = builder_->CreateCall(init_type, llvm::SmallVector<llvm::Value*, 2> {
        llvm::ConstantInt::get(*context_, llvm::APInt(32, type_idx, true)),
        self
    });
    
    llvm::BasicBlock* error = llvm::BasicBlock::Create(*context_, "error", function_);
    llvm::BasicBlock* done = llvm::BasicBlock::Create(*context_, "done", function_);
    llvm::Value* is_error = builder_->CreateIsNull(result);
    builder_->CreateCondBr(is_error, error, done);
    
    // Deliver pending exception.
    builder_->SetInsertPoint(error);
    llvm::Function* deliver_exc = GetDeliverExceptionMethod();
    builder_->CreateCall(deliver_exc, llvm::SmallVector<llvm::Value*, 1>{ self });
    builder_->CreateBr(done);
    
    builder_->SetInsertPoint(done);
    
    return result;
}

void LLVMDexBuilder::VisitLoadClass(art::HLoadClass* cls) {
    if (cls->NeedsAccessCheck()) {
        llvm::Function* init_type = GetInitializeTypeAndVerifyAccessMethod();
        llvm::Value* self = &*function_->args().begin();
        GenerateShadowMapUpdate(cls);
        llvm::Value* result = builder_->CreateCall(init_type, llvm::SmallVector<llvm::Value*, 2> {
            llvm::ConstantInt::get(*context_, llvm::APInt(32, cls->GetTypeIndex(), true)),
            self
        });
        
        llvm::BasicBlock* error = llvm::BasicBlock::Create(*context_, "error", function_);
        llvm::BasicBlock* done = llvm::BasicBlock::Create(*context_, "done", function_);
        llvm::Value* is_error = builder_->CreateIsNull(result);
        builder_->CreateCondBr(is_error, error, done);
        
        // Deliver pending exception.
        builder_->SetInsertPoint(error);
        llvm::Function* deliver_exc = GetDeliverExceptionMethod();
        builder_->CreateCall(deliver_exc, llvm::SmallVector<llvm::Value*, 1>{ self });
        builder_->CreateBr(done);
        
        builder_->SetInsertPoint(done);
        
        cls->SetLLVMValue(result);
        return;
    }
    
    llvm::Value* current_method = cls->InputAt(0)->GetLLVMValue();
    
    if (cls->IsReferrersClass()) {
        DCHECK(!cls->CanCallRuntime());
        DCHECK(!cls->MustGenerateClinitCheck());
        
        int32_t class_offset = art::ArtMethod::DeclaringClassOffset().Int32Value();

        llvm::Value* root_ptr = builder_->CreateConstGEP1_32(current_method, class_offset);
        
        llvm::Value* result = GenerateGcRootFieldLoad(root_ptr);
        cls->SetLLVMValue(result);
    } else {
        size_t pointer_size = art::GetInstructionSetPointerSize(compiler_->GetInstructionSet());
        int32_t cache_offset = art::ArtMethod::DexCacheResolvedTypesOffset(pointer_size).Int32Value();
        size_t type_offset = art::CodeGenerator::GetCacheOffset(cls->GetTypeIndex());
        
        llvm::Value* cache = builder_->CreateConstGEP1_32(current_method, cache_offset);
        cache = builder_->CreatePointerCast(cache, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
        cache = builder_->CreateLoad(cache);
        
        llvm::Value* type_ptr = builder_->CreateConstGEP1_32(cache, type_offset);
        llvm::Value* result = GenerateGcRootFieldLoad(type_ptr);
        
        if (!cls->IsInDexCache() || cls->MustGenerateClinitCheck()) {
            DCHECK(cls->CanCallRuntime());
    
            llvm::BasicBlock* orig_edge = builder_->GetInsertBlock();
            
            llvm::BasicBlock* init = llvm::BasicBlock::Create(*context_, "init", function_);
            llvm::BasicBlock* done = llvm::BasicBlock::Create(*context_, "done", function_);
    
            if (!cls->IsInDexCache()) {
                llvm::BasicBlock* dont_init = llvm::BasicBlock::Create(*context_, "dont init", function_);

                llvm::Value* is_null = builder_->CreateIsNull(result);
                builder_->CreateCondBr(is_null, init, dont_init);
                
                builder_->SetInsertPoint(dont_init);
            }

            if (cls->MustGenerateClinitCheck()) {
                GenerateClassInitializationCheck(result, init, done);
            } else {
                builder_->CreateBr(done);
            }
            
            builder_->SetInsertPoint(init);
            GenerateShadowMapUpdate(cls);
            llvm::Value* result2 = GenerateInitializeType(cls->GetTypeIndex(), cls->MustGenerateClinitCheck());
            builder_->CreateBr(done);
        
            builder_->SetInsertPoint(done);
            
            llvm::PHINode* phi = builder_->CreatePHI(llvm::Type::getInt8PtrTy(*context_), 2);
            phi->addIncoming(result, orig_edge);
            phi->addIncoming(result2, init);
            result = phi;
        }
        
        cls->SetLLVMValue(result);
    }
}

void LLVMDexBuilder::VisitClinitCheck(art::HClinitCheck* check) {
    llvm::BasicBlock* init = llvm::BasicBlock::Create(*context_, "init", function_);
    llvm::BasicBlock* done = llvm::BasicBlock::Create(*context_, "done", function_);
    
    llvm::BasicBlock* orig_edge = builder_->GetInsertBlock();
    art::HLoadClass* cls = check->GetLoadClass();
    llvm::Value* result = cls->GetLLVMValue();
    
    GenerateClassInitializationCheck(result, init, done);
    builder_->SetInsertPoint(init);
    GenerateShadowMapUpdate(check);
    llvm::Value* result2 = GenerateInitializeType(cls->GetTypeIndex(), cls->MustGenerateClinitCheck());
    builder_->CreateBr(done);
    
    builder_->SetInsertPoint(done);
    
    llvm::PHINode* phi = builder_->CreatePHI(llvm::Type::getInt8PtrTy(*context_), 2);
    phi->addIncoming(result, orig_edge);
    phi->addIncoming(result2, init);
    
    check->SetLLVMValue(phi);
}

llvm::Function* LLVMDexBuilder::GetResolveStringMethod() {
    // The string resolver method might be already constucted.
    const char* const resolve_string_name = "artResolveStringFromCode";
    llvm::Function* resolve_string = module_->getFunction(resolve_string_name);
    
    // If not, then construct a new one.
    if (resolve_string == nullptr) {
        // Get LLVM type for return type.
        llvm::Type* ret_type = llvm::Type::getInt8PtrTy(*context_);
        
        // Build argument type array.
        llvm::SmallVector<llvm::Type*, 2> arg_types {
          llvm::Type::getInt32Ty(*context_),
          llvm::Type::getInt8PtrTy(*context_)
        };
        
        // Create function type.
        llvm::FunctionType* resolve_string_ty = llvm::FunctionType::get(ret_type, arg_types, false);
    
        // Create function.
        resolve_string =
            llvm::Function::Create(resolve_string_ty,
                                   compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                                   resolve_string_name, module_);
        if (compiler_->IsWindows()) {
          resolve_string->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
        }
    }
    
    return resolve_string;
}

void LLVMDexBuilder::VisitLoadString(art::HLoadString* load) {
    DCHECK(load->GetLoadKind() == art::HLoadString::LoadKind::kDexCacheViaMethod);
    
    int32_t class_offset = art::ArtMethod::DeclaringClassOffset().Int32Value();
    uint32_t cache_offset = art::mirror::Class::DexCacheStringsOffset().Uint32Value();
    size_t string_offset = art::CodeGenerator::GetCacheOffset(load->GetStringIndex());
    
    llvm::Value* method = load->InputAt(0)->GetLLVMValue();
    
    // Load declaring class from the current method.
    llvm::Value* clazz = GenerateGcRootFieldLoad(builder_->CreateConstGEP1_32(method, class_offset));
    
    // From declaring class get the string cache.
    llvm::Value* cache = builder_->CreateConstGEP1_32(clazz, cache_offset);
    cache = builder_->CreatePointerCast(cache, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
    cache = builder_->CreateLoad(cache);
    
    // From cache load the string.
    llvm::Value* result = GenerateGcRootFieldLoad(builder_->CreateConstGEP1_32(cache, string_offset));

    // Resolve string if the string cache does not contain what we need.
    if (!load->IsInDexCache()) {
        llvm::BasicBlock* orig_edge = builder_->GetInsertBlock();
    
        llvm::Value* cond = builder_->CreateIsNull(result);
        llvm::BasicBlock* null = llvm::BasicBlock::Create(*context_, "null", function_);
        llvm::BasicBlock* not_null = llvm::BasicBlock::Create(*context_, "not null", function_);
        builder_->CreateCondBr(cond, null, not_null);
        
        builder_->SetInsertPoint(null);
        llvm::Function* resolve_string = GetResolveStringMethod();
        llvm::Value* self = &*function_->args().begin();
        GenerateShadowMapUpdate(load);
        llvm::Value* result2 = builder_->CreateCall(resolve_string, llvm::SmallVector<llvm::Value*, 2> {
            llvm::ConstantInt::get(*context_, llvm::APInt(32, load->GetStringIndex(), false)),
            self
        });
        
        llvm::BasicBlock* error = llvm::BasicBlock::Create(*context_, "error", function_);
        llvm::Value* is_error = builder_->CreateIsNull(result2);
        builder_->CreateCondBr(is_error, error, not_null);
        
        // Deliver pending exception.
        builder_->SetInsertPoint(error);
        llvm::Function* deliver_exc = GetDeliverExceptionMethod();
        builder_->CreateCall(deliver_exc, llvm::SmallVector<llvm::Value*, 1>{ self });
        builder_->CreateBr(not_null);

        builder_->SetInsertPoint(not_null);
        
        llvm::PHINode* phi = builder_->CreatePHI(llvm::Type::getInt8PtrTy(*context_), 2);
        phi->addIncoming(result, orig_edge);
        phi->addIncoming(result2, null);
        result = phi;
    }
    
    load->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitLoadException(art::HLoadException* load) {
    bool is_64bit = art::Is64BitInstructionSet(compiler_->GetInstructionSet());
    int32_t exc_offset = is_64bit ? art::Thread::ExceptionOffset<8>().Int32Value() :
                                    art::Thread::ExceptionOffset<4>().Int32Value();
    
    llvm::Value* self = &*function_->args().begin();
    
    llvm::Value* exc = builder_->CreateConstGEP1_32(self, exc_offset);
    exc = builder_->CreatePointerCast(exc, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
    exc = builder_->CreateLoad(exc);
    
    load->SetLLVMValue(exc);
}

void LLVMDexBuilder::VisitClearException(art::HClearException* clear ATTRIBUTE_UNUSED) {
    bool is_64bit = art::Is64BitInstructionSet(compiler_->GetInstructionSet());
    int32_t exc_offset = is_64bit ? art::Thread::ExceptionOffset<8>().Int32Value() :
                                    art::Thread::ExceptionOffset<4>().Int32Value();
    
    llvm::Value* self = &*function_->args().begin();

    llvm::Value* exc_ptr = builder_->CreateConstGEP1_32(self, exc_offset);
    exc_ptr = builder_->CreatePointerCast(exc_ptr, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
    builder_->CreateStore(llvm::ConstantPointerNull::get(llvm::Type::getInt8PtrTy(*context_)), exc_ptr);
}

void LLVMDexBuilder::VisitThrow(art::HThrow* instruction) {
    LOG(art::FATAL) << "Unimplemented!";
    UNREACHABLE();
}

llvm::Value* LLVMDexBuilder::GenerateReferenceLoad(llvm::Value* ptr) {
    ptr = builder_->CreatePointerCast(ptr, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
    return builder_->CreateLoad(ptr);
}

void LLVMDexBuilder::VisitInstanceOf(art::HInstanceOf* instruction) {
    uint32_t class_offset = art::mirror::Object::ClassOffset().Int32Value();
    uint32_t super_offset = art::mirror::Class::SuperClassOffset().Int32Value();
    uint32_t component_offset = art::mirror::Class::ComponentTypeOffset().Int32Value();
    uint32_t primitive_offset = art::mirror::Class::PrimitiveTypeOffset().Int32Value();
    art::TypeCheckKind type_check_kind = instruction->GetTypeCheckKind();
    bool do_null_check = instruction->MustDoNullCheck();
    
    llvm::Value* obj = instruction->InputAt(0)->GetLLVMValue();
    llvm::Value* cls = instruction->InputAt(1)->GetLLVMValue();
    
    llvm::BasicBlock* not_null;
    llvm::BasicBlock* done;
    llvm::BasicBlock* ext_orig_edge;
    if (do_null_check) {
        ext_orig_edge = builder_->GetInsertBlock();
        llvm::Value* cond = builder_->CreateIsNull(obj);
        not_null = llvm::BasicBlock::Create(*context_, "not null", function_);
        done = llvm::BasicBlock::Create(*context_, "done", function_);
        builder_->CreateCondBr(cond, done, not_null);
        
        builder_->SetInsertPoint(not_null);
    }
    
    // Load object class.
    llvm::Value* obj_cls = GenerateReferenceLoad(builder_->CreateConstGEP1_32(obj, class_offset));
    
    // Do the instance of check.
    llvm::Value* result;
    switch (type_check_kind) {
        case art::TypeCheckKind::kExactCheck: {
            result = builder_->CreateICmpEQ(obj_cls, cls);
            break;
        }

        case art::TypeCheckKind::kAbstractClassCheck: {
            llvm::BasicBlock* orig_edge = builder_->GetInsertBlock();
            llvm::BasicBlock* loop_begin = llvm::BasicBlock::Create(*context_, "loop begin", function_);
            llvm::BasicBlock* loop_end = llvm::BasicBlock::Create(*context_, "loop end", function_);
            builder_->SetInsertPoint(loop_begin);
        
            llvm::PHINode* cls_phi = builder_->CreatePHI(llvm::Type::getInt8PtrTy(*context_), 2);
            cls_phi->addIncoming(obj_cls, orig_edge);
            llvm::Value* sup_cls = GenerateReferenceLoad(builder_->CreateConstGEP1_32(cls_phi, super_offset));
            cls_phi->addIncoming(sup_cls, loop_begin);
            llvm::Value* is_null_sup = builder_->CreateIsNull(sup_cls);
            
            llvm::BasicBlock* null_sup = llvm::BasicBlock::Create(*context_, "null super", function_);
            llvm::BasicBlock* not_null_sup = llvm::BasicBlock::Create(*context_, "not null super", function_);
            builder_->CreateCondBr(is_null_sup, null_sup, not_null_sup);
            
            builder_->SetInsertPoint(null_sup);
            llvm::Value* result1 = llvm::ConstantInt::get(*context_, llvm::APInt(1, 0, false));
            builder_->CreateBr(loop_end);
            
            builder_->SetInsertPoint(not_null_sup);
            llvm::BasicBlock* match_sup = llvm::BasicBlock::Create(*context_, "matching super", function_);
            llvm::Value* is_match_sup = builder_->CreateICmpEQ(sup_cls, cls);
            builder_->CreateCondBr(is_match_sup, match_sup, loop_begin);
            
            builder_->SetInsertPoint(match_sup);
            llvm::Value* result2 = llvm::ConstantInt::get(*context_, llvm::APInt(1, 1, false));
            builder_->CreateBr(loop_end);
            
            builder_->SetInsertPoint(loop_end);
            llvm::PHINode* phi = builder_->CreatePHI(llvm::Type::getInt8PtrTy(*context_), 2);
            phi->addIncoming(result1, null_sup);
            phi->addIncoming(result2, match_sup);
            result = phi;
            break;
        }

        case art::TypeCheckKind::kClassHierarchyCheck: {
            llvm::BasicBlock* orig_edge = builder_->GetInsertBlock();
            llvm::BasicBlock* loop_begin = llvm::BasicBlock::Create(*context_, "loop begin", function_);
            llvm::BasicBlock* loop_end = llvm::BasicBlock::Create(*context_, "loop end", function_);
            builder_->SetInsertPoint(loop_begin);
     
            llvm::PHINode* cls_phi = builder_->CreatePHI(llvm::Type::getInt8PtrTy(*context_), 2);
            cls_phi->addIncoming(obj_cls, orig_edge);
            llvm::BasicBlock* match_sup = llvm::BasicBlock::Create(*context_, "matching super", function_);
            llvm::BasicBlock* not_match_sup = llvm::BasicBlock::Create(*context_, "not matching super", function_);
            llvm::Value* is_match_sup = builder_->CreateICmpEQ(cls_phi, cls);
            builder_->CreateCondBr(is_match_sup, match_sup, not_match_sup);
            
            builder_->SetInsertPoint(match_sup);
            llvm::Value* result1 = llvm::ConstantInt::get(*context_, llvm::APInt(1, 1, false));
            builder_->CreateBr(loop_end);
            
            builder_->SetInsertPoint(not_match_sup);
            llvm::Value* sup_cls = GenerateReferenceLoad(builder_->CreateConstGEP1_32(cls_phi, super_offset));
            cls_phi->addIncoming(sup_cls, not_match_sup);
            
            llvm::BasicBlock* null_sup = llvm::BasicBlock::Create(*context_, "null super", function_);
            llvm::Value* is_null_sup = builder_->CreateIsNull(sup_cls);
            builder_->CreateCondBr(is_null_sup, null_sup, loop_begin);
            
            builder_->SetInsertPoint(null_sup);
            llvm::Value* result2 = llvm::ConstantInt::get(*context_, llvm::APInt(1, 0, false));
            builder_->CreateBr(loop_end);
            
            builder_->SetInsertPoint(loop_end);
            llvm::PHINode* phi = builder_->CreatePHI(llvm::Type::getInt8PtrTy(*context_), 2);
            phi->addIncoming(result1, match_sup);
            phi->addIncoming(result2, null_sup);
            result = phi;
            break;
        }

        case art::TypeCheckKind::kArrayObjectCheck: {
            llvm::BasicBlock* end = llvm::BasicBlock::Create(*context_, "end", function_);
            llvm::BasicBlock* match = llvm::BasicBlock::Create(*context_, "matching class", function_);
            llvm::BasicBlock* not_match = llvm::BasicBlock::Create(*context_, "not matching class", function_);
            llvm::Value* is_match = builder_->CreateICmpEQ(obj_cls, cls);
            builder_->CreateCondBr(is_match, match, not_match);
          
            builder_->SetInsertPoint(match);
            llvm::Value* result1 = llvm::ConstantInt::get(*context_, llvm::APInt(1, 1, false));
            builder_->CreateBr(end);
          
            builder_->SetInsertPoint(not_match);
            llvm::Value* comp = GenerateReferenceLoad(builder_->CreateConstGEP1_32(obj_cls, component_offset));
            llvm::BasicBlock* null_comp = llvm::BasicBlock::Create(*context_, "null component", function_);
            llvm::BasicBlock* not_null_comp = llvm::BasicBlock::Create(*context_, "not null component", function_);
            llvm::Value* is_null_comp = builder_->CreateIsNull(comp);
            builder_->CreateCondBr(is_null_comp, null_comp, not_null_comp);
            
            builder_->SetInsertPoint(null_comp);
            llvm::Value* result2 = llvm::ConstantInt::get(*context_, llvm::APInt(1, 0, false));
            builder_->CreateBr(end);
            
            builder_->SetInsertPoint(not_null_comp);
            llvm::Value* prim_type = builder_->CreateConstGEP1_32(comp, primitive_offset);
            prim_type = builder_->CreatePointerCast(prim_type, llvm::Type::getInt1PtrTy(*context_));
            prim_type = builder_->CreateLoad(prim_type);
            llvm::Value* result3 = builder_->CreateICmpEQ(prim_type, llvm::ConstantInt::get(*context_, llvm::APInt(16, art::Primitive::kPrimNot)));
            builder_->CreateBr(end);
            
            builder_->SetInsertPoint(end);
            llvm::PHINode* phi = builder_->CreatePHI(llvm::Type::getInt8PtrTy(*context_), 3);
            phi->addIncoming(result1, match);
            phi->addIncoming(result2, null_comp);
            phi->addIncoming(result3, not_null_comp);
            result = phi;
            break;
        }

        case art::TypeCheckKind::kArrayCheck: {
            llvm::BasicBlock* end = llvm::BasicBlock::Create(*context_, "end", function_);
            llvm::BasicBlock* match = llvm::BasicBlock::Create(*context_, "matching class", function_);
            llvm::BasicBlock* not_match = llvm::BasicBlock::Create(*context_, "not matching class", function_);
            llvm::Value* is_match = builder_->CreateICmpEQ(obj_cls, cls);
            builder_->CreateCondBr(is_match, match, not_match);
            
            builder_->SetInsertPoint(match);
            llvm::Value* result1 = llvm::ConstantInt::get(*context_, llvm::APInt(1, 1, false));
            builder_->CreateBr(end);
          
            builder_->SetInsertPoint(not_match);
            llvm::Function* assignable_function = GetIsAssignableMethod();
            llvm::SmallVector<llvm::Value*, 2> checker_args {
                cls,
                obj_cls
            };
            GenerateShadowMapUpdate(instruction);
            llvm::Value* is_assignable = builder_->CreateCall(assignable_function, checker_args);
            llvm::Value* result2 = builder_->CreateIsNotNull(is_assignable);
            builder_->CreateBr(end);
            
            builder_->SetInsertPoint(end);
            llvm::PHINode* phi = builder_->CreatePHI(llvm::Type::getInt8PtrTy(*context_), 2);
            phi->addIncoming(result1, match);
            phi->addIncoming(result2, not_match);
            result = phi;
            break;
        }

        case art::TypeCheckKind::kUnresolvedCheck:
        case art::TypeCheckKind::kInterfaceCheck: {
            llvm::Function* assignable_function = GetIsAssignableMethod();
            llvm::SmallVector<llvm::Value*, 2> checker_args {
                cls,
                obj_cls
            };
            GenerateShadowMapUpdate(instruction);
            llvm::Value* is_assignable = builder_->CreateCall(assignable_function, checker_args);
            result = builder_->CreateIsNotNull(is_assignable);
            break;
        }
    }
    
    if (do_null_check) {
        llvm::BasicBlock* orig_edge = builder_->GetInsertBlock();
        builder_->CreateBr(done);
        builder_->SetInsertPoint(done);
        llvm::PHINode* phi = builder_->CreatePHI(llvm::Type::getInt1Ty(*context_), 2);
        phi->addIncoming(llvm::ConstantInt::get(llvm::Type::getInt1Ty(*context_), llvm::APInt(1, 0, false)), ext_orig_edge);
        phi->addIncoming(result, orig_edge);
        result = phi;
    }

    instruction->SetLLVMValue(result);
}

llvm::Function* LLVMDexBuilder::GetThrowClassCastExceptionMethod() {
    // The class cast method might be already constucted.
    const char* const throw_class_cast_name = "artThrowClassCastException";
    llvm::Function* throw_class_cast = module_->getFunction(throw_class_cast_name);
    
    // If not, then construct a new one.
    if (throw_class_cast == nullptr) {
        // Get LLVM type for return type.
        llvm::Type* ret_type = llvm::Type::getVoidTy(*context_);
        
        // Build argument type array.
        llvm::SmallVector<llvm::Type*, 3> arg_types {
          llvm::Type::getInt8PtrTy(*context_),
          llvm::Type::getInt8PtrTy(*context_),
          llvm::Type::getInt8PtrTy(*context_)
        };
        
        // Create function type.
        llvm::FunctionType* throw_class_cast_ty = llvm::FunctionType::get(ret_type, arg_types, false);
    
        // Create function.
        throw_class_cast =
            llvm::Function::Create(throw_class_cast_ty,
                                   compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                                   throw_class_cast_name, module_);
        throw_class_cast->setDoesNotReturn();
        if (compiler_->IsWindows()) {
          throw_class_cast->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
        }
    }
    
    return throw_class_cast;
}

void LLVMDexBuilder::VisitCheckCast(art::HCheckCast* instruction) {
    art::TypeCheckKind type_check_kind = instruction->GetTypeCheckKind();
    uint32_t class_offset = art::mirror::Object::ClassOffset().Int32Value();
    uint32_t super_offset = art::mirror::Class::SuperClassOffset().Int32Value();
    uint32_t component_offset = art::mirror::Class::ComponentTypeOffset().Int32Value();
    uint32_t primitive_offset = art::mirror::Class::PrimitiveTypeOffset().Int32Value();
    bool do_null_check = instruction->MustDoNullCheck();
    
    llvm::Value* obj = instruction->InputAt(0)->GetLLVMValue();
    llvm::Value* cls = instruction->InputAt(1)->GetLLVMValue();
    
    llvm::BasicBlock* not_null;
    llvm::BasicBlock* done;
    if (do_null_check) {
        llvm::Value* cond = builder_->CreateIsNull(obj);
        not_null = llvm::BasicBlock::Create(*context_, "not null", function_);
        done = llvm::BasicBlock::Create(*context_, "done", function_);
        builder_->CreateCondBr(cond, done, not_null);
        
        builder_->SetInsertPoint(not_null);
    }
    
    llvm::Function* throw_class_cast = GetThrowClassCastExceptionMethod();
    llvm::Value* self = &*function_->args().begin();
    
    // Load object class.
    llvm::Value* obj_cls = GenerateReferenceLoad(builder_->CreateConstGEP1_32(obj, class_offset));
    
    switch (type_check_kind) {
        case art::TypeCheckKind::kExactCheck:
        case art::TypeCheckKind::kArrayCheck: {
            llvm::BasicBlock* end = llvm::BasicBlock::Create(*context_, "end", function_);
            llvm::BasicBlock* not_match = llvm::BasicBlock::Create(*context_, "not matching class", function_);
            llvm::Value* is_match = builder_->CreateICmpEQ(obj_cls, cls);
            builder_->CreateCondBr(is_match, end, not_match);
            
            builder_->SetInsertPoint(not_match);
            llvm::Function* assignable_function = GetIsAssignableMethod();
            llvm::SmallVector<llvm::Value*, 2> checker_args {
                cls,
                obj_cls
            };
            GenerateShadowMapUpdate(instruction);
            llvm::Value* is_assignable = builder_->CreateCall(assignable_function, checker_args);
            is_assignable = builder_->CreateIsNotNull(is_assignable);
            llvm::BasicBlock* not_assignable = llvm::BasicBlock::Create(*context_, "not assignable", function_);
            builder_->CreateCondBr(is_assignable, end, not_assignable);
            
            builder_->SetInsertPoint(not_assignable);
            builder_->CreateCall(throw_class_cast, llvm::SmallVector<llvm::Value*, 3> {
                cls,
                obj_cls,
                self
            });
            break;
        }
    
        case art::TypeCheckKind::kAbstractClassCheck: {
            llvm::BasicBlock* orig_edge = builder_->GetInsertBlock();
            llvm::BasicBlock* loop_begin = llvm::BasicBlock::Create(*context_, "loop begin", function_);
            llvm::BasicBlock* loop_end = llvm::BasicBlock::Create(*context_, "loop end", function_);
            builder_->SetInsertPoint(loop_begin);
        
            llvm::PHINode* cls_phi = builder_->CreatePHI(llvm::Type::getInt8PtrTy(*context_), 2);
            cls_phi->addIncoming(obj_cls, orig_edge);
            llvm::Value* sup_cls = GenerateReferenceLoad(builder_->CreateConstGEP1_32(cls_phi, super_offset));
            cls_phi->addIncoming(sup_cls, loop_begin);
            llvm::Value* is_null_sup = builder_->CreateIsNull(sup_cls);
            
            llvm::BasicBlock* null_sup = llvm::BasicBlock::Create(*context_, "null super", function_);
            llvm::BasicBlock* not_null_sup = llvm::BasicBlock::Create(*context_, "not null super", function_);
            builder_->CreateCondBr(is_null_sup, null_sup, not_null_sup);
            
            builder_->SetInsertPoint(null_sup);
            GenerateShadowMapUpdate(instruction);
            builder_->CreateCall(throw_class_cast, llvm::SmallVector<llvm::Value*, 3> {
                cls,
                obj_cls,
                self
            });
            builder_->CreateBr(loop_end);
            
            builder_->SetInsertPoint(not_null_sup);
            llvm::Value* is_match_sup = builder_->CreateICmpEQ(sup_cls, cls);
            builder_->CreateCondBr(is_match_sup, loop_end, loop_begin);
            
            builder_->SetInsertPoint(loop_end);
            break;
        }

        case art::TypeCheckKind::kClassHierarchyCheck: {
            llvm::BasicBlock* orig_edge = builder_->GetInsertBlock();
            llvm::BasicBlock* loop_begin = llvm::BasicBlock::Create(*context_, "loop begin", function_);
            llvm::BasicBlock* loop_end = llvm::BasicBlock::Create(*context_, "loop end", function_);
            builder_->SetInsertPoint(loop_begin);
     
            llvm::PHINode* cls_phi = builder_->CreatePHI(llvm::Type::getInt8PtrTy(*context_), 2);
            cls_phi->addIncoming(obj_cls, orig_edge);
            llvm::BasicBlock* not_match_sup = llvm::BasicBlock::Create(*context_, "not matching super", function_);
            llvm::Value* is_match_sup = builder_->CreateICmpEQ(cls_phi, cls);
            builder_->CreateCondBr(is_match_sup, loop_end, not_match_sup);
            
            builder_->SetInsertPoint(not_match_sup);
            llvm::Value* sup_cls = GenerateReferenceLoad(builder_->CreateConstGEP1_32(cls_phi, super_offset));
            cls_phi->addIncoming(sup_cls, not_match_sup);
            
            llvm::BasicBlock* null_sup = llvm::BasicBlock::Create(*context_, "null super", function_);
            llvm::Value* is_null_sup = builder_->CreateIsNull(sup_cls);
            builder_->CreateCondBr(is_null_sup, null_sup, loop_begin);
            
            builder_->SetInsertPoint(null_sup);
            GenerateShadowMapUpdate(instruction);
            builder_->CreateCall(throw_class_cast, llvm::SmallVector<llvm::Value*, 3> {
                cls,
                obj_cls,
                self
            });
            builder_->CreateBr(loop_end);
            
            builder_->SetInsertPoint(loop_end);
            break;
        }

        case art::TypeCheckKind::kArrayObjectCheck: {
            llvm::BasicBlock* end = llvm::BasicBlock::Create(*context_, "end", function_);
            llvm::BasicBlock* not_match = llvm::BasicBlock::Create(*context_, "not matching class", function_);
            llvm::Value* is_match = builder_->CreateICmpEQ(obj_cls, cls);
            builder_->CreateCondBr(is_match, end, not_match);
          
            builder_->SetInsertPoint(not_match);
            llvm::Value* comp = GenerateReferenceLoad(builder_->CreateConstGEP1_32(obj_cls, component_offset));
            llvm::BasicBlock* error = llvm::BasicBlock::Create(*context_, "error", function_);
            llvm::BasicBlock* not_null_comp = llvm::BasicBlock::Create(*context_, "not null component", function_);
            llvm::Value* is_null_comp = builder_->CreateIsNull(comp);
            builder_->CreateCondBr(is_null_comp, error, not_null_comp);
            
            builder_->SetInsertPoint(error);
            GenerateShadowMapUpdate(instruction);
            builder_->CreateCall(throw_class_cast, llvm::SmallVector<llvm::Value*, 3> {
                cls,
                obj_cls,
                self
            });
            builder_->CreateBr(end);
            
            builder_->SetInsertPoint(not_null_comp);
            llvm::Value* prim_type = builder_->CreateConstGEP1_32(comp, primitive_offset);
            prim_type = builder_->CreatePointerCast(prim_type, llvm::Type::getInt1PtrTy(*context_));
            prim_type = builder_->CreateLoad(prim_type);
            llvm::Value* is_prim_match = builder_->CreateICmpEQ(prim_type, llvm::ConstantInt::get(*context_, llvm::APInt(16, art::Primitive::kPrimNot)));
            builder_->CreateCondBr(is_prim_match, end, error);
            
            builder_->SetInsertPoint(end);
            break;
        }
      
        case art::TypeCheckKind::kUnresolvedCheck:
        case art::TypeCheckKind::kInterfaceCheck:
            llvm::BasicBlock* end = llvm::BasicBlock::Create(*context_, "end", function_);
            llvm::BasicBlock* error = llvm::BasicBlock::Create(*context_, "error", function_);
            llvm::Function* assignable_function = GetIsAssignableMethod();
            llvm::SmallVector<llvm::Value*, 2> checker_args {
                cls,
                obj_cls
            };
            GenerateShadowMapUpdate(instruction);
            llvm::Value* is_assignable = builder_->CreateCall(assignable_function, checker_args);
            is_assignable = builder_->CreateIsNotNull(is_assignable);
            builder_->CreateCondBr(is_assignable, end, error);
            
            builder_->SetInsertPoint(error);
            builder_->CreateCall(throw_class_cast, llvm::SmallVector<llvm::Value*, 3> {
                cls,
                obj_cls,
                self
            });
            builder_->CreateBr(end);
            
            builder_->SetInsertPoint(end);
            break;
        }

    if (do_null_check) {
        builder_->CreateBr(done);
        builder_->SetInsertPoint(done);
    }
    
    instruction->SetLLVMValue(obj);
}

llvm::Function* LLVMDexBuilder::GetMonitorOperationMethod(bool is_enter) {
    // Get monitor operation name for the trampoline.
    const char* const mon_op_name = is_enter ? "artLockObjectFromCode" : "artUnlockObjectFromCode";

    // The monitor operation method might be already constucted.
    llvm::Function* mon_op = module_->getFunction(mon_op_name);
    
    // If not, then construct a new one.
    if (mon_op == nullptr) {
        // Get LLVM type for return type.
        llvm::Type* ret_type = llvm::Type::getInt32Ty(*context_);
        
        // Build argument type array.
        llvm::SmallVector<llvm::Type*, 2> arg_types {
          llvm::Type::getInt8PtrTy(*context_),
          llvm::Type::getInt8PtrTy(*context_)
        };
        
        // Create function type.
        llvm::FunctionType* mon_op_ty = llvm::FunctionType::get(ret_type, arg_types, false);
    
        // Create function.
        mon_op =
            llvm::Function::Create(mon_op_ty,
                                   compiler_->IsWindows() ? llvm::GlobalValue::ExternalLinkage : llvm::GlobalValue::ExternalWeakLinkage,
                                   mon_op_name, module_);
        if (compiler_->IsWindows()) {
          mon_op->setDLLStorageClass(llvm::Function::DLLImportStorageClass);
        }
    }
    
    return mon_op;
}

void LLVMDexBuilder::VisitMonitorOperation(art::HMonitorOperation* instruction) {
    bool is_64bit = art::Is64BitInstructionSet(compiler_->GetInstructionSet());
    bool is_enter = instruction->IsEnter();
    int32_t monitor_offset = art::mirror::Object::MonitorOffset().Int32Value();
    uint32_t state_mask = art::LockWord::kStateMaskShifted;
    uint32_t rb_state_unmask = art::LockWord::kReadBarrierStateMaskShiftedToggled;
    uint32_t rb_state_mask = art::LockWord::kReadBarrierStateMaskShifted;
    int32_t thread_id_offset = is_64bit ? art::Thread::ThinLockIdOffset<8>().Int32Value() :
                                       art::Thread::ThinLockIdOffset<4>().Int32Value();
    int32_t lock_count_one = art::LockWord::kThinLockCountOne;
    uint32_t thread_id_mask = 0x0000FFFFu;
    uint32_t max_count = 0x0FFF0000u;

    llvm::Value* self = &*function_->args().begin();
    llvm::Value* obj = instruction->InputAt(0)->GetLLVMValue();

    llvm::BasicBlock* done = llvm::BasicBlock::Create(*context_, "done", function_);
    llvm::BasicBlock* retry = llvm::BasicBlock::Create(*context_, "retry", function_);
    llvm::BasicBlock* slow = llvm::BasicBlock::Create(*context_, "slow", function_);
    
    // For null objects invoke runtime function to throw NPE.
    llvm::Value* is_null = builder_->CreateIsNull(obj);
    builder_->CreateCondBr(is_null, slow, retry);

    // Get the monitor word from the object.
    builder_->SetInsertPoint(retry);
    llvm::Value* monitor_ptr = builder_->CreateConstGEP1_32(obj, monitor_offset);
    monitor_ptr = builder_->CreatePointerCast(monitor_ptr, llvm::Type::getInt32Ty(*context_)->getPointerTo());
    llvm::Value* monitor = builder_->CreateLoad(monitor_ptr);

    // Check locking kind.
    llvm::Value* is_thin_or_empty = builder_->CreateAnd(monitor, state_mask);
    is_thin_or_empty = builder_->CreateIsNull(is_thin_or_empty);
    llvm::BasicBlock* thin_or_empty = llvm::BasicBlock::Create(*context_, "thin or empty", function_);
    builder_->CreateCondBr(is_thin_or_empty, thin_or_empty, slow);
    
    if (is_enter) {
        // Unmask read barrier mask to check whether this is already locked.
        builder_->SetInsertPoint(thin_or_empty);
        llvm::Value* monitor_without_rb = builder_->CreateAnd(monitor, rb_state_unmask);
        llvm::Value* is_already_locked = builder_->CreateIsNotNull(monitor_without_rb);
        llvm::BasicBlock* thin_locked = llvm::BasicBlock::Create(*context_, "thin locked", function_);
        llvm::BasicBlock* not_thin_locked = llvm::BasicBlock::Create(*context_, "not thin locked", function_);
        builder_->CreateCondBr(is_already_locked, thin_locked, not_thin_locked);

        // Check whether the lock is for this thread.
        builder_->SetInsertPoint(thin_locked);
        llvm::Value* thread_id = builder_->CreateConstGEP1_32(self, thread_id_offset);
        thread_id = builder_->CreatePointerCast(thread_id, llvm::Type::getInt32Ty(*context_)->getPointerTo());
        thread_id = builder_->CreateLoad(thread_id);
        llvm::Value* is_ours = builder_->CreateAnd(monitor, thread_id_mask);
        is_ours = builder_->CreateICmpEQ(is_ours, thread_id);
        llvm::BasicBlock* ours = llvm::BasicBlock::Create(*context_, "ours", function_);
        builder_->CreateCondBr(is_ours, ours, slow);
        
        // Check whether we can increment lock count without overflow.
        builder_->SetInsertPoint(ours);
        llvm::Value* is_overflow = builder_->CreateICmpUGE(monitor_without_rb, llvm::ConstantInt::get(*context_, llvm::APInt(32, max_count, false)));
        llvm::BasicBlock* not_overflow = llvm::BasicBlock::Create(*context_, "not overflow", function_);
        builder_->CreateCondBr(is_overflow, slow, not_overflow);
        
        // Increment lock count.
        builder_->SetInsertPoint(not_overflow);
        llvm::Value* inc_thin_lock = builder_->CreateAdd(monitor, llvm::ConstantInt::get(*context_, llvm::APInt(32, lock_count_one, false)));
        
        // Try to store the new thin lock.
        llvm::Value* is_success_inc = builder_->CreateAtomicCmpXchg(monitor_ptr, monitor, inc_thin_lock, llvm::SequentiallyConsistent, llvm::NotAtomic);
        is_success_inc = builder_->CreateExtractValue(is_success_inc, 1);
        builder_->CreateCondBr(is_success_inc, done, retry);
        builder_->CreateBr(done);
        
        // Create new thin lock.
        builder_->SetInsertPoint(not_thin_locked);
        thread_id = builder_->CreateConstGEP1_32(self, thread_id_offset);
        thread_id = builder_->CreatePointerCast(thread_id, llvm::Type::getInt32Ty(*context_)->getPointerTo());
        thread_id = builder_->CreateLoad(thread_id);
        llvm::Value* new_thin_lock = builder_->CreateOr(monitor, thread_id);
        
        // Try to store the new thin lock.
        llvm::Value* is_success_new = builder_->CreateAtomicCmpXchg(monitor_ptr, monitor, new_thin_lock, llvm::SequentiallyConsistent, llvm::NotAtomic);
        is_success_new = builder_->CreateExtractValue(is_success_new, 1);
        builder_->CreateCondBr(is_success_new, done, retry);
    } else {
        // Check whether the lock is for this thread.
        builder_->SetInsertPoint(thin_or_empty);
        llvm::Value* thread_id = builder_->CreateConstGEP1_32(self, thread_id_offset);
        thread_id = builder_->CreatePointerCast(thread_id, llvm::Type::getInt32Ty(*context_)->getPointerTo());
        thread_id = builder_->CreateLoad(thread_id);
        llvm::Value* is_ours = builder_->CreateAnd(monitor, thread_id_mask);
        is_ours = builder_->CreateICmpEQ(is_ours, thread_id);
        llvm::BasicBlock* ours = llvm::BasicBlock::Create(*context_, "ours", function_);
        builder_->CreateCondBr(is_ours, ours, slow);
        
        // Check whether we can decrement lock count without underflow.
        builder_->SetInsertPoint(ours);
        llvm::Value* monitor_without_rb = builder_->CreateAnd(monitor, rb_state_unmask);
        llvm::Value* is_underflow = builder_->CreateICmpULT(monitor_without_rb, llvm::ConstantInt::get(*context_, llvm::APInt(32, lock_count_one, false)));
        llvm::BasicBlock* underflow = llvm::BasicBlock::Create(*context_, "underflow", function_);
        llvm::BasicBlock* not_underflow = llvm::BasicBlock::Create(*context_, "not underflow", function_);
        builder_->CreateCondBr(is_underflow, underflow, not_underflow);
        
        // Decrement lock count.
        builder_->SetInsertPoint(not_underflow);
        llvm::Value* dec_thin_lock = builder_->CreateSub(monitor, llvm::ConstantInt::get(*context_, llvm::APInt(32, lock_count_one, false)));
        
        // Try to store the new thin lock.
        llvm::Value* is_success_dec = builder_->CreateAtomicCmpXchg(monitor_ptr, monitor, dec_thin_lock, llvm::SequentiallyConsistent, llvm::NotAtomic);
        is_success_dec = builder_->CreateExtractValue(is_success_dec, 1);
        builder_->CreateCondBr(is_success_dec, done, retry);
        builder_->CreateBr(done);
        
        // Destroy thin lock.
        builder_->SetInsertPoint(underflow);
        llvm::Value* destroyed_thin_lock = builder_->CreateAnd(monitor, rb_state_mask);
        
        // Try to store the destroyed thin lock.
        llvm::Value* is_success_new = builder_->CreateAtomicCmpXchg(monitor_ptr, monitor, destroyed_thin_lock, llvm::SequentiallyConsistent, llvm::NotAtomic);
        is_success_new = builder_->CreateExtractValue(is_success_new, 1);
        builder_->CreateCondBr(is_success_new, done, retry);
    }
    
    // Invoke runtime function as a last resort.
    builder_->SetInsertPoint(slow);
    llvm::Function* enter = GetMonitorOperationMethod(is_enter);
    GenerateShadowMapUpdate(instruction);
    llvm::Value* is_success_slow = builder_->CreateCall(enter, llvm::SmallVector<llvm::Value*, 2> {
        obj,
        self
    });
    is_success_slow = builder_->CreateIsNull(is_success_slow);
    llvm::BasicBlock* error = llvm::BasicBlock::Create(*context_, "error", function_);
    builder_->CreateCondBr(is_success_slow, done, error);
    
    // Deliver pending exception.
    builder_->SetInsertPoint(error);
    llvm::Function* deliver_exc = GetDeliverExceptionMethod();
    builder_->CreateCall(deliver_exc, llvm::SmallVector<llvm::Value*, 1>{ self });
    builder_->CreateBr(done);
    builder_->SetInsertPoint(done);
}

void LLVMDexBuilder::HandleBinaryOperation(art::HBinaryOperation* instruction, llvm::Instruction::BinaryOps op) {
    llvm::Value* lhs = instruction->GetLeft()->GetLLVMValue();
    llvm::Value* rhs = instruction->GetRight()->GetLLVMValue();
    llvm::Value* result = builder_->CreateBinOp(op, lhs, rhs);
    instruction->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitAnd(art::HAnd* instruction) {
    HandleBinaryOperation(instruction, llvm::Instruction::BinaryOps::And);
}

void LLVMDexBuilder::VisitOr(art::HOr* instruction) {
    HandleBinaryOperation(instruction, llvm::Instruction::BinaryOps::Or);
}

void LLVMDexBuilder::VisitXor(art::HXor* instruction) {
    HandleBinaryOperation(instruction, llvm::Instruction::BinaryOps::Xor);
}

void LLVMDexBuilder::VisitBoundType(art::HBoundType* instruction ATTRIBUTE_UNUSED) {
    instruction->SetLLVMValue(instruction->InputAt(0)->GetLLVMValue());
}

void LLVMDexBuilder::VisitPackedSwitch(art::HPackedSwitch* instruction) {
    uint32_t entries = instruction->GetNumEntries();
    int32_t start_value = instruction->GetStartValue();
    
    llvm::Value* value = instruction->InputAt(0)->GetLLVMValue();
    
    llvm::BasicBlock* default_block = instruction->GetDefaultBlock()->GetLLVMBlock();
    if (default_block == nullptr) {
        default_block = llvm::BasicBlock::Create(*context_, "default", function_);
        instruction->GetDefaultBlock()->SetLLVMBlock(default_block);
    }
    
    auto& case_blocks = instruction->GetBlock()->GetSuccessors();
    
    llvm::SwitchInst* switch_inst = builder_->CreateSwitch(value, default_block, entries);
    
    for (uint32_t i = 0; i < entries; i++) {
        llvm::BasicBlock* case_block = case_blocks[i]->GetLLVMBlock();
        if (case_block == nullptr) {
            case_block = llvm::BasicBlock::Create(*context_, "case", function_);
            case_blocks[i]->SetLLVMBlock(default_block);
        }
    
        switch_inst->addCase(llvm::ConstantInt::get(*context_, llvm::APInt(32, start_value + i, true)), case_block);
    }
}

void LLVMDexBuilder::VisitClassTableGet(art::HClassTableGet* instruction) {
    size_t pointer_size = art::GetInstructionSetPointerSize(compiler_->GetInstructionSet());
    int32_t method_offset;
    if (instruction->GetTableKind() == art::HClassTableGet::TableKind::kVTable) {
        method_offset = art::mirror::Class::EmbeddedVTableEntryOffset(
            instruction->GetIndex(), pointer_size).SizeValue();
    } else {
        method_offset = art::mirror::Class::EmbeddedImTableEntryOffset(
            instruction->GetIndex() % art::mirror::Class::kImtSize, pointer_size).Uint32Value();
    }
    
    llvm::Value* cls = instruction->InputAt(0)->GetLLVMValue();
    
    llvm::Value* result = builder_->CreateConstGEP1_32(cls, method_offset);
    result = builder_->CreatePointerCast(result, llvm::Type::getInt8PtrTy(*context_)->getPointerTo());
    result = builder_->CreateLoad(result);
    
    instruction->SetLLVMValue(result);
}

void LLVMDexBuilder::VisitConstant(art::HConstant* ATTRIBUTE_UNUSED) {
    // Unused
}

void LLVMDexBuilder::VisitUnaryOperation(art::HUnaryOperation* ATTRIBUTE_UNUSED) {
    // Unused
}

void LLVMDexBuilder::VisitBinaryOperation(art::HBinaryOperation* ATTRIBUTE_UNUSED) {
    // Unused
}

void LLVMDexBuilder::VisitCondition(art::HCondition* ATTRIBUTE_UNUSED) {
    // Unused
}

void LLVMDexBuilder::VisitInvoke(art::HInvoke* ATTRIBUTE_UNUSED) {
    // Unused
}
