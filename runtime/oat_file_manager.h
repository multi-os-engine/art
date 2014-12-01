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

#ifndef ART_RUNTIME_OAT_FILE_MANAGER_H_
#define ART_RUNTIME_OAT_FILE_MANAGER_H_

#include <cstdint>
#include <memory>
#include <string>

#include "arch/instruction_set.h"
#include "oat_file.h"
#include "profiler.h"

namespace art {

// Class for oat file management.
//
// This class collects common utilities for managing the status of an oat file
// on the device.
//
// Terminology used here:
//   Dex File - The dex file referred to by the dex location. This is the
//     original, un-optimized, dex file.
//   Odex File - A pre-compiled OAT file. This is called Odex for legacy
//     reasons; it is really an OAT file. The Odex file will typically have a
//     patch delta of 0 and need to be relocated before use.
//   Oat File - An OAT file that is not pre-compiled. The Oat file will have
//     been relocated to some (possibly-out-of date) offset if.
//   Profile File - A file containing the latest profiling information
//     associated with the dex file code.
//   Old Profile File - A file containing a previous snapshot of profiling
//     information associated with the dex file code. This is used to track
//     how the profiling information has changed over time.
//
//  For managing and loading an executable oat file, use the
//  ExecutableOatFileManager class instead.
class OatFileManager {
 public:
  enum Status {
    kUpToDate,
    kNeedsRelocation,
    kNeedsGeneration
  };

  // Constructs an OatFileManager object to manage the oat file corresponding
  // to the given dex location with the target instruction set.
  //
  // The dex_location must not be NULL, and should remain available and
  // unchanged for the duration of the lifetime of the OatFileManager object.
  //
  // The isa should be either the 32 bit or 64 bit variant for the current
  // device. For example, on an arm device, use arm or arm64, not x86.
  OatFileManager(const char* dex_location, const InstructionSet isa);

  // Constructs an OatFileManager, providing an explicit target oat_location
  // to use instead of the standard oat location.
  OatFileManager(const char* dex_location, const char* oat_location,
                 const InstructionSet isa);

  // Constructs an OatFileManager, providing an additional package_name used
  // solely for the purpose of locating profile files.
  //
  // TODO: Why is the name of the profile file based on the package name and
  // not the dex location? If there is no technical reason the dex_location
  // can't be used, we should prefer that instead.
  OatFileManager(const char* dex_location, const InstructionSet isa,
                 const char* package_name);

  // Constructs an OatFileManager with user specified oat location and a
  // package name.
  OatFileManager(const char* dex_location, const char* oat_location,
                 const InstructionSet isa, const char* package_name);

  // Returns the overall compilation status for the given dex location.
  // kUpToDate - The dex file has code compiled for it that can be used
  //   directly.
  // kNeedsRelocation - The dex file has code compiled for it that can be used
  //   once it is relocated.
  // kNeedsGeneration - The dex file has no code compiled for it that can be
  //   used.
  Status GetStatus();

  // Attempts to generate or relocate the oat file as needed to make it up to
  // date.
  // Returns true on success.
  bool MakeUpToDate();

  // Returns true if the dex location refers to an element of the boot class
  // path.
  bool IsInBootClassPath();

  // Constructs the filename for the odex file.
  std::string OdexFileName();

  // Returns true if an odex file exists for the dex location.
  // The odex file may be out of date.
  bool OdexFileExists();

  // Returns true if the odex file is out of date with respect to the dex
  // file or if the odex file does not exist.
  bool OdexFileIsOutOfDate();

  // Returns true if the odex file is up to date and properly relocated.
  // OdexFileIsRelocated() implies !OdexFileIsOutOfDate();
  // TODO: What should this return if relocation is currently disabled in the
  // runtime?
  bool OdexFileIsRelocated();

  // Constructs the filename for the oat file.
  std::string OatFileName();

  // Returns true if an oat file exists for the dex location.
  // The oat file may be out of date.
  bool OatFileExists();

  // Returns true if the oat file is out of date or does not exist.
  // This verifies the dex and image check sums, but not the relocation
  // offset.
  bool OatFileIsOutOfDate();

