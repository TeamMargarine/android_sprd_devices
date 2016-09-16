MALI := libUMP libEGL_mali.so libGLESv1_CM_mali.so libGLESv2_mali.so libMali.so ump.ko mali.ko
INVENSENSE := libmllite.so libmplmpu.so libinvensense_hal

SPRD_FM_APP := FMPlayer

BRCM_FM := \
    fm.$(TARGET_PLATFORM) \
    FmDaemon \
    FmTest

PRODUCT_PROPERTY_OVERRIDES :=

PRODUCT_PACKAGES := \
	DeskClock \
	Bluetooth \
	Calculator \
	Calendar \
	CertInstaller \
	DrmProvider \
	Email \
	Exchange2 \
	Gallery2 \
	InputDevices \
	LatinIME \
	Music \
	MusicFX \
	Provision \
	QuickSearchBox \
	SystemUI \
	CalendarProvider \
	bluetooth-health \
	hciconfig \
	hcitool \
	hcidump \
	bttest\
	hostapd \
	wpa_supplicant.conf \
	calibration_init \
	nvm_daemon \
	modemd\
	engpc\
	audio.a2dp.default

# own copyright packages files
PRODUCT_PACKAGES += \
    FileExplorer \
    AppBackup \
    AudioProfile \
    SprdNote \
    CallFireWall \
    ValidationTools \
    libsprddm \
    libvalidationtoolsjni \
    vtserver	\
    libengappjni \
    Engapp \
    \
    libstagefright_sprd_mpeg4enc	\
    libstagefright_sprd_mpeg4dec \
    libstagefright_sprd_h264dec	\
    libstagefright_sprd_h264enc	\
    libstagefright_sprd_vpxdec \
    libstagefright_soft_mjpgdec \
    libstagefright_sprd_aacdec \
    libstagefright_sprd_mp3dec

# prebuild files
# PRODUCT_PACKAGES += \
#    ES_File_Explorer.apk

PRODUCT_PACKAGES += \
	nvitemd \
	charge \
	refnotify\
	vcharged \
	poweroff_alarm \
	mplayer \
	sqlite3 \
	gralloc.$(TARGET_PLATFORM) \
	hwcomposer.$(TARGET_PLATFORM) \
	camera.$(TARGET_PLATFORM) \
	libisp.so \
	lights.$(TARGET_PLATFORM) \
	audio.primary.$(TARGET_PLATFORM) \
	audio_policy.default.so \
	tinymix \
	libvbeffect \
        sensors.$(TARGET_PLATFORM) \
        $(INVENSENSE) \
	$(MALI) \
	libsprdstreamrecoder \
	libvtmanager

ifeq ($(TARGET_PLATFORM),sc8830)
PRODUCT_PACKAGES += \
	sprd_gsp.$(TARGET_PLATFORM)
endif

PRODUCT_PACKAGES += \
            $(BRCM_FM) \
            $(SPRD_FM_APP)


PRODUCT_COPY_FILES := \
	$(BOARDDIR)/init.rc:root/init.rc \
	$(BOARDDIR)/init.sc8830.rc:root/init.sc8830.rc \
	$(BOARDDIR)/init.sc8830.usb.rc:root/init.sc8830.usb.rc \
	$(BOARDDIR)/ueventd.sc8830.rc:root/ueventd.sc8830.rc \
	$(BOARDDIR)/fstab.sc8830:root/fstab.sc8830 \
	$(BOARDDIR)/nvitem_td.cfg:root/nvitem_td.cfg \
	$(BOARDDIR)/nvitem_w.cfg:root/nvitem_w.cfg \
	device/sprd/common/res/CDROM/adb.iso:system/etc/adb.iso \
	device/sprd/common/libs/audio/apm/devicevolume.xml:system/etc/devicevolume.xml \
	device/sprd/common/libs/audio/apm/formatvolume.xml:system/etc/formatvolume.xml \
        $(BOARDDIR)/hw_params/tiny_hw.xml:system/etc/tiny_hw.xml \
        $(BOARDDIR)/hw_params/codec_pga.xml:system/etc/codec_pga.xml \
	$(BOARDDIR)/hw_params/audio_hw.xml:system/etc/audio_hw.xml \
        $(BOARDDIR)/hw_params/audio_para:system/etc/audio_para \
	$(BOARDDIR)/scripts/ext_symlink.sh:system/bin/ext_symlink.sh \
	$(BOARDDIR)/scripts/ext_data.sh:system/bin/ext_data.sh \
	$(BOARDDIR)/scripts/ext_kill.sh:system/bin/ext_kill.sh \
	$(BOARDDIR)/scripts/ext_chown.sh:system/bin/ext_chown.sh \
	$(BOARDDIR)/headset-keyboard.kl:system/usr/keylayout/headset-keyboard.kl \
	device/sprd/common/libs/mali/egl.cfg:system/lib/egl/egl.cfg \
	device/sprd/common/libs/audio/audio_policy.conf:system/etc/audio_policy.conf \
	device/sprd/sp8830eb/media_codecs.xml:system/etc/media_codecs.xml \
	device/sprd/sp8830eb/media_profiles.xml:system/etc/media_profiles.xml \
	frameworks/native/data/etc/handheld_core_hardware.xml:system/etc/permissions/handheld_core_hardware.xml \
	frameworks/native/data/etc/android.hardware.bluetooth.xml:system/etc/permissions/android.hardware.bluetooth.xml \
	frameworks/native/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
	frameworks/native/data/etc/android.hardware.camera.autofocus.xml:system/etc/permissions/android.hardware.camera.autofocus.xml \
	frameworks/native/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
	frameworks/native/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
        frameworks/native/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
        frameworks/native/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
        frameworks/native/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
        frameworks/native/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml \
		frameworks/native/data/etc/android.hardware.sensor.gyroscope.xml:system/etc/permissions/android.hardware.sensor.gyroscope.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.multitouch.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.xml \
    frameworks/native/data/etc/android.hardware.touchscreen.xml:system/etc/permissions/android.hardware.touchscreen.xml \
	frameworks/native/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
	frameworks/native/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
	frameworks/native/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
	device/sprd/common/res/apn/apns-conf.xml:system/etc/apns-conf.xml \
	device/sprd/partner/brcm/gps/glgps:/system/bin/glgps \
	device/sprd/partner/brcm/gps/gpsconfig_shark.xml:/system/etc/gpsconfig.xml \
	device/sprd/partner/brcm/gps/gps.default.so:/system/lib/hw/gps.default.so

#        frameworks/native/data/etc/android.software.live_wallpaper.xml:system/etc/permissions/android.software.live_wallpaper.xml \


BOARD_WLAN_DEVICE_REV       := bcm4330_b2
$(call inherit-product, hardware/broadcom/wlan/bcmdhd/firmware/bcm4330/device-bcm.mk)
$(call inherit-product, $(BOARDDIR)/../common/apps/engineeringmodel/module.mk)
$(call inherit-product, $(BOARDDIR)/../common/apps/modemassert/module.mk)
