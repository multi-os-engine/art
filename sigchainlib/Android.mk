#
# Copyright (C) 2014 The Android Open Source Project
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

LOCAL_PATH:= $(call my-dir)

include art/build/Android.common.mk

SRC_FILES := sigchain.cc

include $(CLEAR_VARS)

ifeq ($(WITH_HOST_DALVIK),false)
  LOCAL_IS_HOST_MODULE := true
endif

LOCAL_CPP_EXTENSION := $(ART_CPP_EXTENSION)
LOCAL_MODULE_TAGS := optional
LOCAL_MODULE_CLASS := SHARED_LIBRARIES

ifeq ($(WITH_HOST_DALVIK),false)
  LOCAL_CLANG := false
  LOCAL_CFLAGS += $(ART_TARGET_CFLAGS)
else # host
  LOCAL_CLANG := true
  LOCAL_CFLAGS += $(ART_HOST_CFLAGS)
endif

LOCAL_SRC_FILES := $(SRC_FILES)
LOCAL_C_INCLUDES += art/runtime
LOCAL_MODULE:= libsigchain
LOCAL_SHARED_LIBRARIES += liblog libdl

include $(BUILD_SHARED_LIBRARY)

