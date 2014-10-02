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

#ifndef ART_COMPILER_JIT_JIT_H_
#define ART_COMPILER_JIT_JIT_H_

#include "base/mutex.h"
#include "compiler_callbacks.h"
#include "compiled_method.h"
#include "dex/verification_results.h"
#include "dex/quick/dex_file_to_method_inliner_map.h"
#include "driver/compiler_driver.h"
#include "driver/compiler_options.h"
#include "oat_file.h"
#include "utils/swap_space.h"

namespace art {

class InstructionSetFeatures;

namespace mirror {
class ArtMethod;
}

namespace jit {

class Jit {
 public:
  static Jit* Create(size_t code_cache_capacity);
  virtual ~Jit();
  bool CompileMethod(Thread* self, mirror::ArtMethod* method)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  CompilerCallbacks* GetCompilerCallbacks() const;
  size_t CodeCacheSize() const {
    return code_cache_ptr_ - code_mem_map_->Begin();
  }
  size_t CodeCacheRemain() const {
    return code_mem_map_->End() - code_cache_ptr_;
  }
  size_t GetNumMethods() {
    return num_methods_;
  }

 private:
  Mutex lock_;
  uint8_t* code_cache_ptr_;
  size_t num_methods_;
  uint64_t total_time_;
  std::unique_ptr<MemMap> code_mem_map_;
  std::unique_ptr<CompilerOptions> compiler_options_;
  std::unique_ptr<CumulativeLogger> cumulative_logger_;
  std::unique_ptr<VerificationResults> verification_results_;
  std::unique_ptr<DexFileToMethodInlinerMap> method_inliner_map_;
  std::unique_ptr<CompilerCallbacks> callbacks_;
  std::unique_ptr<CompilerDriver> compiler_driver_;
  std::unique_ptr<const InstructionSetFeatures> instruction_set_features_;

  explicit Jit(MemMap* code_mem_map);
  void FlushInstructionCache();
  bool MakeExecutable(CompiledMethod* compiled_method, mirror::ArtMethod* method)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  bool AddToCodeCache(mirror::ArtMethod* method, const CompiledMethod* compiled_method,
                      OatFile::OatMethod* out_method)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_) LOCKS_EXCLUDED(lock_);
  bool MethodCompiled(mirror::ArtMethod* method) const
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  uint8_t* WriteByteArray(const SwapVector<uint8_t>* array) EXCLUSIVE_LOCKS_REQUIRED(lock_);
};

}  // namespace jit

}  // namespace art

#endif  // ART_COMPILER_JIT_JIT_H_
