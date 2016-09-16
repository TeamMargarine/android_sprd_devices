
MALI := libUMP libEGL_mali.so libGLESv1_CM_mali.so libGLESv2_mali.so libMali.so ump.ko mali.ko



SPRD_FM_APP := FMPlayer

#FFOS specific macros, may move into a new file someday
ENABLE_LIBRECOVERY := true
PRODUCT_PROPERTY_OVERRIDES := \
	ro.moz.omx.hw.max_width=720 \
	ro.moz.omx.hw.max_height=576 \
	ro.moz.ril.query_icc_count=true \
	ro.moz.mute.call.to_ril=true \
	ro.moz.ril.numclients=2 \
    ro.moz.ril.data_reg_on_demand=true\
    ro.moz.ril.radio_off_wo_card=true\
    ro.moz.ril.0.network_types = gsm,wcdma\
    ro.moz.ril.1.network_types = gsm

# original apps copied from generic_no_telephony.mk
PRODUCT_PACKAGES := \
	librecovery \
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
	audio.a2dp.default \
        SoundRecorder \
        libmorpho_facesolid.so

# own copyright packages files
PRODUCT_PACKAGES += \
    AudioProfile \
    ValidationTools \
    libvalidationtoolsjni \
    vtserver	\
	\
    libstagefright_sprd_mpeg4enc	\
    libstagefright_sprd_mpeg4dec \
    libstagefright_sprd_h264dec	\
    libstagefright_sprd_aacdec \
    libstagefright_sprd_mp3dec

# prebuild files

PRODUCT_PACKAGES += \
	modem_control\
	mcom_monitor \
	nvitemd \
	charge \
	vcharged7 \
	poweroff_alarm \
	mplayer \
	sqlite3 \
	calibration_init \
	gralloc.$(TARGET_PLATFORM) \
	hwcomposer.$(TARGET_PLATFORM) \
	camera.$(TARGET_PLATFORM) \
	lights.$(TARGET_PLATFORM) \
	audio.primary.$(TARGET_PLATFORM) \
	audio_policy.$(TARGET_PLATFORM) \
	tinymix \
	sensors.$(TARGET_PLATFORM) \
	libmbbms_tel_jni.so \
	$(MALI) \
	zram.sh \
	libsprdstreamrecoder \
	libvtmanager\
   fm.$(TARGET_PLATFORM)

PRODUCT_PACKAGES += \
            $(SPRD_FM_APP)
PRODUCT_COPY_FILES := \
	$(BOARDDIR)/init.rc:root/init.rc \
	$(BOARDDIR)/init.sc7710g.rc:root/init.sc7710g.rc \
	$(BOARDDIR)/init.sc7710g.usb.rc:root/init.sc7710g.usb.rc \
	$(BOARDDIR)/ueventd.sc7710g.rc:root/ueventd.sc7710g.rc \
	$(BOARDDIR)/fstab.sc7710g:root/fstab.sc7710g \
	$(BOARDDIR)/vold.fstab:system/etc/vold.fstab \
	$(BOARDDIR)/modem_images.info:root/modem_images.info \
	$(BOARDDIR)/nvitem.cfg:root/nvitem.cfg \
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
	device/sprd/common/libs/mali/egl.cfg:system/lib/egl/egl.cfg \
	device/sprd/common/libs/audio/audio_policy.conf:system/etc/audio_policy.conf \
	device/sprd/common/res/media/media_codecs.xml:system/etc/media_codecs.xml \
	device/sprd/common/res/media/media_profiles.xml:system/etc/media_profiles.xml \
	frameworks/base/data/etc/android.hardware.bluetooth.xml:system/etc/permissions/android.hardware.bluetooth.xml \
	frameworks/base/data/etc/android.hardware.camera.front.xml:system/etc/permissions/android.hardware.camera.front.xml \
	frameworks/base/data/etc/android.hardware.camera.autofocus.xml:system/etc/permissions/android.hardware.camera.autofocus.xml \
	frameworks/base/data/etc/android.hardware.camera.flash-autofocus.xml:system/etc/permissions/android.hardware.camera.flash-autofocus.xml \
	frameworks/base/data/etc/android.hardware.wifi.xml:system/etc/permissions/android.hardware.wifi.xml \
	frameworks/base/data/etc/android.hardware.touchscreen.multitouch.xml:system/etc/permissions/android.hardware.touchscreen.multitouch.xml \
	frameworks/base/data/etc/android.hardware.touchscreen.xml:system/etc/permissions/android.hardware.touchscreen.xml \
	frameworks/base/data/etc/android.hardware.location.gps.xml:system/etc/permissions/android.hardware.location.gps.xml \
	frameworks/base/data/etc/android.hardware.location.xml:system/etc/permissions/android.hardware.location.xml \
	frameworks/base/data/etc/android.hardware.screen.landscape.xml:system/etc/permissions/android.hardware.screen.landscape.xml \
	frameworks/base/data/etc/android.hardware.screen.portrait.xml:system/etc/permissions/android.hardware.screen.portrait.xml \
	frameworks/base/data/etc/android.hardware.microphone.xml:system/etc/permissions/android.hardware.microphone.xml \
	frameworks/base/data/etc/android.hardware.sensor.accelerometer.xml:system/etc/permissions/android.hardware.sensor.accelerometer.xml \
	frameworks/base/data/etc/android.hardware.sensor.compass.xml:system/etc/permissions/android.hardware.sensor.compass.xml \
	frameworks/base/data/etc/android.hardware.sensor.light.xml:system/etc/permissions/android.hardware.sensor.light.xml \
	frameworks/base/data/etc/android.hardware.sensor.proximity.xml:system/etc/permissions/android.hardware.sensor.proximity.xml \
	frameworks/base/data/etc/android.hardware.telephony.gsm.xml:system/etc/permissions/android.hardware.telephony.gsm.xml \
	frameworks/base/data/etc/android.hardware.usb.accessory.xml:system/etc/permissions/android.hardware.usb.accessory.xml \
	frameworks/base/data/etc/android.software.live_wallpaper.xml:system/etc/permissions/android.software.live_wallpaper.xml \
	device/sprd/common/res/apn/apns-conf.xml:system/etc/apns-conf.xml

$(call inherit-product, $(BOARDDIR)/../common/apps/engineeringmodel/module.mk)
$(call inherit-product, $(BOARDDIR)/../common/apps/modemassert/module.mk)
$(call inherit-product, hardware/broadcom/wlan/bcmdhd/firmware/bcm40181/device-bcm.mk)
$(call inherit-product, $(BOARDDIR)/../common/apps/modemassert/module.mk)
$(call inherit-product, device/sprd/partner/ublox/device-ublox-gps.mk)
