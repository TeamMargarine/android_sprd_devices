#
# Copyright (C) 2014 Spreadtrum Communication Inc.
#

# The uui and cmcc spec may share the configs as non-uui, XXX so DO NOT add
# additional settings here about board. Only add multi-language and oversea
# vendor settings here.

# XXX I will REVERT the config which is duplicated!

# TODO Add the customized boot animation here.

include device/sprd/scx35l_sp9630ea_3592m_3mod/scx35l_sp9630ea_3592m_3mod_dt.mk

include vendor/sprd/UniverseUI/ThemeRes/universeui.mk

PRODUCT_PROPERTY_OVERRIDES += \
                        persist.sys.support.vt=false

# Build the SprdLauncher1
PRODUCT_PACKAGES += \
    SprdLauncher1

# Overrides
PRODUCT_NAME := scx35l_sp9630ea_3592m_3mod_dt_base_UUI
PRODUCT_DEVICE := $(TARGET_BOARD)
PRODUCT_MODEL := sp9630ea_3592m_3mod
PRODUCT_BRAND := Spreadtrum
PRODUCT_MANUFACTURER := Spreadtrum

PRODUCT_LOCALES := zh_CN zh_TW en_US
