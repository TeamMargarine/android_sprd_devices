-include vendor/sprd/operator/cucc/specA/res/boot/boot_res.mk

$(call inherit-product, vendor/sprd/operator/cucc/specA/res/apn/apn_res.mk)

# export original make file
LOCAL_ORIGINAL_PRODUCT_MAKEFILE := device/sprd/scx35_sp7730ec/scx35_sp7730ecplus.mk

include $(LOCAL_ORIGINAL_PRODUCT_MAKEFILE)
# Rename product name and we can lunch it
PRODUCT_NAME := scx35_sp7730eccuccspecBplus_UUI

DEVICE_PACKAGE_OVERLAYS := $(PLATDIR)/overlay_full $(DEVICE_PACKAGE_OVERLAYS)

include vendor/sprd/operator/cucc/specB.mk
include vendor/sprd/UniverseUI/ThemeRes/universeui.mk

$(call inherit-product, vendor/sprd/partner/shark/bluetooth/device-shark-bt.mk)

PRODUCT_PROPERTY_OVERRIDES += \
        persist.sys.support.vt=false \
        ro.homekey.physical=true \
        persist.surpport.50ksearch=0

# SprdLauncher1
PRODUCT_PACKAGES += \
        SprdLauncher1

# SprdLauncher2
#PRODUCT_PACKAGES += \
#        SprdLauncher2

#security support config
$(call inherit-product-if-exists, vendor/sprd/open-source/security_support.mk)
