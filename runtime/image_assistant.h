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

class ImageInfo;

enum ImageState {
  // No image was found or all possibilities are unusable. Requires recompilation.
  kImageUnusable,
  // The image is current but needs to be relocated before use. Only returned for system images
  kImageNeedsRelocation,
  // The image cannot be used because it is out of date. Only returned for cache images. Only
  // returned when relocation is possible.
  kImageOutOfDate,
  // The image will/could be used by a runtime
  kImageUsable,
  // The image is usable but will never be used because a better possibility exists. Only returned
  // for system images.
  kImageNotPrefered,
};

class ImageAssistant {
 public:
  explicit ImageAssistant(const std::string& location)
      : location_(location), isa_(kRuntimeISA) {}
  ImageAssistant(const std::string& location, InstructionSet isa)
      : location_(location), isa_(isa) {}
  ImageAssistant(const char* location, InstructionSet isa)
      : location_(location), isa_(isa) {}

  // Fills the filename with the filename associated with the image location this was constructed
  // with. A return value of false indicates that there is no file present at that filename.
  bool FindSystemImageFilename(std::string* filename) const;
  bool FindCacheImageFilename(std::string* filename) const;

  // Returns the filename of the image corresponding to
  // requested image_location, or the filename where a new image
  // should be written if one doesn't exist. Looks for a generated
  // image in the specified location and then in the dalvik-cache.
  //
  // Returns true if an image was found, false otherwise.
  bool FindImageFilename(std::string* system_location,
                         bool* has_system,
                         std::string* data_location,
                         bool* dalvik_cache_exists,
                         bool* has_data,
                         bool* is_global_cache) const;

  // Gives the image info of the image that should be loaded if possible. (This means always the
  // cache one when relocation is enabled and the system one only if there is no cache image when
  // relocation is disabled.)
  ImageInfo GetSelectedImageInfo() const;
  ImageInfo GetSelectedImageInfo(bool relocation_needed) const;
  ImageInfo GetCacheImageInfo() const;
  ImageInfo GetSystemImageInfo() const;

  bool ImageCreationAllowed(std::string* reason) const;

  const std::string& GetLocation() const {
    return location_;
  }

 private:
  std::string location_;
  InstructionSet isa_;

  DISALLOW_COPY_AND_ASSIGN(ImageAssistant);
};

// Cached data about whichever image the system would select.
class ImageInfo {
 public:
  // Returns true if this is the system partition copy of an image.
  bool IsSystem() const { return is_system_; }

  // Returns true if this is the cache partition copy of an image.
  bool IsCache() const { return !is_system_; }

  // Returns the file this image may be found at, if it exists.
  const std::string& GetFilename() const {
    return (is_system_) ? system_filename_ : cache_filename_;
  }

  // Returns a copy of the images header. The caller is responsible for destroying this.
  bool GetImageHeader(ImageHeader* header) const;
  ImageState GetImageState() const;
  ImageState GetImageState(bool relocation_needed) const;
  bool IsLoaded() const;

  bool IsValid() const;

  File* OpenImage() const;

  bool IsRelocated() const;

 private:
  ImageInfo(std::string system_filename, std::string cache_filename,
            bool is_system)
      : system_filename_(system_filename),
        cache_filename_(cache_filename),
        is_system_(is_system) {}

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
