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

# arm Compiler files that are common to Quick and Optimizing compilers
LIBART_COMPILER_SRC_ARM_FILES := \
	utils/arm/assembler_arm.cc \
	utils/arm/assembler_arm32.cc \
	utils/arm/assembler_thumb2.cc \
	utils/arm/managed_register_arm.cc \
	jni/quick/arm/calling_convention_arm.cc \
	linker/arm/relative_patcher_arm_base.cc \
	linker/arm/relative_patcher_thumb2.cc

# arm Quick compiler files
LIBART_COMPILER_SRC_ARM_QUICK_FILES := \
	dex/quick/arm/assemble_arm.cc \
	dex/quick/arm/call_arm.cc \
	dex/quick/arm/fp_arm.cc \
	dex/quick/arm/int_arm.cc \
	dex/quick/arm/target_arm.cc \
	dex/quick/arm/utility_arm.cc

# arm Optimizing compiler files
LIBART_COMPILER_SRC_ARM_OPTIMIZING_FILES := \
	optimizing/intrinsics_arm.cc \
	optimizing/code_generator_arm.cc

# arm64 Compiler files that are common to Quick and Optimizing compilers
LIBART_COMPILER_SRC_ARM_64_FILES := \
	utils/arm64/assembler_arm64.cc \
	utils/arm64/managed_register_arm64.cc \
	jni/quick/arm64/calling_convention_arm64.cc \
	linker/arm64/relative_patcher_arm64.cc

# arm64 Quick compiler files
LIBART_COMPILER_SRC_ARM_64_QUICK_FILES := \
	dex/quick/arm64/assemble_arm64.cc \
	dex/quick/arm64/call_arm64.cc \
	dex/quick/arm64/fp_arm64.cc \
	dex/quick/arm64/int_arm64.cc \
	dex/quick/arm64/target_arm64.cc \
	dex/quick/arm64/utility_arm64.cc

# arm64 Optimizing compiler files
LIBART_COMPILER_SRC_ARM_64_OPTIMIZING_FILES := \
	optimizing/intrinsics_arm64.cc \
	optimizing/code_generator_arm64.cc

ifeq ($(COMPILER_TYPE),quick)
  LIBART_COMPILER_SRC_ARM_FILES += ${LIBART_COMPILER_SRC_ARM_QUICK_FILES}
  LIBART_COMPILER_SRC_ARM_64_FILES += ${LIBART_COMPILER_SRC_ARM_64_QUICK_FILES}
else ifeq ($(COMPILER_TYPE),optimizing)
  LIBART_COMPILER_SRC_ARM_FILES += ${LIBART_COMPILER_SRC_ARM_OPTIMIZING_FILES}
  LIBART_COMPILER_SRC_ARM_64_FILES += ${LIBART_COMPILER_SRC_ARM_64_OPTIMIZING_FILES}
else
  LIBART_COMPILER_SRC_ARM_FILES += ${LIBART_COMPILER_SRC_ARM_QUICK_FILES}
  LIBART_COMPILER_SRC_ARM_FILES += ${LIBART_COMPILER_SRC_ARM_OPTIMIZING_FILES}
  LIBART_COMPILER_SRC_ARM_64_FILES += ${LIBART_COMPILER_SRC_ARM_64_QUICK_FILES}
  LIBART_COMPILER_SRC_ARM_64_FILES += ${LIBART_COMPILER_SRC_ARM_64_OPTIMIZING_FILES}
endif
