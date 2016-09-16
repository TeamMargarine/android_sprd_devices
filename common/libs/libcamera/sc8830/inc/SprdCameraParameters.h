/*
* hardware/sprd/common/libcamera/SprdCameraParameters.h
 * parameters on sc8825
 *
 * Copyright (C) 2013 Spreadtrum
 * 
 * Author: Shan He <shan.he@spreadtrum.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef ANDROID_HARDWARE_SPRD_CAMERA_PARAMETERS_H
#define ANDROID_HARDWARE_SPRD_CAMERA_PARAMETERS_H

#include <camera/CameraParameters.h>

namespace android {

class SprdCameraParameters : public CameraParameters {
public:
	enum ConfigType {
		kFrontCameraConfig,
		kBackCameraConfig
	};

	typedef struct {
		int width;
		int height;
	} Size;

	typedef struct {
		int x;
		int y;
		int width;
		int height;
	}Rect;

public:
    SprdCameraParameters();
    SprdCameraParameters(const String8 &params);
    ~SprdCameraParameters();

	void setDefault(ConfigType config);

	void getFocusAreas(int *area, int *count);
	void getFocusAreas(int *area, int *count, Size *preview_size,
					 Rect *preview_rect, int orientation, bool mirror);
	int getFocusMode();
	int getWhiteBalance();
	int getCameraId();
	int getJpegQuality();
	int getJpegThumbnailQuality();
	int getEffect();
	int getSceneMode();
	int getZoom();
	int getBrightness();
	int getSharpness();
	int getContrast();
	int getSaturation();
	int getExposureCompensation();
	int getAntiBanding();
	int getIso();
	int getRecordingHint();
	int getFlashMode();
	int getSlowmotion();

	// These sizes have to be a multiple of 16 in each dimension
	static const Size kPreviewSizes[];
	static const int kPreviewSettingCount;
	static const int kDefaultPreviewSize;

	static const unsigned int kFocusZoneMax = 5;
	static const int kInvalidValue = 0xffffffff;
	static const int kFrontCameraConfigCount;
	static const int kBackCameraConfigCount;

	static const char KEY_FOCUS_AREAS[];
	static const char KEY_FOCUS_MODE[];
	static const char KEY_WHITE_BALANCE[];
	static const char KEY_CAMERA_ID[];
	static const char KEY_JPEG_QUALITY[];
	static const char KEY_JPEG_THUMBNAIL_QUALITY[];
	static const char KEY_EFFECT[];
	static const char KEY_SCENE_MODE[];
	static const char KEY_ZOOM[];
	static const char KEY_BRIGHTNESS[];
	static const char KEY_CONTRAST[];
	static const char KEY_EXPOSURE_COMPENSATION[];
	static const char KEY_ANTI_BINDING[];
	static const char KEY_ISO[];
	static const char KEY_RECORDING_HINT[];
	static const char KEY_FLASH_MODE[];
	static const char KEY_SLOWMOTION[];
	static const char KEY_SATURATION[];
	static const char KEY_SHARPNESS[];

private:

};

}//namespace android

#endif
