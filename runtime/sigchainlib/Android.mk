LOCAL_PATH:= $(call my-dir)

include art/build/Android.common.mk

SRC_FILES := sigchain.cc

include $(CLEAR_VARS)

ifeq ($(WITH_HOST_DALVIK),false)
  LOCAL_IS_HOST_MODULE := true
endif

include art/build/Android.libcxx.mk
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
LOCAL_CFLAGS += -Wno-implicit-exception-spec-mismatch
LOCAL_MODULE:= libsigchain
LOCAL_SHARED_LIBRARIES += liblog

include $(BUILD_SHARED_LIBRARY)

