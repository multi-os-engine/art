/*
 * Copyright 2014 The Android Open Source Project
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

#include "jit.h"

#include "compiler_callbacks.h"
#include "driver/compiler_driver.h"
#include "driver/compiler_callbacks_impl.h"
#include "mirror/art_method-inl.h"

namespace art {

namespace jit {

extern "C" void* jit_load(CompilerCallbacks** callbacks) {
  Jit* jit = new Jit;
  *callbacks = jit->GetCompilerCallbacks();
  return jit;
}

extern "C" bool jit_compile_method(void* handle, mirror::ArtMethod* method, Thread* self)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  Jit* jit = reinterpret_cast<Jit*>(handle);
  return jit->CompileMethod(self, method);
}

Jit::Jit() {
  compiler_options_.reset(new CompilerOptions);

  InstructionSet instruction_set = kNone;
  InstructionSetFeatures instruction_set_features;

#if defined(__arm__)
  instruction_set = kThumb2;
#elif defined(__mips__)
  instruction_set = kMips;
#elif defined(__i386__)
  instruction_set = kX86;
#elif defined(__x86_64__)
  instruction_set = kX86_64;
  // TODO: x86_64 compilation support.
  compiler_options_->SetCompilerFilter(CompilerOptions::kInterpretOnly);
#endif

  cumulative_logger_.reset(new CumulativeLogger("jit times"));

  verification_results_.reset(new VerificationResults(compiler_options_.get()));
  method_inliner_map_.reset(new DexFileToMethodInlinerMap);

  callbacks_.reset(new CompilerCallbacksImpl(verification_results_.get(),
                                             method_inliner_map_.get()));

  compiler_driver_.reset(new CompilerDriver(compiler_options_.get(),
          verification_results_.get(),
          method_inliner_map_.get(),
          CompilerBackend::kQuick, instruction_set,
          instruction_set_features,
          true, new CompilerDriver::DescriptorSet,
          2, true, true, cumulative_logger_.get()));
  compiler_driver_->SetSupportBootImageFixup(false);
}

Jit::~Jit() {
}

bool Jit::CompileMethod(Thread* self, mirror::ArtMethod* method) {
  SirtRef<mirror::ArtMethod> method_ref(self, method);
  SirtRef<mirror::Class> class_ref(self, method->GetDeclaringClass());
  LOG(INFO) << "JIT compiling " << PrettyMethod(method);
  if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(class_ref, true, true)) {
    return false;
  }

  compiler_driver_->CompileMethod(self, method);
  return MakeExecutable(method);
}

void Jit::MakeExecutable(const std::vector<uint8_t>& code) {
  CHECK_NE(code.size(), 0U);
  MakeExecutable(&code[0], code.size());
}

void Jit::MakeExecutable(const void* code_start, size_t code_length) {
  CHECK(code_start != nullptr);
  CHECK_NE(code_length, 0U);
  uintptr_t data = reinterpret_cast<uintptr_t>(code_start);
  uintptr_t base = RoundDown(data, kPageSize);
  uintptr_t limit = RoundUp(data + code_length, kPageSize);
  uintptr_t len = limit - base;
  int result = mprotect(reinterpret_cast<void*>(base), len, PROT_READ | PROT_WRITE | PROT_EXEC);
  CHECK_EQ(result, 0);

  // Flush instruction cache
  // Only uses __builtin___clear_cache if GCC >= 4.3.3
#if GCC_VERSION >= 40303
  __builtin___clear_cache(reinterpret_cast<void*>(base), reinterpret_cast<void*>(base + len));
#else
  LOG(FATAL) << "UNIMPLEMENTED: cache flush";
#endif
}

CompilerCallbacks* Jit::GetCompilerCallbacks() const {
    return callbacks_.get();
}

bool Jit::MakeExecutable(mirror::ArtMethod* method) {
  CHECK(method != nullptr);

  const CompiledMethod* compiled_method = nullptr;
  if (!method->IsAbstract()) {
    mirror::DexCache* dex_cache = method->GetDeclaringClass()->GetDexCache();
    const DexFile& dex_file = *dex_cache->GetDexFile();
    compiled_method =
        compiler_driver_->GetCompiledMethod(MethodReference(&dex_file,
                                                            method->GetDexMethodIndex()));
  }
  if (compiled_method != nullptr) {
    const std::vector<uint8_t>* code = compiled_method->GetQuickCode();
    if (code == nullptr) {
      code = compiled_method->GetPortableCode();
    }
    MakeExecutable(*code);
    const void* method_code = CompiledMethod::CodePointer(&(*code)[0],
                                                          compiled_method->GetInstructionSet());
    LOG(INFO) << "MakeExecutable " << PrettyMethod(method) << " code=" << method_code;
    OatFile::OatMethod oat_method = CreateOatMethod(method_code,
                                                    compiled_method->GetFrameSizeInBytes(),
                                                    compiled_method->GetCoreSpillMask(),
                                                    compiled_method->GetFpSpillMask(),
                                                    &compiled_method->GetMappingTable()[0],
                                                    &compiled_method->GetVmapTable()[0],
                                                    &compiled_method->GetGcMap()[0]);
    oat_method.LinkMethod(method);
  } else {
    // No code? You must mean to go into the interpreter.
    const void* method_code = kUsePortableCompiler ? GetPortableToInterpreterBridge()
                                                   : GetQuickToInterpreterBridge();
    OatFile::OatMethod oat_method = CreateOatMethod(method_code,
                                                    kStackAlignment,
                                                    0,
                                                    0,
                                                    nullptr,
                                                    nullptr,
                                                    nullptr);
    oat_method.LinkMethod(method);
  }
  // Create bridges to transition between different kinds of compiled bridge.
  if (method->GetEntryPointFromPortableCompiledCode() == nullptr) {
    method->SetEntryPointFromPortableCompiledCode(GetPortableToQuickBridge());
  } else {
    CHECK(method->GetEntryPointFromQuickCompiledCode() == nullptr);
    method->SetEntryPointFromQuickCompiledCode(GetQuickToPortableBridge());
    method->SetIsPortableCompiled();
  }

  return compiled_method != nullptr;
}

OatFile::OatMethod Jit::CreateOatMethod(const void* code,
                                        const size_t frame_size_in_bytes,
                                        const uint32_t core_spill_mask,
                                        const uint32_t fp_spill_mask,
                                        const uint8_t* mapping_table,
                                        const uint8_t* vmap_table,
                                        const uint8_t* gc_map) {
  const byte* base;
  uint32_t code_offset, mapping_table_offset, vmap_table_offset, gc_map_offset;
  if (mapping_table == nullptr && vmap_table == nullptr && gc_map == nullptr) {
    base = reinterpret_cast<const byte*>(code);  // Base of data points at code.
    base -= kPointerSize;  // Move backward so that code_offset != 0.
    code_offset = kPointerSize;
    mapping_table_offset = 0;
    vmap_table_offset = 0;
    gc_map_offset = 0;
  } else {
    // TODO: 64bit support.
    base = nullptr;  // Base of data in oat file, ie 0.
    code_offset = PointerToLowMemUInt32(code);
    mapping_table_offset = PointerToLowMemUInt32(mapping_table);
    vmap_table_offset = PointerToLowMemUInt32(vmap_table);
    gc_map_offset = PointerToLowMemUInt32(gc_map);
  }
  return OatFile::OatMethod(base,
                            code_offset,
                            frame_size_in_bytes,
                            core_spill_mask,
                            fp_spill_mask,
                            mapping_table_offset,
                            vmap_table_offset,
                            gc_map_offset);
}

}  // namespace jit

}  // namespace art
