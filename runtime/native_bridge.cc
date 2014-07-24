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
#include "jni.h"
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

// Default library name for native-brdige
#define DEFAULT_NATIVE_BRIDGE "libnativebridge.so"
// Property that defines the library name of native-bridge
#define PROP_NATIVE_BRIDGE "persist.native.bridge"
// Property that enables native-bridge
#define PROP_ENABLE_NATIVE_BRIDGE "persist.enable.native.bridge"
// The symbol name exposed by native-bridge with the type of NativeBridgeCallbacks
#define NATIVE_BRIDGE_ITF "NativeBridgeItf"

namespace art {

// ART interfaces to native-bridge
struct NativeBridgeArtCallbacks {
  // Log utility, reserve unused
  int (*logger)(int prio, const char* tag, const char* fmt, ...);
  // Get shorty of a Java method. The shorty is supposed to be persistent in memory.
  //
  // Parameters:
  //   env [IN] pointer to JNIenv
  //   mid [IN] Java methodID
  // Returns:
  //   short descriptor for method
  const char* (*getMethodShorty)(JNIEnv* env, jmethodID mid);
  // Get number of native methods for specified class.
  //
  // Parameters:
  //   env [IN] pointer to JNIenv
  //   clazz [IN] Java class object
  // Returns:
  //   number of native methods
  int (*getNativeMethodCount)(JNIEnv* env, jclass clazz);
  // Get at most 'method_count' native methods for specified class 'clazz'. Results are outputed
  // via 'methods' [OUT]. The signature pointer in JNINativeMethod is reused as the method shorty.
  //
  // Parameters:
  //   env [IN] pointer to JNIenv
  //   clazz [IN] Java class object
  //   methods [OUT] array of method with the name, shorty, and fnPtr
  //   method_count [IN] max number of elements in methods
  // Returns:
  //   number of method it actually wrote to methods
  int (*getNativeMethods)(JNIEnv* env, jclass clazz, JNINativeMethod* methods, uint32_t method_count);
};

// Native-bridge interfaces to ART
struct NativeBridgeCallbacks {
  // Initialize native-bridge. Native-bridge's internal implementation must ensure MT safety
  // and that native-bridge is initialized only once. OK to call this interface for already
  // initialized native-bridge.
  //
  // Parameters:
  //   art_cbs [IN] the pointer to NativeBridgeArtCallbacks.
  // Returns:
  //   TRUE for initialization success, FALSE for initialization fail.
  bool (*initialize)(NativeBridgeArtCallbacks* art_cbs);
  // Load a shared library that is supported by the native-bridge.
  //
  // Parameters:
  //   libpath [IN] path to the shared library
  //   flag [IN] the stardard RTLD_XXX defined in bionic dlfcn.h
  // Returns:
  //   The opaque handle of shared library if sucessful, otherwise NULL
  void* (*loadLibrary)(const char* libpath, int flag);
  // Get a native-bridge trampoline for specified native method. The trampoline has same
  // sigature as the native method.
  //
  // Parameters:
  //   handle [IN] the handle returned from loadLibrary
  //   shorty [IN] short descriptor of native method
  //   len [IN] length of shorty
  // Returns:
  //   address of trampoline of successful, otherwise NULL
  void* (*getTrampoline)(void* handle, const char* name, const char* shorty, uint32_t len);
  // Check whether native library is valid and is for an ABI that is supported by native-bridge.
  //
  // Parameters:
  //   libpath [IN] path to the shared library
  // Returns:
  //   TRUE if library is supported by native-bridge, FALSE otherwise
  bool (*isSupported)(const char* libpath);
};

static const char* GetMethodShorty(JNIEnv* env, jmethodID mid) {
  ScopedObjectAccess soa(env);
  StackHandleScope<1> scope(soa.Self());
  mirror::ArtMethod* m = soa.DecodeMethod(mid);
  MethodHelper mh(scope.NewHandle(m));
  return mh.GetShorty();
}

static int GetNativeMethodCount(JNIEnv* env, jclass clazz) {
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

static int GetNativeMethods(JNIEnv* env, jclass clazz, JNINativeMethod* methods,
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

NativeBridgeArtCallbacks NativeBridgeArtItf = {
  nullptr,
  GetMethodShorty,
  GetNativeMethodCount,
  GetNativeMethods
};

bool NativeBridge::Init() {
  MutexLock mu(Thread::Current(), lock_);

  if (!initialized_) {
    const char* libnb_path = DEFAULT_NATIVE_BRIDGE;
#ifdef HAVE_ANDROID_OS
    char prop_buf[PROP_VALUE_MAX];
    property_get(PROP_ENABLE_NATIVE_BRIDGE, prop_buf, "false");
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

    callbacks_ = reinterpret_cast<NativeBridgeCallbacks*>(dlsym(handle, NATIVE_BRIDGE_ITF));
    if (callbacks_ == nullptr) {
      dlclose(handle);
      return false;
    }

    callbacks_->initialize(&NativeBridgeArtItf);
    initialized_ = true;
  }

  return initialized_;
}

void* NativeBridge::LoadLibrary(const char* libpath, int flag) {
  if (Init())
    return callbacks_->loadLibrary(libpath, flag);
  return nullptr;
}

void* NativeBridge::GetTrampoline(void* handle, const char* name, const char* shorty, uint32_t len) {
  if (Init())
    return callbacks_->getTrampoline(handle, name, shorty, len);
  return nullptr;
}

bool NativeBridge::IsSupported(const char* libpath) {
  if (Init())
    return callbacks_->isSupported(libpath);
  return false;
}

bool NativeBridge::initialized_ = false;
Mutex NativeBridge::lock_("native bridge lock");
NativeBridgeCallbacks* NativeBridge::callbacks_ = nullptr;

};  // namespace art
