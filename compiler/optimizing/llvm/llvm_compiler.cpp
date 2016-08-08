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

#include "llvm_compiler.hpp"

#include "thread.h"

#include <unordered_map>
#include <mutex>

#include "llvm-c/Target.h"
#include "llvm-c/Initialization.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/Transforms/IPO/PassManagerBuilder.h"
#include "llvm/IR/CallingConv.h"
#include "llvm/Target/TargetOptions.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/Module.h"

// For BitCode Generation
#include "llvm/MC/MCContext.h"
#include "llvm/MC/MCSectionMachO.h"
#include "llvm/MC/MCStreamer.h"
#include "llvm-c/BitWriter.h"

#include "llvm_stub_builder.hpp"
#include "llvm_dex_builder.hpp"

#define MOE_LLVM_VERIFY_FUNCTIONS false
#define MOE_LLVM_VERIFY_MODULES true

std::string LLVMCompiler::target_dir_;
llvm::TargetMachine* LLVMCompiler::machine_;

static std::mutex dump_mutex;

struct ClassContext {
    llvm::LLVMContext* context_;
    llvm::Module* module_;
    
    bool has_methods;
    
    ClassContext(const std::string name) {
        context_ = new llvm::LLVMContext;
        module_ = new llvm::Module(name, *context_);
        has_methods = false;
    }
    
    ~ClassContext() {
        if (has_methods) {
            LLVMCompiler::WriteNativeFileForModule(module_, module_->getName());
        }
        delete module_;
        delete context_;
    }
};

static __thread ClassContext* class_context = nullptr;

void LLVMCompiler::StartClass(const std::string& descriptor) {
    class_context = new ClassContext(descriptor);
}

void LLVMCompiler::StopClass() {
    delete class_context;
}

std::tuple<llvm::LLVMContext*, llvm::Module*> LLVMCompiler::RetainClassContext() {
    ClassContext* curr = class_context;
    curr->has_methods = true;
    return std::make_tuple(curr->context_, curr->module_);
}

static __thread llvm::Module* source_module;

llvm::MCStreamer* MachOStreamerCtor(llvm::MCContext &Ctx,
	llvm::MCAsmBackend &TAB, llvm::raw_pwrite_stream &OS,
	llvm::MCCodeEmitter *Emitter, bool RelaxAll,
	bool DWARFMustBeAtTheEnd) {
	llvm::MCStreamer* streamer = llvm::createMachOStreamer(Ctx, TAB, OS, Emitter, RelaxAll,
		DWARFMustBeAtTheEnd);

	// Create __bitcode section.
	llvm::MCSectionMachO* bc_sect =
		Ctx.getMachOSection("__LLVM", "__bitcode", llvm::MachO::S_REGULAR, 0,
			llvm::SectionKind::getReadOnly());
	bc_sect->setAlignment(16);
	LLVMMemoryBufferRef dataRef = LLVMWriteBitcodeToMemoryBuffer((LLVMModuleRef)source_module);
	const char* data = LLVMGetBufferStart(dataRef);
	size_t size = LLVMGetBufferSize(dataRef);
	streamer->SwitchSection(bc_sect);
	streamer->EmitBytes(llvm::StringRef(data, size));
	LLVMDisposeMemoryBuffer(dataRef);

	// Create __cmdline section.
	llvm::MCSectionMachO* cmd_sect =
		Ctx.getMachOSection("__LLVM", "__cmdline", llvm::MachO::S_REGULAR, 0,
			llvm::SectionKind::getReadOnly());
	cmd_sect->setAlignment(16);
	streamer->SwitchSection(cmd_sect);
	char c = 0;
	streamer->EmitBytes(llvm::StringRef(&c, 1));

	return streamer;
}

