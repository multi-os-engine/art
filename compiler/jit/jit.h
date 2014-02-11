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

#include "compiler_callbacks.h"
#include "compiled_method.h"
#include "dex/verification_results.h"
#include "dex/quick/dex_file_to_method_inliner_map.h"
#include "driver/compiler_driver.h"
#include "driver/compiler_options.h"
#include "instruction_set.h"
#include "oat_file.h"

namespace art {

namespace mirror {
class ArtMethod;
}

namespace jit {

class Jit {
 public:
  Jit();

  ~Jit();

  bool CompileMethod(Thread* self, mirror::ArtMethod* method)
      SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);

  CompilerCallbacks* GetCompilerCallbacks() const;

 private:
  std::unique_ptr<CompilerOptions> compiler_options_;
  std::unique_ptr<CumulativeLogger> cumulative_logger_;
  std::unique_ptr<VerificationResults> verification_results_;
  std::unique_ptr<DexFileToMethodInlinerMap> method_inliner_map_;
  std::unique_ptr<CompilerCallbacks> callbacks_;
  std::unique_ptr<CompilerDriver> compiler_driver_;

  bool MakeExecutable(mirror::ArtMethod* method) SHARED_LOCKS_REQUIRED(Locks::mutator_lock_);
  void MakeExecutable(const std::vector<uint8_t>& code);
  void MakeExecutable(const void* code_start, size_t code_length);
  OatFile::OatMethod CreateOatMethod(const void* code,
                                     const size_t frame_size_in_bytes,
                                     const uint32_t core_spill_mask,
                                     const uint32_t fp_spill_mask,
                                     const uint8_t* mapping_table,
                                     const uint8_t* vmap_table,
                                     const uint8_t* gc_map);
};

}  // namespace jit

}  // namespace art

#endif  // ART_COMPILER_JIT_JIT_H_
