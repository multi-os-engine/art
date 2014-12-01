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

#include "dalvik_system_DexFile.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "class_linker.h"
#include "common_throws.h"
#include "dex_file-inl.h"
#include "jni_internal.h"
#include "mirror/class_loader.h"
#include "mirror/object-inl.h"
#include "mirror/string.h"
#include "oat_file_manager.h"
#include "os.h"
#include "profiler.h"
#include "runtime.h"
#include "scoped_thread_state_change.h"
#include "ScopedLocalRef.h"
#include "ScopedUtfChars.h"
#include "utils.h"
#include "well_known_classes.h"
#include "zip_archive.h"

namespace art {

// A smart pointer that provides read-only access to a Java string's UTF chars.
// Unlike libcore's NullableScopedUtfChars, this will *not* throw NullPointerException if
// passed a null jstring. The correct idiom is:
//
//   NullableScopedUtfChars name(env, javaName);
//   if (env->ExceptionCheck()) {
//       return NULL;
//   }
//   // ... use name.c_str()
//
// TODO: rewrite to get rid of this, or change ScopedUtfChars to offer this option.
class NullableScopedUtfChars {
 public:
  NullableScopedUtfChars(JNIEnv* env, jstring s) : mEnv(env), mString(s) {
    mUtfChars = (s != NULL) ? env->GetStringUTFChars(s, NULL) : NULL;
  }

  ~NullableScopedUtfChars() {
    if (mUtfChars) {
      mEnv->ReleaseStringUTFChars(mString, mUtfChars);
    }
  }

  const char* c_str() const {
    return mUtfChars;
  }

  size_t size() const {
    return strlen(mUtfChars);
  }

  // Element access.
  const char& operator[](size_t n) const {
    return mUtfChars[n];
  }

 private:
  JNIEnv* mEnv;
  jstring mString;
  const char* mUtfChars;

