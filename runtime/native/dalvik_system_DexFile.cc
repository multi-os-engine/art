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

#include <algorithm>
#include <fcntl.h>
#include <fstream>
#include <set>
#include <unistd.h>

#include "base/logging.h"
#include "class_linker.h"
#include "common_throws.h"
#include "dex_file-inl.h"
#include "gc/space/image_space.h"
#include "gc/space/space-inl.h"
#include "image.h"
#include "jni_internal.h"
#include "mirror/class_loader.h"
#include "mirror/object-inl.h"
#include "mirror/string.h"
#include "oat.h"
#include "os.h"
#include "runtime.h"
#include "scoped_thread_state_change.h"
#include "ScopedLocalRef.h"
#include "ScopedUtfChars.h"
#include "toStringArray.h"
#include "zip_archive.h"

#ifdef HAVE_ANDROID_OS
#include "cutils/properties.h"
#endif

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

  uint32_t dex_location_checksum;
  uint32_t* dex_location_checksum_pointer = &dex_location_checksum;
  std::string error_msg;
  if (!DexFile::GetChecksum(sourceName.c_str(), dex_location_checksum_pointer, &error_msg)) {
    dex_location_checksum_pointer = NULL;
  }

  ClassLinker* linker = Runtime::Current()->GetClassLinker();
  const DexFile* dex_file;
  if (outputName.c_str() == nullptr) {
    // FindOrCreateOatFileForDexLocation can tolerate a missing dex_location_checksum
    error_msg.clear();
    dex_file = linker->FindDexFileInOatFileFromDexLocation(sourceName.c_str(),
                                                           dex_location_checksum_pointer, &error_msg);
  } else {
    // FindOrCreateOatFileForDexLocation requires the dex_location_checksum
    if (dex_location_checksum_pointer == NULL) {
      ScopedObjectAccess soa(env);
      DCHECK(!error_msg.empty());
      ThrowIOException("%s", error_msg.c_str());
      return 0;
    }
    dex_file = linker->FindOrCreateOatFileForDexLocation(sourceName.c_str(), dex_location_checksum,
                                                         outputName.c_str(), &error_msg);
  }
  if (dex_file == nullptr) {
    ScopedObjectAccess soa(env);
    CHECK(!error_msg.empty());
    ThrowIOException("%s", error_msg.c_str());
    return 0;
  }
  return static_cast<jlong>(reinterpret_cast<uintptr_t>(dex_file));
}

static const DexFile* toDexFile(jlong dex_file_address, JNIEnv* env) {
  const DexFile* dex_file = reinterpret_cast<const DexFile*>(static_cast<uintptr_t>(dex_file_address));
  if (UNLIKELY(dex_file == nullptr)) {
    ScopedObjectAccess soa(env);
    ThrowNullPointerException(NULL, "dex_file == null");
  }
  return dex_file;
}

static void DexFile_closeDexFile(JNIEnv* env, jclass, jlong cookie) {
  const DexFile* dex_file;
  dex_file = toDexFile(cookie, env);
  if (dex_file == nullptr) {
    return;
  }
  if (Runtime::Current()->GetClassLinker()->IsDexFileRegistered(*dex_file)) {
    return;
  }
  delete dex_file;
}

static jclass DexFile_defineClassNative(JNIEnv* env, jclass, jstring javaName, jobject javaLoader,
                                        jlong cookie) {
  const DexFile* dex_file = toDexFile(cookie, env);
  if (dex_file == NULL) {
    VLOG(class_linker) << "Failed to find dex_file";
    return NULL;
  }
  ScopedUtfChars class_name(env, javaName);
  if (class_name.c_str() == NULL) {
    VLOG(class_linker) << "Failed to find class_name";
    return NULL;
  }
  const std::string descriptor(DotToDescriptor(class_name.c_str()));
  const DexFile::ClassDef* dex_class_def = dex_file->FindClassDef(descriptor.c_str());
  if (dex_class_def == NULL) {
    VLOG(class_linker) << "Failed to find dex_class_def";
    return NULL;
  }
  ScopedObjectAccess soa(env);
  ClassLinker* class_linker = Runtime::Current()->GetClassLinker();
  class_linker->RegisterDexFile(*dex_file);
  SirtRef<mirror::ClassLoader> class_loader(soa.Self(), soa.Decode<mirror::ClassLoader*>(javaLoader));
  mirror::Class* result = class_linker->DefineClass(descriptor.c_str(), class_loader, *dex_file,
                                                    *dex_class_def);
  VLOG(class_linker) << "DexFile_defineClassNative returning " << result;
  return soa.AddLocalReference<jclass>(result);
}

