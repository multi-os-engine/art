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

# MIPS Compiler files common to Quick and Optimizing compilers
LIBART_COMPILER_SRC_MIPS_FILES := \
	utils/mips/assembler_mips.cc \
	utils/mips/managed_register_mips.cc \
	utils/mips64/assembler_mips64.cc \
	utils/mips64/managed_register_mips64.cc \
	jni/quick/mips/calling_convention_mips.cc \
	jni/quick/mips64/calling_convention_mips64.cc \
	linker/mips/relative_patcher_mips.cc

# MIPS Quick compiler files
LIBART_COMPILER_SRC_MIPS_QUICK_FILES := \
	dex/quick/mips/assemble_mips.cc \
	dex/quick/mips/call_mips.cc \
	dex/quick/mips/fp_mips.cc \
	dex/quick/mips/int_mips.cc \
	dex/quick/mips/target_mips.cc \
	dex/quick/mips/utility_mips.cc

# MIPS Optimizing compiler files
LIBART_COMPILER_SRC_MIPS_OPTIMIZING_FILES := \
	optimizing/code_generator_mips.cc

LIBART_COMPILER_SRC_FILES += ${LIBART_COMPILER_SRC_MIPS_FILES}
ifeq ($(COMPILER_TYPE),quick)
  LIBART_COMPILER_SRC_FILES += ${LIBART_COMPILER_SRC_MIPS_QUICK_FILES}
else ifeq ($(COMPILER_TYPE),optimizing)
  LIBART_COMPILER_SRC_FILES += ${LIBART_COMPILER_SRC_MIPS_OPTIMIZING_FILES}
else ifeq ($(COMPILER_TYPE),both)
  LIBART_COMPILER_SRC_FILES += ${LIBART_COMPILER_SRC_MIPS_QUICK_FILES}
  LIBART_COMPILER_SRC_FILES += ${LIBART_COMPILER_SRC_MIPS_OPTIMIZING_FILES}
endif

LOCAL_SRC_FILES := $$(LIBART_COMPILER_SRC_FILES)
