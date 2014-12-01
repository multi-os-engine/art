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


namespace art {

// TODO [ruhler]: Do we have to worry about races of oat files being read and
// generated at the same time? The code this was derived from had locks to
// check for something related to this, but the specific scenario isn't clear
// to me.
//
// TODO: Do we need to release the GC to run while we relocate or generate an
// oat file? What does that mean? Who does it affect?

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
    : dex_location_(dex_location), oat_location_from_user_(oat_location),
      isa_(isa), package_name_(package_name), for_execution_(for_execution) {
  CHECK(!for_execution || isa == kRuntimeISA)
    << "OatFileManager for_execution with wrong ISA";

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

OatFileManager::Status OatFileManager::GetStatus() {
  // TODO: Should we check to see if the profile is out of date?

  if (OdexFileIsOutOfDate()) {
    // The DEX file is not pre-compiled.
    // TODO: What if the oat file is not out of date? Could we relocate it
    // from itself?
    return OatFileIsRelocated() ? kUpToDate : kNeedsGeneration;
  } else {
    // The DEX file is pre-compiled. If the oat file isn't up to date, we can
    // patch the pre-compiled version rather than recompiling.
    if (OatFileIsRelocated() || OdexFileIsRelocated()) {
      return kUpToDate;
    } else {
      CHECK(Runtime::Current()->ShouldRelocate())
        << "Attempt to relocate when we shouldn't relocate";
      return kNeedsRelocation;
    }
  }
}

bool OatFileManager::MakeUpToDate() {
  switch (GetStatus()) {
    case kUpToDate: return true;
    case kNeedsRelocation: return RelocateOatFile();
    case kNeedsGeneration: return GenerateOatFile();
  }
  UNREACHABLE();
}

bool OatFileManager::IsInBootClassPath() {
  // TODO: We are assuming that the 64 and 32 bit runtimes have identical
  // boot class paths. This may not be the case. But how do we get the boot
  // class path for an isa not currently running?
  Runtime* runtime = Runtime::Current();
  ClassLinker* class_linker = runtime->GetClassLinker();
  const auto& boot_class_path = class_linker->GetBootClassPath();
  for (size_t i = 0; i < boot_class_path.size(); i++) {
    if (boot_class_path[i]->GetLocation() == std::string(dex_location_)) {
      LOG(INFO) << "Dex location is in boot class path: " << dex_location_;
      return true;
    }
  }
  return false;
}

bool OatFileManager::DexFileExists() {
  CHECK(dex_location_ != nullptr) << "OatFileManager provided no dex location";
  if (!OS::FileExists(dex_location_)) {
    LOG(INFO) << "Dex File does not exist for dex location: " << dex_location_;
    return false;
  }
  return true;
}

std::string OatFileManager::OdexFileName() {
  CHECK(dex_location_ != nullptr) << "OatFileManager provided no dex location";
  // TODO: DexFilenameToOdexFilename should not be in utils.h, it should be a
  // static method of OatFileManager if any one is using it, otherwise inline
  // it here.
  return DexFilenameToOdexFilename(dex_location_, isa_);
}

bool OatFileManager::OdexFileExists() {
  LoadOdexFile();
  return odex_file_.get() != nullptr;
}

bool OatFileManager::OdexFileIsOutOfDate() {
  LoadOdexFile();
  if (odex_file_.get() == nullptr) {
    return true;
  }
  return GivenOatFileIsOutOfDate(odex_file_);
}

bool OatFileManager::OdexFileIsRelocated() {
  // Note: The call to OdexFileIsOutOfDate ensures the oat file is loaded and
  // the image is loaded, so we don't have to do that explicitly.
  if (OdexFileIsOutOfDate()) {
    return false;
  }
  return GivenOatFileIsRelocated(odex_file_);
}

std::string OatFileManager::OatFileName() {
  // If the user gave us the file name already, use it.
  if (oat_location_from_user_ != nullptr) {
    return std::string(oat_location_from_user_);
  }

  // Otherwise, compute the oat file name from the dex location.
  CHECK(dex_location_ != nullptr) << "OatFileManager provided no dex location";

  // TODO: The oat file manager should be the definitive place for determining
  // the oat file name from the dex location, not GetDalvikCacheFilename.
  std::string cache_dir = StringPrintf("%s%s",
      DalvikCacheDirectory().c_str(), GetInstructionSetString(isa_));
  std::string oat_filename;
  std::string error_msg;
  bool have_oat_filename = GetDalvikCacheFilename(dex_location_,
      cache_dir.c_str(), &oat_filename, &error_msg);
  if (!have_oat_filename) {
    LOG(INFO) << "Error when getting oat file name for dex location "
      << dex_location_ << ": " << error_msg;
  }
  return oat_filename;
}

bool OatFileManager::OatFileExists() {
  LoadOatFile();
  return oat_file_.get() != nullptr;
}

bool OatFileManager::OatFileIsOutOfDate() {
  LoadOatFile();
  if (oat_file_.get() == nullptr) {
    return true;
  }
  return GivenOatFileIsOutOfDate(oat_file_);
}

bool OatFileManager::OatFileIsRelocated() {
  // Note: The call to OatFileIsOutOfDate ensures the oat file is loaded and
  // the image is loaded, so we don't have to do that explicitly.
  if (OatFileIsOutOfDate()) {
    return false;
  }
  return GivenOatFileIsRelocated(oat_file_);
}

bool OatFileManager::ProfileExists() {
  LoadProfile();
  return profile_load_succeeded_;
}

bool OatFileManager::OldProfileExists() {
  LoadOldProfile();
  return old_profile_load_succeeded_;
}

// TODO: The IsProfileChangeSignificant implementation was copied from likely
// bit-rotted code.
bool OatFileManager::IsProfileChangeSignificant() {
  // Note: Calling ProfileExists and OldProfileExists ensures profile_ and
  // old_profile_ have been loaded, so we don't need to explicitly load them
  // here.
  if (!ProfileExists() || !OldProfileExists()) {
    return false;
  }

  // TODO: Should we be caching the result of this function rather than
  // re-computing the change percentage each time it is called?

  // TODO: The following code to compare two profile files should live with
  // the rest of the profiler code, not the oat file management code.

  // A change in profile is considered significant if X% (change_thr property)
  // of the top K% (compile_thr property) samples has changed.
  const ProfilerOptions& options = Runtime::Current()->GetProfilerOptions();
  const double top_k_threshold = options.GetTopKThreshold();
  const double change_threshold = options.GetTopKChangeThreshold();
  std::set<std::string> top_k, old_top_k;
  profile_.GetTopKSamples(top_k, top_k_threshold);
  old_profile_.GetTopKSamples(old_top_k, top_k_threshold);
  std::set<std::string> diff;
  std::set_difference(top_k.begin(), top_k.end(), old_top_k.begin(),
      old_top_k.end(), std::inserter(diff, diff.end()));

  // TODO: consider using the usedPercentage instead of the plain diff count.
  double change_percent = 100.0 * static_cast<double>(diff.size())
                                / static_cast<double>(top_k.size());
  std::set<std::string>::iterator end = diff.end();
  for (std::set<std::string>::iterator it = diff.begin(); it != end; it++) {
    LOG(INFO) << "Profile new in topK: " << *it;
  }

  if (change_percent > change_threshold) {
      LOG(INFO) << "Oat File Manager: Profile for " << dex_location_
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
    PLOG(ERROR) << "Failed to open profile file " << old_profile_name
      << ". My uid:gid is " << getuid() << ":" << getgid();
    return;
  }

  struct stat stat_src;
  if (fstat(src.get(), &stat_src) == -1) {
    PLOG(ERROR) << "Failed to get stats for profile file  " << old_profile_name
      << ". My uid:gid is " << getuid() << ":" << getgid();
    return;
  }

  // Create the copy with rw------- (only accessible by system)
  ScopedFd dst(open(profile_name.c_str(), O_WRONLY|O_CREAT|O_TRUNC, 0600));
  if (dst.get()  == -1) {
    PLOG(ERROR) << "Failed to create/write prev profile file " << profile_name
      << ".  My uid:gid is " << getuid() << ":" << getgid();
    return;
  }

#ifdef __linux__
  if (sendfile(dst.get(), src.get(), nullptr, stat_src.st_size) == -1) {
#else
  off_t len;
  if (sendfile(dst.get(), src.get(), 0, &len, nullptr, 0) == -1) {
#endif
    PLOG(ERROR) << "Failed to copy profile file " << old_profile_name
      << " to " << profile_name << ". My uid:gid is " << getuid()
      << ":" << getgid();
  }
}

bool OatFileManager::RelocateOatFile() {
  LoadImageInfo();

  Runtime* runtime = Runtime::Current();
  if (!image_info_load_succeeded_) {
    LOG(WARNING) << "Patching of oat file " << OatFileName()
      << " not attempted because no image location was found.";
    return false;
  }

  if (!runtime->IsDex2OatEnabled()) {
    LOG(WARNING) << "Patching of oat file " << OatFileName()
      << " not attempted because dex2oat is disabled";
    return false;
  }

  std::vector<std::string> argv;
  argv.push_back(runtime->GetPatchoatExecutable());
  argv.push_back("--instruction-set="
      + std::string(GetInstructionSetString(isa_)));
  argv.push_back("--input-oat-file=" + OdexFileName());
  argv.push_back("--output-oat-file=" + OatFileName());
  argv.push_back("--patched-image-location=" + image_location_);

  std::string command_line(Join(argv, ' '));
  LOG(INFO) << "Relocate Oat File: " << command_line;
  std::string error_msg;
  if (!Exec(argv, &error_msg)) {
    // Manually delete the file. This ensures there is no garbage left over if
    // the process unexpectedly died. Ignore unlink failure and propagate the
    // original error.
    TEMP_FAILURE_RETRY(unlink(OatFileName().c_str()));
    return false;
  }

  // Mark that the oat file has changed and we should try to reload.
  oat_file_load_attempted_ = false;
  return true;
}

bool OatFileManager::GenerateOatFile() {
  Runtime* runtime = Runtime::Current();
  if (!runtime->IsDex2OatEnabled()) {
    LOG(WARNING) << "Generation of oat file " << OatFileName()
      << " not attempted because dex2oat is disabled";
    return false;
  }

  std::vector<std::string> args;
  args.push_back("--dex-file=" + std::string(dex_location_));
  args.push_back("--oat-file=" + OatFileName());

  std::string error_msg;
  if (!Dex2Oat(args, &error_msg)) {
    // Manually delete the file. This ensures there is no garbage left over if
    // the process unexpectedly died. Ignore unlink failure and propagate the
    // original error.
    TEMP_FAILURE_RETRY(unlink(OatFileName().c_str()));
    return false;
  }

  // Mark that the oat file has changed and we should try to reload.
  oat_file_load_attempted_ = false;
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
  LOG(INFO) << "Dex2Oat: " << command_line;
  return Exec(argv, error_msg);
}

std::string OatFileManager::DalvikCacheDirectory() {
  // TODO: Should we cache this result instead of recomputing each time this
  // function is called?

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
  const gc::space::ImageSpace* image_space
    = runtime->GetHeap()->GetImageSpace();
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
  }

  // TODO: Fall back to the checksum the odex has for this dex location before
  // returning nullptr?
  return nullptr;
}

void OatFileManager::LoadOdexFile() {
  if (!odex_file_load_attempted_) {
    odex_file_load_attempted_ = true;
    std::string odex_file_name = OdexFileName();
    std::string error_msg;
    odex_file_.reset(OatFile::Open(odex_file_name.c_str(),
          odex_file_name.c_str(), nullptr, nullptr, for_execution_,
          &error_msg));
    if (odex_file_.get() == nullptr) {
      LOG(INFO) << "OatFileManager test for existing pre-compiled oat file "
        << odex_file_name << ": " << error_msg;
    }
  }
}

void OatFileManager::LoadOatFile() {
  if (!oat_file_load_attempted_) {
    oat_file_load_attempted_ = true;
    std::string oat_file_name = OatFileName();
    std::string error_msg;
    oat_file_.reset(OatFile::Open(oat_file_name.c_str(),
          oat_file_name.c_str(), nullptr, nullptr, for_execution_, &error_msg));
    if (oat_file_.get() == nullptr) {
      LOG(INFO) << "OatFileManager test for existing oat file "
        << oat_file_name << ": " << error_msg;
    }
  }
}

void OatFileManager::LoadImageInfo() {
  if (!image_info_load_attempted_) {
    image_info_load_attempted_ = true;

    Runtime* runtime = Runtime::Current();
    const gc::space::ImageSpace* image_space
        = runtime->GetHeap()->GetImageSpace();
    if (image_space == nullptr) {
      return;
    }

    image_location_ = image_space->GetImageLocation();

    if (isa_ == kRuntimeISA) {
      const ImageHeader& image_header = image_space->GetImageHeader();
      image_oat_checksum_ = image_header.GetOatChecksum();
      image_oat_data_begin_ = reinterpret_cast<uintptr_t>(
          image_header.GetOatDataBegin());
      image_patch_delta_ = image_header.GetPatchDelta();
    } else {
      std::unique_ptr<ImageHeader> image_header(
          gc::space::ImageSpace::ReadImageHeaderOrDie(
              image_location_.c_str(), isa_));
      image_oat_checksum_ = image_header->GetOatChecksum();
      image_oat_data_begin_ = reinterpret_cast<uintptr_t>(
          image_header->GetOatDataBegin());
      image_patch_delta_ = image_header->GetPatchDelta();
    }

    image_info_load_succeeded_ = true;
  }
}

void OatFileManager::LoadProfile() {
  if (!profile_load_attempted_) {
    CHECK(package_name_ != nullptr)
      << "pakage_name_ is nullptr: "
      << "profile_load_attempted_ should have been true";
    profile_load_attempted_ = true;
    std::string profile_name = ProfileFileName();
    if (!profile_name.empty()) {
      profile_load_succeeded_ = profile_.LoadFile(profile_name);
    }
  }
}

void OatFileManager::LoadOldProfile() {
  if (!old_profile_load_attempted_) {
    CHECK(package_name_ != nullptr)
      << "pakage_name_ is nullptr: "
      << "old_profile_load_attempted_ should have been true";
    old_profile_load_attempted_ = true;
    std::string old_profile_name = OldProfileFileName();
    if (!old_profile_name.empty()) {
      old_profile_load_succeeded_ = old_profile_.LoadFile(old_profile_name);
    }
  }
}

bool OatFileManager::GivenOatFileIsOutOfDate(std::unique_ptr<OatFile>& file) {
  // TODO: Should we cache these results? If we don't, we can easily end up
  // generating duplicate log entries, which may be confusing to log readers.
  CHECK(file.get() != nullptr) << "GivenOatFileIsOutOfDate given nullptr";

  // Verify the dex checksum.
  // Note: GetOatDexFile will return NULL if the dex checksum doesn't match
  // what we provide, which verifies the primary dex checksum for us.
  uint32_t dex_checksum = 0;
  uint32_t* dex_checksum_pointer = GetRequiredDexChecksum(&dex_checksum);
  const OatFile::OatDexFile* oat_dex_file = file->GetOatDexFile(
      dex_location_, dex_checksum_pointer, true);
  if (oat_dex_file == NULL) {
    return true;
  }

  // Verify the dex checksums for any secondary multidex files
  bool more_secondary_to_check = true;
  for (int i = 1; more_secondary_to_check; i++) {
    std::string secondary_dex_location
      = DexFile::GetMultiDexClassesDexName(i, dex_location_);
    const OatFile::OatDexFile* secondary_oat_dex_file
      = file->GetOatDexFile(secondary_dex_location.c_str(), nullptr, false);
    if (secondary_oat_dex_file != NULL) {
      std::string error_msg;
      uint32_t expected_secondary_checksum = 0;
      if (DexFile::GetChecksum(secondary_dex_location.c_str(),
            &expected_secondary_checksum, &error_msg)) {
        uint32_t actual_secondary_checksum
          = secondary_oat_dex_file->GetDexFileLocationChecksum();
        if (expected_secondary_checksum != actual_secondary_checksum) {
          LOG(INFO) << "Dex checksum does not match for secondary dex: "
            << secondary_dex_location
            << ". Expected: " << expected_secondary_checksum
            << ", Actual: " << actual_secondary_checksum;
          return false;
        }
      } else {
        // If we can't get the checksum for the secondary location, we assume
        // the dex checksum is up to date for this and all other secondary dex
        // files.
        more_secondary_to_check = false;
      }
    } else {
      more_secondary_to_check = false;
    }
  }

  // Verify the image checksum
  LoadImageInfo();
  if (file->GetOatHeader().GetImageFileLocationOatChecksum()
      != image_oat_checksum_) {
    LOG(INFO) << "Oat image checksum does not match image checksum.";
    return true;
  }

  // The checksums are all good; the dex file is not out of date.
  return false;
}

bool OatFileManager::GivenOatFileIsRelocated(std::unique_ptr<OatFile>& file) {
  // TODO: Should we cache these results? If we don't, we can easily end up
  // generating duplicate log entries, which may be confusing to log readers.
  CHECK(file.get() != nullptr) << "GivenOatFileIsRelocated given no oat file";
  if (file->IsPic()) {
    return true;
  }

  // TODO: Why do we check both the patch delta and oat data begin? Is that
  // really what we ought to be doing?
  CHECK(image_info_load_attempted_ && image_info_load_succeeded_)
      << "GivenOatFileIsRelocated expected image info to be loaded already";
  const OatHeader& oat_header = file->GetOatHeader();
  int32_t oat_patch_delta = oat_header.GetImagePatchDelta();
  uintptr_t oat_data_begin = oat_header.GetImageFileLocationOatDataBegin();
  bool relocated = oat_patch_delta == image_patch_delta_
      && oat_data_begin == image_oat_data_begin_;
  if (!relocated) {
    LOG(INFO) << file->GetLocation() <<
      ": Oat file image offset (" << oat_data_begin << ")"
      << " and patch delta (" << oat_patch_delta << ")"
      << " do not match actual image offset (" << image_oat_data_begin_ << ")"
      << " and patch delta(" << image_patch_delta_ << ")";
  }
  return relocated;
}

ExecutableOatFileManager::ExecutableOatFileManager(const char* dex_location)
  : OatFileManager(dex_location, nullptr, kRuntimeISA, nullptr, true) { }

ExecutableOatFileManager::ExecutableOatFileManager(const char* dex_location,
                                                   const char* oat_location)
  : OatFileManager(dex_location, oat_location, kRuntimeISA, nullptr, true) { }

bool ExecutableOatFileManager::LoadDexFiles(
    std::vector<const DexFile*>* dex_files,
    std::unique_ptr<OatFile>& oat_file) {
  oat_file = SelectBestOatFile();
  if (oat_file.get() != nullptr) {
    LoadDexFilesFromGivenOatFile(dex_location_, oat_file, dex_files);
    return true;
  }

  LOG(INFO) << "Oat File Manager: No oat file found,"
     << " attempting to fall back to interpreting the dex file instead.";
  std::string error_msg;
  return DexFile::Open(dex_location_, dex_location_, &error_msg, dex_files);
}

std::unique_ptr<OatFile> ExecutableOatFileManager::SelectBestOatFile() {
  if (OatFileIsRelocated()) {
    return std::move(oat_file_);
  }

  if (OdexFileIsRelocated()) {
    return std::move(odex_file_);
  }

  LOG(INFO) << "Oat File Manager: No relocated oat file found,"
     << " attempting to fall back to interpreting oat file instead.";
  if (!OatFileIsOutOfDate()) {
    for_execution_ = false;
    oat_file_load_attempted_ = false;
    if (!OatFileIsOutOfDate()) {
      return std::move(oat_file_);
    }
  }

  if (!OdexFileIsOutOfDate()) {
    for_execution_ = false;
    odex_file_load_attempted_ = false;
    if (!OdexFileIsOutOfDate()) {
      return std::move(odex_file_);
    }
  }

  return std::unique_ptr<OatFile>();
}

void ExecutableOatFileManager::LoadDexFilesFromGivenOatFile(
    const char* dex_location,
    std::unique_ptr<OatFile>& oat_file,
    std::vector<const DexFile*>* dex_files) {

  // Load the primary dex file.
  std::string error_msg;
  const OatFile::OatDexFile* oat_dex_file = oat_file->GetOatDexFile(
      dex_location, nullptr, false);
  CHECK(oat_dex_file != nullptr) << "Attempt to load out-of-date oat file";

  const DexFile* dex_file = oat_dex_file->OpenDexFile(&error_msg);
  CHECK(dex_file != nullptr) << "Attempt to load out-of-date oat file";
  dex_files->push_back(dex_file);

  // Load secondary multidex files
  bool more_secondary_to_load = true;
  for (int i = 1; more_secondary_to_load; i++) {
    std::string secondary_dex_location
      = DexFile::GetMultiDexClassesDexName(i, dex_location);
    oat_dex_file = oat_file->GetOatDexFile(secondary_dex_location.c_str(),
        nullptr, false);
    if (oat_dex_file != NULL) {
      dex_file = oat_dex_file->OpenDexFile(&error_msg);
      CHECK(dex_file != nullptr) << "Attempt to load out-of-date oat file";
      dex_files->push_back(dex_file);
    } else {
      more_secondary_to_load = false;
    }
  }
}

}  // namespace art

