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

#ifndef ART_RUNTIME_TI_ENV_H_
#define ART_RUNTIME_TI_ENV_H_

#include "globals.h"
#include "thread.h"
#include "java_vm_ext.h"
#include "jni_env_ext.h"


namespace art {
namespace ti {

// The environment for the tool interface
class Env {
 public:
  static void* AllocateForTiEnv(size_t size) {
    return static_cast<void*>(malloc(size));
  }

  // TODO Maybe put all the jvmti functions into a class so we can friend it.
  // The thread this TiEnv is on.
  Thread* const self_;

  // The VM this TiEnv is on.
  JavaVMExt* const vm_;

  JNIEnvExt* const env_;

  bool IsValid() {
    // TODO
    return true;
  }

  Thread* Self() {
    return self_;
  }

  JNIEnvExt* JniEnv() {
    return env_;
  }
};

}  // namespace ti
}  // namespace art

#endif  // ART_RUNTIME_TI_ENV_H_