LLVMCompiler::LLVMCompiler(art::InstructionSet instruction_set, const std::string& platform_name, const std::string& target_dir)
    : instruction_set_(instruction_set),
      reflection_bridge_pool_("reflection bridge pool", 1),
      jni_bridge_pool_("jni bridge pool", 1),
      resolution_trampoline_pool_("resolution trampoline pool", 1),
      interpreter_bridge_pool_("interpreter bridge pool", 1) {
    
    // Initialize LLVM.
    LLVMInitializeAllTargets();
    LLVMInitializeAllTargetMCs();
    LLVMInitializeAllTargetInfos();
    LLVMInitializeAllAsmPrinters();

    LLVMPassRegistryRef Registry = (LLVMPassRegistryRef)llvm::PassRegistry::getPassRegistry();
    LLVMInitializeCore(Registry);
    LLVMInitializeCodeGen(Registry);

	// Cache machine.
	std::string arch;
	switch (instruction_set) {
	case art::kArm:
		arch = "armv7";
		break;
	case art::kArm64:
		arch = "aarch64";
		break;
	case art::kThumb2:
		arch = "thumbv7";
		break;
	case art::kX86:
		arch = "i386";
		break;
	case art::kX86_64:
		arch = "x86_64";
		break;
	default:
		LOG(art::FATAL) << "Unsupported architecture: " << instruction_set;
		UNREACHABLE();
	}

    // Get settings.
    is_windows_ = false;
    llvm::CallingConv::ID jni_calling_convention = llvm::CallingConv::C;
    std::string triplet = arch;// +"-apple-darwin";
    if (platform_name.empty()) {
#ifdef __APPLE__
        triplet += "-apple-darwin";
#elif defined(_WIN32)
        triplet += "-pc-win32-msvc";
        is_windows_ = true;
        jni_calling_convention = llvm::CallingConv::X86_StdCall;
#else
#error Unsupported host platform!
#endif
	} else if (platform_name == "Darwin") {
		triplet += "-apple-darwin";
	} else if (platform_name == "Windows") {
		triplet += "-pc-win32-msvc";
        is_windows_ = true;
        jni_calling_convention = llvm::CallingConv::X86_StdCall;
	} else {
		LOG(art::FATAL) << "Unsupported platform: " << platform_name;
		UNREACHABLE();
	}

	std::string err;
	const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triplet, err);
	target_ = *target;

	llvm::TargetRegistry::RegisterMachOStreamer(target_, MachOStreamerCtor);
	CHECK(err.empty()) << err;

    target_dir_ = target_dir;

	machine_ = target_.createTargetMachine(triplet, "", "", llvm::TargetOptions());
    
    // Create stub builder.
    stub_builder_= new LLVMStubBuilder(jni_calling_convention,
        art::Is64BitInstructionSet(instruction_set), is_windows_);
    
    // Start workers.
    art::Thread* self = art::Thread::Current();
    reflection_bridge_pool_.StartWorkers(self);
    jni_bridge_pool_.StartWorkers(self);
    resolution_trampoline_pool_.StartWorkers(self);
    interpreter_bridge_pool_.StartWorkers(self);
}

LLVMCompiler::~LLVMCompiler() {
    delete stub_builder_;
    
    art::Thread* self = art::Thread::Current();
    reflection_bridge_pool_.Wait(self, false, false);
    jni_bridge_pool_.Wait(self, false, false);
    resolution_trampoline_pool_.Wait(self, false, false);
    interpreter_bridge_pool_.Wait(self, false, false);
}

static void verifyLLVMModule(llvm::Module* module) {
  if (MOE_LLVM_VERIFY_MODULES) {
    std::string err;
    llvm::raw_string_ostream verifier_err(err);
    if (llvm::verifyModule(*module, &verifier_err)) {
      dump_mutex.lock();
      // module->dump();
      LOG(art::FATAL) <<  verifier_err.str();
      dump_mutex.unlock();
    }
  }
}

static void verifyLLVMFunction(llvm::Function* function) {
  if (MOE_LLVM_VERIFY_FUNCTIONS) {
    std::string err;
    llvm::raw_string_ostream verifier_err(err);
    if (llvm::verifyFunction(*function, &verifier_err)) {
      dump_mutex.lock();
      function->dump();
      LOG(art::FATAL) <<  verifier_err.str();
      dump_mutex.unlock();
    }
  }
}

