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

#include "native_bridge.h"

#include "handle_scope-inl.h"
#include "mirror/art_method.h"
#include "object_utils.h"
#include "scoped_thread_state_change.h"

#include <dlfcn.h>
#include "nativebridge/NativeBridge.h"

namespace art {

class NativeBridgeInterfaceHandle {
 public:
  nativebridge::nb_vm_itf_t* handle_;
};

// In the beginning, nothing's available.
bool NativeBridgeInterface::available_ = false;
NativeBridgeInterfaceHandle* NativeBridgeInterface::bridge_ = nullptr;

extern "C" const char * GetMethodShorty(JNIEnv* env, jmethodID mid) {
  ScopedObjectAccess soa(env);
  StackHandleScope<1> scope(soa.Self());
  mirror::ArtMethod* m = soa.DecodeMethod(mid);
  MethodHelper mh(scope.NewHandle(m));
  return mh.GetShorty();
}

void NativeBridgeInterface::Initialize(std::string& native_bridge_library) {
  if (native_bridge_library.length() > 0) {
    void * handle = dlopen(native_bridge_library.c_str(), RTLD_LAZY);

    if (handle != nullptr) {
      nativebridge::nb_vm_itf_t* nb_vm_itf = (nativebridge::nb_vm_itf_t*)dlsym(handle,
                                                                               NB_VM_ITF_SYM);

      if (nb_vm_itf != nullptr) {
        struct env_t {
          void *logger;
          void *getShorty;
        } env;

        env.logger = nullptr;
        env.getShorty = reinterpret_cast<void*>(GetMethodShorty);

        if (nb_vm_itf->init(&env)) {
          NativeBridgeInterface::bridge_ = new NativeBridgeInterfaceHandle();
          NativeBridgeInterface::bridge_->handle_ = nb_vm_itf;
          NativeBridgeInterface::available_ = true;
        } else {
          nb_vm_itf = nullptr;
        }
      }

      if (nb_vm_itf == nullptr) {
        dlclose(handle);
      }
    }
  }
}

void* NativeBridgeInterface::GetInvokeFunctionPointer() {
  if (available_) {
    return reinterpret_cast<void*>(NativeBridgeInterface::bridge_->handle_->invoke);
  }
  return nullptr;
}

bool NativeBridgeInterface::IsNeeded(void* function_ptr) {
  if (available_) {
    return NativeBridgeInterface::bridge_->handle_->isNeeded(function_ptr);
  }
  return false;
}

bool NativeBridgeInterface::IsSupported(const char* lib_path) {
  if (available_) {
      return NativeBridgeInterface::bridge_->handle_->isSupported(lib_path);
  }
  return false;
}

void* NativeBridgeInterface::DlOpen(const char* lib_path, int flags) {
  if (available_) {
    return NativeBridgeInterface::bridge_->handle_->dlopen(lib_path, flags);
  }
  return nullptr;
}

void* NativeBridgeInterface::DlSym(void* handle, const char* symbol) {
  if (available_) {
    return NativeBridgeInterface::bridge_->handle_->dlsym(handle, symbol);
  }
  return nullptr;
}

int NativeBridgeInterface::JniOnLoad(void* function_ptr, JavaVM* vm, void* args) {
  if (available_) {
    return NativeBridgeInterface::bridge_->handle_->jniOnLoad(function_ptr, vm, args);
  }
  return 0;
}

}  // namespace art
