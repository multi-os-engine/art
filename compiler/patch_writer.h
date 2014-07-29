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

#ifndef ART_COMPILER_PATCH_WRITER_H_
#define ART_COMPILER_PATCH_WRITER_H_

#include "instruction_set.h"

namespace art {

// Type of relative patch.
enum PatchFormat {
  kAbsoluteAddressPatch,  // A patch based on absolute addresses (needs relocation).
  kRelativeCallPatch,     // A relative call patch (does not need relocation).
};

class PatchWriter {
 public:
  virtual ~PatchWriter() {}

  /// @brief Write the value at the given location in the text segment.
  /// @details The format used when writing the value depends on the chosen target. The value may
  ///   be written simply as a uint32_t or may be written as one or more assembly instructions.
  virtual void WritePatch(PatchFormat fmt, uint32_t* location, int32_t value) const = 0;

  /// @brief Read the value from the given location.
  /// @note This function does the inverse of what WritePatch() does.
  virtual int32_t ReadPatch(PatchFormat fmt, uint32_t* location) const = 0;

  /// @brief Update the value at the specified location by the given increment.
  virtual int32_t UpdatePatch(PatchFormat fmt, uint32_t* location, off_t inc_value) const {
    int32_t new_value = ReadPatch(fmt, location) + inc_value;
    WritePatch(fmt, location, new_value);
    return new_value;
  }

  /// @brief Create a patch writer appropriate for the given instruction set.
  static PatchWriter* CreatePatchWriter(InstructionSet instruction_set);
};

/// @brief Patch writer which patches 32-bit integers (int32_t).
class GenericPatchWriter : public PatchWriter {
 public:
  void WritePatch(PatchFormat fmt, uint32_t* location, int32_t value) const OVERRIDE {
    location[0] = static_cast<uint32_t>(value);
  }

  int32_t ReadPatch(PatchFormat fmt, uint32_t *location) const OVERRIDE {
    return static_cast<int32_t>(location[0]);
  }
};

// @brief Patch writer which patches movz/movk or bl instructions (Arm64 specific).
class Arm64PatchWriter : public PatchWriter {
 public:
  void WritePatch(PatchFormat fmt, uint32_t* location, int32_t value) const OVERRIDE {
    if (fmt == kRelativeCallPatch) {
      CHECK_EQ(value & 0x3, 0);
      if ((value >= (INT32_C(1) << 27)) || (value < -(INT32_C(1) << 27))) {
        LOG(FATAL) << "Relative call out of range. Delta is " << value;
      }
      uint32_t imm26 = static_cast<uint32_t>((value >> 2) & ((UINT32_C(1) << 26) - 1));
      location[0] = UINT32_C(0x94000000) | imm26;
    } else {
      uint32_t movz_instruction = location[0];
      uint32_t src_instruction_bits = movz_instruction & UINT32_C(0x8000001f);

      if (kIsDebugBuild) {
        // Check that we are really patching a movz/movk pair.
        uint32_t movk_instruction = location[1];
        // Check that we are really patching a movz/movk pair.
        CHECK_EQ((movz_instruction & UINT32_C(0x7fe00000)), UINT32_C(0x52800000))
          << "Expected a movz instruction";
        CHECK_EQ((movk_instruction & UINT32_C(0x7fe00000)), UINT32_C(0x72a00000))
          << "Expected a movk instruction";
        CHECK_EQ((movz_instruction & UINT32_C(0x8000001f)),
                 (movk_instruction & UINT32_C(0x8000001f)))
          << "Expected a matching pair of movz, movk instructions";
      }

      uint32_t value_lo = static_cast<uint32_t>(value) & UINT32_C(0xffff);
      uint32_t value_hi = static_cast<uint32_t>(value) >> 16;
      location[0] = UINT32_C(0x52800000) | (value_lo << 5) | src_instruction_bits;
      location[1] = UINT32_C(0x72a00000) | (value_hi << 5) | src_instruction_bits;
    }
  }

  int32_t ReadPatch(PatchFormat fmt, uint32_t *location) const OVERRIDE {
    if (fmt == kRelativeCallPatch) {
      UNIMPLEMENTED(FATAL);
      return 0;
    } else {
      uint32_t movz_instruction = location[0];
      uint32_t movk_instruction = location[1];

      if (kIsDebugBuild) {
        // Check that we are really patching a movz/movk pair.
        CHECK_EQ((movz_instruction & UINT32_C(0x7fe00000)), UINT32_C(0x52800000))
          << "Expected a movz instruction";
        CHECK_EQ((movk_instruction & UINT32_C(0x7fe00000)), UINT32_C(0x72a00000))
          << "Expected a movk instruction";
        CHECK_EQ((movz_instruction & UINT32_C(0x8000001f)),
                 (movk_instruction & UINT32_C(0x8000001f)))
          << "Expected a matching pair of movz, movk instructions";
      }

      return static_cast<int32_t>(((movz_instruction & UINT32_C(0x1fffe0)) >> 5) |
                                  (((movk_instruction & UINT32_C(0x1fffe0)) >> 5) << 16));
    }
  }
};

inline PatchWriter* PatchWriter::CreatePatchWriter(InstructionSet instruction_set) {
  switch (instruction_set) {
    case kArm64:
      return new Arm64PatchWriter();
    default:
      return new GenericPatchWriter();
  }
}

}  // namespace art
#endif  // ART_COMPILER_PATCH_WRITER_H_
