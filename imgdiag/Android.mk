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

LOCAL_PATH := $(call my-dir)

include art/build/Android.executable.mk

IMGDIAG_SRC_FILES := \
	imgdiag.cc

# TODO: Remove this when the framework (installd) supports pushing the
# right instruction-set parameter for the primary architecture.
ifneq ($(filter ro.zygote=zygote64,$(PRODUCT_DEFAULT_PROPERTY_OVERRIDES)),)
  imgdiag_target_arch := 64
else
  imgdiag_target_arch := 32
endif

ifeq ($(HOST_PREFER_32_BIT),true)
  # We need to explicitly restrict the host arch to 32-bit only, as
  # giving 'both' would make build-art-executable generate a build
  # rule for a 64-bit imgdiag executable too.
  imgdiag_host_arch := 32
else
  imgdiag_host_arch := both
endif

# Note that this tool needs to be built for both 32-bit and 64-bit since it requires
# that the image it's analyzing be the same ISA as the runtime ISA.

ifeq ($(ART_BUILD_TARGET_NDEBUG),true)
  $(eval $(call build-art-executable,imgdiag,$(IMGDIAG_SRC_FILES),libart-compiler libbacktrace libcutils,art/compiler,target,ndebug,$(imgdiag_target_arch)))
endif

ifeq ($(ART_BUILD_TARGET_DEBUG),true)
  $(eval $(call build-art-executable,imgdiag,$(IMGDIAG_SRC_FILES),libartd-compiler libbacktrace libcutils,art/compiler,target,debug,$(imgdiag_target_arch)))
endif

ifeq ($(ART_BUILD_HOST_NDEBUG),true)
  $(eval $(call build-art-executable,imgdiag,$(IMGDIAG_SRC_FILES),libart-compiler libziparchive-host libbacktrace,art/compiler,host,ndebug,$(imgdiag_host_arch)))
endif

ifeq ($(ART_BUILD_HOST_DEBUG),true)
  $(eval $(call build-art-executable,imgdiag,$(IMGDIAG_SRC_FILES),libartd-compiler libziparchive-host libbacktrace,art/compiler,host,debug,$(imgdiag_host_arch)))
endif

# Clear locals now they've served their purpose.
imgdiag_target_arch :=
imgdiag_host_arch :=
