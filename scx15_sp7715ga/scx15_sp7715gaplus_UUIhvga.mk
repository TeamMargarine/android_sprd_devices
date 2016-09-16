-include vendor/sprd/operator/cucc/specA/res/boot/boot_res_hvga.mk

PRODUCT_THEME_PACKAGES := SimpleStyle HelloColor
PRODUCT_THEME_FLAGS := shrink

include device/sprd/scx15_sp7715ga/scx15_sp7715gaplushvga.mk

PRODUCT_NAME := scx15_sp7715gaplus_UUIhvga

DEVICE_PACKAGE_OVERLAYS := $(PLATDIR)/overlay_full $(DEVICE_PACKAGE_OVERLAYS)
PRODUCT_VIDEO_WALLPAPERS := none

include vendor/sprd/UniverseUI/ThemeRes/universeui.mk
$(call inherit-product, vendor/sprd/partner/shark/bluetooth/device-shark-bt.mk)

# We won't add PinyinIME into PRODUCT_PACKAGES as request.
PRODUCT_PACKAGES := $(filter-out PinyinIME, $(PRODUCT_PACKAGES))

PRODUCT_PACKAGES += IDooIME

# SprdLauncher2
PRODUCT_PACKAGES += \
        SprdLauncher2

PRODUCT_PROPERTY_OVERRIDES += \
	ro.device.support.geocode=false \
        persist.sys.support.vt=false \
        persist.surpport.50ksearch=0

PRODUCT_LOCALES := zh_CN zh_TW en_US
ifeq ($(MULTILANGUAGE_SUPPORT),true)
PRODUCT_LOCALES :=en_US en_AU en_CA en_GB en_IE en_IN en_NZ en_SG en_ZA zh_CN zh_TW ar_EG ar_IL fa_AF fa_IR ru_RU fr_BE fr_CA fr_CH fr_FH ro_RO ms_MY bn_BD pt_BR pt_PT sw_KE sw_TZ tl_PH th_TH tr_TR es_ES es_US hi_IN id_ID vi_VN ha_GH ha_NE ha_NG my_MM ce_PH te_IN ta_IN ug_CN ur_IN ur_PK am_ET pl_PL bo_CN bo_IN da_DK de_AT de_CH de_DE de_LI gu_IN nl_BE nl_NL km_KH cs_CZ lo_LA pa_IN pa_PK sl_SI uk_UA el_GR it_CH it_IT ka_CE kk_KZ ko_KR hr_HR lv_LV et_EE as_ET
endif
#Default language when first Boot
PRODUCT_PROPERTY_OVERRIDES += ro.product.locale.language=en
PRODUCT_PROPERTY_OVERRIDES += ro.product.locale.region=US

