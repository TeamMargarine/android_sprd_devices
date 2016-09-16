#
# Copyright (C) 2014 Spreadtrum Communication Inc.
#

# The uui and cmcc spec may share the configs as non-uui, XXX so DO NOT add
# additional settings here about board, only add the vendor specific configs.

# XXX I will REVERT the config which is duplicated!
$(call inherit-product, vendor/sprd/operator/cucc/specA/res/apn/apn_res.mk)
# TODO Add the customized boot animation here.
#security support config
$(call inherit-product-if-exists, vendor/sprd/open-source/security_support.mk)

include device/sprd/scx35l_sp9630ea4mn/scx35l_sp9630ea4mn_dt_plus.mk

include vendor/sprd/UniverseUI/ThemeRes/universeui.mk
include vendor/sprd/operator/cucc/specB.mk

PRODUCT_PROPERTY_OVERRIDES += \
                        persist.sys.support.vt=false

BOARD_KERNEL_SEPARATED_ARM_CLK_1350M := true

# Build the SprdLauncher1
PRODUCT_PACKAGES += \
    SprdLauncher1

# Overrides
PRODUCT_NAME := scx35l_sp9630ea4mn_dt_plus_cuccspec3_UUI_1350MHZ
PRODUCT_DEVICE := $(TARGET_BOARD)
PRODUCT_MODEL := SP9830A
PRODUCT_BRAND := Spreadtrum
PRODUCT_MANUFACTURER := Spreadtrum

PRODUCT_LOCALES := zh_CN zh_TW en_US
