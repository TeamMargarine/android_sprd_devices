LOCAL_PATH := $(call my-dir)

$(call add-radio-file,modem_bins/tlmodem.bin)
$(call add-radio-file,modem_bins/tlnvitem.bin)
$(call add-radio-file,modem_bins/tlldsp.bin)
$(call add-radio-file,modem_bins/tltgdsp.bin)
$(call add-radio-file,modem_bins/pmsys.bin)

# Compile U-Boot
ifneq ($(strip $(TARGET_NO_BOOTLOADER)),true)

INSTALLED_UBOOT_TARGET := $(PRODUCT_OUT)/u-boot.bin
INSTALLED_CHIPRAM_TARGET := $(PRODUCT_OUT)/u-boot-spl-16k.bin
-include u-boot/AndroidUBoot.mk
-include chipram/AndroidChipram.mk

INSTALLED_RADIOIMAGE_TARGET += $(PRODUCT_OUT)/u-boot.bin
INSTALLED_RADIOIMAGE_TARGET += $(PRODUCT_OUT)/u-boot-spl-16k.bin

endif # End of U-Boot

# Compile Linux Kernel
ifneq ($(strip $(TARGET_NO_KERNEL)),true)

-include kernel/AndroidKernel.mk

file := $(INSTALLED_KERNEL_TARGET)
ALL_PREBUILT += $(file)
$(file) : $(TARGET_PREBUILT_KERNEL) | $(ACP)
	$(transform-prebuilt-to-target)

endif # End of Kernel

ifeq ($(strip $(BOARD_KERNEL_SEPARATED_DT)),true)
include vendor/sprd/open-source/tools/dt/generate_dt_image.mk
endif

# INSTALLED_DOWNLOAD_TARGET := sharkl_tddcsfb_3592CF_builddir
# include device/sprd/common/AndroidDownload.mk