/*
 * Copyright (C) 2011 The Android Open Source Project
 * Copyright 2014-2016 Intel Corporation
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

#include "image.h"

#include "base/bit_utils.h"
#include "mirror/object_array.h"
#include "mirror/object_array-inl.h"
#include "mirror/object-inl.h"
#ifdef MOE
#include <TargetConditionals.h>
#endif

namespace art {

const uint8_t ImageHeader::kImageMagic[] = { 'a', 'r', 't', '\n' };
const uint8_t ImageHeader::kImageVersion[] = { '0', '2', '2', '\0' };

#ifndef MOE
ImageHeader::ImageHeader(uint32_t image_begin,
#else
ImageHeader::ImageHeader(uint64_t image_begin,
#endif
                         uint32_t image_size,
                         ImageSection* sections,
#ifndef MOE
                         uint32_t image_roots,
#else
                         uint64_t image_roots,
#endif
                         uint32_t oat_checksum,
#ifndef MOE
                         uint32_t oat_file_begin,
                         uint32_t oat_data_begin,
                         uint32_t oat_data_end,
                         uint32_t oat_file_end,
#else
                         uint64_t oat_file_begin,
                         uint64_t oat_data_begin,
                         uint64_t oat_data_end,
                         uint64_t oat_file_end,
#endif
                         uint32_t pointer_size,
                         bool compile_pic)
  : image_begin_(image_begin),
    image_size_(image_size),
    oat_checksum_(oat_checksum),
    oat_file_begin_(oat_file_begin),
    oat_data_begin_(oat_data_begin),
    oat_data_end_(oat_data_end),
    oat_file_end_(oat_file_end),
    patch_delta_(0),
    image_roots_(image_roots),
    pointer_size_(pointer_size),
    compile_pic_(compile_pic) {

#if defined(MOE) && TARGET_OS_OSX
  size_t pageSize = 4096;
#else
  size_t pageSize = kPageSize;
#endif
  CHECK_EQ(image_begin, RoundUp(image_begin, pageSize));
  CHECK_EQ(oat_file_begin, RoundUp(oat_file_begin, pageSize));
  CHECK_EQ(oat_data_begin, RoundUp(oat_data_begin, pageSize));
  CHECK_LT(image_begin, image_roots);
  CHECK_LT(image_roots, oat_file_begin);
  CHECK_LE(oat_file_begin, oat_data_begin);
  CHECK_LT(oat_data_begin, oat_data_end);
  CHECK_LE(oat_data_end, oat_file_end);
  CHECK(ValidPointerSize(pointer_size_)) << pointer_size_;
  memcpy(magic_, kImageMagic, sizeof(kImageMagic));
  memcpy(version_, kImageVersion, sizeof(kImageVersion));
  std::copy_n(sections, kSectionCount, sections_);
}

void ImageHeader::RelocateImage(off_t delta) {
  CHECK_ALIGNED(delta, kPageSize) << " patch delta must be page aligned";
  image_begin_ += delta;
  oat_file_begin_ += delta;
  oat_data_begin_ += delta;
  oat_data_end_ += delta;
  oat_file_end_ += delta;
  image_roots_ += delta;
  patch_delta_ += delta;
  for (size_t i = 0; i < kImageMethodsCount; ++i) {
    image_methods_[i] += delta;
  }
}

bool ImageHeader::IsValid() const {
  if (memcmp(magic_, kImageMagic, sizeof(kImageMagic)) != 0) {
    return false;
  }
  if (memcmp(version_, kImageVersion, sizeof(kImageVersion)) != 0) {
    return false;
  }
  // Unsigned so wraparound is well defined.
  if (image_begin_ >= image_begin_ + image_size_) {
    return false;
  }
  if (oat_file_begin_ > oat_file_end_) {
    return false;
  }
  if (oat_data_begin_ > oat_data_end_) {
    return false;
  }
#ifndef MOE
  if (oat_file_begin_ >= oat_data_begin_) {
#else
  if (oat_file_begin_ > oat_data_begin_) {
#endif
    return false;
  }
  if (image_roots_ <= image_begin_ || oat_file_begin_ <= image_roots_) {
    return false;
  }
  if (!IsAligned<kPageSize>(patch_delta_)) {
    return false;
  }
#ifdef MOE
  if (!IsAligned<kPageSize>(image_begin_)) {
    return false;
  }
  if (!IsAligned<kPageSize>(oat_data_begin_)) {
    return false;
  }
#endif
  return true;
}

const char* ImageHeader::GetMagic() const {
  CHECK(IsValid());
  return reinterpret_cast<const char*>(magic_);
}

mirror::Object* ImageHeader::GetImageRoot(ImageRoot image_root) const {
  return GetImageRoots()->Get(image_root);
}

mirror::ObjectArray<mirror::Object>* ImageHeader::GetImageRoots() const {
  // Need a read barrier as it's not visited during root scan.
  // Pass in the address of the local variable to the read barrier
  // rather than image_roots_ because it won't move (asserted below)
  // and it's a const member.
  mirror::ObjectArray<mirror::Object>* image_roots =
      reinterpret_cast<mirror::ObjectArray<mirror::Object>*>(image_roots_);
  mirror::ObjectArray<mirror::Object>* result =
      ReadBarrier::BarrierForRoot<mirror::ObjectArray<mirror::Object>, kWithReadBarrier, true>(
          &image_roots);
  DCHECK_EQ(image_roots, result);
  return result;
}

ArtMethod* ImageHeader::GetImageMethod(ImageMethod index) const {
  CHECK_LT(static_cast<size_t>(index), kImageMethodsCount);
  return reinterpret_cast<ArtMethod*>(image_methods_[index]);
}

void ImageHeader::SetImageMethod(ImageMethod index, ArtMethod* method) {
  CHECK_LT(static_cast<size_t>(index), kImageMethodsCount);
  image_methods_[index] = reinterpret_cast<uint64_t>(method);
}

const ImageSection& ImageHeader::GetImageSection(ImageSections index) const {
  CHECK_LT(static_cast<size_t>(index), kSectionCount);
  return sections_[index];
}

std::ostream& operator<<(std::ostream& os, const ImageSection& section) {
  return os << "size=" << section.Size() << " range=" << section.Offset() << "-" << section.End();
}

void ImageSection::VisitPackedArtFields(ArtFieldVisitor* visitor, uint8_t* base) const {
  for (size_t pos = 0; pos < Size(); ) {
    auto* array = reinterpret_cast<LengthPrefixedArray<ArtField>*>(base + Offset() + pos);
    for (size_t i = 0; i < array->Length(); ++i) {
      visitor->Visit(&array->At(i, sizeof(ArtField)));
    }
    pos += array->ComputeSize(array->Length());
  }
}

void ImageSection::VisitPackedArtMethods(ArtMethodVisitor* visitor,
                                         uint8_t* base,
                                         size_t pointer_size) const {
  const size_t method_alignment = ArtMethod::Alignment(pointer_size);
  const size_t method_size = ArtMethod::Size(pointer_size);
  for (size_t pos = 0; pos < Size(); ) {
    auto* array = reinterpret_cast<LengthPrefixedArray<ArtMethod>*>(base + Offset() + pos);
    for (size_t i = 0; i < array->Length(); ++i) {
      visitor->Visit(&array->At(i, method_size, method_alignment));
    }
    pos += array->ComputeSize(array->Length(), method_size, method_alignment);
  }
}

}  // namespace art
