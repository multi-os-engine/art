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

include art/build/Android.common_build.mk

include $(CLEAR_VARS)
LOCAL_CFLAGS += $(ART_TARGET_CFLAGS)
LOCAL_SRC_FILES := runtime/native/byte_swap_utils_benchmark.cc
LOCAL_C_INCLUDES += $(ART_C_INCLUDES) bionic/benchmarks
LOCAL_MODULE_TAGS := debug
LOCAL_MODULE := test-art-benchmark_byte_swap_utils_benchmark
LOCAL_MULTILIB := both
LOCAL_MODULE_STEM_32 := $(LOCAL_MODULE)32
LOCAL_MODULE_STEM_64 := $(LOCAL_MODULE)64
include $(BUILD_NATIVE_BENCHMARK)