static bool WriteObjectFile(llvm::Module* module, llvm::TargetMachine* machine,
                             const std::string& dir, const std::string& filename) {
  verifyLLVMModule(module);

  llvm::legacy::FunctionPassManager fpm(module);
  llvm::PassManagerBuilder pm_builder;
  pm_builder.OptLevel = 3;
  pm_builder.SizeLevel = 2;
  pm_builder.populateFunctionPassManager(fpm);
  fpm.doInitialization();
  for (llvm::Module::iterator F = module->begin(), E = module->end(); F != E; ++F) {
    fpm.run(*F);
  }
  fpm.doFinalization();

  source_module = module;

  std::string object_file = dir + "/" + filename + ".o";
  char* err;
  if (!LLVMTargetMachineEmitToFile((LLVMTargetMachineRef)machine, (LLVMModuleRef)module,
                                   const_cast<char*>(object_file.c_str()), LLVMObjectFile, &err)) {
    // MOE TODO: For some reason this function keeps returning garbage-like error messages.
    // LOG(FATAL) << err;
  }

  return true;
}

void LLVMCompiler::CompileReflectionBridgeMethod(art::Thread* self, const char* shorty, bool is_static) {
    static std::unordered_map<std::string, uint8_t> compiled_methods;

    class ReflectionBridgeCompileTask : public art::Task {
      LLVMStubBuilder* stub_builder_;
      std::string shorty;
      bool is_static;

    public:
      ReflectionBridgeCompileTask(LLVMStubBuilder* stub_builder, const char* shorty, bool is_static)
          : stub_builder_(stub_builder), shorty(shorty), is_static(is_static) {}

      virtual void Run(art::Thread* self) {
        uint8_t k;
        if (!is_static) {
          k = 1;
        } else if (is_static) {
          k = 2;
        }

        auto it = compiled_methods.emplace(shorty, k);
        bool to_compile = it.second;
        if (to_compile) {
          it.first->second |= k;
        } else {
          if (~it.first->second & k) {
            to_compile = true;
            it.first->second |= k;
          }
        }

        if (to_compile) {
          verifyLLVMFunction(stub_builder_->ReflectionBridgeCompile(shorty.c_str(), is_static));
        }
      }

      virtual void Finalize() {
        delete this;
      }
    };

    reflection_bridge_pool_.AddTask(self, new ReflectionBridgeCompileTask(stub_builder_, shorty, is_static));
}

void LLVMCompiler::CompileJniBridgeMethod(art::Thread* self, const char* shorty, bool is_synchronized,
                          bool is_static) {
    static std::unordered_map<std::string, uint8_t> compiled_methods;

    class JniBridgeCompileTask : public art::Task {
      LLVMStubBuilder* stub_builder_;
      std::string shorty;
      bool is_synchronized;
      bool is_static;

    public:
      JniBridgeCompileTask(LLVMStubBuilder* stub_builder, const char* shorty, bool is_synchronized,
                           bool is_static)
          : stub_builder_(stub_builder), shorty(shorty), is_synchronized(is_synchronized),
            is_static(is_static) {}

      virtual void Run(art::Thread* self) {
        uint8_t k;
        if (!is_synchronized && !is_static) {
          k = 1;
        } else if (is_synchronized && !is_static) {
          k = 2;
        } else if (!is_synchronized && is_static) {
          k = 4;
        } else if (is_synchronized && is_static) {
          k = 8;
        }

        auto it = compiled_methods.emplace(shorty, k);
        bool to_compile = it.second;
        if (to_compile) {
          it.first->second |= k;
        } else {
          if (~it.first->second & k) {
            to_compile = true;
            it.first->second |= k;
          }
        }

        if (to_compile) {
          verifyLLVMFunction(stub_builder_->JniBridgeCompile(shorty.c_str(), is_synchronized, is_static));
        }
      }

      virtual void Finalize() {
        delete this;
      }
    };

    jni_bridge_pool_.AddTask(self, new JniBridgeCompileTask(stub_builder_, shorty, is_synchronized, is_static));
}

