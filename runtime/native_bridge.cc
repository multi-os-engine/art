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
#include <dlfcn.h>
#include <stdio.h>
#include "native_bridge.h"
#include "base/mutex.h"
#include "thread.h"
#include "ScopedLocalRef.h"
#include "object_utils.h"
#include "mirror/art_method-inl.h"
#include "scoped_thread_state_change.h"

#ifdef HAVE_ANDROID_OS
#include "cutils/properties.h"
#endif

namespace art {

bool      NativeBridge::initialized_ = false;
nb_itf_t* NativeBridge::nb_itf_ = nullptr;
vm_itf_t  NativeBridge::vm_itf_ = { nullptr, GetMethodShorty, GetNativeMethodCount, GetNativeMethods };
Mutex     NativeBridge::lock_("native bridge lock");

bool NativeBridge::init() {
  if (initialized_) {
    return true;
  } else {
    MutexLock mu(Thread::Current(), lock_);

    if (!initialized_) {
      const char* libnb_path = DEFAULT_NATIVE_BRIDGE;
#ifdef HAVE_ANDROID_OS
      char prop_buf[PROP_VALUE_MAX];
      property_get(PROP_ENABLE_NAIVE_BRIDGE, prop_buf, "false");
      if (strcmp(prop_buf, "true") != 0)
        return false;

      // If prop persist.native.bridge set, overwrite the default name
      int name_len = property_get(PROP_NATIVE_BRIDGE, prop_buf, DEFAULT_NATIVE_BRIDGE);
      if (name_len > 0)
        libnb_path = prop_buf;
#endif
      void* handle = dlopen(libnb_path, RTLD_LAZY);
      if (handle == nullptr)
        return false;

      nb_itf_t* nb_itf = reinterpret_cast<nb_itf_t*>(dlsym(handle, NATIVE_BRIDGE_ITF));
      if (nb_itf == nullptr) {
        dlclose(handle);
        return false;
      }
      nb_itf_ = nb_itf;

      nb_itf_->initialize(&vm_itf_);
      initialized_ = true;
    }
  }
  return initialized_;
}

void* NativeBridge::loadLibrary(const char* libpath, int flag) {
  if (init())
    return nb_itf_->loadLibrary(libpath, flag);
  return nullptr;
}

void* NativeBridge::getTrampoline(void* handle, const char* name, const char* shorty, uint32_t len) {
  if (init())
    return nb_itf_->getTrampoline(handle, name, shorty, len);
  return nullptr;
}

bool  NativeBridge::isSupported(const char* libpath) {
  if (init())
    return nb_itf_->isSupported(libpath);
  return false;
}

extern "C" const char* GetMethodShorty(JNIEnv* env, jmethodID mid) {
  ScopedObjectAccess soa(env);
  StackHandleScope<1> scope(soa.Self());
  mirror::ArtMethod* m = soa.DecodeMethod(mid);
  MethodHelper mh(scope.NewHandle(m));
  return mh.GetShorty();
}

extern "C" int GetNativeMethodCount(JNIEnv* env, jclass clazz) {
  if (clazz == nullptr)
    return 0;

  ScopedObjectAccess soa(env);
  mirror::Class* c = soa.Decode<mirror::Class*>(clazz);

  size_t method_count = 0;
  for (size_t i = 0; i < c->NumDirectMethods(); ++i) {
    mirror::ArtMethod* m = c->GetDirectMethod(i);
    if (m->IsNative())
      method_count++;
  }
  for (size_t i = 0; i < c->NumVirtualMethods(); ++i) {
    mirror::ArtMethod* m = c->GetVirtualMethod(i);
    if (m->IsNative())
      method_count++;
  }
  return method_count;
}

extern "C" int GetNativeMethods(JNIEnv* env, jclass clazz, JNINativeMethod* methods,
                                uint32_t method_count) {
  if ((clazz == nullptr) || (methods == nullptr))
    return 0;

  ScopedObjectAccess soa(env);
  mirror::Class* c = soa.Decode<mirror::Class*>(clazz);

  size_t count = 0;
  for (size_t i = 0; i < c->NumDirectMethods(); ++i) {
    mirror::ArtMethod* m = c->GetDirectMethod(i);
    if (m->IsNative() && count < method_count) {
      methods[count].name = m->GetName();
      methods[count].signature = m->GetShorty();
      methods[count].fnPtr = const_cast<void*>(m->GetNativeMethod());
      count++;
    }
  }
  for (size_t i = 0; i < c->NumVirtualMethods(); ++i) {
    mirror::ArtMethod* m = c->GetVirtualMethod(i);
    if (m->IsNative() && count < method_count) {
      methods[count].name = m->GetName();
      methods[count].signature = m->GetShorty();
      methods[count].fnPtr = const_cast<void*>(m->GetNativeMethod());
      count++;
    }
  }
  return count;
}

};  // namespace art
