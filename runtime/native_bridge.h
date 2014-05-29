/*
 * Copyright (C) 2014 The Android Open Source Project
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

#ifndef ART_RUNTIME_NATIVE_BRIDGE_H_
#define ART_RUNTIME_NATIVE_BRIDGE_H_

#include "jni.h"
#include <string>

namespace art {

class NativeBridgeInterfaceHandle;

static constexpr bool kAlwaysUseNativeBridge = true;
typedef void (*INVOKE_FN_PTR)(void);

class NativeBridgeInterface {
 public:
  static bool IsAvailable() {
    return available_;
  }

  static bool AlwaysUseBridge() {
    return kAlwaysUseNativeBridge;
  }

  static void Initialize(std::string& native_bridge_library);

  static void* GetInvokeFunctionPointer();

  static bool IsNeeded(void* function_ptr);
  static bool IsSupported(const char* lib_path);

  static void* DlOpen(const char* lib_path, int flags);
  static void* DlSym(void* handle, const char* symbol);

  static int JniOnLoad(void* function_ptr, JavaVM* vm, void* args);

 private:
  static bool available_;
  static NativeBridgeInterfaceHandle* bridge_;
};

}  // namespace art

#endif  // ART_RUNTIME_NATIVE_BRIDGE_H_
