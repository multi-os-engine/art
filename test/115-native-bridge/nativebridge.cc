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
#include <vector>

#include "jni.h"
#include "stdio.h"
#include "string.h"
#include "unistd.h"

#include "native_bridge.h"


// Native bridge interfaces...

struct NativeBridgeArtCallbacks {
  const char* (*getMethodShorty)(JNIEnv* env, jmethodID mid);
  int (*getNativeMethodCount)(JNIEnv* env, jclass clazz);
  int (*getNativeMethods)(JNIEnv* env, jclass clazz, JNINativeMethod* methods,
       uint32_t method_count);
};

struct NativeBridgeCallbacks {
  bool (*initialize)(NativeBridgeArtCallbacks* art_cbs);
  void* (*loadLibrary)(const char* libpath, int flag);
  void* (*getTrampoline)(void* handle, const char* name, const char* shorty, uint32_t len);
  bool (*isSupported)(const char* libpath);
};

struct NativeBridgeMethod {
  const char* name;
  const char* signature;
  void* fnPtr;
  void* trampoline;
};

static NativeBridgeMethod* find_native_brdige_method(const char *name);
static NativeBridgeArtCallbacks* art_itf;

static jint trampoline_JNI_OnLoad(JavaVM* vm, void* reserved) {
  typedef jint (*FnPtr_t)(JavaVM*, void*);
  JNIEnv* env = nullptr;
  jclass klass;
  jmethodID mid;
  int i, count1, count2;
  const char* shorty = nullptr;
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>(find_native_brdige_method("JNI_OnLoad")->fnPtr);

  vm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);
  if (env == nullptr) {
    return 0;
  }

  klass = env->FindClass("Main");
  if (klass != nullptr) {
    count1 = art_itf->getNativeMethodCount(env, klass);
    JNINativeMethod* methods = reinterpret_cast<JNINativeMethod*>
        (malloc(sizeof(JNINativeMethod) * count1));
    if (methods == nullptr) {
      return 0;
    }
    count2 = art_itf->getNativeMethods(env, klass, methods, count1);
    if (count1 == count2) {
      printf("JNI function count is %d\n", count1);
    }

    for (i = 0; i < count1; i++) {
      NativeBridgeMethod* nb_method = find_native_brdige_method(methods[i].name);
      if (nb_method != nullptr) {
        mid = env->GetStaticMethodID(klass, methods[i].name, nb_method->signature);
        if (mid != nullptr) {
          shorty = art_itf->getMethodShorty(env, mid);
          if (strcmp(shorty, methods[i].signature) == 0) {
            printf("JNI: name is %s, signature is %s, shorty is %s\n",
                   methods[i].name, nb_method->signature, shorty);
          }
        }
      }
    }
    free(methods);
  }

  printf("%s called!\n", __FUNCTION__);
  return fnPtr(vm, reserved);
}

static void trampoline_Java_Main_testFindClassOnAttachedNativeThread(JNIEnv* env,
                                                                     jclass klass) {
  typedef void (*FnPtr_t)(JNIEnv*, jclass);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>
    (find_native_brdige_method("testFindClassOnAttachedNativeThread")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass);
}

static void trampoline_Java_Main_testFindFieldOnAttachedNativeThreadNative(JNIEnv* env,
                                                                           jclass klass) {
  typedef void (*FnPtr_t)(JNIEnv*, jclass);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>
    (find_native_brdige_method("testFindFieldOnAttachedNativeThreadNative")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass);
}

static void trampoline_Java_Main_testCallStaticVoidMethodOnSubClassNative(JNIEnv* env,
                                                                          jclass klass) {
  typedef void (*FnPtr_t)(JNIEnv*, jclass);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>
    (find_native_brdige_method("testCallStaticVoidMethodOnSubClassNative")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass);
}

static jobject trampoline_Java_Main_testGetMirandaMethodNative(JNIEnv* env, jclass klass) {
  typedef jobject (*FnPtr_t)(JNIEnv*, jclass);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>
    (find_native_brdige_method("testGetMirandaMethodNative")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass);
}

static void trampoline_Java_Main_testZeroLengthByteBuffers(JNIEnv* env, jclass klass) {
  typedef void (*FnPtr_t)(JNIEnv*, jclass);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>
    (find_native_brdige_method("testZeroLengthByteBuffers")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass);
}

static jbyte trampoline_Java_Main_byteMethod(JNIEnv* env, jclass klass, jbyte b1, jbyte b2,
                                             jbyte b3, jbyte b4, jbyte b5, jbyte b6,
                                             jbyte b7, jbyte b8, jbyte b9, jbyte b10) {
  typedef jbyte (*FnPtr_t)(JNIEnv*, jclass, jbyte, jbyte, jbyte, jbyte, jbyte,
                           jbyte, jbyte, jbyte, jbyte, jbyte);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>(find_native_brdige_method("byteMethod")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10);
}

static jshort trampoline_Java_Main_shortMethod(JNIEnv* env, jclass klass, jshort s1, jshort s2,
                                               jshort s3, jshort s4, jshort s5, jshort s6,
                                               jshort s7, jshort s8, jshort s9, jshort s10) {
  typedef jshort (*FnPtr_t)(JNIEnv*, jclass, jshort, jshort, jshort, jshort, jshort,
                            jshort, jshort, jshort, jshort, jshort);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>(find_native_brdige_method("shortMethod")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass, s1, s2, s3, s4, s5, s6, s7, s8, s9, s10);
}

