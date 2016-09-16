LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_SRC_FILES:= \
	nvitem.c \
	crc32b.c

LOCAL_SHARED_LIBRARIES := \
    libhardware_legacy \
    libc \
    libutils 

ifeq ($(strip $(TARGET_USERIMAGES_USE_EXT4)),true)
LOCAL_CFLAGS := -DCONFIG_EMMC
endif

LOCAL_MODULE := nvm_daemon
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
