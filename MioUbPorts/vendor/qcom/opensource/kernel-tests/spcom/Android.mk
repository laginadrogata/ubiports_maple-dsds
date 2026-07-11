# verify it isn't a simulator build
ifeq ($(call is-vendor-board-platform,QCOM),true)
ifeq ($(call is-board-platform-in-list,msm8998),true)

LOCAL_PATH := $(call my-dir)

# the dlkm
DLKM_DIR   := device/qcom/common/dlkm

include $(CLEAR_VARS)
LOCAL_MODULE      := spcom_ktest.ko
LOCAL_MODULE_TAGS := debug
include $(DLKM_DIR)/AndroidKernelModule.mk

# the test script
include $(CLEAR_VARS)
LOCAL_MODULE := spcom_ktest.sh
LOCAL_MODULE_CLASS := EXECUTABLES
LOCAL_SRC_FILES := spcom_ktest.sh
LOCAL_MODULE_TAGS := optional debug
LOCAL_MODULE_PATH := $(TARGET_OUT_DATA)/kernel-tests
include $(BUILD_PREBUILT)

endif
endif