  // Returns true if the oat file is up to date and properly relocated.
  // OatFileIsRelocated() implies !OatFileIsOutOfDate();
  // TODO: What should this return if relocation is currently disabled in the
  // runtime?
  bool OatFileIsRelocated();

  // Returns true if there is an accessible profile associated with the dex
  // location.
  // This will return false if profiling is disabled.
  bool ProfileExists();

  // Returns true if there is an accessible old profile associated with the
  // dex location.
  // This will return false if profiling is disabled.
  bool OldProfileExists();

  // Returns true if there has been a significant change between the old
  // profile and the current profile.
  // This will return false if profiling is disabled.
  bool IsProfileChangeSignificant();

  // Copy the current profile to the old profile location.
  void CopyProfileFile();

  // Generates the oat file by relocation from the odex file.
  // This does not check the current status before attempting to relocate the
  // oat file.
  // Returns true on success.
  // This will fail if dex2oat is not enabled in the current runtime.
  bool RelocateOatFile();

  // Generate the oat file from the dex file.
  // This does not check the current status before attempting to generate the
  // oat file.
  // Returns true on success.
  // This will fail if dex2oat is not enabled in the current runtime.
  bool GenerateOatFile();

  // Exececutes dex2oat using the current runtime configuration overridden with
  // the given arguments. This does not check to see if dex2oat is enabled in
  // the runtime configuration.
  // Returns true on success.
  //
  // If there is a failure, the value of error_msg will be set to a string
  // describing why there was failure. error_msg must not be nullptr.
  //
  // TODO: The OatFileManager probably isn't the right place to have this
  // function.
  static bool Dex2Oat(const std::vector<std::string>& args,
                      std::string* error_msg);

 private:
  // Constructs an OatFileManager with user specified oat location, a package
  // name, and the option to be executable.
  //
  // If for_execution is true, the isa must match the current runtime isa.
  OatFileManager(const char* dex_location, const char* oat_location,
                 const InstructionSet isa, const char* package_name,
                 bool for_execution);

  // Returns the path to the dalvik cache directory.
  // Does not check existence of the cache or try to create it.
  // Includes the trailing slash.
  // Returns an empty string if we can't get the dalvik cache directory path.
  std::string DalvikCacheDirectory();

  // Constructs the filename for the profile file.
  // Returns an empty string if we do not have the necessary information to
  // construct the filename.
  std::string ProfileFileName();

  // Constructs the filename for the old profile file.
  // Returns an empty string if we do not have the necessary information to
  // construct the filename.
  std::string OldProfileFileName();

  // Returns the current image location.
  // Returns an empty string if the image location could not be retrieved.
  //
  // TODO: This method should belong with an image file manager, not
  // the oat file manager.
  static std::string ImageLocation();

  // Gets the dex checksum required for an up-to-date oat file.
  // Updates the value pointed to by dex_checksum with the required checksum,
  // if any. Returns dex_checksum if a required checksum was located. Returns
  // nullptr if the required checksum was not found.
  uint32_t* GetRequiredDexChecksum(uint32_t* dex_checksum);

  // We load information lazily to avoid work where it is not needed and to
  // share work where possible.
  // The following convention is used for lazy loading of information:
  //  foo_load_attempted_ is true if we have tried to load foo
  //  LoadFoo() is used to ensure foo is loaded. If an attempt to load foo has
  //    already been made, does nothing. Otherwise attempts to load foo.
  //
  // TODO: Can we encapsulate this lazy loading somehow? And have some way to
  // automatically ensure things are loaded only when needed and as needed?
  // Because this makes the code complicated, with subtle assumptions about
  // the side effects of methods.
  void LoadOdexFile();
  void LoadOatFile();
  void LoadImageInfo();
  void LoadProfile();
  void LoadOldProfile();

  // Return true if the given oat file is out of date.
  // file must not be NULL.
  bool GivenOatFileIsOutOfDate(const OatFile& file);

  // Return true if the given oat file is relocated.
  // file must not be NULL.
  // Assumes the image info has already been loaded.
  bool GivenOatFileIsRelocated(const OatFile& file);

  // In a properly constructed OatFileManager object, dex_location_ should
  // never be nullptr.
  const char* dex_location_ = nullptr;

