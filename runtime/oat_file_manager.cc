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

#include "oat_file_manager.h"

#include <set>

#include <fcntl.h>
#ifdef __linux__
#include <sys/sendfile.h>
#else
#include <sys/socket.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/logging.h"
#include "base/stringprintf.h"
#include "class_linker.h"
#include "gc/heap.h"
#include "gc/space/image_space.h"
#include "image.h"
#include "oat.h"
#include "os.h"
#include "profiler.h"
#include "runtime.h"
#include "utils.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#include "ScopedFd.h"
#pragma GCC diagnostic pop

// TAG used for verbose logging.
#define TAG oat_file_manager

namespace art {

OatFileManager::OatFileManager(const char* dex_location,
                               const InstructionSet isa)
    : OatFileManager(dex_location, nullptr, isa, nullptr, false) { }

OatFileManager::OatFileManager(const char* dex_location,
                               const char* oat_location,
                               const InstructionSet isa)
    : OatFileManager(dex_location, oat_location, isa, nullptr, false) { }

OatFileManager::OatFileManager(const char* dex_location,
                               const InstructionSet isa,
                               const char* package_name)
    : OatFileManager(dex_location, nullptr, isa, package_name, false) { }

OatFileManager::OatFileManager(const char* dex_location,
                               const char* oat_location,
                               const InstructionSet isa,
                               const char* package_name,
                               bool for_execution)
    : dex_location_(dex_location), isa_(isa),
      package_name_(package_name), for_execution_(for_execution) {
  CHECK(!for_execution || isa == kRuntimeISA)
    << "OatFileManager for_execution with wrong ISA";

  // If the user gave a target oat location, save that as the cached oat
  // location now so we won't try to construct the default location later.
  if (oat_location != nullptr) {
    cached_oat_file_name_ = std::string(oat_location);
  }

  // If there is no package name given, we will not be able to find any
  // profiles associated with this dex location. Preemptively mark that to
  // be the case, rather than trying to find and load the profiles later.
  // Similarly, if profiling is disabled.
  if (package_name == nullptr
      || !Runtime::Current()->GetProfilerOptions().IsEnabled()) {
    profile_load_attempted_ = true;
    profile_load_succeeded_ = false;
    old_profile_load_attempted_ = true;
    old_profile_load_succeeded_ = false;
  }
}

OatFileManager::~OatFileManager() {
  // Clean up the lock file.
  if (lock_file_.get() != nullptr) {
    lock_file_->Erase();
    TEMP_FAILURE_RETRY(unlink(LockFileName().c_str()));
  }
}

bool OatFileManager::Lock(std::string* error_msg) {
  CHECK(error_msg != nullptr);

  if (lock_file_.get() == nullptr) {
    lock_file_.reset(OS::CreateEmptyFile(LockFileName().c_str()));
    if (lock_file_.get() == nullptr) {
      *error_msg = "Failed to create lock file " + LockFileName();
      return false;
    } else {
      if (!flock_.Init(lock_file_.get(), error_msg)) {
        TEMP_FAILURE_RETRY(unlink(LockFileName().c_str()));
        return false;
      }
    }
  }
  return true;
}

OatFileManager::Status OatFileManager::GetStatus() {
  // TODO: If the profiling code is ever restored, it's worth considering
  // whether we should check to see if the profile is out of date here.

  if (OdexFileIsOutOfDate()) {
    // The DEX file is not pre-compiled.
    // TODO: What if the oat file is not out of date? Could we relocate it
    // from itself?
    return OatFileIsUpToDate() ? kUpToDate : kOutOfDate;
  } else {
    // The DEX file is pre-compiled. If the oat file isn't up to date, we can
    // patch the pre-compiled version rather than recompiling.
    if (OatFileIsUpToDate() || OdexFileIsUpToDate()) {
      return kUpToDate;
    } else {
      return kNeedsRelocation;
    }
  }
}

bool OatFileManager::MakeUpToDate(std::string* error_msg) {
  switch (GetStatus()) {
    case kUpToDate: return true;
    case kNeedsRelocation: return RelocateOatFile(error_msg);
    case kOutOfDate: return GenerateOatFile(error_msg);
  }
  UNREACHABLE();
}

bool OatFileManager::IsInCurrentBootClassPath() {
  // Note: We don't cache the result of this, because we only expect it to be
  // called once.
  Runtime* runtime = Runtime::Current();
  ClassLinker* class_linker = runtime->GetClassLinker();
  const auto& boot_class_path = class_linker->GetBootClassPath();
  for (size_t i = 0; i < boot_class_path.size(); i++) {
    if (boot_class_path[i]->GetLocation() == std::string(dex_location_)) {
      VLOG(TAG) << "Dex location " << dex_location_ << " is in boot class path";
      return true;
    }
  }
  return false;
}

std::string OatFileManager::OdexFileName() {
  if (cached_odex_file_name_.empty()) {
    CHECK(dex_location_ != nullptr) << "OatFileManager: null dex location";
    // TODO: DexFilenameToOdexFilename should not be in utils.h, it should be
    // a static method of OatFileManager if any one is using it, otherwise
    // inline it here.
    cached_odex_file_name_ = DexFilenameToOdexFilename(dex_location_, isa_);
  }
  return cached_odex_file_name_;
}

bool OatFileManager::OdexFileExists() {
  return GetOdexFile() != nullptr;
}

bool OatFileManager::OdexFileIsOutOfDate() {
  if (!odex_file_is_out_of_date_attempted_) {
    odex_file_is_out_of_date_attempted_ = true;
    OatFile* odex_file = GetOdexFile();
    if (odex_file == nullptr) {
      cached_odex_file_is_out_of_date_ = true;
    } else {
      cached_odex_file_is_out_of_date_ = GivenOatFileIsOutOfDate(*odex_file);
    }
  }
  return cached_odex_file_is_out_of_date_;
}

bool OatFileManager::OdexFileIsUpToDate() {
  if (!odex_file_is_up_to_date_attempted_) {
    odex_file_is_up_to_date_attempted_ = true;
    OatFile* odex_file = GetOdexFile();
    if (odex_file == nullptr) {
      cached_odex_file_is_up_to_date_ = false;
    } else {
      cached_odex_file_is_up_to_date_ = GivenOatFileIsUpToDate(*odex_file);
    }
  }
  return cached_odex_file_is_up_to_date_;
}

std::string OatFileManager::OatFileName() {
  if (cached_oat_file_name_.empty()) {
    // Compute the oat file name from the dex location.
    CHECK(dex_location_ != nullptr) << "OatFileManager: null dex location";

    // TODO: The oat file manager should be the definitive place for determining
    // the oat file name from the dex location, not GetDalvikCacheFilename.
    std::string cache_dir = StringPrintf("%s%s",
        DalvikCacheDirectory().c_str(), GetInstructionSetString(isa_));
    std::string error_msg;
    bool have_oat_filename = GetDalvikCacheFilename(dex_location_,
        cache_dir.c_str(), &cached_oat_file_name_, &error_msg);

    // TODO: What should we return if there is an error getting the oat file
    // name? We should do something more sensible than abort here.
    CHECK(have_oat_filename)
      << "Error getting oat file name for dex location "
      << dex_location_ << ": " << error_msg;
  }
  return cached_oat_file_name_;
}

bool OatFileManager::OatFileExists() {
  return GetOatFile() != nullptr;
}

bool OatFileManager::OatFileIsOutOfDate() {
  if (!oat_file_is_out_of_date_attempted_) {
    oat_file_is_out_of_date_attempted_ = true;
    OatFile* oat_file = GetOatFile();
    if (oat_file == nullptr) {
      cached_oat_file_is_out_of_date_ = true;
    } else {
      cached_oat_file_is_out_of_date_ = GivenOatFileIsOutOfDate(*oat_file);
    }
  }
  return cached_oat_file_is_out_of_date_;
}

bool OatFileManager::OatFileIsUpToDate() {
  if (!oat_file_is_up_to_date_attempted_) {
    oat_file_is_up_to_date_attempted_ = true;
    OatFile* oat_file = GetOatFile();
    if (oat_file == nullptr) {
      cached_oat_file_is_up_to_date_ = false;
    } else {
      cached_oat_file_is_up_to_date_ = GivenOatFileIsUpToDate(*oat_file);
    }
  }
  return cached_oat_file_is_up_to_date_;
}

bool OatFileManager::GivenOatFileIsOutOfDate(const OatFile& file) {
  // Verify the dex checksum.
  // Note: GetOatDexFile will return NULL if the dex checksum doesn't match
  // what we provide, which verifies the primary dex checksum for us.
  uint32_t dex_checksum = 0;
  uint32_t* dex_checksum_pointer = GetRequiredDexChecksum(&dex_checksum);
  const OatFile::OatDexFile* oat_dex_file = file.GetOatDexFile(
      dex_location_, dex_checksum_pointer, false);
  if (oat_dex_file == NULL) {
    return true;
  }

  // Verify the dex checksums for any secondary multidex files
  for (int i = 1; ; i++) {
    std::string secondary_dex_location
      = DexFile::GetMultiDexClassesDexName(i, dex_location_);
    const OatFile::OatDexFile* secondary_oat_dex_file
      = file.GetOatDexFile(secondary_dex_location.c_str(), nullptr, false);
    if (secondary_oat_dex_file == NULL) {
      // There are no more secondary dex files to check.
      break;
    }

    std::string error_msg;
    uint32_t expected_secondary_checksum = 0;
    if (DexFile::GetChecksum(secondary_dex_location.c_str(),
          &expected_secondary_checksum, &error_msg)) {
      uint32_t actual_secondary_checksum
        = secondary_oat_dex_file->GetDexFileLocationChecksum();
      if (expected_secondary_checksum != actual_secondary_checksum) {
        VLOG(TAG) << "Dex checksum does not match for secondary dex: "
          << secondary_dex_location
          << ". Expected: " << expected_secondary_checksum
          << ", Actual: " << actual_secondary_checksum;
        return false;
      }
    } else {
      // If we can't get the checksum for the secondary location, we assume
      // the dex checksum is up to date for this and all other secondary dex
      // files.
      break;
    }
  }

  // Verify the image checksum
  ImageInfo* image_info = GetImageInfo();
  if (image_info == nullptr) {
    VLOG(TAG) << "No image for oat image checksum to match against.";
    return true;
  }

  if (file.GetOatHeader().GetImageFileLocationOatChecksum() != image_info->oat_checksum) {
    VLOG(TAG) << "Oat image checksum does not match image checksum.";
    return true;
  }

  // The checksums are all good; the dex file is not out of date.
  return false;
}

bool OatFileManager::GivenOatFileIsUpToDate(const OatFile& file) {
  if (GivenOatFileIsOutOfDate(file)) {
    return false;
  }

  if (file.IsPic()) {
    return true;
  }

  ImageInfo* image_info = GetImageInfo();
  if (image_info == nullptr) {
    VLOG(TAG) << "No image for to check oat relocation against.";
    return false;
  }

  // TODO: Why do we check both the patch delta and oat data begin? Is that
  // really what we ought to be doing?
  const OatHeader& oat_header = file.GetOatHeader();
  int32_t oat_patch_delta = oat_header.GetImagePatchDelta();
  uintptr_t oat_data_begin = oat_header.GetImageFileLocationOatDataBegin();
  bool relocated = oat_patch_delta == image_info->patch_delta
      && oat_data_begin == image_info->oat_data_begin;
  if (!relocated) {
    VLOG(TAG) << file.GetLocation() <<
      ": Oat file image offset (" << oat_data_begin << ")"
      << " and patch delta (" << oat_patch_delta << ")"
      << " do not match actual image offset (" << image_info->oat_data_begin
      << ") and patch delta(" << image_info->patch_delta << ")";
  }
  return relocated;
}

bool OatFileManager::ProfileExists() {
  return GetProfile() != nullptr;
}

bool OatFileManager::OldProfileExists() {
  return GetOldProfile() != nullptr;
}

// TODO: The IsProfileChangeSignificant implementation was copied from likely
// bit-rotted code.
bool OatFileManager::IsProfileChangeSignificant() {
  ProfileFile* profile = GetProfile();
  if (profile == nullptr) {
    return false;
  }

  ProfileFile* old_profile = GetOldProfile();
  if (old_profile == nullptr) {
    return false;
  }

  // TODO: The following code to compare two profile files should live with
  // the rest of the profiler code, not the oat file management code.

  // A change in profile is considered significant if X% (change_thr property)
  // of the top K% (compile_thr property) samples has changed.
  const ProfilerOptions& options = Runtime::Current()->GetProfilerOptions();
  const double top_k_threshold = options.GetTopKThreshold();
  const double change_threshold = options.GetTopKChangeThreshold();
  std::set<std::string> top_k, old_top_k;
  profile->GetTopKSamples(top_k, top_k_threshold);
  old_profile->GetTopKSamples(old_top_k, top_k_threshold);
  std::set<std::string> diff;
  std::set_difference(top_k.begin(), top_k.end(), old_top_k.begin(),
      old_top_k.end(), std::inserter(diff, diff.end()));

  // TODO: consider using the usedPercentage instead of the plain diff count.
  double change_percent = 100.0 * static_cast<double>(diff.size())
                                / static_cast<double>(top_k.size());
  std::set<std::string>::iterator end = diff.end();
  for (std::set<std::string>::iterator it = diff.begin(); it != end; it++) {
    VLOG(TAG) << "Profile new in topK: " << *it;
  }

  if (change_percent > change_threshold) {
      VLOG(TAG) << "Oat File Manager: Profile for " << dex_location_
        << "has changed significantly: (top "
        << top_k_threshold << "% samples changed in proportion of "
        << change_percent << "%)";
      return true;
  }
  return false;
}

// TODO: The CopyProfileFile implementation was copied from likely bit-rotted
// code.
void OatFileManager::CopyProfileFile() {
  if (!ProfileExists()) {
    return;
  }

  std::string profile_name = ProfileFileName();
  std::string old_profile_name = OldProfileFileName();

  ScopedFd src(open(old_profile_name.c_str(), O_RDONLY));
  if (src.get() == -1) {
    PLOG(WARNING) << "Failed to open profile file " << old_profile_name
      << ". My uid:gid is " << getuid() << ":" << getgid();
    return;
  }

  struct stat stat_src;
  if (fstat(src.get(), &stat_src) == -1) {
    PLOG(WARNING) << "Failed to get stats for profile file  " << old_profile_name
      << ". My uid:gid is " << getuid() << ":" << getgid();
    return;
  }

  // Create the copy with rw------- (only accessible by system)
  ScopedFd dst(open(profile_name.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0600));
  if (dst.get()  == -1) {
    PLOG(WARNING) << "Failed to create/write prev profile file " << profile_name
      << ".  My uid:gid is " << getuid() << ":" << getgid();
    return;
  }

#ifdef __linux__
  if (sendfile(dst.get(), src.get(), nullptr, stat_src.st_size) == -1) {
#else
  off_t len;
  if (sendfile(dst.get(), src.get(), 0, &len, nullptr, 0) == -1) {
#endif
    PLOG(WARNING) << "Failed to copy profile file " << old_profile_name
      << " to " << profile_name << ". My uid:gid is " << getuid()
      << ":" << getgid();
  }
}

bool OatFileManager::RelocateOatFile(std::string* error_msg) {
  CHECK(error_msg != nullptr);

  ImageInfo* image_info = GetImageInfo();
  Runtime* runtime = Runtime::Current();
  if (image_info == nullptr) {
    *error_msg = "Patching of oat file " + OatFileName()
      + " not attempted because no image location was found.";
    return false;
  }

  if (!runtime->IsDex2OatEnabled()) {
    *error_msg = "Patching of oat file " + OatFileName()
      + " not attempted because dex2oat is disabled";
    return false;
  }

  std::vector<std::string> argv;
  argv.push_back(runtime->GetPatchoatExecutable());
  argv.push_back("--instruction-set=" + std::string(GetInstructionSetString(isa_)));
  argv.push_back("--input-oat-file=" + OdexFileName());
  argv.push_back("--output-oat-file=" + OatFileName());
  argv.push_back("--patched-image-location=" + image_info->location);

  std::string command_line(Join(argv, ' '));
  if (!Exec(argv, error_msg)) {
    // Manually delete the file. This ensures there is no garbage left over if
    // the process unexpectedly died.
    TEMP_FAILURE_RETRY(unlink(OatFileName().c_str()));
    return false;
  }

  // Mark that the oat file has changed and we should try to reload.
  ClearOatFileCache();
  return true;
}

bool OatFileManager::GenerateOatFile(std::string* error_msg) {
  CHECK(error_msg != nullptr);

  Runtime* runtime = Runtime::Current();
  if (!runtime->IsDex2OatEnabled()) {
    *error_msg = "Generation of oat file " + OatFileName()
      + " not attempted because dex2oat is disabled";
    return false;
  }

  std::vector<std::string> args;
  args.push_back("--dex-file=" + std::string(dex_location_));
  args.push_back("--oat-file=" + OatFileName());

  if (!Dex2Oat(args, error_msg)) {
    // Manually delete the file. This ensures there is no garbage left over if
    // the process unexpectedly died.
    TEMP_FAILURE_RETRY(unlink(OatFileName().c_str()));
    return false;
  }

  // Mark that the oat file has changed and we should try to reload.
  ClearOatFileCache();
  return true;
}

bool OatFileManager::Dex2Oat(const std::vector<std::string>& args,
                             std::string* error_msg) {
  Runtime* runtime = Runtime::Current();
  std::string image_location = ImageLocation();
  if (image_location.empty()) {
    *error_msg = "No image location found for Dex2Oat.";
    return false;
  }

  std::vector<std::string> argv;
  argv.push_back(runtime->GetCompilerExecutable());
  argv.push_back("--runtime-arg");
  argv.push_back("-classpath");
  argv.push_back("--runtime-arg");
  argv.push_back(runtime->GetClassPathString());
  runtime->AddCurrentRuntimeFeaturesAsDex2OatArguments(&argv);

  if (!runtime->IsVerificationEnabled()) {
    argv.push_back("--compiler-filter=verify-none");
  }

  if (runtime->MustRelocateIfPossible()) {
    argv.push_back("--runtime-arg");
    argv.push_back("-Xrelocate");
  } else {
    argv.push_back("--runtime-arg");
    argv.push_back("-Xnorelocate");
  }

  if (!kIsTargetBuild) {
    argv.push_back("--host");
  }

  argv.push_back("--boot-image=" + image_location);

  std::vector<std::string> compiler_options = runtime->GetCompilerOptions();
  argv.insert(argv.end(), compiler_options.begin(), compiler_options.end());

  argv.insert(argv.end(), args.begin(), args.end());

  std::string command_line(Join(argv, ' '));
  return Exec(argv, error_msg);
}

std::string OatFileManager::LockFileName() {
  return OatFileName() + ".flock";
}

std::string OatFileManager::DalvikCacheDirectory() {
  // Note: We don't cache this, because it will only be called once by
  // OatFileName, and we don't care about the performance of the profiling
  // code, which isn't used in practice.

  // TODO: The work done in GetDalvikCache is overkill for what we need.
  // Ideally a new API for getting the DalvikCacheDirectory the way we want
  // (without existence testing, creation, or death) is provided with the rest
  // of the GetDalvikCache family of functions. Until such an API is in place,
  // we use GetDalvikCache to avoid duplicating the logic for determining the
  // dalvik cache directory.
  std::string result;
  bool have_android_data;
  bool dalvik_cache_exists;
  bool is_global_cache;
  GetDalvikCache("", false, &result, &have_android_data, &dalvik_cache_exists, &is_global_cache);
  return result;
}

std::string OatFileManager::ProfileFileName() {
  if (package_name_ != nullptr) {
    return DalvikCacheDirectory() + std::string("profiles/") + package_name_;
  }
  return "";
}

std::string OatFileManager::OldProfileFileName() {
  std::string profile_name = ProfileFileName();
  if (profile_name.empty()) {
    return "";
  }
  return profile_name + "@old";
}

std::string OatFileManager::ImageLocation() {
  Runtime* runtime = Runtime::Current();
  const gc::space::ImageSpace* image_space = runtime->GetHeap()->GetImageSpace();
  if (image_space == nullptr) {
    return "";
  }
  return image_space->GetImageLocation();
}

uint32_t* OatFileManager::GetRequiredDexChecksum(uint32_t* dex_checksum) {
  CHECK(dex_location_ != nullptr) << "OatFileManager provided no dex location";
  std::string error_msg;
  if (DexFile::GetChecksum(dex_location_, dex_checksum, &error_msg)) {
    return dex_checksum;
  } else {
    // This can happen if the original dex file has been stripped from the apk.
    VLOG(TAG) << "OatFileManager: " << error_msg;
  }

  // TODO: Fall back to the checksum the odex has for this dex location before
  // returning nullptr?
  return nullptr;
}

OatFile* OatFileManager::GetOdexFile() {
  CHECK(!oat_file_released_) << "OdexFile called after oat file released.";
  if (!odex_file_load_attempted_) {
    odex_file_load_attempted_ = true;
    std::string odex_file_name = OdexFileName();
    std::string error_msg;
    cached_odex_file_.reset(OatFile::Open(odex_file_name.c_str(),
          odex_file_name.c_str(), nullptr, nullptr, for_execution_,
          &error_msg));
    if (cached_odex_file_.get() == nullptr) {
      VLOG(TAG) << "OatFileManager test for existing pre-compiled oat file "
        << odex_file_name << ": " << error_msg;
    }
  }
  return cached_odex_file_.get();
}

void OatFileManager::ClearOdexFileCache() {
  odex_file_load_attempted_ = false;
  cached_odex_file_.reset();
  odex_file_is_out_of_date_attempted_ = false;
  odex_file_is_up_to_date_attempted_ = false;
}

OatFile* OatFileManager::GetOatFile() {
  CHECK(!oat_file_released_) << "OatFile called after oat file released.";
  if (!oat_file_load_attempted_) {
    oat_file_load_attempted_ = true;
    std::string oat_file_name = OatFileName();
    std::string error_msg;
    cached_oat_file_.reset(OatFile::Open(oat_file_name.c_str(),
          oat_file_name.c_str(), nullptr, nullptr, for_execution_, &error_msg));
    if (cached_oat_file_.get() == nullptr) {
      VLOG(TAG) << "OatFileManager test for existing oat file "
        << oat_file_name << ": " << error_msg;
    }
  }
  return cached_oat_file_.get();
}

void OatFileManager::ClearOatFileCache() {
  oat_file_load_attempted_ = false;
  cached_oat_file_.reset();
  oat_file_is_out_of_date_attempted_ = false;
  oat_file_is_up_to_date_attempted_ = false;
}

OatFileManager::ImageInfo* OatFileManager::GetImageInfo() {
  if (!image_info_load_attempted_) {
    image_info_load_attempted_ = true;

    Runtime* runtime = Runtime::Current();
    const gc::space::ImageSpace* image_space = runtime->GetHeap()->GetImageSpace();
    if (image_space != nullptr) {
      cached_image_info_.location = image_space->GetImageLocation();

      if (isa_ == kRuntimeISA) {
        const ImageHeader& image_header = image_space->GetImageHeader();
        cached_image_info_.oat_checksum = image_header.GetOatChecksum();
        cached_image_info_.oat_data_begin = reinterpret_cast<uintptr_t>(image_header.GetOatDataBegin());
        cached_image_info_.patch_delta = image_header.GetPatchDelta();
      } else {
        std::unique_ptr<ImageHeader> image_header(
            gc::space::ImageSpace::ReadImageHeaderOrDie(
                cached_image_info_.location.c_str(), isa_));
        cached_image_info_.oat_checksum = image_header->GetOatChecksum();
        cached_image_info_.oat_data_begin = reinterpret_cast<uintptr_t>(image_header->GetOatDataBegin());
        cached_image_info_.patch_delta = image_header->GetPatchDelta();
      }
    }
    image_info_load_succeeded_ = (image_space != nullptr);
  }
  return image_info_load_succeeded_ ? &cached_image_info_ : nullptr;
}

ProfileFile* OatFileManager::GetProfile() {
  if (!profile_load_attempted_) {
    CHECK(package_name_ != nullptr)
      << "pakage_name_ is nullptr: "
      << "profile_load_attempted_ should have been true";
    profile_load_attempted_ = true;
    std::string profile_name = ProfileFileName();
    if (!profile_name.empty()) {
      profile_load_succeeded_ = cached_profile_.LoadFile(profile_name);
    }
  }
  return profile_load_succeeded_ ? &cached_profile_ : nullptr;
}

ProfileFile* OatFileManager::GetOldProfile() {
  if (!old_profile_load_attempted_) {
    CHECK(package_name_ != nullptr)
      << "pakage_name_ is nullptr: "
      << "old_profile_load_attempted_ should have been true";
    old_profile_load_attempted_ = true;
    std::string old_profile_name = OldProfileFileName();
    if (!old_profile_name.empty()) {
      old_profile_load_succeeded_ = cached_old_profile_.LoadFile(old_profile_name);
    }
  }
  return old_profile_load_succeeded_ ? &cached_old_profile_ : nullptr;
}


ExecutableOatFileManager::ExecutableOatFileManager(const char* dex_location)
  : OatFileManager(dex_location, nullptr, kRuntimeISA, nullptr, true) { }

ExecutableOatFileManager::ExecutableOatFileManager(const char* dex_location,
                                                   const char* oat_location)
  : OatFileManager(dex_location, oat_location, kRuntimeISA, nullptr, true) { }

std::unique_ptr<OatFile> ExecutableOatFileManager::GetBestOatFile() {
  if (OatFileIsUpToDate()) {
    oat_file_released_ = true;
    return std::move(cached_oat_file_);
  }

  if (OdexFileIsUpToDate()) {
    oat_file_released_ = true;
    return std::move(cached_odex_file_);
  }

  VLOG(TAG) << "Oat File Manager: No relocated oat file found,"
     << " attempting to fall back to interpreting oat file instead.";
  if (!OatFileIsOutOfDate()) {
    for_execution_ = false;
    ClearOatFileCache();
    if (!OatFileIsOutOfDate()) {
      oat_file_released_ = true;
      return std::move(cached_oat_file_);
    }
  }

  if (!OdexFileIsOutOfDate()) {
    for_execution_ = false;
    ClearOdexFileCache();
    if (!OdexFileIsOutOfDate()) {
      oat_file_released_ = true;
      return std::move(cached_odex_file_);
    }
  }

  return std::unique_ptr<OatFile>();
}

std::vector<std::unique_ptr<const DexFile>> ExecutableOatFileManager::LoadDexFiles(const OatFile& oat_file, const char* dex_location) {
  std::vector<std::unique_ptr<const DexFile>> dex_files;

  // Load the primary dex file.
  std::string error_msg;
  const OatFile::OatDexFile* oat_dex_file = oat_file.GetOatDexFile(
      dex_location, nullptr, false);
  if (oat_dex_file == nullptr) {
    LOG(WARNING) << "Attempt to load out-of-date oat file "
      << oat_file.GetLocation() << " for dex location " << dex_location;
    return std::vector<std::unique_ptr<const DexFile>>();
  }

  std::unique_ptr<const DexFile> dex_file = oat_dex_file->OpenDexFile(&error_msg);
  if (dex_file.get() == nullptr) {
    LOG(WARNING) << "Failed to open dex file from oat dex file: " << error_msg;
    return std::vector<std::unique_ptr<const DexFile>>();
  }
  dex_files.push_back(std::move(dex_file));

  // Load secondary multidex files
  for (int i = 1; ; i++) {
    std::string secondary_dex_location = DexFile::GetMultiDexClassesDexName(i, dex_location);
    oat_dex_file = oat_file.GetOatDexFile(secondary_dex_location.c_str(), nullptr, false);
    if (oat_dex_file == NULL) {
      // There are no more secondary dex files to load.
      break;
    }

    dex_file = oat_dex_file->OpenDexFile(&error_msg);
    if (dex_file.get() == nullptr) {
      LOG(WARNING) << "Failed to open dex file from oat dex file: " << error_msg;
      return std::vector<std::unique_ptr<const DexFile>>();
    }
    dex_files.push_back(std::move(dex_file));
  }
  return dex_files;
}

}  // namespace art

