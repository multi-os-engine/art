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

#include "arch/instruction_set.h"
#include "arch/instruction_set_features.h"
#include "compiler_callbacks.h"
#include "dex/quick_compiler_callbacks.h"
#include "driver/compiler_driver.h"
#include "mirror/art_method-inl.h"
#include "oat_file-inl.h"
#include "object_lock.h"
#include "verifier/method_verifier-inl.h"

namespace art {

namespace jit {

Jit* Jit::Create(size_t code_cache_capacity) {
  std::string error_msg;
  // Map name specific for android_os_Debug.cpp.
  MemMap* map = MemMap::MapAnonymous("jit-code-cache", nullptr, code_cache_capacity,
                                     PROT_READ | PROT_WRITE | PROT_EXEC, false, &error_msg);
  if (map != nullptr) {
    return new Jit(map);
  }
  LOG(WARNING) << "Failed map jit code cache with size " << PrettySize(code_cache_capacity);
  return nullptr;
}

extern "C" void* jit_load(CompilerCallbacks** callbacks) {
  LOG(INFO) << "jit_load";
  Jit* jit = Jit::Create(64 * MB);
  CHECK(jit != nullptr);
  *callbacks = jit->GetCompilerCallbacks();
  LOG(INFO) << "Done jit_load";
  return jit;
}

extern "C" bool jit_compile_method(void* handle, mirror::ArtMethod* method, Thread* self)
    SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) {
  Jit* jit = reinterpret_cast<Jit*>(handle);
  DCHECK(jit != nullptr);
  return jit->CompileMethod(self, method);
}

Jit::Jit(MemMap* code_mem_map) : lock_("Jit lock"), num_methods_(0), total_time_(0),
    code_mem_map_(code_mem_map) {
  compiler_options_.reset(new CompilerOptions);
  code_cache_ptr_ = code_mem_map_->Begin();
  InstructionSet instruction_set = kRuntimeISA;
  instruction_set_features_.reset(InstructionSetFeatures::FromCppDefines());
  cumulative_logger_.reset(new CumulativeLogger("jit times"));
  verification_results_.reset(new VerificationResults(compiler_options_.get()));
  method_inliner_map_.reset(new DexFileToMethodInlinerMap);
  callbacks_.reset(new QuickCompilerCallbacks(verification_results_.get(),
                                              method_inliner_map_.get()));
  std::unique_ptr<std::set<std::string>> compiled_classes(new std::set<std::string>);
  compiler_driver_.reset(new CompilerDriver(
      compiler_options_.get(), verification_results_.get(), method_inliner_map_.get(),
      Compiler::kQuick, instruction_set, instruction_set_features_.get(), false,
      nullptr, compiled_classes.release(), 1, false, true, "",
      cumulative_logger_.get(), -1, ""));
  compiler_driver_->SetSupportBootImageFixup(false);
}

Jit::~Jit() {
}

bool Jit::MethodCompiled(mirror::ArtMethod* method) const {
  return code_mem_map_->HasAddress(method->GetEntryPointFromQuickCompiledCode());
}

bool Jit::CompileMethod(Thread* self, mirror::ArtMethod* method) {
  uint64_t start_time = NanoTime();
  StackHandleScope<2> hs(self);
  self->AssertNoPendingException();
  Handle<mirror::ArtMethod> h_method(hs.NewHandle(method));
  if (MethodCompiled(method)) {
    LOG(INFO) << "Already compiled " << PrettyMethod(method);
    return true;  // Already compiled
  }
  Handle<mirror::Class> h_class(hs.NewHandle(h_method->GetDeclaringClass()));
  VLOG(jit) << "JIT initializing " << PrettyMethod(h_method.Get());
  if (!Runtime::Current()->GetClassLinker()->EnsureInitialized(self, h_class, true, true)) {
    return false;
  }
  const DexFile* dex_file = h_class->GetDexCache()->GetDexFile();
  MethodReference method_ref(dex_file, h_method->GetDexMethodIndex());
  // Only verify if we don't already have verification results.
  if (verification_results_->GetVerifiedMethod(method_ref) == nullptr) {
    std::string error;
    if (verifier::MethodVerifier::VerifyMethod(h_method.Get(), true, &error) ==
        verifier::MethodVerifier::kHardFailure) {
      VLOG(jit) << "Not compile method due to verification failure "
                << PrettyMethod(h_method.Get());
      return false;
    }
  }
  std::unique_ptr<CompiledMethod> compiled_method(
      compiler_driver_->CompileMethod(self, h_method.Get()));
  if (compiled_method.get() == nullptr) {
    return false;
  }
  total_time_ += NanoTime() - start_time;
  const bool result = MakeExecutable(compiled_method.get(), h_method.Get());
  delete compiled_method->GetMappingTable();
  delete compiled_method->GetVmapTable();
  delete compiled_method->GetGcMap();
  delete compiled_method->GetCFIInfo();
  return result;
}

void Jit::FlushInstructionCache() {
  // __clear_cache(reinterpret_cast<char*>(code_mem_map_->Begin()), static_cast<int>(CodeCacheSize()));
}

CompilerCallbacks* Jit::GetCompilerCallbacks() const {
  return callbacks_.get();
}

uint8_t* Jit::WriteByteArray(const SwapVector<uint8_t>* array) {
  CHECK_GE(CodeCacheRemain(), array->size());
  uint8_t* base = code_cache_ptr_;
  std::copy(array->begin(), array->end(), base);
  code_cache_ptr_ += array->size();
  return base;
}

bool Jit::AddToCodeCache(mirror::ArtMethod* method, const CompiledMethod* compiled_method,
                         OatFile::OatMethod* out_method) {
  const auto* quick_code = compiled_method->GetQuickCode();
  if (quick_code == nullptr) {
    // TODO: Add support for Portable.
    return false;
  }
  MutexLock mu(Thread::Current(), lock_);
  const uint8_t* base = code_cache_ptr_;
  // Write out pre-header stuff.
  uint8_t* mapping_table = WriteByteArray(compiled_method->GetMappingTable());
  uint8_t* vmap_table = WriteByteArray(compiled_method->GetVmapTable());
  uint8_t* gc_map = WriteByteArray(compiled_method->GetGcMap());
  code_cache_ptr_ = AlignUp(code_cache_ptr_, 16);
  // Reserve space for header which is directly before the code.
  code_cache_ptr_ += sizeof(OatQuickMethodHeader);
  // Write out the code.
  const size_t thumb_offset = compiled_method->CodeDelta();
  code_cache_ptr_ = reinterpret_cast<uint8_t*>(
      compiled_method->AlignCode(reinterpret_cast<uintptr_t>(code_cache_ptr_)));
  uint8_t* code_ptr = WriteByteArray(quick_code);
  OatQuickMethodHeader* method_header = reinterpret_cast<OatQuickMethodHeader*>(code_ptr) - 1;
  // Construct the header last.
  const uint32_t frame_size_in_bytes = compiled_method->GetFrameSizeInBytes();
  const uint32_t core_spill_mask = compiled_method->GetCoreSpillMask();
  const uint32_t fp_spill_mask = compiled_method->GetFpSpillMask();
  const size_t code_size = quick_code->size() * sizeof(uint8_t);
  CHECK_NE(code_size, 0U);
  // Write out the method header last.
  method_header = new(method_header)OatQuickMethodHeader(
      code_ptr - mapping_table, code_ptr - vmap_table, base - gc_map, frame_size_in_bytes, core_spill_mask,
      fp_spill_mask, code_size);
  const uint32_t code_offset = code_ptr - base + thumb_offset;
  *out_method = OatFile::OatMethod(base, code_offset);
  DCHECK_EQ(out_method->GetGcMap(), gc_map);
  DCHECK_EQ(out_method->GetMappingTable(), mapping_table);
  DCHECK_EQ(out_method->GetVmapTable(), vmap_table);
  DCHECK_EQ(out_method->GetFrameSizeInBytes(), frame_size_in_bytes);
  DCHECK_EQ(out_method->GetCoreSpillMask(), core_spill_mask);
  DCHECK_EQ(out_method->GetFpSpillMask(), fp_spill_mask);
  LOG(INFO) << "Added " << PrettyMethod(method) << " ccache size " << PrettySize(CodeCacheSize())
            << ": " << reinterpret_cast<void*>(code_ptr) << ","
            << reinterpret_cast<void*>(code_ptr + code_size) << " time=" << PrettyDuration(total_time_);
  return true;
}

bool Jit::MakeExecutable(CompiledMethod* compiled_method, mirror::ArtMethod* method) {
  CHECK(method != nullptr);
  CHECK(compiled_method != nullptr);
  OatFile::OatMethod oat_method;
  if (!AddToCodeCache(method, compiled_method, &oat_method)) {
    return false;
  }
  VLOG(jit) << "MakeExecutable " << PrettyMethod(method) << " code="
      << reinterpret_cast<const void*>(oat_method.GetQuickCode());
  FlushInstructionCache();
  oat_method.LinkMethod(method);
  CHECK(MethodCompiled(method));
  return true;
}

}  // namespace jit
}  // namespace art
