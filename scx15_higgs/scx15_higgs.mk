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

TARGET_PLATFORM := scx15
PLATDIR := device/sprd/$(TARGET_PLATFORM)

TARGET_BOARD := scx15_higgs
BOARDDIR := device/sprd/$(TARGET_BOARD)

# include general common configs
$(call inherit-product, $(PLATDIR)/device.mk)
$(call inherit-product, $(PLATDIR)/emmc/emmc_device.mk)
$(call inherit-product, $(PLATDIR)/proprietories.mk)

DEVICE_PACKAGE_OVERLAYS := $(BOARDDIR)/overlay $(PLATDIR)/overlay

PRODUCT_AAPT_CONFIG := hdpi

# Set default USB interface
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
	persist.sys.usb.config=mass_storage

PRODUCT_PROPERTY_OVERRIDES += \
	keyguard.no_require_sim=true \
	ro.com.android.dataroaming=false \
	ro.msms.phone_count=2 \
	persist.msms.phone_count=2 \
	persist.msmslt=0 \
	ro.modem.t.count=2 \
	persist.sys.modem.diag=,gser \
	sys.usb.gser.count=2

# board-specific modules
PRODUCT_PACKAGES += \
	sensors.$(TARGET_PLATFORM) \
	fm.$(TARGET_PLATFORM) \
	ValidationTools \
	EngineerMode

#	libmllite.so \
#	libmplmpu.so \
#	libinvensense_hal
#
# board-specific files
PRODUCT_COPY_FILES += \
	$(BOARDDIR)/init.board.rc:root/init.board.rc \
       $(BOARDDIR)/init.recovery.board.rc:root/init.recovery.board.rc \
	$(BOARDDIR)/audio_params/tiny_hw.xml:system/etc/tiny_hw.xml \
	$(BOARDDIR)/audio_params/codec_pga.xml:system/etc/codec_pga.xml \
	$(BOARDDIR)/audio_params/audio_hw.xml:system/etc/audio_hw.xml \
	$(BOARDDIR)/audio_params/audio_para:system/etc/audio_para \
	$(BOARDDIR)/audio_params/audio_policy.conf:system/etc/audio_policy.conf \
    $(BOARDDIR)/focaltech_ts.idc:system/usr/idc/focaltech_ts.idc\
    vendor/sprd/partner/marvell/L2000/gps.default.so:system/lib/hw/gps.default.so \
	vendor/sprd/partner/marvell/L2000/libagps_hal.so:system/lib/libagps_hal.so \
	vendor/sprd/partner/marvell/L2000/rom.bin:system/etc/rom.bin \
	vendor/sprd/partner/marvell/L2000/config/pxa_testcfg.ini:system/etc/pxa_testcfg.ini \
	vendor/sprd/partner/marvell/L2000/config/mrvl_agps_default.conf:system/etc/mrvl_agps_default.conf \
	vendor/sprd/partner/marvell/L2000/config/mrvl_gps_platform.conf:system/etc/mrvl_gps_platform.conf \
	vendor/sprd/partner/marvell/L2000/AGPS_CA.pem:system/etc/AGPS_CA.pem \
	vendor/sprd/partner/marvell/L2000/RXNdata/license.key:system/etc/license.key \
	vendor/sprd/partner/marvell/L2000/RXNdata/security.key:system/etc/security.key \
	vendor/sprd/partner/marvell/L2000/RXNdata/MSLConfig.txt:system/etc/MSLConfig.txt \
	vendor/sprd/partner/marvell/L2000/RXNdata/rxn_config_data:system/etc/rxn_config_data \
	frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
	frameworks/native/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
	frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
	frameworks/native/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml

$(call inherit-product, vendor/sprd/open-source/res/boot/boot_res_8830s.mk)
$(call inherit-product, frameworks/native/build/phone-hdpi-512-dalvik-heap.mk)
$(call inherit-product-if-exists, vendor/sprd/open-source/common_packages.mk)
$(call inherit-product-if-exists, vendor/sprd/open-source/base_special_packages.mk)
$(call inherit-product, hardware/broadcom/wlan/bcmdhd/firmware/bcm4330/device-bcm.mk)

# Overrides
PRODUCT_NAME := scx15_higgs
PRODUCT_DEVICE := $(TARGET_BOARD)
PRODUCT_MODEL := higgs
PRODUCT_BRAND := Spreadtrum
PRODUCT_MANUFACTURER := Spreadtrum

PRODUCT_LOCALES := zh_CN zh_TW en_US
