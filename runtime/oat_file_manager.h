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
#include "base/scoped_flock.h"
#include "base/unix_file/fd_file.h"
#include "oat_file.h"
#include "os.h"
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
//     patch delta of 0 and need to be relocated before use for the purposes
//     of address space layout randomization (ASLR).
//   Oat File - An OAT file that is not pre-compiled. The Oat file will have
//     been relocated to some (possibly-out-of date) offset for ASLR.
//   Profile File - A file containing the latest profiling information
//     associated with the dex file code.
//   Old Profile File - A file containing a previous snapshot of profiling
//     information associated with the dex file code. This is used to track
//     how the profiling information has changed over time.
//
//   Out Of Date - An oat file is said to be out of date if the file does
//     not exist, or is out of date with respect to the dex file or boot
//     image.
//   Up To Date - An oat file is said to be up to date if it is not out of
//     date and has been properly relocated for the purposes of ASLR.
//   Needs Relocation - An oat file is said to need relocation if the code is
//     up to date, but not yet properly relocated for ASLR. In this case, the
//     oat file is neither "out of date" nor "up to date".
//
// The oat file manager is intended to be used with dex locations not on the
// boot class path. See the IsInCurrentBootClassPath method for a way to check
// if the dex location is in the current boot class path.
//
// For managing and loading an executable oat file, use the
// ExecutableOatFileManager class instead.
//
// TODO: What if the dex location is in the boot class path for a different
// ISA (32/64 bit), but not the current boot class path?
//
// TODO: All the profiling related code is old and untested. It should either
// be restored and tested, or removed.
class OatFileManager {
 public:
  enum Status {
    kUpToDate,
    kNeedsRelocation,
    kOutOfDate
  };

  // Constructs an OatFileManager object to manage the oat file corresponding
  // to the given dex location with the target instruction set.
  //
  // The dex_location must not be NULL and should remain available and
  // unchanged for the duration of the lifetime of the OatFileManager object.
  //
  // Note: Currently the dex_location must be absolute (start with '/').
  // It must be longer than 4 characters, and the extension must be '.dex',
  // '.zip', or '.apk'.
  // TODO: Relax these restrictions?
  //
  // The isa should be either the 32 bit or 64 bit variant for the current
  // device. For example, on an arm device, use arm or arm64.
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

  ~OatFileManager();

  // Obtains a lock on the target oat file.
  // Only one OatFileManager object can hold the lock for a target oat file at
  // a time. The Lock is released automatically when the OatFileManager object
  // goes out of scope.
  //
  // Returns true on success.
  // Returns false on error, in which case error_msg will contain more
  // information on the error.
  //
  // The 'error_msg' argument must not be null. Does nothing if the lock is
  // already held.
  //
  // This is intended to be used to avoid race conditions when multiple
  // processes generate oat files.
  bool Lock(std::string* error_msg);

  // Returns the overall compilation status for the given dex location.
  Status GetStatus();

  // Attempts to generate or relocate the oat file as needed to make it up to
  // date.
  // Returns true on success.
  //
  // If there is a failure, the value of error_msg will be set to a string
  // describing why there was failure. error_msg must not be nullptr.
  bool MakeUpToDate(std::string* error_msg);

  // Returns true if the dex location refers to an element of the current boot
  // class path.
  bool IsInCurrentBootClassPath();

  // These methods return the location and status of the odex file for the dex
  // location.
  std::string OdexFileName();
  bool OdexFileExists();
  bool OdexFileIsOutOfDate();
  bool OdexFileIsUpToDate();

  // These methods return the location and status of the target oat file for
  // the dex location.
  std::string OatFileName();
  bool OatFileExists();
  bool OatFileIsOutOfDate();
  bool OatFileIsUpToDate();

  // These methods return the status for a given oat file with respect to the
  // dex location.
  bool GivenOatFileIsOutOfDate(const OatFile& file);
  bool GivenOatFileIsUpToDate(const OatFile& file);

  // Returns true if there is an accessible profile associated with the dex
  // location.
  // This returns false if profiling is disabled.
  bool ProfileExists();

