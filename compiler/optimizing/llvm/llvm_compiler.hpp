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

#ifndef llvm_compiler_hpp
#define llvm_compiler_hpp

#include "arch/instruction_set.h"
#include "thread_pool.h"
#include "dex_file.h"
#include "nodes.h"
#include "optimizing_compiler_stats.h"
#include "driver/compiler_options.h"

#include "llvm/IR/Module.h"
#include "llvm/Support/TargetRegistry.h"

class LLVMStubBuilder;

class LLVMCompiler {

    bool is_windows_;
    art::InstructionSet instruction_set_;

    llvm::Target target_;
    
    static std::string target_dir_;
    static llvm::TargetMachine* machine_;

    LLVMStubBuilder* stub_builder_;

    art::ThreadPool reflection_bridge_pool_;
    art::ThreadPool jni_bridge_pool_;
    art::ThreadPool resolution_trampoline_pool_;
    art::ThreadPool interpreter_bridge_pool_;

public:
    LLVMCompiler(art::InstructionSet instruction_set, const std::string& platform_name, const std::string& target_dir);
    ~LLVMCompiler();

    bool IsWindows() {
        return is_windows_;
    }

    art::InstructionSet GetInstructionSet() {
        return instruction_set_;
    }

    void CompileStub(art::Thread* self, const art::DexFile& dex_file, uint32_t method_idx, uint32_t access_flags);

    static void WriteNativeFileForModule(llvm::Module* module, const std::string& name);

    static void StartClass(const std::string& descriptor);
    static void StopClass();

    struct ScopedClassNotifier {
        
        ScopedClassNotifier(const std::string& descriptor) {
            StartClass(descriptor);
        }
        
        ~ScopedClassNotifier() {
            StopClass();
        }
        
    };
    
    static std::tuple<llvm::LLVMContext*, llvm::Module*> RetainClassContext();
    
    static llvm::Type* GetLLVMType(llvm::LLVMContext* context, char type) {
      switch (type) {
        case 'Z':
        case 'B':
          return llvm::Type::getInt8Ty(*context);

        case 'C':
        case 'S':
          return llvm::Type::getInt16Ty(*context);

        case 'I':
          return llvm::Type::getInt32Ty(*context);

        case 'J':
          return llvm::Type::getInt64Ty(*context);

        case 'F':
          return llvm::Type::getFloatTy(*context);

        case 'D':
          return llvm::Type::getDoubleTy(*context);

        case 'L':
          return llvm::Type::getInt8PtrTy(*context);

        case 'V':
          return llvm::Type::getVoidTy(*context);

        default:
          LOG(art::FATAL) << "UNREACHABLE";
      }
      return nullptr;
    }
    
    static llvm::Type* GetLLVMType(llvm::LLVMContext* context, art::Primitive::Type type) {
      switch (type) {
        case art::Primitive::kPrimBoolean:
        case art::Primitive::kPrimByte:
          return llvm::Type::getInt8Ty(*context);

        case art::Primitive::kPrimChar:
        case art::Primitive::kPrimShort:
          return llvm::Type::getInt16Ty(*context);

        case art::Primitive::kPrimInt:
          return llvm::Type::getInt32Ty(*context);

        case art::Primitive::kPrimLong:
          return llvm::Type::getInt64Ty(*context);

        case art::Primitive::kPrimFloat:
          return llvm::Type::getFloatTy(*context);

        case art::Primitive::kPrimDouble:
          return llvm::Type::getDoubleTy(*context);

        case art::Primitive::kPrimNot:
          return llvm::Type::getInt8PtrTy(*context);

        case art::Primitive::kPrimVoid:
          return llvm::Type::getVoidTy(*context);

        default:
          LOG(art::FATAL) << "UNREACHABLE";
      }
      return nullptr;
    }

    static art::HInvokeStaticOrDirect::DispatchInfo GetSupportedInvokeStaticOrDirectDispatch(
        const art::HInvokeStaticOrDirect::DispatchInfo& desired_dispatch_info,
        art::MethodReference target_method) {
        
        art::HInvokeStaticOrDirect::MethodLoadKind method_load_kind = desired_dispatch_info.method_load_kind;
        if (method_load_kind != art::HInvokeStaticOrDirect::MethodLoadKind::kDexCacheViaMethod &&
            method_load_kind != art::HInvokeStaticOrDirect::MethodLoadKind::kStringInit &&
            method_load_kind != art::HInvokeStaticOrDirect::MethodLoadKind::kRecursive) {
            method_load_kind = art::HInvokeStaticOrDirect::MethodLoadKind::kDexCacheViaMethod;
        }
        
        art::HInvokeStaticOrDirect::CodePtrLocation code_ptr_location = desired_dispatch_info.code_ptr_location;
        if (code_ptr_location != art::HInvokeStaticOrDirect::CodePtrLocation::kCallArtMethod &&
            code_ptr_location != art::HInvokeStaticOrDirect::CodePtrLocation::kCallSelf) {
            code_ptr_location = art::HInvokeStaticOrDirect::CodePtrLocation::kCallArtMethod;
        }
        
        return art::HInvokeStaticOrDirect::DispatchInfo {
            method_load_kind,
            code_ptr_location,
            0u,
            0u
        };
    }

    static art::HLoadString::LoadKind GetSupportedLoadStringKind(art::HLoadString::LoadKind desired_string_load_kind) {
        return art::HLoadString::LoadKind::kDexCacheViaMethod;
    }

    void CompileDexGraph(art::HGraph* graph,
                         const art::CompilerOptions& compiler_options,
                         art::OptimizingCompilerStats* stats);

    void WriteNativeFiles();

private:
    void CompileReflectionBridgeMethod(art::Thread* self, const char* shorty, bool is_static);
    void CompileJniBridgeMethod(art::Thread* self, const char* shorty, bool is_synchronized, bool is_static);
    void CompileResolutionTrampolineMethod(art::Thread* self, const char* shorty, bool is_static);
    void CompileInterpreterBridgeMethod(art::Thread* self, const char* shorty, bool is_static);
    
};

#endif /* llvm_compiler_hpp */
