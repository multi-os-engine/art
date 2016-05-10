/*
 * Copyright (C) 2016 The Android Open Source Project
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

#ifndef ART_COMPILER_OPTIMIZING_DEX_CACHE_ARRAY_FIXUPS_MIPS_H_
#define ART_COMPILER_OPTIMIZING_DEX_CACHE_ARRAY_FIXUPS_MIPS_H_

#include "nodes.h"
#include "optimization.h"

namespace art {
namespace mips {

class DexCacheArrayFixups : public HOptimization {
 public:
  DexCacheArrayFixups(HGraph* graph, OptimizingCompilerStats* stats)
      : HOptimization(graph, "dex_cache_array_fixups_mips", stats) {}

  void Run() OVERRIDE;
};

}  // namespace mips
}  // namespace art

#endif  // ART_COMPILER_OPTIMIZING_DEX_CACHE_ARRAY_FIXUPS_MIPS_H_
