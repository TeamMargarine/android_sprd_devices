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
PLATDIR := device/sprd/scx35

TARGET_BOARD := scx35_sc9620openphone
BOARDDIR := device/sprd/$(TARGET_BOARD)

# call connectivity_configure_9620.mk before calling device.mk
$(call inherit-product, vendor/sprd/open-source/res/productinfo/connectivity_configure_9620.mk)

# include general common configs
$(call inherit-product, $(PLATDIR)/device.mk)
$(call inherit-product, $(PLATDIR)/emmc/emmc_device.mk)
$(call inherit-product, $(PLATDIR)/proprietories.mk)

DEVICE_PACKAGE_OVERLAYS := $(BOARDDIR)/overlay $(PLATDIR)/overlay

PRODUCT_AAPT_CONFIG := hdpi xhdpi

# Set default USB interface
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
	persist.sys.usb.config=mass_storage

PRODUCT_PROPERTY_OVERRIDES += \
        persist.sys.modem.diag=,gser \
        sys.usb.gser.count=8 \
        keyguard.no_require_sim=true \
        ro.com.android.dataroaming=false \
        persist.msms.phone_count=1 \
        persist.msms.phone_default=0 \
        persist.radio.ssda.mode=svlte \
        persist.radio.ssda.testmode=0 \
        ro.modem.external.enable=1 \
        persist.modem.l.nvp=l_ \
	persist.modem.t.enable=1 \
        persist.modem.t.cs=1 \
        persist.modem.t.ps=1 \
        persist.modem.t.rsim=0 \
        persist.modem.l.enable=1 \
        persist.modem.l.cs=0 \
        persist.modem.l.ps=1 \
        persist.modem.l.rsim=1

# board-specific modules
PRODUCT_PACKAGES += \
        sensors.$(TARGET_PLATFORM) \
        fm.$(TARGET_PLATFORM) \
        ValidationTools \
        libmllite.so \
        libmplmpu.so \
        libinvensense_hal

#[[ for autotest
PRODUCT_PACKAGES += autotest
#]]

# board-specific files
PRODUCT_COPY_FILES += \
	$(BOARDDIR)/init.board.rc:root/init.board.rc \
       $(BOARDDIR)/init.recovery.board.rc:root/init.recovery.board.rc \
	$(BOARDDIR)/audio_params/tiny_hw_lineincall.xml:system/etc/tiny_hw_lineincall.xml \
	$(BOARDDIR)/audio_params/tiny_hw.xml:system/etc/tiny_hw.xml \
	$(BOARDDIR)/audio_params/codec_pga.xml:system/etc/codec_pga.xml \
	$(BOARDDIR)/audio_params/audio_hw.xml:system/etc/audio_hw.xml \
	$(BOARDDIR)/audio_params/audio_para:system/etc/audio_para \
	$(BOARDDIR)/audio_params/audio_policy.conf:system/etc/audio_policy.conf \
	$(BOARDDIR)/focaltech_ts.idc:system/usr/idc/focaltech_ts.idc \
	$(BOARDDIR)/imei_config/imei1.txt:prodnv/imei1.txt \
	$(BOARDDIR)/imei_config/imei2.txt:prodnv/imei2.txt \
	$(BOARDDIR)/imei_config/imei3.txt:prodnv/imei3.txt \
	$(BOARDDIR)/imei_config/imei4.txt:prodnv/imei4.txt \
	frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
	frameworks/native/data/etc/android.hardware.camera.autofocus.xml:system/etc/permissions/android.hardware.camera.autofocus.xml \
	frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
	frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
	frameworks/native/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
	frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
	frameworks/native/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml \
	frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:system/etc/permissions/android.hardware.sensor.gyroscope.xml

$(call inherit-product, vendor/sprd/open-source/res/boot/boot_res_9620.mk)
$(call inherit-product, frameworks/native/build/phone-xhdpi-512-dalvik-heap.mk)

$(call inherit-product-if-exists, vendor/sprd/open-source/common_packages.mk)
$(call inherit-product-if-exists, vendor/sprd/open-source/base_special_packages.mk)
$(call inherit-product, vendor/sprd/partner/shark/bluetooth/device-shark-bt.mk)
$(call inherit-product, vendor/sprd/gps/CellGuide_2351/device-sprd-gps.mk)

# Overrides
PRODUCT_NAME := scx35_sc9620openphonebase
PRODUCT_DEVICE := $(TARGET_BOARD)
PRODUCT_MODEL := sc9620openphone
PRODUCT_BRAND := Spreadtrum
PRODUCT_MANUFACTURER := Spreadtrum

PRODUCT_LOCALES := zh_CN zh_TW en_US