  // Returns true if there is an accessible old profile associated with the
  // dex location.
  // This returns false if profiling is disabled.
  bool OldProfileExists();

  // Returns true if there has been a significant change between the old
  // profile and the current profile.
  // This returns false if profiling is disabled.
  bool IsProfileChangeSignificant();

  // Copy the current profile to the old profile location.
  void CopyProfileFile();

  // Generates the oat file by relocation from the odex file.
  // This does not check the current status before attempting to relocate the
  // oat file.
  // Returns true on success.
  // This will fail if dex2oat is not enabled in the current runtime.
  //
  // If there is a failure, the value of error_msg will be set to a string
  // describing why there was failure. error_msg must not be nullptr.
  bool RelocateOatFile(std::string* error_msg);

  // Generate the oat file from the dex file.
  // This does not check the current status before attempting to generate the
  // oat file.
  // Returns true on success.
  // This will fail if dex2oat is not enabled in the current runtime.
  //
  // If there is a failure, the value of error_msg will be set to a string
  // describing why there was failure. error_msg must not be nullptr.
  bool GenerateOatFile(std::string* error_msg);

  // Executes dex2oat using the current runtime configuration overridden with
  // the given arguments. This does not check to see if dex2oat is enabled in
  // the runtime configuration.
  // Returns true on success.
  //
  // If there is a failure, the value of error_msg will be set to a string
  // describing why there was failure. error_msg must not be nullptr.
  //
  // TODO: The OatFileManager probably isn't the right place to have this
  // function.
  static bool Dex2Oat(const std::vector<std::string>& args, std::string* error_msg);

 private:
  struct ImageInfo {
    uint32_t oat_checksum = 0;
    uintptr_t oat_data_begin = 0;
    int32_t patch_delta = 0;
    std::string location;
  };

  // Constructs an OatFileManager with user specified oat location, a package
  // name, and the option to be executable.
  //
  // If for_execution is true, the isa must match the current runtime isa.
  OatFileManager(const char* dex_location, const char* oat_location,
                 const InstructionSet isa, const char* package_name,
                 bool for_execution);

  // Returns the path to the file to use for locking.
  std::string LockFileName();

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
  // Returns dex_checksum if a required checksum was located. Returns
  // nullptr if the required checksum was not found.
  // The caller shouldn't clean up or free the returned pointer.
  uint32_t* GetRequiredDexChecksum();

  // Returns the loaded odex file.
  // Loads the file if needed. Returns nullptr if the file failed to load.
  // The caller shouldn't clean up or free the returned pointer.
  OatFile* GetOdexFile();

  // Clear any cached information about the odex file that depends on the
  // contents of the file.
  void ClearOdexFileCache();

  // Returns the loaded oat file.
  // Loads the file if needed. Returns nullptr if the file failed to load.
  // The caller shouldn't clean up or free the returned pointer.
  OatFile* GetOatFile();

  // Clear any cached information about the oat file that depends on the
  // contents of the file.
  void ClearOatFileCache();

  // Returns the loaded image info.
  // Loads the image info if needed. Returns nullptr if the image info failed
  // to load.
  // The caller shouldn't clean up or free the returned pointer.
  ImageInfo* GetImageInfo();

  // Returns the loaded profile.
  // Loads the profile if needed. Returns nullptr if the profile failed
  // to load.
  // The caller shouldn't clean up or free the returned pointer.
  ProfileFile* GetProfile();

  // Returns the loaded old profile.
  // Loads the old profile if needed. Returns nullptr if the old profile
  // failed to load.
  // The caller shouldn't clean up or free the returned pointer.
  ProfileFile* GetOldProfile();

  // To implement Lock(), we lock a dummy file where the oat file would go
  // (adding ".flock" to the target file name) and retain th lock for the
  // remaining lifetime of the OatFileManager object.
  std::unique_ptr<File> lock_file_;
  ScopedFlock flock_;

  // In a properly constructed OatFileManager object, dex_location_ should
  // never be nullptr.
  const char* dex_location_ = nullptr;

  // In a properly constructed OatFileManager object, isa_ should be either
  // the 32 or 64 bit variant for the current device.
  const InstructionSet isa_ = kNone;

