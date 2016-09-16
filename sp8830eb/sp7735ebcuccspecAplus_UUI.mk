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

TARGET_PLATFORM := sc8830
TARGET_BOARD := sp8830eb
BOARDDIR := device/sprd/$(TARGET_BOARD)

DEVICE_PACKAGE_OVERLAYS := $(BOARDDIR)/overlay
PRODUCT_PACKAGE_OVERLAYS := vendor/sprd/operator/cucc/specA/overlay

PRODUCT_AAPT_CONFIG := hdpi xhdpi

PRODUCT_PROPERTY_OVERRIDES := \
	keyguard.no_require_sim=true \
	ro.com.android.dataroaming=false \
	persist.msms.phone_count=2 \
	ro.msms.phone_count=2 \
	ro.digital.fm.support=0 \
	persist.msms.phone_default=0 \
	ro.modem.count=1 \
	ro.modem.w.enable=1 \
	ro.modem.w.dev=/dev/cpw \
	ro.modem.w.tty=/dev/stty_w \
	ro.modem.w.eth=seth_w \
	ro.modem.w.snd=2 \
	ro.modem.w.diag=/dev/slog_w \
	ro.modem.w.loop=/dev/spipe_w0 \
	ro.modem.w.nv=/dev/spipe_w1 \
	ro.modem.w.assert=/dev/spipe_w2 \
	ro.modem.w.vbc=/dev/spipe_w6 \
	ro.modem.w.id=0 \
	ro.modem.w.count=2 \
	ro.config.hw.cmmb_support=false \
	ro.config.hw.camera_support=false \
	ro.config.hw.search_support=false \
    persist.surpport.oplpnn=true \
    persist.surpport.cphsfirst=false

#PRODUCT_PROPERTY_OVERRIDES += universe_ui_support=true

ifeq ($(TARGET_BUILD_VARIANT),user)
  PRODUCT_PROPERTY_OVERRIDES += persist.sys.sprd.modemreset=1
else
  PRODUCT_PROPERTY_OVERRIDES += persist.sys.sprd.modemreset=0
endif

# Set default USB interface
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
	persist.sys.usb.config=mass_storage

PRODUCT_PACKAGES := \
    MsmsPhone \
    Settings \
    MsmsStk \
    Stk1 \
    sprdcudm \
    framework2


# prebuild files
PRODUCT_PACKAGES += \
	moffice_5.3.3_1033_cn00353_multidex_135897.apk \
	Weibo3.3.0_300_ZhanXun_0308.apk \
	20130308_iFlyIME_v3.0.1265_Spreadtrum_01430228.apk \
	Wostore.apk \
	UnicomClient.apk \
	116114.apk \
        DeviceRegister_debug.apk

# packages files
PRODUCT_PACKAGES += \
	
# add  system properties
PRODUCT_PROPERTY_OVERRIDES += \
	ro.operator=cucc \
	ro.operator.version=specA
	
PRODUCT_COPY_FILES := \
	$(BOARDDIR)/sprd-keypad.kl:system/usr/keylayout/sprd-keypad.kl \
	$(BOARDDIR)/sci-keypad.kl:system/usr/keylayout/sci-keypad.kl \
	$(BOARDDIR)/sec_touchscreen.idc:system/usr/idc/sec_touchscreen.idc \
	$(BOARDDIR)/focaltech_ts.idc:system/usr/idc/focaltech_ts.idc

$(call inherit-product, frameworks/native/build/phone-HD720-dalvik-heap.mk)

# include classified configs
#$(call inherit-product, vendor/sprd/operator/cucc/specA/res/apn/apn_res.mk)
$(call inherit-product, $(BOARDDIR)/base.mk)
$(call inherit-product, $(BOARDDIR)/proprietories.mk)
#$(call inherit-product, vendor/sprd/operator/cucc/specA/res/boot/boot_res.mk)
#$(call inherit-product, vendor/sprd/UniverseUI/universeui.mk)

# include standard configs
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)
$(call inherit-product, $(SRC_TARGET_DIR)/product/telephony.mk)

# Overrides
PRODUCT_NAME := sp7735ebcuccspecAplus_UUI
PRODUCT_DEVICE := $(TARGET_BOARD)
PRODUCT_MODEL := sp8830eb
PRODUCT_BRAND := Spreadtrum
PRODUCT_MANUFACTURER := Spreadtrum

PRODUCT_LOCALES := zh_CN zh_TW en_US
ifeq ($(MULTILANGUAGE_SUPPORT),true)
  PRODUCT_PACKAGES += $(MULTILANGUAGE_PRODUCT_PACKAGES)
  PRODUCT_LOCALES := zh_CN zh_TW en_US en_AU en_CA en_GB en_IE en_IN en_NZ en_SG en_ZA bn_BD in_ID id_ID ms_MY ar_EG ar_IL th_TH vi_VN es_US es_ES pt_PT pt_BR ru_RU hi_IN my_MM fr_BE fr_CA fr_CH fr_FR tl_PH ur_IN ur_PK fa_AF fa_IR tr_TR sw_KE sw_TZ ro_RO te_IN ta_IN ha_GH ha_NE ha_NG ug_CN ce_PH bo_CN bo_IN it_CH it_IT de_AT de_CH de_DE de_LI el_GR cs_CZ pa_IN gu_IN km_KH lo_LA nl_BE nl_NL pl_PL am_ET
endif
PRODUCT_PROPERTY_OVERRIDES += ro.product.locale.language=zh
PRODUCT_PROPERTY_OVERRIDES += ro.product.locale.region=CN
HAVE_WLAN_CU_FEATURE := true
