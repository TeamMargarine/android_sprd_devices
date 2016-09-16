#security support config
$(call inherit-product-if-exists, vendor/sprd/open-source/security_support.mk)
# export original make file
LOCAL_ORIGINAL_PRODUCT_MAKEFILE := device/sprd/scx35_sc9620openphone/scx35_sc9620openphonebase.mk

include $(LOCAL_ORIGINAL_PRODUCT_MAKEFILE)
# Rename product name and we can lunch it
PRODUCT_NAME := scx35_sc9620basecmccspec5_UUI

PRODUCT_PROPERTY_OVERRIDES += \
        persist.sys.support.vt=false

PRODUCT_PACKAGES += \
    SprdLauncher1

#CMCC Recommended AndManhua
PRODUCT_PACKAGES += \
    Manhua_CMCC

#[[ for autotest
PRODUCT_PACKAGES += autotest
#]]
$(warning ${PRODUCT_PACKAGES})
DEVICE_PACKAGE_OVERLAYS := $(PLATDIR)/overlay_full $(DEVICE_PACKAGE_OVERLAYS)

include vendor/sprd/UniverseUI/ThemeRes/universeui.mk
include vendor/sprd/operator/cmcc/spec5.mk

