# export original make file
LOCAL_ORIGINAL_PRODUCT_MAKEFILE := device/sprd/scx35_sc9620openphone_zt/scx35_sc9620openphone_ztbase.mk

include $(LOCAL_ORIGINAL_PRODUCT_MAKEFILE)
# Rename product name and we can lunch it
PRODUCT_NAME := scx35_sc9620basecmccspec3_UUI_zt


# Set cg prop
PRODUCT_DEFAULT_PROPERTY_OVERRIDES += \
	ro.product.hardware=P1 \
	ro.product.cg_version=4.4.001.P1.8019 \
	ro.cg.product.device=Coolpad.8019 \
	ro.product.releasetime=2014-05-04 \
	ro.product.model=Coolpad.8019 \
	ro.product.brand=Coolpad \
	ro.product.name=Coolpad.8019 \
	ro.product.device=Coolpad.8019 \
	ro.product.manufacturer=Coolpad \
	ro.yulong.version.software=4.4.001.P1.8019 \
	ro.yulong.version.hardware=P1 \
	ro.build.display.id=4.4.001.P1.8019 \
	ro.product.board.customer=cgmobile \
	ro.config.ringtone=Coolpad.ogg \
	ro.config.notification_sound=Message01.ogg \
	ro.config.alarm_alert=Alarm.ogg

PRODUCT_PROPERTY_OVERRIDES += \
        persist.sys.support.vt=false

PRODUCT_PACKAGES += \
    SprdLauncher1

#[[ for autotest
PRODUCT_PACKAGES += autotest
#]]

DEVICE_PACKAGE_OVERLAYS := $(PLATDIR)/overlay_full $(DEVICE_PACKAGE_OVERLAYS)

include vendor/sprd/UniverseUI/ThemeRes/universeui.mk
include vendor/sprd/operator/cmcc/spec3.mk

