LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := qrngtest.c
LOCAL_C_FLAGS := -lpthread
LOCAL_SHARED_LIBRARIES := libc
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/kernel-tests
LOCAL_MODULE = qrngtest
LOCAL_MODULE_TAGS := optional debug
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := qrngtest.sh
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := qrngtest.sh
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/kernel-tests
include $(BUILD_PREBUILT)
