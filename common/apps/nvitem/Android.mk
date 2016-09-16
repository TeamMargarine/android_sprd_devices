ifneq ($(TARGET_SIMULATOR),true)
LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE_PATH := $(TARGET_ROOT_OUT_BIN)
LOCAL_UNSTRIPPED_PATH := $(TARGET_ROOT_OUT_BIN_UNSTRIPPED)

LOCAL_C_INCLUDES    +=  $(LOCAL_PATH) \
			$(LOCAL_PATH)/$(BOARD_PRODUCT_NAME)/ \
			$(LOCAL_PATH)/common \
			device/sprd/common/apps/engineeringmodel/engcs
LOCAL_SRC_FILES:= \
	nvitem_buf.c \
	nvitem_fs.c \
	nvitem_os.c \
	nvitem_packet.c \
	nvitem_server.c \
	nvitem_sync.c	

ifeq ($(strip $(TARGET_BOARD_PLATFORM)),sc8830)
LOCAL_SRC_FILES += nvitem_channe_spipe.c
else
LOCAL_SRC_FILES += nvitem_channel.c
endif

LOCAL_SHARED_LIBRARIES := \
    libhardware_legacy \
    libc \
    libutils \
    libengclient \
    liblog

#ifeq ($(strip $(TARGET_USERIMAGES_USE_EXT4)),true)
#LOCAL_CFLAGS := -DCONFIG_EMMC
#endif

LOCAL_MODULE := nvitemd
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
endif
