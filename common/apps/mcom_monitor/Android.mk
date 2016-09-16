LOCAL_PATH:= $(call my-dir)
#mcom_monoitor , monitor mcom (such as:SDIO) communicate status
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	mcom_monitor.c

LOCAL_SHARED_LIBRARIES := \
    libutils 

LOCAL_MODULE := mcom_monitor
LOCAL_MODULE_TAGS := optional

include $(BUILD_EXECUTABLE)

