LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_SHARED_LIBRARIES  := libcutils

LOCAL_SRC_FILES     := mfserial.c

LOCAL_MODULE := mfserial
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
