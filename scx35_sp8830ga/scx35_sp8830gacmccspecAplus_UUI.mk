-include vendor/sprd/operator/cmcc/specA/res/boot/boot_res_qhd.mk

# export original make file
LOCAL_ORIGINAL_PRODUCT_MAKEFILE := device/sprd/scx35_sp8830ga/scx35_sp8830gaplus.mk

PRODUCT_THEME_PACKAGES := SimpleStyle HelloColor
PRODUCT_THEME_FLAGS := shrink

include $(LOCAL_ORIGINAL_PRODUCT_MAKEFILE)
# Rename product name and we can lunch it
PRODUCT_NAME := scx35_sp8830gacmccspecAplus_UUI

PRODUCT_PACKAGES += \
    SprdLauncher1

PRODUCT_PROPERTY_OVERRIDES += \
        persist.sys.support.vt=false \
        persist.surpport.50ksearch=0

DEVICE_PACKAGE_OVERLAYS := $(PLATDIR)/overlay_full $(DEVICE_PACKAGE_OVERLAYS)

# That means the cmcc preloadapp will not be built into current products
TARGET_DISABLE_VENDOR_PRELOADAPP := true

include vendor/sprd/operator/cmcc/specA.mk
include vendor/sprd/UniverseUI/ThemeRes/universeui.mk

$(call inherit-product, vendor/sprd/partner/shark/bluetooth/device-shark-bt.mk)

#security support config
$(call inherit-product-if-exists, vendor/sprd/open-source/security_support.mk)

