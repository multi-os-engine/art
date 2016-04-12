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

#ifndef ART_RUNTIME_JIT_JIT_INSTRUMENTATION_H_
#define ART_RUNTIME_JIT_JIT_INSTRUMENTATION_H_

#include <unordered_map>

#include "instrumentation.h"

#include "atomic.h"
#include "base/macros.h"
#include "base/mutex.h"
#include "gc_root.h"
#include "jni.h"
#include "object_callbacks.h"
#include "thread_pool.h"

namespace art {
namespace mirror {
  class Object;
  class Throwable;
}  // namespace mirror
class ArtField;
class ArtMethod;
union JValue;
class Thread;

namespace jit {
static constexpr int16_t kJitCheckForOSR = -1;
static constexpr int16_t kJitHotnessDisabled = -2;

}  // namespace jit
}  // namespace art

#endif  // ART_RUNTIME_JIT_JIT_INSTRUMENTATION_H_
