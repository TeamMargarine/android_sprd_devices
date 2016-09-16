#
# Copyright (C) 2007 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

TARGET_PLATFORM := spavd
TARGET_BOARD := spavd
BOARDDIR := device/sprd/$(TARGET_BOARD)

DEVICE_PACKAGE_OVERLAYS := $(BOARDDIR)/overlay

PRODUCT_AAPT_CONFIG := hdpi

PRODUCT_PROPERTY_OVERRIDES := \
	keyguard.no_require_sim=true \
	ro.com.android.dataroaming=false \
	persist.msms.phone_count=2 \
	ro.msms.phone_count=2 \
	persist.msms.phone_default=0 \
	ro.modem.count=1 \
	ro.modem.t.enable=1 \
	ro.modem.t.dev=/dev/cpt \
	ro.modem.t.tty=/dev/stty_td \
	ro.modem.t.eth=seth_td \
	ro.modem.t.snd=1 \
	ro.modem.t.diag=/dev/slog_td \
	ro.modem.t.loop=/dev/spipe_td0 \
	ro.modem.t.nv=/dev/spipe_td1 \
	ro.modem.t.assert=/dev/spipe_td2 \
	ro.modem.t.vbc=/dev/spipe_td6 \
	ro.modem.t.id=0 \
	ro.modem.t.count=2 \



# Set default USB interface
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
	persist.sys.usb.config=mass_storage

PRODUCT_PACKAGES := \
    MsmsPhone \
    Settings \
    MsmsStk \
    Stk1 \
    framework2


PRODUCT_PACKAGES += \

# audio libraries.
PRODUCT_PACKAGES += \
	audio.primary.goldfish \
	audio_policy.default \
	local_time.default

ENABLE_BLUETOOTH := true
PRODUCT_PACKAGE_OVERLAYS := $(BOARDDIR)/sdk_overlay

#PRODUCT_CHARACTERISTICS := tablet
#include frameworks/native/build/tablet-dalvik-heap.mk
#PRODUCT_COPY_FILES += \
#    frameworks/native/data/etc/tablet_core_hardware.xml:system/etc/permissions/tablet_core_hardware.xml

#$(call inherit-product, frameworks/base/build/phone-hdpi-dalvik-heap.mk)
$(call inherit-product, frameworks/native/build/tablet-dalvik-heap.mk)

# include classified configs
$(call inherit-product, $(BOARDDIR)/base.mk)
$(call inherit-product, $(BOARDDIR)/proprietories.mk)
# avd start loading animation resource. default is "android"
#$(call inherit-product, device/sprd/common/res/boot/boot_res.mk)

# include standard configs
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/sdk.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/telephony.mk)

# Overrides
PRODUCT_NAME := spavdphone
PRODUCT_DEVICE := $(TARGET_BOARD)
PRODUCT_MODEL := sprd-avd
PRODUCT_BRAND := Spreadtrum
PRODUCT_MANUFACTURER := Spreadtrum

PRODUCT_LOCALES := zh_CN zh_TW en_US
ifeq ($(MULTILANGUAGE_SUPPORT),true)
  PRODUCT_PACKAGES += $(MULTILANGUAGE_PRODUCT_PACKAGES)
  PRODUCT_LOCALES := zh_CN zh_TW en_US en_AU en_CA en_GB en_IE en_IN en_NZ en_SG en_ZA bn_BD in_ID id_ID ms_MY ar_EG ar_IL th_TH vi_VN es_US es_ES pt_PT pt_BR ru_RU hi_IN my_MM fr_BE fr_CA fr_CH fr_FR tl_PH ur_IN ur_PK fa_AF fa_IR tr_TR sw_KE sw_TZ ro_RO te_IN ta_IN ha_GH ha_NE ha_NG ug_CN ce_PH bo_CN bo_IN it_CH it_IT de_AT de_CH de_DE de_LI el_GR cs_CZ pa_IN gu_IN km_KH lo_LA nl_BE nl_NL pl_PL am_ET
endif
PRODUCT_PROPERTY_OVERRIDES += ro.product.locale.language=zh
PRODUCT_PROPERTY_OVERRIDES += ro.product.locale.region=CN