  // Optional oat location given by the user. If this is set, it should be
  // used, otherwise, the standard oat location should be computed.
  // Note: Use the OatFileName method to get the proper oat location rather
  // than accessing this field directly.
  const char* oat_location_from_user_ = nullptr;

  // In a properly constructed OatFileManager object, isa_ should be either
  // the 32 or 64 bit variant for the current device.
  const InstructionSet isa_ = kNone;

  // The package name, used solely to find the profile file.
  // This may be nullptr in a properly constructed object. In this case,
  // profile_load_attempted_ and old_profile_load_attempted_ will be true, and
  // profile_load_succeeded_ and old_profile_load_succeeded_ will be false.
  const char* package_name_ = nullptr;

  // Whether we will attempt to load oat files as executable.
  bool for_execution_ = false;

  bool odex_file_load_attempted_ = false;
  std::unique_ptr<OatFile> odex_file_;

  bool oat_file_load_attempted_ = false;
  std::unique_ptr<OatFile> oat_file_;

  // TODO: The image info should probably be moved out of the oat file manager
  // to an image file manager.
  bool image_info_load_attempted_ = false;
  bool image_info_load_succeeded_ = false;
  uint32_t image_oat_checksum_ = 0;
  uintptr_t image_oat_data_begin_ = 0;
  int32_t image_patch_delta_ = 0;
  std::string image_location_;

  bool profile_load_attempted_ = false;
  bool profile_load_succeeded_ = false;
  ProfileFile profile_;

  bool old_profile_load_attempted_ = false;
  bool old_profile_load_succeeded_ = false;
  ProfileFile old_profile_;

  // For debugging only.
  // If this flag is set, the oat or odex file has been released to the user
  // of the OatFileManager object and the OatFileManager object is in a bad
  // state and should no longer be used.
  bool oat_file_released_ = false;

  friend class ExecutableOatFileManager;
  DISALLOW_COPY_AND_ASSIGN(OatFileManager);
};

// An OatFileManager for managing executable oat files.
//
// An oat file can be loaded executable only if the ISA matches the current
// runtime. This class enforces that the ISA matches the current runtime and
// allows you to get the loaded oat file.
class ExecutableOatFileManager : public OatFileManager {
 public:
  // Constructs an ExecutableOatFileManager object to manage the oat file
  // corresponding to the given dex location for the current runtime
  // instruction set.
  explicit ExecutableOatFileManager(const char* dex_location);

  // Constructs an ExecutableOatFileManager, providing an explicit target
  // oat_location to use instead of the standard oat location.
  ExecutableOatFileManager(const char* dex_location, const char* oat_location);

  // Loads the dex files in the oat file for the dex location.
  // Does not attempt to make the oat file up to date before loading.
  // This loads multiple dex files in the case of multidex.
  // The oat_file will be set to the oat file loaded if any. It is strictly an
  // out parameter. There may be no oat file if we had to fall back to the
  // original unquickened dex file.
  // Returns true on success.
  //
  // The caller is responsible for freeing the dex_files and oat_file
  // returned, if any. The dex_files will only remain valid as long as the
  // oat_file is valid if an oat file is returned.
  //
  // After this call, no other methods of the OatFileManager should be called,
  // because access to the loaded oat file has been taken away from the
  // OatFileManager object, even if the call was not successful.
  bool LoadDexFiles(std::vector<const DexFile*>* dex_files,
                    std::unique_ptr<OatFile>* oat_file);

 private:
  // Returns an oat file that can be used for loading dex files.
  //
  // After this call, no other methods of the OatFileManager should be called,
  // because access to the loaded oat file has been taken away from the
  // OatFileManager object.
  std::unique_ptr<OatFile> GetBestOatFile();

  // Load dex files from the given oat file.
  // This assumes loading will be successful.
  // The caller is responsible for freeing the returned dex files.
  // The returned dex_files will remain valid as long as the given oat_file.
  static void LoadDexFilesFromGivenOatFile(
      const char* dex_location,
      const OatFile& oat_file,
      std::vector<const DexFile*>* dex_files);

  DISALLOW_COPY_AND_ASSIGN(ExecutableOatFileManager);
};


}  // namespace art

#endif  // ART_RUNTIME_OAT_FILE_MANAGER_H_