static jobjectArray DexFile_getClassNameList(JNIEnv* env, jclass, jlong cookie) {
  const DexFile* dex_file;
  dex_file = toDexFile(cookie, env);
  if (dex_file == nullptr) {
    return nullptr;
  }

  std::vector<std::string> class_names;
  for (size_t i = 0; i < dex_file->NumClassDefs(); ++i) {
    const DexFile::ClassDef& class_def = dex_file->GetClassDef(i);
    const char* descriptor = dex_file->GetClassDescriptor(class_def);
    class_names.push_back(DescriptorToDot(descriptor));
  }
  return toStringArray(env, class_names);
}

// Copy a profile file
static void CopyProfileFile(const char* oldfile, const char* newfile) {
  int fd = open(oldfile, O_RDONLY);
  if (fd < 0) {
    // If we can't open the file show the uid:gid of the this process to allow
    // diagnosis of the problem.
    LOG(ERROR) << "Failed to open profile file " << oldfile<< ".  My uid:gid is "
      << getuid() << ":" << getgid();
    return;
  }

  // Create the copy with rw------- (only accessible by system)
  int fd2 = open(newfile, O_WRONLY|O_CREAT|O_TRUNC, 0600);
  if (fd2 < 0) {
    // If we can't open the file show the uid:gid of the this process to allow
    // diagnosis of the problem.
    LOG(ERROR) << "Failed to create/write prev profile file " << newfile << ".  My uid:gid is "
      << getuid() << ":" << getgid();
    return;
  }
  char buf[4096];
  while (true) {
    int n = read(fd, buf, sizeof(buf));
    if (n <= 0) {
      break;
    }
    write(fd2, buf, n);
  }
  close(fd);
  close(fd2);
}


static bool loadTopKMethods(std::set<std::string>& topKMethods, const std::string& profileFileName,
    double targetTopKPercentage) {
  LOG(VERBOSE) << "reading profile file " << profileFileName;
  struct stat st;
  int err = stat(profileFileName.c_str(), &st);
  if (err == -1) {
    LOG(VERBOSE) << "not found";
    return false;
  }
  std::ifstream in(profileFileName.c_str());
  if (!in) {
    LOG(VERBOSE) << "profile file " << profileFileName << " exists but can't be opened";
    LOG(VERBOSE) << "file owner: " << st.st_uid << ":" << st.st_gid;
    LOG(VERBOSE) << "me: " << getuid() << ":" << getgid();
    LOG(VERBOSE) << "file permissions: " << std::oct << st.st_mode;
    LOG(VERBOSE) << "errno: " << errno;
    return false;
  }
  // The first line contains summary information.
  std::string line;
  std::getline(in, line);
  if (in.eof()) {
    return false;
  }
  std::vector<std::string> summary_info;
  Split(line, '/', summary_info);
  if (summary_info.size() != 3) {
    // Bad summary info.  It should be count/total/bootpath.
    return false;
  }
  // This is the number of hits in all methods.
  uint32_t total_count = 0;
  for (int i = 0 ; i < 3; ++i) {
    total_count += atoi(summary_info[i].c_str());
  }

  // Now read each line until the end of file.  Each line consists of 3 fields separated by '/'.
  // Store the info in descending order given by the most used methods.
  typedef std::set<std::pair<int, std::string>> ProfileSet;
  ProfileSet countSet;
  while (!in.eof()) {
    std::getline(in, line);
    if (in.eof()) {
      break;
    }
    std::vector<std::string> info;
    Split(line, '/', info);
    if (info.size() != 3) {
      // Malformed.
      break;
    }
    int count = atoi(info[1].c_str());
    countSet.insert(std::make_pair(-count, info[0]));
  }

  ProfileSet::iterator end = countSet.end();
  ProfileSet::iterator begin = countSet.begin();
  uint32_t curTotalCount = 0;
  uint32_t prevCount = 0;
  double prevTopKPercent = 0;

  for (ProfileSet::iterator it = begin; it != end; it++) {
    uint32_t count = -it->first;
    double usedPercent = (count * 100.0) / total_count;
    curTotalCount += count;
    // Methods with the same count should be part of the same top K percentage bucket.
    double topKPercentage = (it != begin) && (prevCount == count)
      ? prevTopKPercent
      : 100 * static_cast<double>(curTotalCount) / static_cast<double>(total_count);

    if (topKPercentage - usedPercent < targetTopKPercentage) {
      topKMethods.insert(it->second);
    } else {
      break;
    }
    prevTopKPercent = topKPercentage;
    prevCount = count;
  }
  return true;
}

