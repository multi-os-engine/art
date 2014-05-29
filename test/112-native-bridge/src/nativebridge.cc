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

// A simple implementation of the native-bridge interface.

#include <algorithm>
#include <dlfcn.h>
#include "jni.h"
#include <vector>
#include "nativebridge/NativeBridge.h"
#include "stdio.h"
#include "string.h"

static std::vector<void*> symbols;

extern "C" bool native_bridge_init(void* /* args */) {
  return true;
}

extern "C" void* native_bridge_dlopen(const char* libpath, int flag) {
  return dlopen(libpath, flag);
}

extern "C" void* native_bridge_dlsym(void* handle, const char* symbol) {
  void* sym = dlsym(handle, symbol);
  if (sym != nullptr) {
    symbols.push_back(sym);
  }
  return sym;
}

extern "C" bool native_bridge_issupported(const char* libpath) {
  if (libpath == nullptr) {
    return false;
  }
  return strcmp(libpath, "libjavacore.so") != 0;
}

extern "C" jvalue native_bridge_invoke(void* pEnv, void* clazz, int argInfo, int argc,
                                       const int* argv, const char* shorty, void* func) {
  // Assumes this is JniTest.booleanMethod, which returns its first explicit argument.
  jvalue ret;
  ret.z = *reinterpret_cast<const jboolean*>(argv);
  printf("Native bridge invoked.\n");
  return ret;
}

extern "C" int native_bridge_onjniload(void* func, void* jniVm, void* arg) {
  // TODO: Implement.
  typedef int (*JNI_OnLoadFn)(JavaVM*, void*);
  JNI_OnLoadFn jni_on_load = reinterpret_cast<JNI_OnLoadFn>(func);
  return (*jni_on_load)(reinterpret_cast<JavaVM*>(jniVm), arg);
}

extern "C" bool native_bridge_isneeded(void* func) {
  auto it = std::find(symbols.begin(), symbols.end(), func);
  return it != symbols.end();
}

nativebridge::nb_vm_itf_t native_bridge_vm_itf = {
  .init = &native_bridge_init,
  .dlopen = &native_bridge_dlopen,
  .dlsym = &native_bridge_dlsym,
  .invoke = reinterpret_cast<nativebridge::NB_ITF_INVOKE>(&native_bridge_invoke),
  .jniOnLoad = &native_bridge_onjniload,
  .isNeeded = &native_bridge_isneeded,
  .isSupported = &native_bridge_issupported
};


