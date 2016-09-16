PRODUCT_THEME_PACKAGES := SimpleStyle HelloColor
PRODUCT_THEME_FLAGS := shrink

# export original make file
LOCAL_ORIGINAL_PRODUCT_MAKEFILE := device/sprd/scx35_sp7731gea/scx35_sp7731geaplus_dt.mk

include $(LOCAL_ORIGINAL_PRODUCT_MAKEFILE)
# Rename product name and we can lunch it
PRODUCT_NAME := scx35_sp7731geaplus_UUI_dt

PRODUCT_PROPERTY_OVERRIDES += \
        persist.sys.support.vt=false \
        persist.surpport.50ksearch=0

DEVICE_PACKAGE_OVERLAYS := $(PLATDIR)/overlay_full $(DEVICE_PACKAGE_OVERLAYS)


include vendor/sprd/UniverseUI/ThemeRes/universeui.mk
$(call inherit-product, vendor/sprd/partner/shark/bluetooth/device-shark-bt.mk)
ifeq ($(strip $(GMS_SUPPORT)), true)
$(call inherit-product-if-exists, vendor/sprd/partner/google/products/gms.mk)
endif

# Change to launcher3 and disable ThemeSettings ->
# SprdLauncher2
#PRODUCT_PACKAGES += \
#        SprdLauncher2
PRODUCT_PACKAGES := $(filter-out ThemeSettings,$(PRODUCT_PACKAGES))
#<-

#[[ for autotest
PRODUCT_PACKAGES += autotest
#]]

