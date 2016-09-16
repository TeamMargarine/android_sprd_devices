ifeq ($(strip $(BOARD_KERNEL_SEPARATED_DT)),true)
PRODUCT_COPY_FILES += \
	$(PLATDIR)/nand/fstab_dt.scx15:root/fstab.scx15
else
PRODUCT_COPY_FILES += \
	$(PLATDIR)/nand/fstab.scx15:root/fstab.scx15
endif
# SPRD:add for mount cache to sdcard @{
PRODUCT_PACKAGES += \
    mke2fs \
    mkcached.sh \
	busybox
# @}

PRODUCT_PROPERTY_OVERRIDES += \
	ro.storage.flash_type=1

ifndef STORAGE_INTERNAL
  STORAGE_INTERNAL := emulated
endif
ifndef STORAGE_PRIMARY
  STORAGE_PRIMARY := external
endif

-include $(PLATDIR)/storage_device.mk