static jboolean trampoline_Java_Main_booleanMethod(JNIEnv* env, jclass klass, jboolean b1,
                                                   jboolean b2, jboolean b3, jboolean b4,
                                                   jboolean b5, jboolean b6, jboolean b7,
                                                   jboolean b8, jboolean b9, jboolean b10) {
  typedef jboolean (*FnPtr_t)(JNIEnv*, jclass, jboolean, jboolean, jboolean, jboolean, jboolean,
                              jboolean, jboolean, jboolean, jboolean, jboolean);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>(find_native_brdige_method("booleanMethod")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10);
}

static jchar trampoline_Java_Main_charMethod(JNIEnv* env, jclass klass, jchar c1, jchar c2,
                                             jchar c3, jchar c4, jchar c5, jchar c6,
                                             jchar c7, jchar c8, jchar c9, jchar c10) {
  typedef jchar (*FnPtr_t)(JNIEnv*, jclass, jchar, jchar, jchar, jchar, jchar,
                           jchar, jchar, jchar, jchar, jchar);
  FnPtr_t fnPtr = reinterpret_cast<FnPtr_t>(find_native_brdige_method("charMethod")->fnPtr);
  printf("%s called!\n", __FUNCTION__);
  return fnPtr(env, klass, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10);
}

NativeBridgeMethod native_bridge_methods[] = {
  { "JNI_OnLoad", "", nullptr,
    reinterpret_cast<void*>(trampoline_JNI_OnLoad) },
  { "booleanMethod", "(ZZZZZZZZZZ)Z", nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_booleanMethod) },
  { "byteMethod", "(BBBBBBBBBB)B", nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_byteMethod) },
  { "charMethod", "(CCCCCCCCCC)C", nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_charMethod) },
  { "shortMethod", "(SSSSSSSSSS)S", nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_shortMethod) },
  { "testCallStaticVoidMethodOnSubClassNative", "()V", nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_testCallStaticVoidMethodOnSubClassNative) },
  { "testFindClassOnAttachedNativeThread", "()V", nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_testFindClassOnAttachedNativeThread) },
  { "testFindFieldOnAttachedNativeThreadNative", "()V", nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_testFindFieldOnAttachedNativeThreadNative) },
  { "testGetMirandaMethodNative", "()Ljava/lang/reflect/Method;", nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_testGetMirandaMethodNative) },
  { "testZeroLengthByteBuffers", "()V", nullptr,
    reinterpret_cast<void*>(trampoline_Java_Main_testZeroLengthByteBuffers) },
};

static NativeBridgeMethod* find_native_brdige_method(const char *name) {
  const char* pname = name;
  if (strncmp(name, "Java_Main_", 10) == 0) {
    pname += 10;
  }

  for (size_t i = 0; i < sizeof(native_bridge_methods) / sizeof(native_bridge_methods[0]); i++) {
    if (strcmp(pname, native_bridge_methods[i].name) == 0) {
      return &native_bridge_methods[i];
    }
  }
  return nullptr;
}

// NativeBridgeCallbacks implementations
extern "C" bool native_bridge_initialize(NativeBridgeArtCallbacks* art_cbs) {
  if (art_cbs != nullptr) {
    art_itf = art_cbs;
    printf("Native bridge initialized.\n");
  }
  return true;
}

extern "C" void* native_bridge_loadLibrary(const char* libpath, int flag) {
  size_t len = strlen(libpath);
  char* tmp = new char[len + 10];
  strncpy(tmp, libpath, len);
  tmp[len - 3] = '2';
  tmp[len - 2] = '.';
  tmp[len - 1] = 's';
  tmp[len] = 'o';
  tmp[len + 1] = 0;
  void* handle = dlopen(tmp, flag);
  delete[] tmp;

  if (handle == nullptr) {
    printf("Handle = nullptr!\n");
    printf("Was looking for %s.\n", libpath);
    printf("Error = %s.\n", dlerror());
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
      printf("Current working dir: %s\n", cwd);
    }
  }
  return handle;
}

extern "C" void* native_bridge_getTrampoline(void* handle, const char* name, const char* shorty,
                                             uint32_t len) {
  printf("Getting trampoline. name is %s, shorty is %s\n", name, shorty);

  // The name here is actually the JNI name, so we can directly do the lookup.
  void* sym = dlsym(handle, name);
  NativeBridgeMethod* method = find_native_brdige_method(name);
  if (method == nullptr)
    return nullptr;
  method->fnPtr = sym;

  return method->trampoline;
}

extern "C" bool native_bridge_isSupported(const char* libpath) {
  printf("Checking for support.\n");

  if (libpath == nullptr) {
    return false;
  }
  // We don't want to hijack javacore. So we should get libarttest...
  return strcmp(libpath, "libjavacore.so") != 0;
}

NativeBridgeCallbacks NativeBridgeItf {
  .initialize = &native_bridge_initialize,
  .loadLibrary = &native_bridge_loadLibrary,
  .getTrampoline = &native_bridge_getTrampoline,
  .isSupported = &native_bridge_isSupported
};
