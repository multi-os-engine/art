
LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := oatmap.proto
LOCAL_MODULE := libartoatmapproto
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
proto_header_dir := $(call local-generated-sources-dir)/proto/$(LOCAL_PATH)
LOCAL_EXPORT_C_INCLUDE_DIRS += $(proto_header_dir)
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := oatmap.proto
LOCAL_MODULE := libartoatmapproto
LOCAL_MODULE_CLASS := SHARED_LIBRARIES
proto_header_dir := $(call local-generated-sources-dir)/proto/$(LOCAL_PATH)
LOCAL_EXPORT_C_INCLUDE_DIRS += $(proto_header_dir)
include $(BUILD_HOST_SHARED_LIBRARY)
