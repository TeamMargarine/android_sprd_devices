#
# Copyright (C) 2011 The Android Open-Source Project
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

TARGET_CPU_VARIANT := cortex-a7
TARGET_ARCH:= arm
TARGET_CPU_ABI := armeabi-v7a
TARGET_CPU_ABI2 := armeabi
TARGET_CPU_SMP := true
TARGET_ARCH_VARIANT := armv7-a-neon
ARCH_ARM_HAVE_TLS_REGISTER := true

TARGET_BOARD_PLATFORM := sc8830
TARGET_BOOTLOADER_BOARD_NAME := sp8830eb

# config u-boot
TARGET_NO_BOOTLOADER := false
UBOOT_DEFCONFIG := sp8830eb

# Enable the optimized DEX
ifneq ($(filter user, $(TARGET_BUILD_VARIANT)),)
WITH_DEXPREOPT=true
endif

# config kernel
TARGET_NO_KERNEL := false
KERNEL_DEFCONFIG := sp8830eb-native_defconfig
USES_UNCOMPRESSED_KERNEL := true
BOARD_KERNEL_BASE := 0x00000000
BOARD_KERNEL_CMDLINE := console=ttyS1,115200n8

# use default init.rc
TARGET_PROVIDES_INIT_RC := true
RECOVERY_PROVIDES_INIT_RC := true

#select 2M,3M,5M,8M
CAMERA_SUPPORT_SIZE := 8M
#zsl capture
TARGET_BOARD_CAMERA_CAPTURE_MODE := true
#back camera rotation capture
TARGET_BOARD_BACK_CAMERA_ROTATION := false
#front camera rotation capture
TARGET_BOARD_FRONT_CAMERA_ROTATION :=  false
#rotation capture
TARGET_BOARD_CAMERA_ROTATION_CAPTURE := true
#video size 1080P,720P,D1
TARGET_BOARD_CAMERA_SUPPORT_VIDEO_SIZE := 1080P



BOARD_HAVE_BLUETOOTH := true
BOARD_HAVE_BLUETOOTH_BCM := true
BOARD_HAVE_FM_BCM := true
BOARD_USE_SPRD_FMAPP := true

# board specific modules
BOARD_USES_TINYALSA_AUDIO := true
BOARD_USES_ALSA_AUDIO := false
BUILD_WITH_ALSA_UTILS := false
BOARD_USE_VETH := true
BOARD_SPRD_RIL := true
USE_SPRD_HWCOMPOSER  := true
TARGET_RECOVERY_UI_LIB := librecovery_ui_sp8830
TARGET_RECOVERY_PIXEL_FORMAT := "RGBA_8888"

# ext4 partition layout
TARGET_USERIMAGES_USE_EXT4 := true
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 300000000
BOARD_USERDATAIMAGE_PARTITION_SIZE := 2000000000
BOARD_CACHEIMAGE_PARTITION_SIZE := 150000000
BOARD_PRODNVIMAGE_PARTITION_SIZE := 5242880
BOARD_FLASH_BLOCK_SIZE := 4096
BOARD_PRODNVIMAGE_FILE_SYSTEM_TYPE := ext4
BOARD_CACHEIMAGE_FILE_SYSTEM_TYPE := ext4

BOARD_CACHEIMAGE_SPARSE_EXT_FLAG := true
BOARD_USERDATAIMAGE_SPARSE_EXT_FLAG := true
BOARD_SYSTEMIMAGE_SPARSE_EXT_FLAG := false

BOARD_EGL_CFG := device/sprd/common/libs/mali/egl.cfg
USE_OPENGL_RENDERER := true


BOARD_WPA_SUPPLICANT_DRIVER := NL80211
WPA_SUPPLICANT_VERSION     := VER_0_8_X
BOARD_WPA_SUPPLICANT_PRIVATE_LIB := lib_driver_cmd_bcmdhd
BOARD_HOSTAPD_DRIVER        := NL80211
BOARD_HOSTAPD_PRIVATE_LIB   := lib_driver_cmd_bcmdhd
BOARD_WLAN_DEVICE           := bcmdhd
WIFI_DRIVER_FW_PATH_PARAM   := "/sys/module/bcmdhd/parameters/firmware_path"
WIFI_DRIVER_FW_PATH_STA     := "/etc/wifi/fw_bcmdhd.bin"
WIFI_DRIVER_FW_PATH_P2P     := "/etc/wifi/fw_bcmdhd_p2p.bin"
WIFI_DRIVER_FW_PATH_AP      := "/etc/wifi/fw_bcmdhd_apsta.bin"
USE_CAMERA_STUB := true

BOARD_USES_GENERIC_AUDIO := true

# sensor config
USE_INVENSENSE_LIB := true
#USE_SPRD_SENSOR_LIB := true
#BOARD_HAVE_ACC := Lis3dh
#BOARD_ACC_INSTALL := 6
#BOARD_HAVE_ORI := Akm
#BOARD_ORI_INSTALL := 7
BOARD_HAVE_PLS := LTR558ALS

TARGET_BOARD_BACK_CAMERA_ROTATION := false
TARGET_BOARD_FRONT_CAMERA_ROTATION := false
TARGET_BOARD_BACK_CAMERA_INTERFACE := mipi
TARGET_BOARD_FRONT_CAMERA_INTERFACE := ccir
TARGET_BOARD_NO_FRONT_SENSOR := false
TARGET_BOARD_CAMERA_ANTI_SHAKE := false
TARGET_BOARD_CAMERA_DMA_COPY := false
TARGET_BOARD_CAMERA_HAL_VERSIONG := HAL1.0

USE_BOOT_AT_DIAG := true
