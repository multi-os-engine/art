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

#ifndef ART_RUNTIME_IMAGE_ASSISTANT_H_
#define ART_RUNTIME_IMAGE_ASSISTANT_H_

#include <cstdint>
#include <memory>
#include <string>

#include "arch/instruction_set.h"
#include "image.h"
#include "os.h"

namespace art {

// This file declares the ImageState, ImageAssistant and ImageInfo classes.

// Forward declaration, defined below.
class ImageInfo;

enum class ImageState {
  // No image was found or all possibilities are unusable. Requires recompilation.
  kImageUnusable,
  // The image is current but needs to be relocated before use. Only returned for system images.
  // Only returned when relocation is enabled. If relocation was disabled this would become
  // kImageUsable.
  kImageNeedsRelocation,
  // The image cannot be used because it is out of date (checksum does not match /system image or
  // the cache image does not exist and a /system one does). Only returned for cache images. Only
  // returned when relocation is possible.
  kImageOutOfDate,
  // The image will/could be used by a runtime
  kImageUsable,
  // The image is usable but will never be used because a better possibility exists (there is a
  // cache-image with a matching checksum). Only returned for system images.
  kImageNotPreferred,
};

class ImageAssistant {
 public:
  explicit ImageAssistant(const std::string& location)
      : location_(location), isa_(kRuntimeISA) {}
  ImageAssistant(const std::string& location, InstructionSet isa)
      : location_(location), isa_(isa) {}
  ImageAssistant(const char* location, InstructionSet isa)
      : location_(location), isa_(isa) {}

  std::string GetImageFilename() const;
  std::string GetSystemImageFilename() const;
  std::string GetCacheImageFilename() const;

  // Gives the image info of the image that should be loaded if possible. (This means always the
  // cache one when relocation is enabled and the system one only if there is no cache image when
  // relocation is disabled.)
  ImageInfo GetImageInfo() const;
  // Do the same as GetImageInfo above except perform the computation assuming that relocation is
  // needed or not depending on the argument.
  ImageInfo GetImageInfo(bool relocation_needed) const;
  // Get the image information for the image in DALVIK_CACHE
  ImageInfo GetCacheImageInfo() const;
  // Get the image information for the image in /system
  ImageInfo GetSystemImageInfo() const;

  // Returns true if the image-space would consider creating an image for the location possible.
  // This means that the dalvik-cache exists or is not the global one (which needs special
  // permissions).
  bool ImageCreationAllowed(std::string* error_msg) const;

  const std::string& GetLocation() const {
    return location_;
  }

  InstructionSet GetIsa() const {
    return isa_;
  }

  static bool ImageIsUsable(const char* location, InstructionSet isa);

 private:
  ImageInfo GetSpecialImageInfo(bool is_system) const;

  const std::string location_;
  const InstructionSet isa_;
};

// Cached data about whichever image the system would select.
class ImageInfo {
 public:
  // Returns true if this is the system partition copy of an image.
  bool IsSystemImage() const { return is_system_; }

  // Returns the file this image may be found at, if it exists.
  const std::string& GetFilename() const {
    return is_system_ ? system_filename_ : cache_filename_;
  }

  // Writes a copy of the image's header and returns true if the image is readable and valid.
  // Otherwise the contents of 'header' are undefined after this call.
  bool GetImageHeader(ImageHeader* header) const;

  // Returns the image state. It determines whether relocation is enabled by asking the runtime.
  // Whether relocation is enabled or not may affect the returned states. For example if relocation
  // is not enabled kImageOutOfDate and kImageNeedsRelocation will never be returned (since these
  // both say that relocation of some sort is needed). This allows the user not to worry about the
  // state of relocation.
  ImageState GetImageState() const;
  ImageState GetImageState(bool relocation_needed) const;
  bool IsImageLoaded() const;

  bool IsImageValid() const;

  std::unique_ptr<File> OpenImage(std::string* error_msg) const;

  bool IsRelocated() const;

  bool IsImageUsable() const {
    return GetImageState() == ImageState::kImageUsable;
  }
  bool IsImageUsable(bool relocation_needed) const {
    return GetImageState(relocation_needed) == ImageState::kImageUsable;
  }

 private:
  ImageInfo(std::string system_filename,
            std::string cache_filename,
            bool is_system)
      : system_filename_(system_filename),
        cache_filename_(cache_filename),
        is_system_(is_system) {}

  std::unique_ptr<File> OpenImageUnchecked() const;

  // Since several functions require being able to examine both image files in order to run we store
  // both of the filenames in this object. We can use these functions to do some recursive calls to
  // the other type of image. For example we need to read the other image type to see if their
  // checksums match, which requires opening both files (if they exist).
  ImageInfo GetSystemInfo() const {
    return ImageInfo(system_filename_, cache_filename_, true);
  }
  ImageInfo GetCacheInfo() const {
    return ImageInfo(system_filename_, cache_filename_, false);
  }

  std::string system_filename_;
  std::string cache_filename_;
  bool is_system_;

  friend class ImageAssistant;
};

}  // namespace art

#endif  // ART_RUNTIME_IMAGE_ASSISTANT_H_
