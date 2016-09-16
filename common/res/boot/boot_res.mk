ifeq ( $(BOOT_ANIMATION_ENABLE),true)
LOCAL_PATH:= device/sprd/common/res/boot

ifneq ($(filter sp8830ea , $(TARGET_BOARD)),)
PRODUCT_COPY_FILES += \
        $(LOCAL_PATH)/bootanimation_8830.zip:system/media/bootanimation.zip \
        $(LOCAL_PATH)/bootsound.mp3:system/media/bootsound.mp3 \
        $(LOCAL_PATH)/shutdownanimation_8830.zip:system/media/shutdownanimation.zip \
        $(LOCAL_PATH)/shutdownsound.mp3:system/media/shutdownsound.mp3
else
ifneq ($(filter sp8830% sp8825% sp6825%, $(TARGET_PRODUCT)),)
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/bootanimation_8825.zip:system/media/bootanimation.zip \
	$(LOCAL_PATH)/bootsound.mp3:system/media/bootsound.mp3 \
	$(LOCAL_PATH)/shutdownanimation_8825.zip:system/media/shutdownanimation.zip \
	$(LOCAL_PATH)/shutdownsound.mp3:system/media/shutdownsound.mp3
else
PRODUCT_COPY_FILES += \
	$(LOCAL_PATH)/bootanimation.zip:system/media/bootanimation.zip \
	$(LOCAL_PATH)/bootsound.mp3:system/media/bootsound.mp3 \
	$(LOCAL_PATH)/shutdownanimation.zip:system/media/shutdownanimation.zip \
	$(LOCAL_PATH)/shutdownsound.mp3:system/media/shutdownsound.mp3
endif
endif
endif
