-include vendor/sprd/operator/cucc/specA/res/boot/boot_res_fwvga.mk

$(call inherit-product, vendor/sprd/operator/cucc/specA/res/apn/apn_res.mk)

include device/sprd/scx15_sp7715ea/scx15_sp7715eaplus.mk

PRODUCT_NAME := scx15_sp7715eacuccspecAplus_UUI

DEVICE_PACKAGE_OVERLAYS := $(PLATDIR)/overlay_full $(DEVICE_PACKAGE_OVERLAYS)

include vendor/sprd/operator/cucc/specA.mk
include vendor/sprd/UniverseUI/ThemeRes/universeui.mk

$(call inherit-product, vendor/sprd/partner/shark/bluetooth/device-shark-bt.mk)

PRODUCT_PROPERTY_OVERRIDES += \
        persist.sys.support.vt=false \
        persist.surpport.50ksearch=0

# SprdLauncher1
PRODUCT_PACKAGES += \
        SprdLauncher1

# SprdLauncher2
#PRODUCT_PACKAGES += \
#        SprdLauncher2

#security support config
$(call inherit-product-if-exists, vendor/sprd/open-source/security_support.mk)
