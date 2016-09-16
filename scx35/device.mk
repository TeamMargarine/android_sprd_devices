
# include aosp base configs
$(call inherit-product, $(SRC_TARGET_DIR)/product/full_base.mk)

# sprd telephony
PRODUCT_PACKAGES += \
	Dialer \
	Mms


# graphics modules
PRODUCT_PACKAGES += \
	libGLES_mali.so \
	libboost.so \
	mali.ko

# video modules
PRODUCT_PACKAGES += \
	libstagefright_sprd_soft_mpeg4dec \
	libstagefright_sprd_soft_h264dec \
	libstagefright_sprd_mpeg4dec \
	libstagefright_sprd_mpeg4enc \
	libstagefright_sprd_h264dec \
	libstagefright_sprd_h264enc \
	libstagefright_sprd_vpxdec \
	libstagefright_soft_mjpgdec \
	libstagefright_soft_imaadpcmdec \
	libstagefright_sprd_aacdec \
	libstagefright_sprd_mp3dec \
	libstagefright_sprd_apedec

# default audio
PRODUCT_PACKAGES += \
	audio.a2dp.default \
	audio.usb.default \
	audio.r_submix.default \
	libaudio-resampler

# sprd HAL modules
PRODUCT_PACKAGES += \
	audio.primary.sc8830 \
	audio_policy.sc8830 \
	gralloc.sc8830 \
	camera.sc8830 \
	camera2.sc8830 \
	lights.sc8830 \
	hwcomposer.sc8830 \
	sprd_gsp.sc8830
#	sensors.sc8830

# misc modules
PRODUCT_PACKAGES += \
	sqlite3 \
	charge \
	poweroff_alarm \
	mplayer \
	e2fsck \
	tinymix \
	tinycap \
	tinyplay \
	factorytest \
	audio_vbc_eq \
	calibration_init \
	modemd \
	engpc \
	dns6 \
	radvd \
	dhcp6s \
	modem_control\
	libengappjni \
	nvitemd \
	batterysrv \
	refnotify \
	wcnd \
	libsprdstreamrecoder \
	libvtmanager  \
	zram.sh \
	bdt \
	blktrace \
	blkparse \
	p2p_supplicant_marlin_inc.conf \
	iperf \
	wpa_cli

# general configs
PRODUCT_COPY_FILES += \
	$(PLATDIR)/init.sc8830.rc:root/init.sc8830.rc \
	$(PLATDIR)/init.sc8830.usb.rc:root/init.sc8830.usb.rc \
	$(PLATDIR)/ueventd.sc8830.rc:root/ueventd.sc8830.rc \
	$(PLATDIR)/headset-keyboard.kl:system/usr/keylayout/headset-keyboard.kl \
	$(PLATDIR)/sci-keypad.kl:system/usr/keylayout/sci-keypad.kl \
	$(PLATDIR)/gpio-keys.kl:system/usr/keylayout/gpio-keys.kl \
	$(PLATDIR)/media_codecs.xml:system/etc/media_codecs.xml \
	$(PLATDIR)/media_profiles.xml:system/etc/media_profiles.xml \
	$(PLATDIR)/engtest_sample.wav:system/media/engtest_sample.wav \
        vendor/sprd/open-source/res/spn/spn-conf.xml:system/etc/spn-conf.xml \
	vendor/sprd/open-source/res/productinfo/productinfo.bin:prodnv/productinfo.bin \
	vendor/sprd/open-source/res/CDROM/adb.iso:system/etc/adb.iso \
	vendor/sprd/open-source/libs/mali/egl.cfg:system/lib/egl/egl.cfg \
	vendor/sprd/open-source/apps/scripts/ext_data.sh:system/bin/ext_data.sh \
	vendor/sprd/open-source/apps/scripts/ext_kill.sh:system/bin/ext_kill.sh \
	vendor/sprd/open-source/apps/scripts/inputfreq.sh:system/bin/inputfreq.sh \
	vendor/sprd/open-source/apps/scripts/recoveryfreq.sh:system/bin/recoveryfreq.sh \
	vendor/sprd/open-source/apps/scripts/bih_config.sh:system/bin/bih_config.sh \
	frameworks/native/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml \
	frameworks/native/data/etc/android.hardware.bluetooth.xml:system/etc/permissions/android.hardware.bluetooth.xml \
	frameworks/native/data/etc/android.hardware.bluetooth_le.xml:system/etc/permissions/android.hardware.bluetooth_le.xml \
	frameworks/native/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
	frameworks/native/data/etc/android.hardware.touchscreen.multitouch.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.xml \
	frameworks/native/data/etc/android.hardware.touchscreen.xml:system/etc/permissions/android.hardware.touchscreen.xml \
	frameworks/native/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
	frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
	frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
	frameworks/native/data/etc/android.hardware.wifi.direct.xml:system/etc/permissions/android.hardware.wifi.direct.xml


ifeq ($(strip GMS_SUPPORT),true)
ifneq ($(strip $(wildcard frameworks/native/data/etc/gms.android.hardware.location.gps.xml)),)

PRODUCT_COPY_FILES += \
	frameworks/native/data/etc/gms.android.hardware.location.gps.xml:system/etc/permissions/gms.android.hardware.location.gps.xml

endif # File exist
endif # GMS_SUPPORT

APN_VERSION := $(shell cat frameworks/base/core/res/res/xml/apns.xml|grep "<apns version"|cut -d \" -f 2)
PRODUCT_COPY_FILES += vendor/sprd/operator/operator_res/apn/apns-conf_$(APN_VERSION).xml:system/etc/apns-conf.xml

ifeq ($(strip $(USE_SPRD_WCN)),true)
PRODUCT_PROPERTY_OVERRIDES += \
	ro.modem.wcn.enable=1 \
	ro.modem.wcn.dev=/dev/cpwcn \
	ro.modem.wcn.tty=/deiv/stty_wcn \
	ro.modem.wcn.diag=/dev/slog_wcn \
	ro.modem.wcn.assert=/dev/spipe_wcn2 \
	ro.modem.wcn.id=1 \
	ro.modem.wcn.count=1 \
	camera.disable_zsl_mode=1 \
	ro.digital.fm.support=1
endif

ifeq ($(TARGET_BUILD_VARIANT),user)

PRODUCT_PROPERTY_OVERRIDES += \
	persist.sys.sprd.modemreset=1 \
	ro.adb.secure=1 \
	persist.sys.sprd.wcnreset=1 \
        persist.sys.engpc.disable=1

else
PRODUCT_PROPERTY_OVERRIDES += \
	persist.sys.sprd.modemreset=0 \
	ro.adb.secure=0 \
	persist.sys.sprd.wcnreset=0 \
        persist.sys.engpc.disable=0

endif # TARGET_BUILD_VARIANT == user

# add GPS engineer mode apk
PRODUCT_PACKAGES += \
        SGPS
