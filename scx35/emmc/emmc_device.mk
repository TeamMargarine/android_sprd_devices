ifeq ($(strip $(BOARD_KERNEL_SEPARATED_DT)),true)
PRODUCT_COPY_FILES += \
	$(PLATDIR)/emmc/fstab_dt.sc8830:root/fstab.sc8830
else
PRODUCT_COPY_FILES += \
	$(PLATDIR)/emmc/fstab.sc8830:root/fstab.sc8830
endif

PRODUCT_PROPERTY_OVERRIDES += \
	ro.storage.flash_type=2

ifndef STORAGE_INTERNAL
  STORAGE_INTERNAL := physical
endif
ifndef STORAGE_PRIMARY
  STORAGE_PRIMARY := external
endif

-include $(PLATDIR)/storage_device.mk

