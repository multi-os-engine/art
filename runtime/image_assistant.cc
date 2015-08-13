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

#include "image_assistant.h"

#include "base/unix_file/fd_file.h"
#include "gc/heap.h"
#include "gc/space/image_space.h"
#include "runtime.h"
#include "utils.h"
#include "os.h"

namespace art {

bool ImageInfo::IsImageValid() const {
  ImageHeader header;
  return GetImageHeader(&header);
}

std::unique_ptr<File> ImageInfo::OpenImage() const {
  return std::unique_ptr<File>(
      OS::OpenFileForReading(is_system_ ? system_filename_.c_str() : cache_filename_.c_str()));
}

bool ImageInfo::GetImageHeader(ImageHeader* header) const {
  DCHECK(header != nullptr);
  std::unique_ptr<File> image_file(OpenImage());
  if (image_file == nullptr) {
    return false;
  }
  const bool success = image_file->ReadFully(header, sizeof(ImageHeader));
  if (!success || !header->IsValid()) {
    return false;
  }
  return true;
}

ImageState ImageInfo::GetImageState() const {
  Runtime* runtime = Runtime::Current();
  return GetImageState(runtime == nullptr || runtime->ShouldRelocate());
}

ImageState ImageInfo::GetImageState(bool relocation_needed) const {
  if (is_system_) {
    // Possibilities: kImageUnusable, kImageUsable, kImageNeedsRelocation, kImageNotPreferred
    if (!IsImageValid()) {
      // We cannot use this image. The header fails verification or does not exist.
      return ImageState::kImageUnusable;
    }
    // We always prefer cache if it is available so we will do this recursive call to find out about
    // the cache state.
    ImageState cache_state = GetCacheInfo().GetImageState(relocation_needed);
    if (cache_state == ImageState::kImageUsable) {
      // We have a usable cache image so will not use this one.
      return ImageState::kImageNotPreferred;
    } else {
      // This is a good image, but relocate if need be.
      CHECK(cache_state == ImageState::kImageOutOfDate ||
            cache_state == ImageState::kImageUnusable);
      return relocation_needed ? ImageState::kImageNeedsRelocation : ImageState::kImageUsable;
    }
  } else {
    // Possibilities: kImageUnusable, kImageUsable, kImageOutOfDate
    ImageHeader system_header;
    ImageHeader cache_header;
    bool got_cache_header = GetImageHeader(&cache_header);
    bool got_system_header = GetSystemInfo().GetImageHeader(&system_header);
    if (got_system_header && !got_cache_header) {
      return relocation_needed ? ImageState::kImageOutOfDate : ImageState::kImageUnusable;
    } else if (!got_system_header && !got_cache_header) {
      // We cannot find anything!
      return ImageState::kImageUnusable;
    } else if (!got_system_header && got_cache_header) {
      // No system image to check, so we are good.
      return ImageState::kImageUsable;
    } else if (system_header.GetOatChecksum() != cache_header.GetOatChecksum()) {
      // We have a checksum mis-match. Relocate if we can otherwise we cannot use this.
      return relocation_needed ? ImageState::kImageOutOfDate : ImageState::kImageUnusable;
    } else {
      // Everything is awesome.
      return ImageState::kImageUsable;
    }
  }
}

bool ImageInfo::IsRelocated() const {
  if (IsSystemImage()) {
    // Only the cache image can be a relocated image
    return false;
  }
  ImageHeader sys, cache;
  if (!GetImageHeader(&cache) || !GetSystemInfo().GetImageHeader(&sys)) {
    // If the cache image does not exist/is invalid or the system image does not exist/is invalid
    // then we cannot be a relocated image.
    return false;
  }
  // We are a relocated image if we have the same checksum as the system image.
  return cache.GetOatChecksum() == sys.GetOatChecksum();
}

bool ImageAssistant::ImageCreationAllowed(std::string* reason) const {
  DCHECK(reason != nullptr);
  bool is_global_cache = false;
  bool have_android_data = false;
  bool dalvik_cache_exists = false;
  std::string dalvik_cache;
  GetDalvikCache(GetInstructionSetString(isa_),
                 false,
                 &dalvik_cache,
                 &have_android_data,
                 &dalvik_cache_exists,
                 &is_global_cache);
  if (!dalvik_cache_exists) {
    *reason = "dalvik_cache does not exist.";
    return false;
  }
  if (!is_global_cache) {
    return true;
  }
  Runtime* runtime = Runtime::Current();
  if (runtime != nullptr && runtime->IsZygote()) {
    return true;
  } else {
    *reason = "Only the zygote may create the global image";
    return false;
  }
}

bool ImageInfo::IsImageLoaded() const {
  Runtime* runtime = Runtime::Current();
  if (runtime == nullptr) {
    return false;
  }
  gc::Heap* heap = runtime->GetHeap();
  if (heap == nullptr) {
    return false;
  }
  gc::space::ImageSpace* space = heap->GetImageSpace();
  if (space == nullptr) {
    return false;
  }
  std::string file;
  return space->GetImageFilename() == GetFilename();
}

bool ImageAssistant::FindImageFilename(std::string* system_filename,
                                       bool* has_system,
                                       std::string* cache_filename,
                                       bool* dalvik_cache_exists,
                                       bool* has_cache,
                                       bool* is_global_cache) const {
  DCHECK(system_filename != nullptr);
  DCHECK(cache_filename != nullptr);
  DCHECK(has_system != nullptr);
  DCHECK(dalvik_cache_exists != nullptr);
  DCHECK(has_cache != nullptr);
  DCHECK(is_global_cache != nullptr);
  *has_system = false;
  *has_cache = false;
  *dalvik_cache_exists = false;
  // image_location = /system/framework/boot.art
  // system_image_location = /system/framework/<image_isa>/boot.art
  *system_filename = GetSystemImageFilename(location_.c_str(), isa_);
  if (OS::FileExists(system_filename->c_str())) {
    *has_system = true;
  }

  bool have_android_data = false;
  std::string dalvik_cache;
  // Fill the out arguments.
  GetDalvikCache(GetInstructionSetString(isa_),
                 false,
                 &dalvik_cache,
                 &have_android_data,
                 dalvik_cache_exists,
                 is_global_cache);

  std::string error_msg;
  // Make sure the location is valid.
  if (!GetDalvikCacheFilename(location_.c_str(),
                              dalvik_cache.c_str(),
                              cache_filename,
                              &error_msg)) {
    LOG(WARNING) << error_msg;
    return *has_system;
  }
  *has_cache = have_android_data && *dalvik_cache_exists && OS::FileExists(cache_filename->c_str());
  return *has_system || *has_cache;
}

bool ImageAssistant::FindSystemImageFilename(std::string* filename) const {
  std::string cache_filename;
  bool has_system = false;
  bool dalvik_cache_exists = false;
  bool has_cache = false;
  bool is_global_cache = false;
  FindImageFilename(filename,
                    &has_system,
                    &cache_filename,
                    &dalvik_cache_exists,
                    &has_cache,
                    &is_global_cache);
  return has_system;
}

bool ImageAssistant::FindCacheImageFilename(std::string* filename) const {
  std::string system_filename;
  bool has_system = false;
  bool dalvik_cache_exists = false;
  bool has_cache = false;
  bool is_global_cache = false;
  FindImageFilename(&system_filename,
                    &has_system,
                    filename,
                    &dalvik_cache_exists,
                    &has_cache,
                    &is_global_cache);
  return has_cache;
}

ImageInfo ImageAssistant::GetImageInfo() const {
  Runtime* runtime = Runtime::Current();
  return GetImageInfo(runtime == nullptr || runtime->ShouldRelocate());
}

ImageInfo ImageAssistant::GetImageInfo(bool relocation_needed) const {
  ImageInfo cache = GetCacheImageInfo();
  if (relocation_needed || cache.GetImageState(relocation_needed) == ImageState::kImageUsable) {
    return cache;
  } else {
    ImageInfo system = cache.GetSystemInfo();
    if (system.GetImageState(relocation_needed) == ImageState::kImageUsable) {
      return system;
    } else {
      return cache;
    }
  }
}

ImageInfo ImageAssistant::GetCacheImageInfo() const {
  std::string cache_filename, system_filename;
  bool has_system = false;
  bool has_cache = false;
  bool dalvik_cache_exists = false;
  bool is_global_cache = false;
  FindImageFilename(&system_filename, &has_system, &cache_filename,
                    &dalvik_cache_exists, &has_cache, &is_global_cache);
  return ImageInfo(system_filename, cache_filename, false);
}

ImageInfo ImageAssistant::GetSystemImageInfo() const {
  // Use internal helper function to get the other type.
  return GetCacheImageInfo().GetSystemInfo();
}

bool ImageAssistant::ImageIsUsable(const char* location, InstructionSet isa) {
  return ImageAssistant(location, isa).GetImageInfo().GetImageState() == ImageState::kImageUsable;
}

}  // namespace art
