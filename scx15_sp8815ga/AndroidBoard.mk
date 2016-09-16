LOCAL_PATH := $(call my-dir)

$(call add-radio-file,modem_bins/tdmodem.bin)
$(call add-radio-file,modem_bins/tdnvitem.bin)
$(call add-radio-file,modem_bins/tddsp.bin)
$(call add-radio-file,modem_bins/wcnnvitem.bin)
$(call add-radio-file,modem_bins/wcnmodem.bin)

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
