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

# mips Compiler files common to Quick and Optimizing compilers
LIBART_COMPILER_SRC_MIPS_FILES := \
	jni/quick/mips/calling_convention_mips.cc \
	utils/mips/assembler_mips.cc \
	utils/mips/managed_register_mips.cc

# mips Quick compiler files
LIBART_COMPILER_SRC_MIPS_QUICK_FILES := \
	dex/quick/mips/assemble_mips.cc \
	dex/quick/mips/call_mips.cc \
	dex/quick/mips/fp_mips.cc \
	dex/quick/mips/int_mips.cc \
	dex/quick/mips/target_mips.cc \
	dex/quick/mips/utility_mips.cc

# mips Optimizing compiler files
LIBART_COMPILER_SRC_MIPS_OPTIMIZING_FILES :=

# mips64 Compiler files common to Quick and Optimizing compilers
LIBART_COMPILER_SRC_MIPS_64_FILES := \
	jni/quick/mips64/calling_convention_mips64.cc \
	utils/mips64/assembler_mips64.cc \
	utils/mips64/managed_register_mips64.cc

# mips64 Quick compiler files
LIBART_COMPILER_SRC_MIPS_64_QUICK_FILES :=

# mips64 Optimizing compiler files
LIBART_COMPILER_SRC_MIPS_64_OPTIMIZING_FILES :=

# dummy files for other ISAs when building only for mips
LIBART_COMPILER_SRC_MIPS_UNSUPPORTED_FILES := \
	error/quick/unsupported_arm.cc \
	error/quick/unsupported_arm64.cc \
	error/quick/unsupported_mips64.cc \
	error/quick/unsupported_x86.cc \
	error/quick/unsupported_x86_64.cc \
	error/optimizing/unsupported_arm.cc \
	error/optimizing/unsupported_arm64.cc \
	error/optimizing/unsupported_mips64.cc \
	error/optimizing/unsupported_x86.cc \
	error/optimizing/unsupported_x86_64.cc

# dummy files for other ISAs when building only for mips64
LIBART_COMPILER_SRC_MIPS_64_UNSUPPORTED_FILES := \
	error/quick/unsupported_arm.cc \
	error/quick/unsupported_arm64.cc \
	error/quick/unsupported_x86.cc \
	error/quick/unsupported_x86_64.cc \
	error/optimizing/unsupported_arm.cc \
	error/optimizing/unsupported_arm64.cc \
	error/optimizing/unsupported_x86.cc \
	error/optimizing/unsupported_x86_64.cc

ifeq ($(COMPILER_TYPE),quick)
  LIBART_COMPILER_SRC_MIPS_FILES += ${LIBART_COMPILER_SRC_MIPS_QUICK_FILES}
  LIBART_COMPILER_SRC_MIPS_64_FILES += ${LIBART_COMPILER_SRC_MIPS_64_QUICK_FILES}
else ifeq ($(COMPILER_TYPE),optimizing)
  LIBART_COMPILER_SRC_MIPS_FILES += ${LIBART_COMPILER_SRC_MIPS_OPTIMIZING_FILES}
  LIBART_COMPILER_SRC_MIPS_64_FILES += ${LIBART_COMPILER_SRC_MIPS_64_OPTIMIZING_FILES}
else
  LIBART_COMPILER_SRC_MIPS_FILES += ${LIBART_COMPILER_SRC_MIPS_QUICK_FILES}
  LIBART_COMPILER_SRC_MIPS_FILES += ${LIBART_COMPILER_SRC_MIPS_OPTIMIZING_FILES}
  LIBART_COMPILER_SRC_MIPS_64_FILES += ${LIBART_COMPILER_SRC_MIPS_64_QUICK_FILES}
  LIBART_COMPILER_SRC_MIPS_64_FILES += ${LIBART_COMPILER_SRC_MIPS_64_OPTIMIZING_FILES}
endif