static double getDoubleProperty(const char* property, double minValue, double maxValue, double defaultValue) {
#ifndef HAVE_ANDROID_OS
  return defaultValue;
#else
  char buf[PROP_VALUE_MAX];
  char* endptr;

  property_get(property, buf, "");
  double value = strtod(buf, &endptr);

  if (value == 0 && endptr == buf) {
    value = defaultValue;
  } else if (value < minValue || value > maxValue) {
    value = defaultValue;
  }
  return value;
#endif
}

static jboolean DexFile_isDexOptNeededInternal(JNIEnv* env, jclass, jstring javaFilename,
    jstring javaPkgname, jboolean defer) {
  const bool kVerboseLogging = false;  // Spammy logging.
  const bool kDebugLogging = true;  // Logging useful for debugging.
  ScopedUtfChars filename(env, javaFilename);

  if ((filename.c_str() == nullptr) || !OS::FileExists(filename.c_str())) {
    LOG(ERROR) << "DexFile_isDexOptNeeded file '" << filename.c_str() << "' does not exist";
    ScopedLocalRef<jclass> fnfe(env, env->FindClass("java/io/FileNotFoundException"));
    const char* message = (filename.c_str() == nullptr) ? "<empty file name>" : filename.c_str();
    env->ThrowNew(fnfe.get(), message);
    return JNI_FALSE;
  }

  // Always treat elements of the bootclasspath as up-to-date.  The
  // fact that code is running at all means that this should be true.
  Runtime* runtime = Runtime::Current();
  ClassLinker* class_linker = runtime->GetClassLinker();
  const std::vector<const DexFile*>& boot_class_path = class_linker->GetBootClassPath();
  for (size_t i = 0; i < boot_class_path.size(); i++) {
    if (boot_class_path[i]->GetLocation() == filename.c_str()) {
      if (kVerboseLogging) {
        LOG(INFO) << "DexFile_isDexOptNeeded ignoring boot class path file: " << filename.c_str();
      }
      return JNI_FALSE;
    }
  }

  // Check the profile file.  We need to rerun dex2oat if the profile has changed significantly
  // since the last time, or it's new.
  // If the 'defer' argument is true then this will be retried later.  In this case we
  // need to make sure that the profile file copy is not made so that we will get the
  // same result second time.
  if (javaPkgname != NULL) {
    ScopedUtfChars pkgname(env, javaPkgname);
    std::string profile_file = GetDalvikCacheOrDie(GetAndroidData()) + std::string("/profiles/") +
    pkgname.c_str();

    std::string profile_cache_dir = GetDalvikCacheOrDie(GetAndroidData()) + "/profile-cache";

    // Make the profile cache if it doesn't exist.
    mkdir(profile_cache_dir.c_str(), 0700);

    // The previous profile file (a copy of the profile the last time this was run) is
    // in the dalvik-cache directory because this is owned by system.  The profiles
    // directory is owned by install so system cannot write files in there.
    std::string prev_profile_file = profile_cache_dir + std::string("/") + pkgname.c_str();

    struct stat profstat, prevstat;
    int e1 = stat(profile_file.c_str(), &profstat);
    int e2 = stat(prev_profile_file.c_str(), &prevstat);
    if (e1 < 0) {
      // No profile file, need to run dex2oat
      if (kDebugLogging) {
        LOG(INFO) << "DexFile_isDexOptNeeded profile file " << profile_file << " doesn't exist";
      }
      return JNI_TRUE;
    }
    if (e2 == 0) {
      // There is a previous profile file.  Check if the profile has changed significantly.
      // A change in profile is considered significant if X% (change_thr property) of the top K% 
      // (compile_thr property) samples has changed. 

      double topKThreshold = getDoubleProperty("dalvik.vm.profiler.dex2oat.compile_thr", 10.0, 90.0, 90.0);
      double changeThreshold = getDoubleProperty("dalvik.vm.profiler.dex2oat.change_thr", 1.0, 90.0, 10.0);
      double changePercent = 0.0;
      std::set<std::string> newTopK, oldTopK;
      bool newOk = loadTopKMethods(newTopK, profile_file, topKThreshold);
      bool oldOk = loadTopKMethods(oldTopK, prev_profile_file, topKThreshold);
      if (!newOk || !oldOk) {
        if (kDebugLogging) {
          LOG(INFO) << "Ignoring invalid profiles";
        }
      } else if (newTopK.empty() || oldTopK.empty()) {
        changePercent = 100.0;
      } else {
        std::set<std::string> diff;
        std::set_difference(newTopK.begin(), newTopK.end(), oldTopK.begin(), oldTopK.end(), 
          std::inserter(diff, diff.end()));
        changePercent = 100.0 * static_cast<double>(diff.size()) / static_cast<double>(newTopK.size());
      }

      if (changePercent > changeThreshold) {
        if (kDebugLogging) {
          LOG(INFO) << "DexFile_isDexOptNeeded size of new profile file " << profile_file <<
          " is significantly different from old profile file " << prev_profile_file << " (top "
          << topKThreshold << "% samples changed in proportion of " << changePercent << "%)";
        }
        if (!defer) {
          CopyProfileFile(profile_file.c_str(), prev_profile_file.c_str());
        }
        return JNI_TRUE;
      }
    } else {
      // Previous profile does not exist.  Make a copy of the current one.
      if (kDebugLogging) {
        LOG(INFO) << "DexFile_isDexOptNeeded previous profile doesn't exist: " << prev_profile_file;
      }
      if (!defer) {
        CopyProfileFile(profile_file.c_str(), prev_profile_file.c_str());
      }
      return JNI_TRUE;
    }
  }

  // Check if we have an odex file next to the dex file.
  std::string odex_filename(OatFile::DexFilenameToOdexFilename(filename.c_str()));
  std::string error_msg;
  UniquePtr<const OatFile> oat_file(OatFile::Open(odex_filename, odex_filename, NULL, false,
                                                  &error_msg));
  if (oat_file.get() == nullptr) {
    if (kVerboseLogging) {
      LOG(INFO) << "DexFile_isDexOptNeeded failed to open oat file '" << filename.c_str()
          << "': " << error_msg;
    }
    error_msg.clear();
  } else {
    const art::OatFile::OatDexFile* oat_dex_file = oat_file->GetOatDexFile(filename.c_str(), NULL,
                                                                           kDebugLogging);
    if (oat_dex_file != nullptr) {
      uint32_t location_checksum;
      // If its not possible to read the classes.dex assume up-to-date as we won't be able to
      // compile it anyway.
      if (!DexFile::GetChecksum(filename.c_str(), &location_checksum, &error_msg)) {
        if (kVerboseLogging) {
          LOG(INFO) << "DexFile_isDexOptNeeded ignoring precompiled stripped file: "
              << filename.c_str() << ": " << error_msg;
        }
        return JNI_FALSE;
      }
      if (ClassLinker::VerifyOatFileChecksums(oat_file.get(), filename.c_str(), location_checksum,
                                              &error_msg)) {
        if (kVerboseLogging) {
          LOG(INFO) << "DexFile_isDexOptNeeded precompiled file " << odex_filename
              << " has an up-to-date checksum compared to " << filename.c_str();
        }
        return JNI_FALSE;
      } else {
        if (kVerboseLogging) {
          LOG(INFO) << "DexFile_isDexOptNeeded found precompiled file " << odex_filename
              << " with an out-of-date checksum compared to " << filename.c_str()
              << ": " << error_msg;
        }
        error_msg.clear();
      }
    }
  }

  // Check if we have an oat file in the cache
  std::string cache_location(GetDalvikCacheFilenameOrDie(filename.c_str()));
  oat_file.reset(OatFile::Open(cache_location, filename.c_str(), NULL, false, &error_msg));
  if (oat_file.get() == nullptr) {
    if (kDebugLogging) {
      LOG(INFO) << "DexFile_isDexOptNeeded cache file " << cache_location
          << " does not exist for " << filename.c_str() << ": " << error_msg;
    }
    return JNI_TRUE;
  }

  for (const auto& space : runtime->GetHeap()->GetContinuousSpaces()) {
    if (space->IsImageSpace()) {
      // TODO: Ensure this works with multiple image spaces.
      const ImageHeader& image_header = space->AsImageSpace()->GetImageHeader();
      if (oat_file->GetOatHeader().GetImageFileLocationOatChecksum() !=
          image_header.GetOatChecksum()) {
        if (kDebugLogging) {
          ScopedObjectAccess soa(env);
          LOG(INFO) << "DexFile_isDexOptNeeded cache file " << cache_location
              << " has out-of-date oat checksum compared to "
              << oat_file->GetLocation();
        }
        return JNI_TRUE;
      }
      if (oat_file->GetOatHeader().GetImageFileLocationOatDataBegin()
          != reinterpret_cast<uintptr_t>(image_header.GetOatDataBegin())) {
        if (kDebugLogging) {
          ScopedObjectAccess soa(env);
          LOG(INFO) << "DexFile_isDexOptNeeded cache file " << cache_location
              << " has out-of-date oat begin compared to "
              << oat_file->GetLocation();
        }
        return JNI_TRUE;
      }
    }
  }

  uint32_t location_checksum;
  if (!DexFile::GetChecksum(filename.c_str(), &location_checksum, &error_msg)) {
    if (kDebugLogging) {
      LOG(ERROR) << "DexFile_isDexOptNeeded failed to compute checksum of " << filename.c_str()
            << " (error " << error_msg << ")";
    }
    return JNI_TRUE;
  }

  if (!ClassLinker::VerifyOatFileChecksums(oat_file.get(), filename.c_str(), location_checksum,
                                           &error_msg)) {
    if (kDebugLogging) {
      LOG(INFO) << "DexFile_isDexOptNeeded cache file " << cache_location
          << " has out-of-date checksum compared to " << filename.c_str()
          << " (error " << error_msg << ")";
    }
    return JNI_TRUE;
  }

  if (kVerboseLogging) {
    LOG(INFO) << "DexFile_isDexOptNeeded cache file " << cache_location
              << " is up-to-date for " << filename.c_str();
  }
  CHECK(error_msg.empty()) << error_msg;
  return JNI_FALSE;
}

// public API, NULL pkgname
static jboolean DexFile_isDexOptNeeded(JNIEnv* env, jclass c, jstring javaFilename) {
  return DexFile_isDexOptNeededInternal(env, c, javaFilename, NULL, false);
}


static JNINativeMethod gMethods[] = {
  NATIVE_METHOD(DexFile, closeDexFile, "(J)V"),
  NATIVE_METHOD(DexFile, defineClassNative, "(Ljava/lang/String;Ljava/lang/ClassLoader;J)Ljava/lang/Class;"),
  NATIVE_METHOD(DexFile, getClassNameList, "(J)[Ljava/lang/String;"),
  NATIVE_METHOD(DexFile, isDexOptNeeded, "(Ljava/lang/String;)Z"),
  NATIVE_METHOD(DexFile, isDexOptNeededInternal, "(Ljava/lang/String;Ljava/lang/String;Z)Z"),
  NATIVE_METHOD(DexFile, openDexFileNative, "(Ljava/lang/String;Ljava/lang/String;I)J"),
};

void register_dalvik_system_DexFile(JNIEnv* env) {
  REGISTER_NATIVE_METHODS("dalvik/system/DexFile");
}

}  // namespace art
