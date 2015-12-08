/*
 * Copyright (C) 2015 The Android Open Source Project
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

#include "dex_file.h"

#include "jit/offline_profiling_info.h"
#include "jni.h"
#include "mirror/class-inl.h"
#include "oat_file_assistant.h"
#include "oat_file_manager.h"
#include "scoped_thread_state_change.h"
#include "thread.h"

namespace art {
namespace {

static std::string get_dex_location(jclass cls) {
  ScopedObjectAccess soa(Thread::Current());
  return soa.Decode<mirror::Class*>(cls)->GetDexCache()->GetDexFile()->GetLocation();
}

extern "C" JNIEXPORT jstring JNICALL Java_Main_getDexLocation(JNIEnv* env, jclass cls) {
  std::string dex_location = get_dex_location(cls);
  return env->NewStringUTF(dex_location.c_str());
}

extern "C" JNIEXPORT jstring JNICALL Java_Main_getProfileInfoDump(
      JNIEnv* env, jclass cls, jstring filename) {
  std::string dex_location = get_dex_location(cls);
  const OatFile* oat_file = nullptr;
  std::vector<std::string> errors;
  std::vector<std::unique_ptr<const DexFile>> dex_files =
      Runtime::Current()->GetOatFileManager().OpenDexFilesFromOat(
          dex_location.c_str(),
          nullptr,
          &oat_file,
          &errors);

  std::vector<const DexFile*> dex_files_raw;
  for (size_t i = 0; i < dex_files.size(); i++) {
    dex_files_raw.push_back(dex_files[i].get());
  }

  const char* filename_chars = env->GetStringUTFChars(filename, nullptr);
  ProfileCompilationInfo info(filename_chars);

  std::string result = info.Load(dex_files_raw)
      ? info.DumpInfo(/*print_full_dex_location*/false)
      : "Could not load profile info";

  env->ReleaseStringUTFChars(filename, filename_chars);
  // Return the dump of the profile info. It will be compared against a golden value.
  return env->NewStringUTF(result.c_str());
}

}  // namespace
}  // namespace art