  // Disallow copy and assignment.
  NullableScopedUtfChars(const NullableScopedUtfChars&);
  void operator=(const NullableScopedUtfChars&);
};

static jlong DexFile_openDexFileNative(JNIEnv* env, jclass, jstring javaSourceName, jstring javaOutputName, jint) {
  ScopedUtfChars sourceName(env, javaSourceName);
  if (sourceName.c_str() == NULL) {
    return 0;
  }
  NullableScopedUtfChars outputName(env, javaOutputName);
  if (env->ExceptionCheck()) {
    return 0;
  }

  ClassLinker* linker = Runtime::Current()->GetClassLinker();
  std::unique_ptr<std::vector<const DexFile*>> dex_files(new std::vector<const DexFile*>());

  bool success = linker->OpenDexFilesFromOat(sourceName.c_str(), outputName.c_str(),
                                             dex_files.get());

  if (success) {
    return static_cast<jlong>(reinterpret_cast<uintptr_t>(dex_files.release()));
  } else {
    // The vector should be empty after a failed loading attempt.
    DCHECK_EQ(0U, dex_files->size());

    ScopedObjectAccess soa(env);
    return 0;
  }
}

static std::vector<const DexFile*>* toDexFiles(jlong dex_file_address, JNIEnv* env) {
  std::vector<const DexFile*>* dex_files = reinterpret_cast<std::vector<const DexFile*>*>(
      static_cast<uintptr_t>(dex_file_address));
  if (UNLIKELY(dex_files == nullptr)) {
    ScopedObjectAccess soa(env);
    ThrowNullPointerException(NULL, "dex_file == null");
  }
  return dex_files;
}

static void DexFile_closeDexFile(JNIEnv* env, jclass, jlong cookie) {
  std::unique_ptr<std::vector<const DexFile*>> dex_files(toDexFiles(cookie, env));
  if (dex_files.get() == nullptr) {
    return;
  }
  ScopedObjectAccess soa(env);

  size_t index = 0;
  for (const DexFile* dex_file : *dex_files) {
    if (Runtime::Current()->GetClassLinker()->IsDexFileRegistered(*dex_file)) {
      (*dex_files)[index] = nullptr;
    }
    index++;
  }

  STLDeleteElements(dex_files.get());
  // Unique_ptr will delete the vector itself.
}

static jclass DexFile_defineClassNative(JNIEnv* env, jclass, jstring javaName, jobject javaLoader,
                                        jlong cookie) {
  std::vector<const DexFile*>* dex_files = toDexFiles(cookie, env);
  if (dex_files == NULL) {
    VLOG(class_linker) << "Failed to find dex_file";
    return NULL;
  }
  ScopedUtfChars class_name(env, javaName);
  if (class_name.c_str() == NULL) {
    VLOG(class_linker) << "Failed to find class_name";
    return NULL;
  }
  const std::string descriptor(DotToDescriptor(class_name.c_str()));
  const size_t hash(ComputeModifiedUtf8Hash(descriptor.c_str()));
  for (const DexFile* dex_file : *dex_files) {
    const DexFile::ClassDef* dex_class_def = dex_file->FindClassDef(descriptor.c_str(), hash);
    if (dex_class_def != nullptr) {
      ScopedObjectAccess soa(env);
      ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
      class_linker->RegisterDexFile(*dex_file);
      StackHandleScope<1> hs(soa.Self());
      Handle<mirror::ClassLoader> class_loader(
          hs.NewHandle(soa.Decode<mirror::ClassLoader*>(javaLoader)));
      mirror::Class* result = class_linker->DefineClass(soa.Self(), descriptor.c_str(), hash,
                                                        class_loader, *dex_file, *dex_class_def);
      if (result != nullptr) {
        VLOG(class_linker) << "DexFile_defineClassNative returning " << result
                           << " for " << class_name.c_str();
        return soa.AddLocalReference<jclass>(result);
      }
    }
  }
  VLOG(class_linker) << "Failed to find dex_class_def " << class_name.c_str();
  return nullptr;
}

// Needed as a compare functor for sets of const char
struct CharPointerComparator {
  bool operator()(const char *str1, const char *str2) const {
    return strcmp(str1, str2) < 0;
  }
};

// Note: this can be an expensive call, as we sort out duplicates in MultiDex files.
static jobjectArray DexFile_getClassNameList(JNIEnv* env, jclass, jlong cookie) {
  jobjectArray result = nullptr;
  std::vector<const DexFile*>* dex_files = toDexFiles(cookie, env);

  if (dex_files != nullptr) {
    // Push all class descriptors into a set. Use set instead of unordered_set as we want to
    // retrieve all in the end.
    std::set<const char*, CharPointerComparator> descriptors;
    for (const DexFile* dex_file : *dex_files) {
      for (size_t i = 0; i < dex_file->NumClassDefs(); ++i) {
        const DexFile::ClassDef& class_def = dex_file->GetClassDef(i);
        const char* descriptor = dex_file->GetClassDescriptor(class_def);
        descriptors.insert(descriptor);
      }
    }

    // Now create output array and copy the set into it.
    result = env->NewObjectArray(descriptors.size(), WellKnownClasses::java_lang_String, nullptr);
    if (result != nullptr) {
      auto it = descriptors.begin();
      auto it_end = descriptors.end();
      jsize i = 0;
      for (; it != it_end; it++, ++i) {
        std::string descriptor(DescriptorToDot(*it));
        ScopedLocalRef<jstring> jdescriptor(env, env->NewStringUTF(descriptor.c_str()));
        if (jdescriptor.get() == nullptr) {
          return nullptr;
        }
        env->SetObjectArrayElement(result, i, jdescriptor.get());
      }
    }
  }
  return result;
}

// Java: dalvik.system.DexFile.UP_TO_DATE
static const jbyte kUpToDate = 0;
// Java: dalvik.system.DexFile.DEXOPT_NEEDED
static const jbyte kPatchoatNeeded = 1;
// Java: dalvik.system.DexFile.PATCHOAT_NEEDED
static const jbyte kDexoptNeeded = 2;

static jbyte IsDexOptNeededInternal(JNIEnv* env, const char* filename,
    const char* pkgname, const char* instruction_set, const jboolean defer) {

  if ((filename == nullptr) || !OS::FileExists(filename)) {
    LOG(ERROR) << "DexFile_isDexOptNeeded file '" << filename << "' does not exist";
    ScopedLocalRef<jclass> fnfe(env, env->FindClass("java/io/FileNotFoundException"));
    const char* message = (filename == nullptr) ? "<empty file name>" : filename;
    env->ThrowNew(fnfe.get(), message);
    return kUpToDate;
  }

  const InstructionSet target_instruction_set = GetInstructionSetFromString(instruction_set);
  if (target_instruction_set == kNone) {
    ScopedLocalRef<jclass> iae(env, env->FindClass("java/lang/IllegalArgumentException"));
    std::string message(StringPrintf("Instruction set %s is invalid.", instruction_set));
    env->ThrowNew(iae.get(), message.c_str());
    return 0;
  }

  OatFileManager oat_file_manager(filename, target_instruction_set, pkgname);

  // Always treat elements of the bootclasspath as up-to-date.  The
  // fact that code is running at all means that this should be true.
  // TODO [ruhler]: Is this right? I'm skeptical.
  if (oat_file_manager.IsInBootClassPath()) {
    return kUpToDate;
  }

  // TODO: Checking the profile should probably be done in the GetStatus()
  // function. We have it here because GetStatus() should not be copying
  // profile files. But who should be copying profile files?
  if (oat_file_manager.OdexFileIsOutOfDate()) {
    // Needs recompile if profile has changed significantly.
    if (Runtime::Current()->GetProfilerOptions().IsEnabled()) {
      if (oat_file_manager.IsProfileChangeSignificant()) {
        if (!defer) {
          oat_file_manager.CopyProfileFile();
        }
        return kDexoptNeeded;
      } else if (oat_file_manager.ProfileExists()
          && !oat_file_manager.OldProfileExists()) {
        if (!defer) {
          oat_file_manager.CopyProfileFile();
        }
      }
    }
  }

  OatFileManager::Status status = oat_file_manager.GetStatus();
  switch (status) {
    case OatFileManager::kUpToDate: return kUpToDate;
    case OatFileManager::kNeedsRelocation: return kPatchoatNeeded;
    case OatFileManager::kNeedsGeneration: return kDexoptNeeded;
  }
  UNREACHABLE();
}

static jbyte DexFile_isDexOptNeededInternal(JNIEnv* env, jclass, jstring javaFilename,
    jstring javaPkgname, jstring javaInstructionSet, jboolean defer) {
  ScopedUtfChars filename(env, javaFilename);
  if (env->ExceptionCheck()) {
    return 0;
  }

  NullableScopedUtfChars pkgname(env, javaPkgname);

  ScopedUtfChars instruction_set(env, javaInstructionSet);
  if (env->ExceptionCheck()) {
    return 0;
  }

  return IsDexOptNeededInternal(env, filename.c_str(), pkgname.c_str(),
                                instruction_set.c_str(), defer);
}

// public API, NULL pkgname
static jboolean DexFile_isDexOptNeeded(JNIEnv* env, jclass, jstring javaFilename) {
  const char* instruction_set = GetInstructionSetString(kRuntimeISA);
  ScopedUtfChars filename(env, javaFilename);
  return kUpToDate != IsDexOptNeededInternal(env, filename.c_str(), nullptr /* pkgname */,
                                             instruction_set, false /* defer */);
}


static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(DexFile, closeDexFile, "(J)V"),
  NATIVE_METHOD(DexFile, defineClassNative, "(Ljava/lang/String;Ljava/lang/ClassLoader;J)Ljava/lang/Class;"),
  NATIVE_METHOD(DexFile, getClassNameList, "(J)[Ljava/lang/String;"),
  NATIVE_METHOD(DexFile, isDexOptNeeded, "(Ljava/lang/String;)Z"),
  NATIVE_METHOD(DexFile, isDexOptNeededInternal, "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Z)B"),
  NATIVE_METHOD(DexFile, openDexFileNative, "(Ljava/lang/String;Ljava/lang/String;I)J"),
};

void register_dalvik_system_DexFile(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("dalvik/system/DexFile");
}

}  // namespace art
