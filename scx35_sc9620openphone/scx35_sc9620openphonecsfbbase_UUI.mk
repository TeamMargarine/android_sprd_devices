# export original make file
LOCAL_ORIGINAL_PRODUCT_MAKEFILE := device/sprd/scx35_sc9620openphone/scx35_sc9620openphonecsfbbase.mk

include $(LOCAL_ORIGINAL_PRODUCT_MAKEFILE)
# Rename product name and we can lunch it
PRODUCT_NAME := scx35_sc9620openphonecsfbbase_UUI

PRODUCT_PROPERTY_OVERRIDES += \
        persist.sys.support.vt=false

#[[ for autotest
PRODUCT_PACKAGES += autotest
#]]

include vendor/sprd/UniverseUI/ThemeRes/universeui.mk
