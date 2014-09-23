/*
 * Copyright (C) 2011 The Android Open Source Project
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

#ifndef ART_RUNTIME_ASM_SUPPORT_H_
#define ART_RUNTIME_ASM_SUPPORT_H_

#if defined(__cplusplus)
#include "runtime.h"
#include "thread.h"
#endif

#include "read_barrier_c.h"

#if defined(__arm__) ||  defined(__aarch64__) || defined(__mips__)
// In quick code for ARM, ARM64 and MIPS we make poor use of registers and perform frequent suspend
// checks in the event of loop back edges. The SUSPEND_CHECK_INTERVAL constant is loaded into a
// register at the point of an up-call or after handling a suspend check. It reduces the number of
// loads of the TLS suspend check value by the given amount (turning it into a decrement and compare
// of a register). This increases the time for a thread to respond to requests from GC and the
// debugger, damaging GC performance and creating other unwanted artifacts. For example, this count
// has the effect of making loops and Java code look cold in profilers, where the count is reset
// impacts where samples will occur. Reducing the count as much as possible improves profiler
// accuracy in tools like traceview.
// TODO: get a compiler that can do a proper job of loop optimization and remove this.
#define SUSPEND_CHECK_INTERVAL 1000
#endif

#if defined(__cplusplus)

#define ADD_TEST_EQ(x, y) CHECK_EQ(x, y);

static inline void CheckAsmSupportOffsetsAndSizes() {
#else
#define ADD_TEST_EQ(x, y)
#endif

// Size of references to the heap on the stack.
#define STACK_REFERENCE_SIZE 4
ADD_TEST_EQ(static_cast<size_t>(STACK_REFERENCE_SIZE), sizeof(art::StackReference<art::mirror::Object>))

// Note: these callee save methods loads require read barriers.
// Offset of field Runtime::callee_save_methods_[kSaveAll]
#define RUNTIME_SAVE_ALL_CALLEE_SAVE_FRAME_OFFSET 0
ADD_TEST_EQ(static_cast<size_t>(RUNTIME_SAVE_ALL_CALLEE_SAVE_FRAME_OFFSET),
            art::Runtime::GetCalleeSaveMethodOffset(art::Runtime::kSaveAll))

// Offset of field Runtime::callee_save_methods_[kRefsOnly]
#define RUNTIME_REFS_ONLY_CALLEE_SAVE_FRAME_OFFSET __SIZEOF_POINTER__
ADD_TEST_EQ(static_cast<size_t>(RUNTIME_REFS_ONLY_CALLEE_SAVE_FRAME_OFFSET),
            art::Runtime::GetCalleeSaveMethodOffset(art::Runtime::kRefsOnly))

// Offset of field Runtime::callee_save_methods_[kRefsAndArgs]
#define RUNTIME_REFS_AND_ARGS_CALLEE_SAVE_FRAME_OFFSET (2 * __SIZEOF_POINTER__)
ADD_TEST_EQ(static_cast<size_t>(RUNTIME_REFS_AND_ARGS_CALLEE_SAVE_FRAME_OFFSET),
            art::Runtime::GetCalleeSaveMethodOffset(art::Runtime::kRefsAndArgs))

// Offset of field Thread::tls32_.state_and_flags.
#define THREAD_FLAGS_OFFSET 0
ADD_TEST_EQ(THREAD_FLAGS_OFFSET,
            art::Thread::ThreadFlagsOffset<__SIZEOF_POINTER__>().Int32Value())

// Offset of field Thread::tls32_.thin_lock_thread_id.
#define THREAD_ID_OFFSET 12
ADD_TEST_EQ(THREAD_ID_OFFSET,
            art::Thread::ThinLockIdOffset<__SIZEOF_POINTER__>().Int32Value())

// Offset of field Thread::tlsPtr_.card_table.
#define THREAD_CARD_TABLE_OFFSET 120
ADD_TEST_EQ(THREAD_CARD_TABLE_OFFSET,
            art::Thread::CardTableOffset<__SIZEOF_POINTER__>().Int32Value())

// Offset of field Thread::tlsPtr_.exception.
#define THREAD_EXCEPTION_OFFSET (THREAD_CARD_TABLE_OFFSET + __SIZEOF_POINTER__)
ADD_TEST_EQ(THREAD_EXCEPTION_OFFSET,
            art::Thread::ExceptionOffset<__SIZEOF_POINTER__>().Int32Value())

// Offset of field Thread::tlsPtr_.managed_stack.top_quick_frame_.
#define THREAD_TOP_QUICK_FRAME_OFFSET (THREAD_CARD_TABLE_OFFSET + (3 * __SIZEOF_POINTER__))
ADD_TEST_EQ(THREAD_TOP_QUICK_FRAME_OFFSET,
            art::Thread::TopOfManagedStackOffset<__SIZEOF_POINTER__>().Int32Value())

// Offset of field Thread::tlsPtr_.managed_stack.top_quick_frame_.
#define THREAD_SELF_OFFSET (THREAD_CARD_TABLE_OFFSET + (8 * __SIZEOF_POINTER__))
ADD_TEST_EQ(THREAD_SELF_OFFSET,
            art::Thread::SelfOffset<__SIZEOF_POINTER__>().Int32Value())

// Offsets within java.lang.Object.
#define MIRROR_OBJECT_CLASS_OFFSET 0
#define MIRROR_OBJECT_LOCK_WORD_OFFSET 4

#if defined(USE_BAKER_OR_BROOKS_READ_BARRIER)
#define MIRROR_OBJECT_HEADER_SIZE 16
#else
#define MIRROR_OBJECT_HEADER_SIZE 8
#endif

// Offsets within java.lang.Class.
#define MIRROR_CLASS_COMPONENT_TYPE_OFFSET (4 + MIRROR_OBJECT_HEADER_SIZE)

// Array offsets.
#define MIRROR_ARRAY_LENGTH_OFFSET      MIRROR_OBJECT_HEADER_SIZE
#define MIRROR_CHAR_ARRAY_DATA_OFFSET   (4 + MIRROR_OBJECT_HEADER_SIZE)
#define MIRROR_OBJECT_ARRAY_DATA_OFFSET (4 + MIRROR_OBJECT_HEADER_SIZE)

// Offsets within java.lang.String.
#define MIRROR_STRING_VALUE_OFFSET  MIRROR_OBJECT_HEADER_SIZE
#define MIRROR_STRING_COUNT_OFFSET  (4 + MIRROR_OBJECT_HEADER_SIZE)
#define MIRROR_STRING_OFFSET_OFFSET (12 + MIRROR_OBJECT_HEADER_SIZE)

// Offsets within java.lang.reflect.ArtMethod.
#define MIRROR_ART_METHOD_DEX_CACHE_METHODS_OFFSET (4 + MIRROR_OBJECT_HEADER_SIZE)
#define MIRROR_ART_METHOD_PORTABLE_CODE_OFFSET     (32 + MIRROR_OBJECT_HEADER_SIZE)
#define MIRROR_ART_METHOD_QUICK_CODE_OFFSET        (40 + MIRROR_OBJECT_HEADER_SIZE)

#if defined(__cplusplus)
}  // End of CheckAsmSupportOffsets.
#endif

#endif  // ART_RUNTIME_ASM_SUPPORT_H_