  // The package name, used solely to find the profile file.
  // This may be nullptr in a properly constructed object. In this case,
  // profile_load_attempted_ and old_profile_load_attempted_ will be true, and
  // profile_load_succeeded_ and old_profile_load_succeeded_ will be false.
  const char* package_name_ = nullptr;

  // Whether we will attempt to load oat files executable.
  bool for_execution_ = false;

  // Cached value of the required dex checksum.
  // This should be accessed only by the GetRequiredDexChecksum() method.
  uint32_t cached_required_dex_checksum;
  bool required_dex_checksum_attempted = false;
  bool required_dex_checksum_found;

  // Cached value of the odex file name.
  // This should be accessed only by the OdexFileName() method.
  // We use the empty string to indicate the cached_odex_file_name_ has not
  // yet been constructed.
  std::string cached_odex_file_name_;

  // Cached value of the loaded odex file.
  // Use the GetOdexFile method rather than accessing this directly, unless you
  // know the odex file isn't out of date.
  bool odex_file_load_attempted_ = false;
  std::unique_ptr<OatFile> cached_odex_file_;

  // Cached results for OdexFileIsOutOfDate
  bool odex_file_is_out_of_date_attempted_ = false;
  bool cached_odex_file_is_out_of_date_;

  // Cached results for OdexFileIsUpToDate
  bool odex_file_is_up_to_date_attempted_ = false;
  bool cached_odex_file_is_up_to_date_;

  // Cached value of the oat file name.
  // This should be accessed only by the OatFileName() method.
  // We use the empty string to indicate the cached_oat_file_name_ has not yet
  // been constructed.
  std::string cached_oat_file_name_;

  // Cached value of the loaded odex file.
  // Use the GetOatFile method rather than accessing this directly, unless you
  // know the odex file isn't out of date.
  bool oat_file_load_attempted_ = false;
  std::unique_ptr<OatFile> cached_oat_file_;

  // Cached results for OatFileIsOutOfDate
  bool oat_file_is_out_of_date_attempted_ = false;
  bool cached_oat_file_is_out_of_date_;

  // Cached results for OatFileIsUpToDate
  bool oat_file_is_up_to_date_attempted_ = false;
  bool cached_oat_file_is_up_to_date_;

  // Cached value of the image info.
  // Use the GetImageInfo method rather than accessing these directly.
  // TODO: The image info should probably be moved out of the oat file manager
  // to an image file manager.
  bool image_info_load_attempted_ = false;
  bool image_info_load_succeeded_ = false;
  ImageInfo cached_image_info_;

  // Cached value of the profile file.
  // Use the GetProfile method rather than accessing these directly.
  bool profile_load_attempted_ = false;
  bool profile_load_succeeded_ = false;
  ProfileFile cached_profile_;

  // Cached value of the profile file.
  // Use the GetOldProfile method rather than accessing these directly.
  bool old_profile_load_attempted_ = false;
  bool old_profile_load_succeeded_ = false;
  ProfileFile cached_old_profile_;

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

  // Returns an oat file that can be used for loading dex files.
  // Returns nullptr if no suitable oat file was found.
  //
  // After this call, no other methods of the OatFileManager should be called,
  // because access to the loaded oat file has been taken away from the
  // OatFileManager object.
  std::unique_ptr<OatFile> GetBestOatFile();

  // Loads the dex files in the given oat file for the given dex location.
  // The oat file should be up to date for the given dex location.
  // This loads multiple dex files in the case of multidex.
  // Returns an empty vector if no dex files for that location could be loaded
  // from the oat file.
  //
  // The caller is responsible for freeing the dex_files returned, if any. The
  // dex_files will only remain valid as long as the oat_file is valid.
  static std::vector<std::unique_ptr<const DexFile>> LoadDexFiles(
      const OatFile& oat_file, const char* dex_location);

 private:
  DISALLOW_COPY_AND_ASSIGN(ExecutableOatFileManager);
};


}  // namespace art

#endif  // ART_RUNTIME_OAT_FILE_MANAGER_H_
