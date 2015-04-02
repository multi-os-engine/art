#
# Copyright (C) 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

LOCAL_PATH := $(call my-dir)

include art/build/Android.common_build.mk

# X86 Compiler files common to Quick and Optimizing compilers
LIBART_COMPILER_SRC_X86_FILES := \
	jni/quick/x86/calling_convention_x86.cc \
	linker/x86/relative_patcher_x86.cc \
	linker/x86/relative_patcher_x86_base.cc \
	utils/x86/assembler_x86.cc \
	utils/x86/managed_register_x86.cc

# X86 Quick compiler files
LIBART_COMPILER_SRC_X86_QUICK_FILES := \
	dex/quick/x86/assemble_x86.cc \
	dex/quick/x86/call_x86.cc \
	dex/quick/x86/fp_x86.cc \
	dex/quick/x86/int_x86.cc \
	dex/quick/x86/target_x86.cc \
	dex/quick/x86/utility_x86.cc

# X86 Optimizing compiler files
LIBART_COMPILER_SRC_X86_OPTIMIZING_FILES := \
	optimizing/code_generator_x86.cc \
	optimizing/intrinsics_x86.cc

# X86_64 Compiler files common to Quick and Optimizing compilers
LIBART_COMPILER_SRC_X86_64_FILES := \
	jni/quick/x86_64/calling_convention_x86_64.cc \
	linker/x86_64/relative_patcher_x86_64.cc \
	utils/x86_64/assembler_x86_64.cc \
	utils/x86_64/managed_register_x86_64.cc

# X86_64 Quick compiler files
LIBART_COMPILER_SRC_X86_64_QUICK_FILES :=

# X86_64 Optimizing compiler files
LIBART_COMPILER_SRC_X86_64_OPTIMIZING_FILES := \
	optimizing/code_generator_x86_64.cc \
	optimizing/intrinsics_x86_64.cc

# dummy files for x86
LIBART_COMPILER_SRC_X86_UNSUPPORTED_FILES := \
	error/quick/unsupported_x86.cc \
	error/optimizing/unsupported_x86.cc

# dummy files for x86_64
LIBART_COMPILER_SRC_X86_64_UNSUPPORTED_FILES := \
	error/quick/unsupported_x86_64.cc \
	error/optimizing/unsupported_x86_64.cc

ifeq ($(COMPILER_TYPE),quick)
  LIBART_COMPILER_SRC_X86_FILES += ${LIBART_COMPILER_SRC_X86_QUICK_FILES}
  LIBART_COMPILER_SRC_X86_64_FILES += ${LIBART_COMPILER_SRC_X86_64_QUICK_FILES}
else ifeq ($(COMPILER_TYPE),optimizing)
  LIBART_COMPILER_SRC_X86_FILES += ${LIBART_COMPILER_SRC_X86_OPTIMIZING_FILES}
  LIBART_COMPILER_SRC_X86_64_FILES += ${LIBART_COMPILER_SRC_X86_64_OPTIMIZING_FILES}
else
  LIBART_COMPILER_SRC_X86_FILES += ${LIBART_COMPILER_SRC_X86_QUICK_FILES}
  LIBART_COMPILER_SRC_X86_FILES += ${LIBART_COMPILER_SRC_X86_OPTIMIZING_FILES}
  LIBART_COMPILER_SRC_X86_64_FILES += ${LIBART_COMPILER_SRC_X86_64_QUICK_FILES}
  LIBART_COMPILER_SRC_X86_64_FILES += ${LIBART_COMPILER_SRC_X86_64_OPTIMIZING_FILES}
endif
