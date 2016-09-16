#
# Copyright (C) 2014 Spreadtrum Communication Inc.
#

# The uui and cmcc spec may share the configs as non-uui, XXX so DO NOT add
# additional settings here about board, only add the vendor specific configs.

# XXX I will REVERT the config which is duplicated!

# TODO Add the customized boot animation here.
#security support config
$(call inherit-product-if-exists, vendor/sprd/open-source/security_support.mk)

include device/sprd/A2800-d/scx35l_sp9630ea_dt_plus.mk

include vendor/sprd/UniverseUI/ThemeRes/universeui.mk
include vendor/sprd/operator/cmcc/spec3.mk

PRODUCT_PROPERTY_OVERRIDES += \
                        persist.sys.support.vt=false

# Build the SprdLauncher1
PRODUCT_PACKAGES += \
    SprdLauncher1

# board-specific modules
 PRODUCT_PACKAGES += \
            Validator \
            se_nena_nenamark2_5 \
            VoiceCycle \
            libnena_jni.so
# Overrides
PRODUCT_NAME := A2800
PRODUCT_DEVICE := $(TARGET_BOARD)
PRODUCT_MODEL := sp9630ea
PRODUCT_BRAND := Spreadtrum
PRODUCT_MANUFACTURER := Spreadtrum

PRODUCT_LOCALES := zh_CN zh_TW en_US
