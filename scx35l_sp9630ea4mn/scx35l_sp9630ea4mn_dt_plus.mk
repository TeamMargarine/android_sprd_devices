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

TARGET_PLATFORM := sc9630
PLATDIR := device/sprd/scx35

TARGET_BOARD := scx35l_sp9630ea4mn
BOARDDIR := device/sprd/$(TARGET_BOARD)

BOARD_KERNEL_PAGESIZE := 2048
BOARD_KERNEL_SEPARATED_DT := true

# include general common configs
$(call inherit-product, vendor/sprd/open-source/res/productinfo/connectivity_9630ea3mn.mk)
$(call inherit-product, $(PLATDIR)/device.mk)
$(call inherit-product, $(PLATDIR)/emmc/emmc_device.mk)
$(call inherit-product, $(PLATDIR)/proprietories.mk)

DEVICE_PACKAGE_OVERLAYS := $(BOARDDIR)/overlay $(PLATDIR)/overlay
# Remove video wallpaper feature
PRODUCT_VIDEO_WALLPAPERS := none
BUILD_FPGA := false
PRODUCT_AAPT_CONFIG := hdpi

PRODUCT_WIFI_DEVICE := sprd

# Set default USB interface
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
	persist.sys.usb.config=mass_storage

PRODUCT_PROPERTY_OVERRIDES += \
	keyguard.no_require_sim=true \
	ro.com.android.dataroaming=false \
	ro.msms.phone_count=2 \
        ro.modem.lf.count=2 \
	persist.msms.phone_count=2 \
	persist.msms.phone_default=0 \
        persist.sys.modem.diag=,gser \
        sys.usb.gser.count=8 \
        ro.modem.external.enable=0 \
        persist.sys.support.vt=false \
        persist.modem.lf.cs=0 \
        persist.modem.lf.ps=1 \
        persist.modem.lf.rsim=1 \
        persist.radio.ssda.mode=fdd-csfb \
        persist.radio.ssda.testmode=6 \
        persist.radio.ssda.testmode1=10 \
        persist.support.oplpnn=true \
        persist.support.cphsfirst=false \
        lmk.autocalc=false

# board-specific modules
PRODUCT_PACKAGES += \
        sensors.sc8830 \
        fm.$(TARGET_PLATFORM) \
        ValidationTools \
        download

#[[ for autotest
        PRODUCT_PACKAGES += autotest
#]]

# board-specific files
PRODUCT_COPY_FILES += \
	$(BOARDDIR)/sprd-gpio-keys.kl:system/usr/keylayout/sprd-gpio-keys.kl \
	$(BOARDDIR)/sprd-eic-keys.kl:system/usr/keylayout/sprd-eic-keys.kl \
	$(BOARDDIR)/init.board.rc:root/init.board.rc \
	$(BOARDDIR)/audio_params/tiny_hw.xml:system/etc/tiny_hw.xml \
	$(BOARDDIR)/audio_params/codec_pga.xml:system/etc/codec_pga.xml \
	$(BOARDDIR)/audio_params/audio_hw.xml:system/etc/audio_hw.xml \
	$(BOARDDIR)/audio_params/audio_para:system/etc/audio_para \
	$(BOARDDIR)/audio_params/audio_policy.conf:system/etc/audio_policy.conf \
	$(BOARDDIR)/focaltech_ts.idc:system/usr/idc/focaltech_ts.idc \
	$(BOARDDIR)/msg2138_ts.idc:system/usr/idc/msg2138_ts.idc \
	frameworks/native/data/etc/android.hardware.camera.flash-noautofocus.xml:system/etc/permissions/android.hardware.camera.flash-noautofocus.xml \
	frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
	frameworks/native/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml \
	frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
	frameworks/native/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
	frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml

$(call inherit-product, vendor/sprd/operator/cucc/specA/res/boot/boot_res_fwvga_4g.mk)
$(call inherit-product, frameworks/native/build/phone-hdpi-512-dalvik-heap.mk)

$(call inherit-product-if-exists, vendor/sprd/open-source/common_packages.mk)
$(call inherit-product-if-exists, vendor/sprd/open-source/plus_special_packages.mk)
$(call inherit-product, vendor/sprd/partner/shark/bluetooth/device-shark-bt.mk)
$(call inherit-product, vendor/sprd/gps/CellGuide_2351/device-sprd-gps.mk)
ifeq ($(PRODUCT_WIFI_DEVICE),bcm)
$(call inherit-product, hardware/broadcom/wlan/bcmdhd/firmware/bcm4343s/device-bcm.mk)
endif

# Overrides
PRODUCT_NAME := scx35l_sp9630ea4mn_dt_plus
PRODUCT_DEVICE := $(TARGET_BOARD)
PRODUCT_MODEL := SP9830A
PRODUCT_BRAND := Spreadtrum
PRODUCT_MANUFACTURER := Spreadtrum

PRODUCT_LOCALES := zh_CN zh_TW en_US
