-include vendor/sprd/operator/cucc/specA/res/boot/boot_res.mk

# export original make file
LOCAL_ORIGINAL_PRODUCT_MAKEFILE := device/sprd/scx35_sp7730ectrisim/scx35_sp7730ecplus.mk

include $(LOCAL_ORIGINAL_PRODUCT_MAKEFILE)
# Rename product name and we can lunch it
PRODUCT_NAME := scx35_sp7730ectrisimcuccspecAplus_UUI

DEVICE_PACKAGE_OVERLAYS := $(PLATDIR)/overlay_full $(DEVICE_PACKAGE_OVERLAYS)

include vendor/sprd/operator/cucc/specB.mk
include vendor/sprd/UniverseUI/ThemeRes/universeui.mk

$(call inherit-product, vendor/sprd/partner/shark/bluetooth/device-shark-bt.mk) 

PRODUCT_PACKAGES += \
    SprdLauncher1

PRODUCT_PACKAGES += \
    SprdLauncher2
