/*
 * Copyright (C) 2008 The Android Open Source Project
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

#include "java_lang_Runtime.h"

#include <dlfcn.h>
#include <limits.h>
#include <unistd.h>
#include <fcntl.h>

#include "gc/heap.h"
#include "handle_scope-inl.h"
#include "jni_internal.h"
#include "mirror/class_loader.h"
#include "runtime.h"
#include "scoped_thread_state_change.h"
#include "ScopedUtfChars.h"
#include "ScopedFd.h"
#include "verify_object-inl.h"

#include <sstream>
#include <ziparchive/zip_archive.h>
#ifdef HAVE_ANDROID_OS
#include "cutils/properties.h"
#ifdef __LP64__
#define CPU_ABI_LIST_PROPERTY "ro.product.cpu.abilist64"
#else
#define CPU_ABI_LIST_PROPERTY "ro.product.cpu.abilist32"
#endif
#endif

namespace art {

static void Runtime_gc(JNIEnv*, jclass) {
  if (Runtime::Current()->IsExplicitGcDisabled()) {
      LOG(INFO) << "Explicit GC skipped.";
      return;
  }
  Runtime::Current()->GetHeap()->CollectGarbage(false);
}

[[noreturn]] static void Runtime_nativeExit(JNIEnv*, jclass, jint status) {
  LOG(INFO) << "System.exit called, status: " << status;
  Runtime::Current()->CallExitHook(status);
  exit(status);
}

#ifdef HAVE_ANDROID_OS
static std::vector<std::string> gCpuAbilist;
static std::vector<std::string> gDexPathVector;

static void InitCpuAbilist() {
  if (!gCpuAbilist.empty()) {
    return;
  }

  std::unique_ptr<char[]> cpu_abilist_str(new char[PROPERTY_VALUE_MAX]);
  property_get(CPU_ABI_LIST_PROPERTY, cpu_abilist_str.get(), "");

  char* separator;
  char* current_str = cpu_abilist_str.get();
  while ((separator = strchr(current_str, ',')) != nullptr) {
    *separator = '\0';
    if (strlen(current_str) > 0) {
      gCpuAbilist.push_back(std::string(current_str));
    }
    current_str = separator + 1;
  }

  if (strlen(current_str) > 0) {
    gCpuAbilist.push_back(std::string(current_str));
  }

  if (gCpuAbilist.empty()) {
    LOG(ERROR) << "Invalid " << CPU_ABI_LIST_PROPERTY << " property: " << cpu_abilist_str.get() << ". Won't be able to load libraries from apk";
    return;
  }
}

static void UpdateDexPath(const char* path) {
  gDexPathVector.clear();

  if (path == nullptr) {
    return;
  }

  InitCpuAbilist();

  char* dex_path = strdup(path);
  std::unique_ptr<char> dex_path_guard(dex_path);

  char* separator;
  while ((separator = strchr(dex_path, ':')) != nullptr) {
    *separator = '\0';
    if (strlen(dex_path) > 0) {
      gDexPathVector.push_back(std::string(dex_path));
    }
    dex_path = separator + 1;
  }

  if (strlen(dex_path) > 0) {
    gDexPathVector.push_back(std::string(dex_path));
  }
}

static int ApkLookupFn(const char* filename, int* fd, off64_t* offset, int* close_file) {
  if (strchr(filename, '/') != nullptr) {
    return -1;
  }

  for (auto abi : gCpuAbilist) {
    for (auto path : gDexPathVector) {
      ScopedFd zip_fd(TEMP_FAILURE_RETRY(open(path.c_str(), O_RDONLY | O_CLOEXEC)));
      if (zip_fd.get() == -1) {
        continue;
      }

      ZipArchiveHandle zip_handle = nullptr;
      auto zip_guard_fn = [](ZipArchiveHandle* handle) {
        if (*handle != nullptr) {
          CloseArchive(*handle);
        }
      };
      std::unique_ptr<ZipArchiveHandle, decltype(zip_guard_fn)> zip_guard(&zip_handle, zip_guard_fn);

      if (OpenArchiveFd(zip_fd.get(), nullptr, &zip_handle, false) != 0) {
        continue;
      }

      std::stringstream ss;
      ss << "lib/" << abi << "/" << filename;
      std::string entry_name = ss.str();

      ZipEntry entry;
      if (FindEntry(zip_handle, ZipEntryName(entry_name.c_str()), &entry) != 0 || entry.method != kCompressStored) {
        continue;
      }

      *offset = entry.offset;
      *fd = zip_fd.release();
      *close_file = true;

      return 0;
    }
  }

  return -1;
}
#else
static void UpdateDexPath(const char*) { }
static int ApkLookupFn(const char*, int*, off_t*, int*) {
  return -1;
}
#endif

static jstring Runtime_nativeLoad(JNIEnv* env, jclass, jstring javaFilename, jobject javaLoader, jstring javaLdLibraryPath, jstring javaDexPath) {
  ScopedUtfChars filename(env, javaFilename);
  if (filename.c_str() == nullptr) {
    return nullptr;
  }

  if (javaLdLibraryPath != nullptr) {
    ScopedUtfChars ldLibraryPath(env, javaLdLibraryPath);
    if (ldLibraryPath.c_str() == nullptr) {
      return nullptr;
    }
    void* sym = dlsym(RTLD_DEFAULT, "android_update_LD_LIBRARY_PATH");
    if (sym != nullptr) {
      typedef void (*Fn)(const char*);
      Fn android_update_LD_LIBRARY_PATH = reinterpret_cast<Fn>(sym);
      android_update_LD_LIBRARY_PATH(ldLibraryPath.c_str());
    } else {
      LOG(ERROR) << "android_update_LD_LIBRARY_PATH not found; .so dependencies will not work!";
    }
  }

  std::string error_msg;
  typedef int (*lookup_fn_t)(const char* filename, int* fd, off64_t* offset, int* close_flag);
  typedef void (*android_update_lookup_fn_t)(lookup_fn_t);

  android_update_lookup_fn_t android_update_lookup_fn = nullptr;
  auto guard_fn = [](android_update_lookup_fn_t* fn) {
    if (*fn != nullptr) {
      (*fn)(nullptr);
    }
  };

  std::unique_ptr<android_update_lookup_fn_t, decltype(guard_fn)> guard(&android_update_lookup_fn, guard_fn);

  if (javaDexPath != nullptr) {
    ScopedUtfChars dexPath(env, javaDexPath);
    void* sym = dlsym(RTLD_DEFAULT, "android_update_lookup_fn");
    if (sym != nullptr) {
      UpdateDexPath(dexPath.c_str());
      android_update_lookup_fn = reinterpret_cast<android_update_lookup_fn_t>(sym);
      android_update_lookup_fn(ApkLookupFn);
    } else {
      LOG(WARNING) << "android_update_lookup_fn not found; .so dependencies may not work!";
    }
  }

  std::string detail;
  {
    JavaVMExt* vm = Runtime::Current()->GetJavaVM();
    bool success = vm->LoadNativeLibrary(env, filename.c_str(), javaLoader, &error_msg);
    if (success) {
      return nullptr;
    }
  }

  // Don't let a pending exception from JNI_OnLoad cause a CheckJNI issue with NewStringUTF.
  env->ExceptionClear();
  return env->NewStringUTF(error_msg.c_str());
}

static jlong Runtime_maxMemory(JNIEnv*, jclass) {
  return Runtime::Current()->GetHeap()->GetMaxMemory();
}

static jlong Runtime_totalMemory(JNIEnv*, jclass) {
  return Runtime::Current()->GetHeap()->GetTotalMemory();
}

static jlong Runtime_freeMemory(JNIEnv*, jclass) {
  return Runtime::Current()->GetHeap()->GetFreeMemory();
}

static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(Runtime, freeMemory, "!()J"),
  NATIVE_METHOD(Runtime, gc, "()V"),
  NATIVE_METHOD(Runtime, maxMemory, "!()J"),
  NATIVE_METHOD(Runtime, nativeExit, "(I)V"),
  NATIVE_METHOD(Runtime, nativeLoad, "(Ljava/lang/String;Ljava/lang/ClassLoader;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;"),
  NATIVE_METHOD(Runtime, totalMemory, "!()J"),
};

void register_java_lang_Runtime(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("java/lang/Runtime");
}

}  // namespace art
