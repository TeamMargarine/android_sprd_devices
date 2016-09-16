#
# Copyright (C) 2014 Spreadtrum Communication Inc.
#

# The uui and cmcc spec may share the configs as non-uui, XXX so DO NOT add
# additional settings here about board, only add the vendor specific configs.

# XXX I will REVERT the config which is duplicated!

# TODO Add the customized boot animation here.
#security support config
$(call inherit-product-if-exists, vendor/sprd/open-source/security_support.mk)

include device/sprd/scx35l_sp9630ea/scx35l_sp9630ea_dt_plus.mk

include vendor/sprd/UniverseUI/ThemeRes/universeui.mk

PRODUCT_PROPERTY_OVERRIDES += \
                        persist.sys.support.vt=false

# Build the SprdLauncher1
PRODUCT_PACKAGES += \
    SprdLauncher1

# Overrides
PRODUCT_NAME := scx35l_sp9630ea_dt_plus_UUI
PRODUCT_DEVICE := $(TARGET_BOARD)
PRODUCT_MODEL := sp9630ea
PRODUCT_BRAND := Spreadtrum
PRODUCT_MANUFACTURER := Spreadtrum

PRODUCT_LOCALES := zh_CN zh_TW en_US
