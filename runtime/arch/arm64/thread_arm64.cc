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

#include "thread.h"

#include "asm_support_arm64.h"
#include "base/logging.h"
#include "base/macros.h"
#include "quick_method_frame_info_arm64.h"

namespace art {

void Thread::InitCpu() {
  CHECK_EQ(THREAD_FLAGS_OFFSET, ThreadFlagsOffset<8>().Int32Value());
  CHECK_EQ(THREAD_CARD_TABLE_OFFSET, CardTableOffset<8>().Int32Value());
  CHECK_EQ(THREAD_EXCEPTION_OFFSET, ExceptionOffset<8>().Int32Value());
  CHECK_EQ(THREAD_ID_OFFSET, ThinLockIdOffset<8>().Int32Value());
}

void Thread::CleanupCpu() {
  // Do nothing.
}

// Misuse this file to statically check the assembly routine sizes against the C-level sizes.

namespace {

static_assert(FRAME_SIZE_SAVE_ALL_CALLEE_SAVE == arm64::Arm64CalleeSaveFrameSize(Runtime::kSaveAll),
              "Unexpected size for save-all runtime method frame size");

static_assert(FRAME_SIZE_REFS_ONLY_CALLEE_SAVE ==
                  arm64::Arm64CalleeSaveFrameSize(Runtime::kRefsOnly),
              "Unexpected size for refs-ony runtime method frame size");

static_assert(FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE ==
                  arm64::Arm64CalleeSaveFrameSize(Runtime::kRefsAndArgs),
              "Unexpected size " STRINGIFY(FRAME_SIZE_REFS_AND_ARGS_CALLEE_SAVE) " for refs-and-args runtime method frame size");

}  // namespace


}  // namespace art