void LLVMCompiler::CompileResolutionTrampolineMethod(art::Thread* self, const char* shorty, bool is_static) {
    static std::unordered_map<std::string, uint8_t> compiled_methods;

    class ResolutionTrampolineCompileTask : public art::Task {
      LLVMStubBuilder* stub_builder_;
      std::string shorty;
      bool is_static;

    public:
      ResolutionTrampolineCompileTask(LLVMStubBuilder* stub_builder, const char* shorty, bool is_static)
          : stub_builder_(stub_builder), shorty(shorty), is_static(is_static) {}

      virtual void Run(art::Thread* self) {
        uint8_t k;
        if (!is_static) {
          k = 1;
        } else if (is_static) {
          k = 2;
        }

        auto it = compiled_methods.emplace(shorty, k);
        bool to_compile = it.second;
        if (to_compile) {
          it.first->second |= k;
        } else {
          if (~it.first->second & k) {
            to_compile = true;
            it.first->second |= k;
          }
        }

        if (to_compile) {
          verifyLLVMFunction(stub_builder_->ResolutionTrampolineCompile(shorty.c_str(), is_static));
        }
      }

      virtual void Finalize() {
        delete this;
      }
    };

    resolution_trampoline_pool_.AddTask(self, new ResolutionTrampolineCompileTask(stub_builder_, shorty, is_static));
}

void LLVMCompiler::CompileInterpreterBridgeMethod(art::Thread* self, const char* shorty, bool is_static) {
    static std::unordered_map<std::string, uint8_t> compiled_methods;

    class InterpreterBridgeCompileTask : public art::Task {
      LLVMStubBuilder* stub_builder_;
      std::string shorty;
      bool is_static;

    public:
      InterpreterBridgeCompileTask(LLVMStubBuilder* stub_builder, const char* shorty, bool is_static)
          : stub_builder_(stub_builder), shorty(shorty), is_static(is_static) {}

      virtual void Run(art::Thread* self) {
        uint8_t k;
        if (!is_static) {
          k = 1;
        } else if (is_static) {
          k = 2;
        }

        auto it = compiled_methods.emplace(shorty, k);
        bool to_compile = it.second;
        if (to_compile) {
          it.first->second |= k;
        } else {
          if (~it.first->second & k) {
            to_compile = true;
            it.first->second |= k;
          }
        }

        if (to_compile) {
          verifyLLVMFunction(stub_builder_->InterpreterBridgeCompile(shorty.c_str(), is_static));
        }
      }

      virtual void Finalize() {
        delete this;
      }
    };

    interpreter_bridge_pool_.AddTask(self, new InterpreterBridgeCompileTask(stub_builder_, shorty, is_static));
}

void LLVMCompiler::CompileDexGraph(art::HGraph* graph,
                         const art::CompilerOptions& compiler_options,
                         art::OptimizingCompilerStats* stats) {
#ifdef MOE_COMPILE_DEX_GRAPH
    LLVMDexBuilder dex_builder(this, graph, compiler_options, stats);
    dex_builder.VisitReversePostOrder();
    verifyLLVMFunction(dex_builder.GetFunction());
#endif
}

void LLVMCompiler::CompileStub(art::Thread* self, const art::DexFile& dex_file, uint32_t method_idx, uint32_t access_flags) {
    const char* shorty = dex_file.GetMethodShorty(dex_file.GetMethodId(method_idx));
    CompileReflectionBridgeMethod(self, shorty, access_flags & art::kAccStatic);
    CompileInterpreterBridgeMethod(self, shorty, access_flags & art::kAccStatic);
    if ((access_flags & art::kAccNative)) {
      CompileJniBridgeMethod(self, shorty, access_flags & art::kAccSynchronized,
                                     access_flags & art::kAccStatic);
    }
    if ((access_flags & art::kAccStatic) != 0) {
      CompileResolutionTrampolineMethod(self, shorty, access_flags & art::kAccStatic);
    }
}

void LLVMCompiler::WriteNativeFiles() {
    WriteObjectFile(stub_builder_->GetReflectionBridgeModule(), machine_, target_dir_, "!reflection_bridges");
    WriteObjectFile(stub_builder_->GetJniBridgeModule(), machine_, target_dir_, "!jni_bridges");
    WriteObjectFile(stub_builder_->GetResolutionTrampolineModule(), machine_, target_dir_, "!resolution_trampolines");
    WriteObjectFile(stub_builder_->GetInterpreterBridgeModule(), machine_, target_dir_, "!interpreter_bridges");
}

void LLVMCompiler::WriteNativeFileForModule(llvm::Module* module, const std::string& name) {
    WriteObjectFile(module, machine_, target_dir_, name);
}
