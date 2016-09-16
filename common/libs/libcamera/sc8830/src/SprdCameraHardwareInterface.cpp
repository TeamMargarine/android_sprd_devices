/*
* hardware/sprd/hsdroid/libcamera/sprdcamerahardwareinterface.cpp
 *
 * Copyright (C) 2013 Spreadtrum
 *
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

#define LOG_NDEBUG 0
#define LOG_TAG "SprdCameraHardware"

#include <utils/Log.h>
#include <utils/String16.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>
#include <cutils/properties.h>

#include "../../../gralloc/gralloc_priv.h"
#include "ion_sprd.h"

#include <camera/Camera.h>
#include <media/hardware/MetadataBufferType.h>

#include "SprdOEMCamera.h"
#include "SprdCameraHardwareInterface.h"

#ifdef CONFIG_CAMERA_ISP
extern "C" {
#include "isp_video.h"
}
#endif

//////////////////////////////////////////////////////////////////
namespace android {

#define STOP_PREVIEW_BEFORE_CAPTURE 0

#define LOGV       ALOGV
#define LOGE       ALOGE
#define LOGI       ALOGI
#define LOGW       ALOGW
#define LOGD       ALOGD

#define PRINT_TIME 				0
#define ROUND_TO_PAGE(x)  	(((x)+0xfff)&~0xfff)
#define ARRAY_SIZE(x) 		(sizeof(x) / sizeof(*x))
#define METADATA_SIZE 		12 // (4 * 3)

#define SET_PARM(x,y) 	do { \
							LOGV("%s: set camera param: %s, %d", __func__, #x, y); \
							camera_set_parm (x, y, NULL, NULL); \
						} while(0)
static nsecs_t s_start_timestamp = 0;
static nsecs_t s_end_timestamp = 0;
static int s_use_time = 0;

#define GET_START_TIME do { \
                             s_start_timestamp = systemTime(); \
                       }while(0)
#define GET_END_TIME do { \
                             s_end_timestamp = systemTime(); \
                       }while(0)
#define GET_USE_TIME do { \
                            s_use_time = (s_end_timestamp - s_start_timestamp)/1000000; \
                     }while(0)

///////////////////////////////////////////////////////////////////
//static members
/////////////////////////////////////////////////////////////////////////////////////////
gralloc_module_t const* SprdCameraHardware::mGrallocHal = NULL;

const CameraInfo SprdCameraHardware::kCameraInfo[] = { 
	{
        CAMERA_FACING_BACK,
        90,  /* orientation */
    },
#ifndef CONFIG_DCAM_SENSOR_NO_FRONT_SUPPORT
    {
        CAMERA_FACING_FRONT,
        270,  /* orientation */
    },
#endif
};

const CameraInfo SprdCameraHardware::kCameraInfo3[] = {
    {
        CAMERA_FACING_BACK,
        90,  /* orientation */
    },

    {
        CAMERA_FACING_FRONT,
        270,  /* orientation */
    },

    {
        2,
        0,  /* orientation */
    }
};	

int SprdCameraHardware::getPropertyAtv()
{
	char propBuf_atv[10] = {0};
	int atv = 0;
	
	property_get("sys.camera.atv", propBuf_atv, "0");
	if (0 == strcmp(propBuf_atv, "1")) {
		atv = 1;
	}
	else {
		atv = 0;
	}

	LOGV("getPropertyAtv: %d", atv);

	return atv;
}

int SprdCameraHardware::getNumberOfCameras()
{
	int num = 0;

	if (1 == getPropertyAtv()) {
		num = sizeof(SprdCameraHardware::kCameraInfo3) / sizeof(SprdCameraHardware::kCameraInfo3[0]);
	} else {
		num = sizeof(SprdCameraHardware::kCameraInfo) / sizeof(SprdCameraHardware::kCameraInfo[0]);
	}

	LOGV("getNumberOfCameras: %d",num);
	return num;
}

int SprdCameraHardware::getCameraInfo(int cameraId, struct camera_info *cameraInfo)
{
	if (1 == getPropertyAtv()) {
		memcpy(cameraInfo, &kCameraInfo3[cameraId], sizeof(CameraInfo));
	} else {
		memcpy(cameraInfo, &kCameraInfo[cameraId], sizeof(CameraInfo));
	}
	return 0;
}
/////////////////////////////////////////////////////////////////////////////////////////
//SprdCameraHardware: public functions
/////////////////////////////////////////////////////////////////////////////////////////
SprdCameraHardware::SprdCameraHardware(int cameraId)
	: mParameters(),
	mPreviewHeight(-1),
	mPreviewWidth(-1),
	mRawHeight(-1),
	mRawWidth(-1),
	mRawHeap(NULL),
	mMiscHeap(NULL),
	mFDAddr(0),    
	mRawHeapSize(0),
	mMiscHeapSize(0),
	mJpegHeapSize(0),
	mPreviewHeapSize(0),    
	mPreviewHeapNum(0),
	mPreviewHeapArray(NULL),
	mPreviewFormat(1),
	mPictureFormat(1),
	mPreviewStartFlag(0),
	mRecordingMode(0),
	mJpegSize(0),
	mNotify_cb(0),
	mData_cb(0),
	mData_cb_timestamp(0),
	mGetMemory_cb(0),
	mUser(0),
	mMsgEnabled(0),
	mPreviewWindow(NULL),
	mMetadataHeap(NULL),
	mIsStoreMetaData(false),
	mIsFreqChanged(false),
	mZoomLevel(0),
	mCameraId(cameraId),
    miSPreviewFirstFrame(1),
    mCaptureMode(CAMERA_ZSL_MODE)
{
	LOGV("openCameraHardware: E cameraId: %d.", cameraId);

    memset(mPreviewHeapArray_phy, 0, sizeof(mPreviewHeapArray_phy));
    memset(mPreviewHeapArray_vir, 0, sizeof(mPreviewHeapArray_vir));

	setCameraState(SPRD_INIT, STATE_CAMERA);

	if (!mGrallocHal) {
		int ret = hw_get_module(GRALLOC_HARDWARE_MODULE_ID, (const hw_module_t **)&mGrallocHal);
		if (ret)
			LOGE("ERR(%s):Fail on loading gralloc HAL", __func__);
	}

	if (1 == getPropertyAtv()) {
		mCameraId = 5; //for ATV
	} else {
		mCameraId = cameraId;
	}

	initDefaultParameters();

	LOGV("openCameraHardware: X cameraId: %d.", cameraId);
}

SprdCameraHardware::~SprdCameraHardware()
{
}

void SprdCameraHardware::release()
{
    LOGV("release E");
    LOGV("mLock:release S .\n");
    Mutex::Autolock l(&mLock);

    // Either preview was ongoing, or we are in the middle or taking a
    // picture.  It's the caller's responsibility to make sure the camera
    // is in the idle or init state before destroying this object.	
	LOGV("release:camera state = %s, preview state = %s, capture state = %s",
		getCameraStateStr(getCameraState()), getCameraStateStr(getPreviewState()), 
		getCameraStateStr(getCaptureState()));
	
    if (isCapturing()) {
		cancelPictureInternal();
    }	
	
    if (isPreviewing()) {
		stopPreviewInternal();
    }	
	
	if (isCameraInit()) {
		// When libqcamera detects an error, it calls camera_cb from the
		// call to camera_stop, which would cause a deadlock if we
		// held the mStateLock.  For this reason, we have an intermediate
		// state SPRD_INTERNAL_STOPPING, which we use to check to see if the
		// camera_cb was called inline.
		setCameraState(SPRD_INTERNAL_STOPPING, STATE_CAMERA);

		LOGV("stopping camera.");
		if(CAMERA_SUCCESS != camera_stop(camera_cb, this)){
			setCameraState(SPRD_ERROR, STATE_CAMERA);
			mMetadataHeap = NULL;
			LOGE("release: fail to camera_stop().");
			LOGV("mLock:release E.\n");
			return;
		}

		WaitForCameraStop();
	}	

	mMetadataHeap = NULL;		

	LOGV("release X");
	LOGV("mLock:release E.\n");		
}

int SprdCameraHardware::getCameraId() const
{
    return mCameraId;
}

status_t SprdCameraHardware::startPreview()
{
	LOGV("startPreview: E");
	Mutex::Autolock l(&mLock);

	setCaptureRawMode(0);

	if(mPreviewStartFlag == 0) {
	    LOGV("don't need to start preview.");
	    return NO_ERROR;
	}	

	bool isRecordingMode = (mMsgEnabled & CAMERA_MSG_VIDEO_FRAME) > 0 ? true : false;	
	return startPreviewInternal(isRecordingMode);
}

void SprdCameraHardware::stopPreview()
{
	LOGV("stopPreview: E");
	Mutex::Autolock l(&mLock);			

	stopPreviewInternal();
	setRecordingMode(false);
	LOGV("stopPreview: X");
}

bool SprdCameraHardware::previewEnabled()
{
    bool ret = 0;
    LOGV("mLock:previewEnabled S.\n");
    Mutex::Autolock l(&mLock);
    LOGV("mLock:previewEnabled E.\n");
    if(0 == mPreviewStartFlag) {
        return 1;
    }  else if (2 == mPreviewStartFlag) {
        return 0;
    } else {
        return isPreviewing();
    }
}

status_t SprdCameraHardware::setPreviewWindow(preview_stream_ops *w)
{
    int min_bufs;

    mPreviewWindow = w;
    LOGV("%s: mPreviewWindow %p", __func__, mPreviewWindow);

    if (!w) {
        LOGE("preview window is NULL!");
        mPreviewStartFlag = 0;
        return NO_ERROR;
    }

    mPreviewStartFlag = 1;

    if (isPreviewing()){
        LOGI("stop preview (window change)");
        stopPreviewInternal();
    }	

    if (w->get_min_undequeued_buffer_count(w, &min_bufs)) {
        LOGE("%s: could not retrieve min undequeued buffer count", __func__);
        return INVALID_OPERATION;
    }

    if (min_bufs >= kPreviewBufferCount) {
        LOGE("%s: min undequeued buffer count %d is too high (expecting at most %d)", __func__,
             min_bufs, kPreviewBufferCount - 1);
    }

    LOGV("%s: setting buffer count to %d", __func__, kPreviewBufferCount);
    if (w->set_buffer_count(w, kPreviewBufferCount)) {
        LOGE("%s: could not set buffer count", __func__);
        return INVALID_OPERATION;
    }

    int preview_width;
    int preview_height;
    mParameters.getPreviewSize(&preview_width, &preview_height);
    LOGV("%s: preview size: %dx%d.", __func__, preview_width, preview_height);
	
#if CAM_OUT_YUV420_UV
    int hal_pixel_format = HAL_PIXEL_FORMAT_YCbCr_420_SP;
#else
    int hal_pixel_format = HAL_PIXEL_FORMAT_YCrCb_420_SP;
#endif

    const char *str_preview_format = mParameters.getPreviewFormat();
	int usage;

    LOGV("%s: preview format %s", __func__, str_preview_format);

	usage = GRALLOC_USAGE_SW_WRITE_OFTEN;

    if (preview_width > 640) {

		if (w->set_usage(w, usage )) {
        	LOGE("%s: could not set usage on gralloc buffer", __func__);
        	return INVALID_OPERATION;
    	}
    } else {

		if (w->set_usage(w, usage )) {
        	LOGE("%s: could not set usage on gralloc buffer", __func__);
        	return INVALID_OPERATION;
    	}
    }

    if (w->set_buffers_geometry(w,
                                preview_width, preview_height,
                                hal_pixel_format)) {
        LOGE("%s: could not set buffers geometry to %s",
             __func__, str_preview_format);
        return INVALID_OPERATION;
    }

   status_t ret = startPreviewInternal(isRecordingMode());
   if (ret != NO_ERROR) {
		return INVALID_OPERATION;
   }

    return NO_ERROR;
}

status_t SprdCameraHardware::takePicture()
{
    GET_START_TIME;
	takepicture_mode mode = getCaptureMode();
	LOGV("takePicture: E");

	LOGV("ISP_TOOL:takePicture: %d E", mode);
	
    print_time();

    Mutex::Autolock l(&mLock);

	if (!iSZslMode()) {
		//stop preview first for debug
	    if (isPreviewing()) {
			LOGV("call stopPreviewInternal in takePicture().");
			stopPreviewInternal();
	    }
	    LOGV("ok to stopPreviewInternal in takePicture. preview state = %d", getPreviewState());

		if (isPreviewing()) {
			LOGE("takePicture: stop preview error!, preview state = %d", getPreviewState());
			return UNKNOWN_ERROR;
		}
		if (!initCapture(mData_cb != NULL)) {
			deinitCapture();
			LOGE("takePicture initCapture failed. Not taking picture.");
			return UNKNOWN_ERROR;
	    }
	}

	//wait for last capture being finished
	if (isCapturing()) {
		WaitForCaptureDone();
	}

	LOGV("start to initCapture in takePicture.");	

	setCameraState(SPRD_INTERNAL_RAW_REQUESTED, STATE_CAPTURE);
    LOGV("INTERPOLATION::takePicture:mRawWidth=%d,mZoomLevel=%d",mRawWidth,mZoomLevel);
    if(CAMERA_SUCCESS != camera_take_picture(camera_cb, this, mode))
    {
    	setCameraState(SPRD_ERROR, STATE_CAPTURE);
		LOGE("takePicture: fail to camera_take_picture.");
		return UNKNOWN_ERROR;
    }

	bool result = WaitForCaptureStart();

	print_time();
    LOGV("takePicture: X");
    	
    return result ? NO_ERROR : UNKNOWN_ERROR;	
}

status_t SprdCameraHardware::cancelPicture()
{
	Mutex::Autolock l(&mLock);	

	return cancelPictureInternal();
}

status_t SprdCameraHardware::setTakePictureSize(uint32_t width, uint32_t height)
{
	mRawWidth = width;
	mRawHeight = height;

	return NO_ERROR;
}

status_t SprdCameraHardware::startRecording()
{
	LOGV("mLock:startRecording S.\n");
	Mutex::Autolock l(&mLock);

#if STOP_PREVIEW_BEFORE_CAPTURE			
	if (isPreviewing()) {
		LOGV("wxz call stopPreviewInternal in startRecording().");
		setCameraState(SPRD_INTERNAL_PREVIEW_STOPPING, STATE_PREVIEW);
		
		if(CAMERA_SUCCESS != camera_stop_preview()){
			setCameraState(SPRD_ERROR, STATE_PREVIEW);
			freePreviewMem();
			LOGE("startRecording: fail to camera_stop_preview().");
			return INVALID_OPERATION;
		}

		WaitForPreviewStop();

		LOGV("startRecording: Freeing preview heap.");
		freePreviewMem();
	}
#endif

	return startPreviewInternal(true);
}

void SprdCameraHardware::stopRecording()
{
	LOGV("stopRecording: E");
	Mutex::Autolock l(&mLock);
	stopPreviewInternal();
	LOGV("stopRecording: X");
}

void SprdCameraHardware::releaseRecordingFrame(const void *opaque)
{
	//LOGV("releaseRecordingFrame E. ");
	Mutex::Autolock l(&mLock);
	uint8_t *addr = (uint8_t *)opaque;
	uint32_t index;

	if (!isPreviewing()) {
		LOGE("releaseRecordingFrame: Preview not in progress!");
		return;
	}

	if(mIsStoreMetaData) {
		index = (addr - (uint8_t *)mMetadataHeap->data) / (METADATA_SIZE);
	}
	else {
		for (index=0; index<kPreviewBufferCount; index++)
		{
			if (addr == mPreviewHeapArray_vir[index])	break;
		}
	}

	if(index > kPreviewBufferCount){
		LOGV("releaseRecordingFrame error: index: %d, data: %x, w=%d, h=%d \n",
		index, (uint32_t)addr, mPreviewWidth, mPreviewHeight);
	}
	//LOGV("releaseRecordingFrame: index: %d, offset: %x, size: %x.", index,offset,size);
	camera_release_frame(index);
	LOGV("releaseRecordingFrame: index: %d", index);
	//LOGV("releaseRecordingFrame X. ");
}

bool SprdCameraHardware::recordingEnabled()
{
	LOGV("recordingEnabled: E");
	Mutex::Autolock l(&mLock);
	LOGV("recordingEnabled: X");
	
	return isPreviewing() && (mMsgEnabled & CAMERA_MSG_VIDEO_FRAME);
}

status_t SprdCameraHardware::autoFocus()
{
	LOGV("Starting auto focus.");
	LOGV("mLock:autoFocus S.\n");
	Mutex::Autolock l(&mLock);

	if (!isPreviewing()) {
		LOGE("autoFocus: not previewing");
		return INVALID_OPERATION;
	}

	if(0 != camera_start_autofocus(CAMERA_AUTO_FOCUS, camera_cb, this)){
		LOGE("auto foucs fail.");
		//return INVALID_OPERATION;
	}

	LOGV("mLock:autoFocus E.\n");
	return NO_ERROR;				
}

status_t SprdCameraHardware::cancelAutoFocus()
{
	return camera_cancel_autofocus();
}

void SprdCameraHardware::setCaptureRawMode(bool mode)
{
	mCaptureRawMode = mode;
	LOGV("ISP_TOOL: setCaptureRawMode: %d, %d", mode, mCaptureRawMode);
}

status_t SprdCameraHardware::setParameters(const SprdCameraParameters& params)
{
	LOGV("setParameters: E params = %p", &params);
	LOGV("mLock:setParameters S.\n");
	Mutex::Autolock l(&mLock);

#if 0
	// FIXME: verify params
	// yuv422sp is here only for legacy reason. Unfortunately, we release
	// the code with yuv422sp as the default and enforced setting. The
	// correct setting is yuv420sp.
	if(strcmp(params.getPreviewFormat(), "yuv422sp")== 0) {
		mPreviewFormat = 0;
	}
	else if(strcmp(params.getPreviewFormat(), "yuv420sp") == 0) {
		mPreviewFormat = 1;
	}
	else if(strcmp(params.getPreviewFormat(), "rgb565") == 0) {
		mPreviewFormat = 2;
	}
	else if(strcmp(params.getPreviewFormat(), "yuv420p") == 0) {
		mPreviewFormat = 3;
	}
	else {
		LOGE("Onlyyuv422sp/yuv420sp/rgb565 preview is supported.\n");
		return INVALID_OPERATION;
	}

	if(strcmp(params.getPictureFormat(), "yuv422sp")== 0) {
		mPictureFormat = 0;
	}
	else if(strcmp(params.getPictureFormat(), "yuv420sp")== 0) {
		mPictureFormat = 1;
	}
	else if(strcmp(params.getPictureFormat(), "rgb565")== 0) {
		mPictureFormat = 2;
	}
	else if(strcmp(params.getPictureFormat(), "jpeg")== 0) {
		mPictureFormat = 3;
	}
	else {
		LOGE("Onlyyuv422sp/yuv420sp/rgb565/jpeg  picture format is supported.\n");
		return INVALID_OPERATION;
	}	
#else	
	mPreviewFormat = 1;
	mPictureFormat = 1;
#endif	

	LOGV("setParameters: mPreviewFormat=%d,mPictureFormat=%d.",mPreviewFormat,mPictureFormat);

	// FIXME: will this make a deep copy/do the right thing? String8 i
	// should handle it
	mParameters = params;

	// libqcamera only supports certain size/aspect ratios
	// find closest match that doesn't exceed app's request
	int width = 0, height = 0;
	int rawWidth = 0, rawHeight = 0;
	params.getPreviewSize(&width, &height);
	LOGV("setParameters: requested preview size %d x %d", width, height);
	params.getPictureSize(&rawWidth, &rawHeight);
	LOGV("setParameters:requested picture size %d x %d", width, height);
	
	mPreviewWidth = (width + 1) & ~1;
	mPreviewHeight = (height + 1) & ~1;
	mRawHeight = (rawHeight + 1) & ~1;
	mRawWidth = (rawWidth + 1) & ~1;
	LOGV("setParameters: requested picture size %d x %d", mRawWidth, mRawHeight);
	LOGV("setParameters: requested preview size %d x %d", mPreviewWidth, mPreviewHeight);
	
	if (camera_set_change_size(mRawWidth, mRawHeight, mPreviewWidth, mPreviewHeight)) {
		mPreviewStartFlag = 2;
		stopPreviewInternal();
		if (NO_ERROR != startPreviewInternal(isRecordingMode())) {
			return UNKNOWN_ERROR;
		}
	}

	if ((1 == params.getInt("zsl")) &&
		((mCaptureMode != CAMERA_ZSL_CONTINUE_SHOT_MODE) || (mCaptureMode != CAMERA_ZSL_MODE))) {
		LOGI("mode change:stop preview.");
		if (isPreviewing()) {
			mPreviewStartFlag = 2;
			stopPreviewInternal();
			if (NO_ERROR != startPreviewInternal(isRecordingMode())) {
				return UNKNOWN_ERROR;
			}
		}
	}
	if ((0 == params.getInt("zsl")) &&
		((mCaptureMode == CAMERA_ZSL_CONTINUE_SHOT_MODE) || (mCaptureMode == CAMERA_ZSL_MODE))) {
		LOGI("mode change:stop preview.");
		if (isPreviewing()) {
			mPreviewStartFlag = 2;
			stopPreviewInternal();
			if (NO_ERROR != startPreviewInternal(isRecordingMode())) {
				return UNKNOWN_ERROR;
			}
		}
	}
	
	if(NO_ERROR != setCameraParameters()){
		return UNKNOWN_ERROR;
	}

	LOGV("mLock:setParameters E.\n");
	return NO_ERROR;
}

SprdCameraParameters SprdCameraHardware::getParameters() const
{
	LOGV("getParameters: EX");
	return mParameters;
}

void SprdCameraHardware::setCallbacks(camera_notify_callback notify_cb,
                                    camera_data_callback data_cb,
                                    camera_data_timestamp_callback data_cb_timestamp,
                                    camera_request_memory get_memory,
                                    void *user)
{
	mNotify_cb = notify_cb;
	mData_cb = data_cb;
	mData_cb_timestamp = data_cb_timestamp;
	mGetMemory_cb = get_memory;
	mUser = user;
}

void SprdCameraHardware::enableMsgType(int32_t msgType)
{
	LOGV("mLock:enableMsgType S .\n");
	Mutex::Autolock lock(mLock);
	mMsgEnabled |= msgType;
	LOGV("mLock:enableMsgType E .\n");
}

void SprdCameraHardware::disableMsgType(int32_t msgType)
{
	LOGV("'mLock:disableMsgType S.\n");
	//Mutex::Autolock lock(mLock);
	mMsgEnabled &= ~msgType;
	LOGV("'mLock:disableMsgType E.\n");
}

bool SprdCameraHardware::msgTypeEnabled(int32_t msgType)
{
	LOGV("mLock:msgTypeEnabled S.\n");
	Mutex::Autolock lock(mLock);
	LOGV("mLock:msgTypeEnabled E.\n");
	return (mMsgEnabled & msgType);
}

status_t SprdCameraHardware::sendCommand(int32_t cmd, int32_t arg1, int32_t arg2)
{
    uint32_t buffer_size = mPreviewWidth * mPreviewHeight * 3 / 2;
    uint32_t addr = 0;
    LOGE("sendCommand: facedetect mem size 0x%x.",buffer_size);
	if(CAMERA_CMD_START_FACE_DETECTION == cmd){
		setFdmem(buffer_size);
        camera_set_start_facedetect(1);
	} else if(CAMERA_CMD_STOP_FACE_DETECTION == cmd) {
	    LOGE("sendCommand: not support the CAMERA_CMD_STOP_FACE_DETECTION.");
        camera_set_start_facedetect(0);
        FreeFdmem();
	}

	return NO_ERROR;
}

status_t SprdCameraHardware::storeMetaDataInBuffers(bool enable)
{
    // FIXME:
    // metadata buffer mode can be turned on or off.
    // Spreadtrum needs to fix this.
    if (!enable) {
		LOGE("Non-metadata buffer mode is not supported!");
		mIsStoreMetaData = false;
		return INVALID_OPERATION;
    }
	
	if(NULL == mMetadataHeap) {
		if(NULL == (mMetadataHeap = mGetMemory_cb(-1, METADATA_SIZE, kPreviewBufferCount, NULL))) {
			LOGE("fail to alloc memory for the metadata for storeMetaDataInBuffers.");
			return INVALID_OPERATION;
		}
	}
	
	mIsStoreMetaData = true;

	return NO_ERROR;
}

status_t SprdCameraHardware::dump(int fd) const
{
	const size_t SIZE = 256;
	char buffer[SIZE];
	String8 result;
	const Vector<String16> args;

	// Dump internal primitives.
//	snprintf(buffer, 255, "SprdCameraHardware::dump: state (%s, %s, %s)\n", 
//				getCameraStateStr(getCameraState()), getCameraStateStr(getPreviewState()), getCameraStateStr(getCaptureState()));
	result.append(buffer);
	snprintf(buffer, 255, "preview width(%d) x height (%d)\n", mPreviewWidth, mPreviewHeight);
	result.append(buffer);
	snprintf(buffer, 255, "raw width(%d) x height (%d)\n", mRawWidth, mRawHeight);
	result.append(buffer);
	snprintf(buffer, 255, "preview frame size(%d), raw size (%d), jpeg size (%d) and jpeg max size (%d)\n", mPreviewHeapSize, mRawHeapSize, mJpegSize, mJpegHeapSize);
	result.append(buffer);
	write(fd, result.string(), result.size());
	mParameters.dump(fd, args);
	return NO_ERROR;
}

//////////////////////////////////////////////////////////////////////////////
//SprdCameraHardware: private functions
//////////////////////////////////////////////////////////////////////////////
const char* const SprdCameraHardware::getCameraStateStr(
        SprdCameraHardware::Sprd_camera_state s)
{
        static const char* states[] = {
#define STATE_STR(x) #x
            STATE_STR(SPRD_INIT),
            STATE_STR(SPRD_IDLE),
            STATE_STR(SPRD_ERROR),
            STATE_STR(SPRD_PREVIEW_IN_PROGRESS),
            STATE_STR(SPRD_WAITING_RAW),
            STATE_STR(SPRD_WAITING_JPEG),
            STATE_STR(SPRD_INTERNAL_PREVIEW_STOPPING),
            STATE_STR(SPRD_INTERNAL_CAPTURE_STOPPING),
            STATE_STR(SPRD_INTERNAL_PREVIEW_REQUESTED),
            STATE_STR(SPRD_INTERNAL_RAW_REQUESTED),
            STATE_STR(SPRD_INTERNAL_STOPPING),
#undef STATE_STR
        };
        return states[s];
}

void SprdCameraHardware::print_time()
{
#if PRINT_TIME
	struct timeval time;
	gettimeofday(&time, NULL);
	LOGV("time: %lld us.", time.tv_sec * 1000000LL + time.tv_usec);
#endif
}

// Called with mStateLock held!
bool SprdCameraHardware::setCameraDimensions()
{
	if (isPreviewing() || isCapturing()) {
		LOGE("setCameraDimensions: expecting state SPRD_IDLE, not %s, %s",
				getCameraStateStr(getPreviewState()), 
				getCameraStateStr(getCaptureState()));
		return false;
	}

	if (0 != camera_set_dimensions(mRawWidth, mRawHeight, mPreviewWidth, 
 								mPreviewHeight, NULL, NULL)) {
 		return false;
 	}

	return true;
}

void SprdCameraHardware::setCameraPreviewMode()
{
	if (isRecordingMode()) {
		SET_PARM(CAMERA_PARM_PREVIEW_MODE, CAMERA_PREVIEW_MODE_MOVIE);
	} else {
		SET_PARM(CAMERA_PARM_PREVIEW_MODE, CAMERA_PREVIEW_MODE_SNAPSHOT);
	}	
}

bool SprdCameraHardware::isRecordingMode()
{
	return mRecordingMode;
}

void SprdCameraHardware::setRecordingMode(bool enable)
{
	mRecordingMode = enable;
}

void SprdCameraHardware::setCameraState(Sprd_camera_state state, state_owner owner)
{

	LOGV("setCameraState:state: %s, owner: %d", getCameraStateStr(state), owner);
	Mutex::Autolock stateLock(&mStateLock);	
	
	switch (state) {
	//camera state
	case SPRD_INIT:
		mCameraState.camera_state = SPRD_INIT;
		mCameraState.preview_state = SPRD_IDLE;
		mCameraState.capture_state = SPRD_IDLE;
		break;

	case SPRD_IDLE:
		switch (owner) {
		case STATE_CAMERA:
			mCameraState.camera_state = SPRD_IDLE;
			break;

		case STATE_PREVIEW:
			mCameraState.preview_state = SPRD_IDLE;
			break;

		case STATE_CAPTURE:
			mCameraState.capture_state = SPRD_IDLE;
			break;
		}
		break;

	case SPRD_INTERNAL_STOPPING:
		mCameraState.camera_state = SPRD_INTERNAL_STOPPING;
		break;				

	case SPRD_ERROR:
		switch (owner) {
		case STATE_CAMERA:
			mCameraState.camera_state = SPRD_ERROR;
			break;

		case STATE_PREVIEW:
			mCameraState.preview_state = SPRD_ERROR;
			break;

		case STATE_CAPTURE:
			mCameraState.capture_state = SPRD_ERROR;
			break;
		}		
		break;

	//preview state
	case SPRD_PREVIEW_IN_PROGRESS:
		mCameraState.preview_state = SPRD_PREVIEW_IN_PROGRESS;
		break;

	case SPRD_INTERNAL_PREVIEW_STOPPING:
		mCameraState.preview_state = SPRD_INTERNAL_PREVIEW_STOPPING;
		break;

	case SPRD_INTERNAL_PREVIEW_REQUESTED:
		mCameraState.preview_state = SPRD_INTERNAL_PREVIEW_REQUESTED;
		break;

	//capture state
	case SPRD_INTERNAL_RAW_REQUESTED:
		mCameraState.capture_state = SPRD_INTERNAL_RAW_REQUESTED;
		break;		

	case SPRD_WAITING_RAW:
		mCameraState.capture_state = SPRD_WAITING_RAW;
		break;

	case SPRD_WAITING_JPEG:
		mCameraState.capture_state = SPRD_WAITING_JPEG;
		break;

	case SPRD_INTERNAL_CAPTURE_STOPPING:
		mCameraState.capture_state = SPRD_INTERNAL_CAPTURE_STOPPING;
		break;		

	default:
		LOGD("setCameraState: error");
		break;
	}

	LOGV("setCameraState:camera state = %s, preview state = %s, capture state = %s", getCameraStateStr(mCameraState.camera_state),
				getCameraStateStr(mCameraState.preview_state), getCameraStateStr(mCameraState.capture_state));
}

SprdCameraHardware::Sprd_camera_state SprdCameraHardware::getCameraState()
{
	LOGV("getCameraState: %s", getCameraStateStr(mCameraState.camera_state));
	return mCameraState.camera_state;
}

SprdCameraHardware::Sprd_camera_state SprdCameraHardware::getPreviewState()
{
	LOGV("getPreviewState: %s", getCameraStateStr(mCameraState.preview_state));
	return mCameraState.preview_state;
}

SprdCameraHardware::Sprd_camera_state SprdCameraHardware::getCaptureState()
{
	LOGV("getCaptureState: %s", getCameraStateStr(mCameraState.capture_state));
	return mCameraState.capture_state;
}

bool SprdCameraHardware::isCameraInit()
{
	LOGV("isCameraInit: %s", getCameraStateStr(mCameraState.camera_state));
	return (SPRD_IDLE == mCameraState.camera_state);
}

bool SprdCameraHardware::isCameraIdle()
{	
	return (SPRD_IDLE == mCameraState.preview_state
			&& SPRD_IDLE == mCameraState.capture_state);
}

bool SprdCameraHardware::isPreviewing()
{
	LOGV("isPreviewing: %s", getCameraStateStr(mCameraState.preview_state));
	return (SPRD_PREVIEW_IN_PROGRESS == mCameraState.preview_state);
}

bool SprdCameraHardware::isCapturing()
{
	LOGV("isCapturing: %s", getCameraStateStr(mCameraState.capture_state));
#if 1
	return (SPRD_WAITING_RAW == mCameraState.capture_state 
			|| SPRD_WAITING_JPEG == mCameraState.capture_state);
#else
	return (SPRD_IDLE != mCameraState.capture_state);
#endif
}

bool SprdCameraHardware::WaitForCameraStart()
{
	Mutex::Autolock stateLock(&mStateLock);
	
    while(SPRD_IDLE != mCameraState.camera_state 
			&& SPRD_ERROR != mCameraState.camera_state) {
        LOGV("WaitForCameraStart: waiting for SPRD_IDLE");
        mStateWait.wait(mStateLock);
        LOGV("WaitForCameraStart: woke up");
    }

	return SPRD_IDLE == mCameraState.camera_state;
}

bool SprdCameraHardware::WaitForCameraStop()
{
	Mutex::Autolock stateLock(&mStateLock);
	
	if (SPRD_INTERNAL_STOPPING == mCameraState.camera_state)
	{
	    while(SPRD_INIT != mCameraState.camera_state 
				&& SPRD_ERROR != mCameraState.camera_state) {
	        LOGV("WaitForCameraStop: waiting for SPRD_IDLE");
	        mStateWait.wait(mStateLock);
	        LOGV("WaitForCameraStop: woke up");
	    }
	}

	return SPRD_INIT == mCameraState.camera_state;
}

bool SprdCameraHardware::WaitForPreviewStart()
{
	Mutex::Autolock stateLock(&mStateLock);	
	while(SPRD_PREVIEW_IN_PROGRESS != mCameraState.preview_state
		&& SPRD_ERROR != mCameraState.preview_state) {
		LOGV("WaitForPreviewStart: waiting for SPRD_PREVIEW_IN_PROGRESS");
		mStateWait.wait(mStateLock);
		LOGV("WaitForPreviewStart: woke up");
	}	

	return SPRD_PREVIEW_IN_PROGRESS == mCameraState.preview_state;
}

bool SprdCameraHardware::WaitForPreviewStop()
{
	Mutex::Autolock statelock(&mStateLock);
    while (SPRD_IDLE != mCameraState.preview_state  
			&& SPRD_ERROR != mCameraState.preview_state) {
		LOGV("WaitForPreviewStop: waiting for SPRD_IDLE");
		mStateWait.wait(mStateLock);
		LOGV("WaitForPreviewStop: woke up");
    }

	return SPRD_IDLE == mCameraState.preview_state;
}

bool SprdCameraHardware::WaitForCaptureStart()
{
	Mutex::Autolock stateLock(&mStateLock);
	
    // It's possible for the YUV callback as well as the JPEG callbacks
    // to be invoked before we even make it here, so we check for all
    // possible result states from takePicture.
	while (SPRD_WAITING_RAW != mCameraState.capture_state  
		 && SPRD_WAITING_JPEG != mCameraState.capture_state
		 && SPRD_IDLE != mCameraState.capture_state 
		 && SPRD_ERROR != mCameraState.camera_state) {
		LOGV("WaitForCaptureStart: waiting for SPRD_WAITING_RAW or SPRD_WAITING_JPEG");
		mStateWait.wait(mStateLock);
		LOGV("WaitForCaptureStart: woke up, state is %s",
				getCameraStateStr(mCameraState.capture_state));
	}	

	return (SPRD_WAITING_RAW == mCameraState.capture_state 
			|| SPRD_WAITING_JPEG == mCameraState.capture_state 
			|| SPRD_IDLE == mCameraState.capture_state);
}

bool SprdCameraHardware::WaitForCaptureDone()
{	
	Mutex::Autolock stateLock(&mStateLock);
	while (SPRD_IDLE != mCameraState.capture_state
		 && SPRD_ERROR != mCameraState.capture_state) {
		LOGV("WaitForCaptureDone: waiting for SPRD_IDLE");
		mStateWait.wait(mStateLock);
		LOGV("WaitForCaptureDone: woke up");
	}

	return SPRD_IDLE == mCameraState.capture_state;	
}

// Called with mLock held!
bool SprdCameraHardware::startCameraIfNecessary()
{
	if (!isCameraInit()) {
		LOGV("waiting for camera_init to initialize.startCameraIfNecessary");
		if(CAMERA_SUCCESS != camera_init(mCameraId)){
			setCameraState(SPRD_INIT, STATE_CAMERA);
			LOGE("CameraIfNecessary: fail to camera_init().");
			return false;
		}

		LOGV("waiting for camera_start.g_camera_id: %d.", mCameraId);
		if(CAMERA_SUCCESS != camera_start(camera_cb, this, mPreviewHeight, mPreviewWidth)){
			setCameraState(SPRD_ERROR, STATE_CAMERA);
			LOGE("CameraIfNecessary: fail to camera_start().");
			return false;
		}
		
		LOGV("OK to camera_start.");
		WaitForCameraStart();
		
		LOGV("init camera: initializing parameters");
	}
	else 
		LOGV("camera hardware has been started already");
	
	return true;
}





sprd_camera_memory_t* SprdCameraHardware::GetPmem(int buf_size, int num_bufs)
{	
	sprd_camera_memory_t *memory = (sprd_camera_memory_t *)malloc(sizeof(*memory));
	if(NULL == memory) {
		LOGE("wxz: Fail to GetPmem, memory is NULL.");
		return NULL;
	}
	
	camera_memory_t *camera_memory;
	int paddr, psize;
	int order = 0, acc = buf_size *num_bufs ;
	acc = camera_get_size_align_page(acc);
	MemoryHeapIon *pHeapIon = new MemoryHeapIon("/dev/ion", acc , MemoryHeapBase::NO_CACHING, ION_HEAP_CARVEOUT_MASK);

	camera_memory = mGetMemory_cb(pHeapIon->getHeapID(), acc/num_bufs, num_bufs, NULL);

	if(NULL == camera_memory) {
		goto getpmem_end;
	}
	
	if(0xFFFFFFFF == (uint32_t)camera_memory->data) {
		camera_memory = NULL;
		LOGE("Fail to GetPmem().");
		goto getpmem_end;
	}
	
	pHeapIon->get_phy_addr_from_ion(&paddr, &psize);
	memory->ion_heap = pHeapIon;
	memory->camera_memory = camera_memory;
	memory->phys_addr = paddr;
	memory->phys_size = psize;
	memory->handle = camera_memory->handle;
	//memory->data = camera_memory->data;
	memory->data = pHeapIon->getBase();

	LOGV("GetPmem: phys_addr 0x%x, data: 0x%x, size: 0x%x, phys_size: 0x%x.",
			memory->phys_addr, (uint32_t)camera_memory->data,
			camera_memory->size, memory->phys_size);

getpmem_end:
	return memory;
}

void SprdCameraHardware::FreePmem(sprd_camera_memory_t* memory)
{
	if(memory){
		if(memory->camera_memory->release){
			memory->camera_memory->release(memory->camera_memory);
			memory->camera_memory = NULL;
		} 
		else {
			LOGE("fail to FreePmem: NULL is camera_memory->release.");
		}

		if(memory->ion_heap) {
			delete memory->ion_heap;
			memory->ion_heap = NULL;
		}

		free(memory);
	} 
	else {
		LOGV("FreePmem: NULL");
	}
}

void SprdCameraHardware::setFdmem(uint32_t size)
{	
	if (mFDAddr) {
		free((void*)mFDAddr);
		mFDAddr = 0;		
	}
		
	uint32_t addr = (uint32_t)malloc(size);
	mFDAddr = addr;
	camera_set_fd_mem(0, addr, size);
}

void SprdCameraHardware::FreeFdmem(void)
{
	if (mFDAddr) {
		camera_set_fd_mem(0,0,0);
		free((void*)mFDAddr);
		mFDAddr = 0;
	}
}

//mPreviewHeapSize must be set before this function be called
bool SprdCameraHardware::allocatePreviewMem()
{
	int i;
	uint32_t buffer_size = camera_get_size_align_page(mPreviewHeapSize);
	mPreviewHeapNum = kPreviewBufferCount;
	
	if(camera_get_rot_set())
	{
		/* allocate more buffer for rotation */
		mPreviewHeapNum += kPreviewRotBufferCount;
		LOGV("initPreview: rotation, increase buffer: %d \n", mPreviewHeapNum);
	}

	mPreviewHeapArray = (sprd_camera_memory_t**)malloc(mPreviewHeapNum * sizeof(sprd_camera_memory_t*));
	if (mPreviewHeapArray == NULL)
	{
	        return false;
	}
	for (i=0; i<mPreviewHeapNum; i++)
	{
		sprd_camera_memory_t* mPreviewHeap = GetPmem(buffer_size, 1);
		if(NULL == mPreviewHeap)
			return false;

		if(NULL == mPreviewHeap->handle) {
			LOGE("Fail to GetPmem mPreviewHeap. buffer_size: 0x%x.", buffer_size);
			return false;
		}

		if(mPreviewHeap->phys_addr & 0xFF) {
			LOGE("error: the mPreviewHeap is not 256 bytes aligned.");
			return false;
		}

                mPreviewHeapArray[i] = mPreviewHeap;
                mPreviewHeapArray_phy[i] = mPreviewHeap->phys_addr;
                mPreviewHeapArray_vir[i] = mPreviewHeap->data;
	}

	return true;
}

uint32_t SprdCameraHardware::getRedisplayMem()
{
	uint32_t buffer_size = camera_get_size_align_page(mPreviewWidth*mPreviewHeight*3/2);

	mReDisplayHeap = GetPmem(buffer_size, 1);

	if(NULL == mReDisplayHeap)
		return 0;

	if(NULL == mReDisplayHeap->handle) {
		LOGE("Fail to GetPmem mReDisplayHeap. buffer_size: 0x%x.", buffer_size);
		return 0;
	}

	if(mReDisplayHeap->phys_addr & 0xFF) {
		LOGE("error: the mReDisplayHeap is not 256 bytes aligned.");
		return 0;
	}
	return mReDisplayHeap->phys_addr;
}

void SprdCameraHardware::FreeReDisplayMem()
{
	LOGI("free redisplay mem.");
	FreePmem(mReDisplayHeap);
	mReDisplayHeap = NULL;
}

void SprdCameraHardware::freePreviewMem()
{
	int i;
	FreeFdmem();

	for (i=0; i<mPreviewHeapNum; i++)
	{
		FreePmem(mPreviewHeapArray[i]);
		mPreviewHeapArray[i] = NULL;
	}
	if (mPreviewHeapArray != NULL)
	{
		free(mPreviewHeapArray);
		mPreviewHeapArray = NULL;
	}

	mPreviewHeapSize = 0;
	mPreviewHeapNum = 0;
}

bool SprdCameraHardware::initPreview()
{
	uint32_t page_size, buffer_size;
	uint32_t preview_buff_cnt = kPreviewBufferCount;

	if (!startCameraIfNecessary())
		return false;

	setCameraPreviewMode();

	// Tell libqcamera what the preview and raw dimensions are.  We
	// call this method even if the preview dimensions have not changed,
	// because the picture ones may have.
	// NOTE: if this errors out, mCameraState != SPRD_IDLE, which will be
	//       checked by the caller of this method.
	if (!setCameraDimensions()) {
		LOGE("initPreview: setCameraDimensions failed");
		return false;
	}
	
	LOGV("initPreview: preview size=%dx%d", mPreviewWidth, mPreviewHeight);

	camerea_set_preview_format(mPreviewFormat);

	switch (mPreviewFormat) {
	case 0:
		case 1:	//yuv420
		mPreviewHeapSize = mPreviewWidth * mPreviewHeight * 3 / 2;
		break;

	default:
		return false;
	}
	
	if (!allocatePreviewMem()) {
		freePreviewMem();
		return false;
	}

	if (camera_set_preview_mem((uint32_t)mPreviewHeapArray_phy,
								(uint32_t)mPreviewHeapArray_vir,
								camera_get_size_align_page(mPreviewHeapSize),
								(uint32_t)mPreviewHeapNum))
		return false;
	
	setPreviewFreq();

	return true;
}

void SprdCameraHardware::deinitPreview()
{
	freePreviewMem();
	camera_set_preview_mem(0, 0, 0, 0);
	restoreFreq();
}

//mJpegHeapSize/mRawHeapSize/mMiscHeapSize must be set before this function being called
bool SprdCameraHardware::allocateCaptureMem(bool initJpegHeap)
{	
	uint32_t buffer_size = 0;
	
	LOGV("allocateCaptureMem, mJpegHeapSize = %d, mRawHeapSize = %d, mMiscHeapSize %d",
			mJpegHeapSize, mRawHeapSize, mMiscHeapSize);

	buffer_size = camera_get_size_align_page(mRawHeapSize);
	LOGV("allocateCaptureMem:mRawHeap align size = %d . count %d ",buffer_size, kRawBufferCount);

	mRawHeap = GetPmem(buffer_size, kRawBufferCount);
	if(NULL == mRawHeap)
		goto allocate_capture_mem_failed;
	
	if(NULL == mRawHeap->handle){
		LOGE("allocateCaptureMem: Fail to GetPmem mRawHeap.");
		goto allocate_capture_mem_failed;
	}
	
	if (mMiscHeapSize > 0) {
		buffer_size = camera_get_size_align_page(mMiscHeapSize);
		mMiscHeap = GetPmem(buffer_size, kRawBufferCount);
		if(NULL == mMiscHeap) {
			goto allocate_capture_mem_failed;
		}
		
		if(NULL == mMiscHeap->handle){
			LOGE("allocateCaptureMem: Fail to GetPmem mMiscHeap.");
			goto allocate_capture_mem_failed;
		}
	}

	if (initJpegHeap) {		
		// ???
		mJpegHeap = NULL;		
		
		buffer_size = camera_get_size_align_page(mJpegHeapSize);		
		mJpegHeap = new AshmemPool(buffer_size,
		                           kJpegBufferCount,
		                           0,
		                           0,
		                           "jpeg");
		
		if (!mJpegHeap->initialized()) {
			LOGE("allocateCaptureMem: initializing mJpegHeap failed.");
			goto allocate_capture_mem_failed;
		}
		
		LOGV("allocateCaptureMem: initJpeg success");
	}
	
	LOGV("allocateCaptureMem: X");
	return true;

allocate_capture_mem_failed:
	freeCaptureMem();
	
	mJpegHeap = NULL;
	mJpegHeapSize = 0;
	
	return false;
}

void SprdCameraHardware::freeCaptureMem()
{
	FreePmem(mRawHeap);
	mRawHeap = NULL;
	mRawHeapSize = 0;
	
	FreePmem(mMiscHeap);
	mMiscHeap = NULL;
	mMiscHeapSize = 0;

	//mJpegHeap = NULL;
}


// Called with mLock held!
bool SprdCameraHardware::initCapture(bool initJpegHeap)
{
	uint32_t local_width = 0, local_height = 0;
	uint32_t mem_size0 = 0, mem_size1 = 0;

	LOGV("initCapture E, %d", initJpegHeap);

	if(!startCameraIfNecessary())
		return false;

    camera_set_dimensions(mRawWidth,
                         mRawHeight,
                         mPreviewWidth,
                         mPreviewHeight,
                         NULL,
                         NULL);	

	if (camera_capture_max_img_size(&local_width, &local_height))
		return false;

	if (camera_capture_get_buffer_size(mCameraId, local_width, local_height, &mem_size0, &mem_size1))
		return false;

	mRawHeapSize = mem_size0;
	mMiscHeapSize = mem_size1;
	mJpegHeapSize = mRawHeapSize;
	mJpegHeap = NULL;

	if (!allocateCaptureMem(initJpegHeap)) {
		return false;
	}

	if (NULL != mMiscHeap) {
		if (camera_set_capture_mem(0,
								(uint32_t)mRawHeap->phys_addr,
								(uint32_t)mRawHeap->data,
								(uint32_t)mRawHeap->phys_size,
								(uint32_t)mMiscHeap->phys_addr,
								(uint32_t)mMiscHeap->data,
								(uint32_t)mMiscHeap->phys_size))
			return false;
	} else {
		if (camera_set_capture_mem(0,
								(uint32_t)mRawHeap->phys_addr,
								(uint32_t)mRawHeap->data,
								(uint32_t)mRawHeap->phys_size,
								0,
								0,
								0))
			return false;		
	}
	
	LOGV("initCapture X success");
	return true;
}

void SprdCameraHardware::deinitCapture()
{
	camera_set_capture_mem(0, 0, 0, 0, 0, 0, 0);							
	freeCaptureMem();	
}

void SprdCameraHardware::changeEmcFreq(char flag)
{
	int fp_emc_freq = -1;
	fp_emc_freq = open("/sys/emc/emc_freq", O_WRONLY);
	
	if (fp_emc_freq > 0) {
		write(fp_emc_freq, &flag, sizeof(flag));
		close(fp_emc_freq);
		LOGV("changeEmcFreq: %c \n", flag);
	} else {
		LOGV("changeEmcFreq: changeEmcFreq failed \n");
	}
}

void SprdCameraHardware::setPreviewFreq()
{
	
}

void SprdCameraHardware::restoreFreq()
{

}

status_t SprdCameraHardware::startPreviewInternal(bool isRecording)
{
	takepicture_mode mode = getCaptureMode();
	LOGV("startPreview E isRecording=%d.",isRecording);

	if (isPreviewing()) {
		LOGE("startPreview is already in progress, doing nothing.");
		LOGV("mLock:startPreview E.\n");
		setRecordingMode(isRecording);
		setCameraPreviewMode();
		return NO_ERROR;		
	}
	//to do it
/*	if (!iSZslMode()) {
		// We check for these two states explicitly because it is possible
		// for startPreview() to be called in response to a raw or JPEG
		// callback, but before we've updated the state from SPRD_WAITING_RAW
		// or SPRD_WAITING_JPEG to SPRD_IDLE.  This is because in camera_cb(),
		// we update the state *after* we've made the callback.  See that
		// function for an explanation.
		if (isCapturing()) {
			WaitForCaptureDone();
		}

	    if (isCapturing() || isPreviewing()) {
			//LOGE("startPreview X Camera state is %s, expecting SPRD_IDLE!",
			//getCameraStateStr(mCaptureState));
			LOGV("mLock:startPreview E.\n");
			return INVALID_OPERATION;
	    }
	}*/
	setRecordingMode(isRecording);

	if (!initPreview()) {
		LOGE("startPreview X initPreview failed.  Not starting preview.");
		LOGV("mLock:startPreview E.\n");
		deinitPreview();
		return UNKNOWN_ERROR;
	}
	if (iSZslMode()) {
	    if (!initCapture(mData_cb != NULL)) {
			deinitCapture();
			LOGE("initCapture failed. Not taking picture.");
			return UNKNOWN_ERROR;
	    }
	}

	setCameraState(SPRD_INTERNAL_PREVIEW_REQUESTED, STATE_PREVIEW);
	camera_ret_code_type qret = camera_start_preview(camera_cb, this,mode);
	LOGV("camera_start_preview X");
	if (qret != CAMERA_SUCCESS) {
		LOGE("startPreview failed: sensor error.");
		setCameraState(SPRD_ERROR, STATE_PREVIEW);
		deinitPreview();
		return UNKNOWN_ERROR;
	}

	bool result = WaitForPreviewStart();

	LOGV("startPreview X,mRecordingMode=%d.",isRecordingMode());
	
	return result ? NO_ERROR : UNKNOWN_ERROR;
}

void SprdCameraHardware::stopPreviewInternal()
{
    nsecs_t start_timestamp = systemTime();
    nsecs_t end_timestamp;
	LOGV("stopPreviewInternal E");

    if (!isPreviewing()) {
		LOGE("Preview not in progress!");
		return;
    }		

	setCameraState(SPRD_INTERNAL_PREVIEW_STOPPING, STATE_PREVIEW);

	if(CAMERA_SUCCESS != camera_stop_preview()) {
		setCameraState(SPRD_ERROR, STATE_PREVIEW);
		deinitPreview();
		LOGE("stopPreviewInternal: fail to camera_stop_preview().");
		return;
	}

	WaitForPreviewStop();	
	deinitPreview();
	deinitCapture();
    end_timestamp = systemTime();
    LOGE("Stop Preview Time:%lld(ms).",(end_timestamp - start_timestamp)/1000000);
    LOGV("stopPreviewInternal: X Preview has stopped.");	
}

takepicture_mode SprdCameraHardware::getCaptureMode()
{
	if (1 == mParameters.getInt("hdr")) {
        mCaptureMode = CAMERA_HDR_MODE;
    } else if ((1 == mParameters.getInt("zsl"))&&(1 != mParameters.getInt("capture-mode"))) {
		mCaptureMode = CAMERA_ZSL_CONTINUE_SHOT_MODE;
    } else if ((1 != mParameters.getInt("zsl"))&&(1 != mParameters.getInt("capture-mode"))) {
		mCaptureMode = CAMERA_NORMAL_CONTINUE_SHOT_MODE;
    } else if (1 == mParameters.getInt("zsl")) {
		mCaptureMode = CAMERA_ZSL_MODE;
    } else if (1 != mParameters.getInt("zsl")) {
		mCaptureMode = CAMERA_NORMAL_MODE;
    } else {
		mCaptureMode = mCaptureMode;
    }
	if (1 == mCaptureRawMode) {
		mCaptureMode = CAMERA_RAW_MODE;
	}
/*	mCaptureMode = CAMERA_HDR_MODE;*/
	LOGI("cap mode %d.\n", mCaptureMode);

	return mCaptureMode;
}

bool SprdCameraHardware::iSDisplayCaptureFrame()
{
	bool ret = true;

	if ((CAMERA_ZSL_MODE == mCaptureMode)
		|| (CAMERA_ZSL_CONTINUE_SHOT_MODE == mCaptureMode)) {
		ret = false;
	}
	LOGI("dislapy capture flag is %d.",ret);
	return ret;
}

bool SprdCameraHardware::iSZslMode()
{
	bool ret = true;
	if ((CAMERA_ZSL_MODE != mCaptureMode)
		&& (CAMERA_ZSL_CONTINUE_SHOT_MODE != mCaptureMode)) {
		ret = false;
	}
	return ret;
}


status_t SprdCameraHardware::cancelPictureInternal()
{
	bool result = true;
	
	LOGV("cancelPictureInternal: E, state = %s", getCameraStateStr(getCaptureState()));
	
	switch (getCaptureState()) {
	case SPRD_INTERNAL_RAW_REQUESTED:
	case SPRD_WAITING_RAW:
	case SPRD_WAITING_JPEG:
		LOGD("camera state is %s, stopping picture.", getCameraStateStr(getCaptureState()));
		setCameraState(SPRD_INTERNAL_CAPTURE_STOPPING, STATE_CAPTURE);
		if (0 != camera_stop_capture()) {
			LOGE("cancelPictureInternal: camera_stop_capture failed!");
			return UNKNOWN_ERROR;
		}
		
		result = WaitForCaptureDone();
		break;
	
	default:
		LOGV("not taking a picture (state %s)", getCameraStateStr(getCaptureState()));
		break;
	}
	
	LOGV("cancelPictureInternal: X");
	return result ? NO_ERROR : UNKNOWN_ERROR;
}

status_t SprdCameraHardware::initDefaultParameters()
{
	LOGV("initDefaultParameters E");
	SprdCameraParameters p;

	SprdCameraParameters::ConfigType config = (1 == mCameraId) 
			? SprdCameraParameters::kFrontCameraConfig
			: SprdCameraParameters::kBackCameraConfig;
	
	p.setDefault(config);
	
    if (setParameters(p) != NO_ERROR) {
		LOGE("Failed to set default parameters?!");
		return UNKNOWN_ERROR;
    }
	
    LOGV("initDefaultParameters X.");
    return NO_ERROR;
}

status_t SprdCameraHardware::setCameraParameters()
{
	LOGV("setCameraParameters: E");

	// Because libqcamera is broken, for the camera_set_parm() calls
	// SprdCameraHardware camera_cb() is called synchronously,
	// so we cannot wait on a state change.  Also, we have to unlock
	// the mStateLock, because camera_cb() acquires it.

	if(true != startCameraIfNecessary())
	    return UNKNOWN_ERROR;

	//wxz20120316: check the value of preview-fps-range. //CR168435
	int min,max;
	mParameters.getPreviewFpsRange(&min, &max);
	if((min > max) || (min < 0) || (max < 0)){
		LOGE("Error to FPS range: mix: %d, max: %d.", min, max);
		return UNKNOWN_ERROR;
	}
	LOGV("setCameraParameters: preview fps range: min = %d, max = %d", min, max);

	//check preview size
	int w,h;
	mParameters.getPreviewSize(&w, &h);
	if((w < 0) || (h < 0)){
		mParameters.setPreviewSize(640, 480);
		return UNKNOWN_ERROR;
	}
	LOGV("setCameraParameters: preview size: wxmax", w, h);

	// Default Rotation - none
	int rotation = mParameters.getInt("rotation");

    // Rotation may be negative, but may not be -1, because it has to be a
    // multiple of 90.  That's why we can still interpret -1 as an error,
    if (rotation == -1) {
        LOGV("rotation not specified or is invalid, defaulting to 0");
        rotation = 0;
    }
    else if (rotation % 90) {
        LOGV("rotation %d is not a multiple of 90 degrees!  Defaulting to zero.",
             rotation);
        rotation = 0;
    }
    else {
        // normalize to [0 - 270] degrees
        rotation %= 360;
        if (rotation < 0) rotation += 360;
    }
	
    SET_PARM(CAMERA_PARM_ENCODE_ROTATION, rotation);
    if (1 == mParameters.getInt("sensororientation")){
    	SET_PARM(CAMERA_PARM_ORIENTATION, 1); //for portrait
	} 
	else {
    	SET_PARM(CAMERA_PARM_ORIENTATION, 0); //for landscape
	}
	
    rotation = mParameters.getInt("sensorrotation");
    if (-1 == rotation)		
        rotation = 0;
	
    SET_PARM(CAMERA_PARM_SENSOR_ROTATION, rotation);
    SET_PARM(CAMERA_PARM_SHOT_NUM, mParameters.getInt("capture-mode"));
/*	if (1 == mParameters.getInt("hdr")) {
		SET_PARM(CAMERA_PARM_SHOT_NUM, HDR_CAP_NUM);
	}*/
	
	int is_mirror = (mCameraId == 1) ? 1 : 0;
	int ret = 0;

	SprdCameraParameters::Size preview_size = {0};
	SprdCameraParameters::Rect preview_rect = {0};
	int area[4 * SprdCameraParameters::kFocusZoneMax + 1] = {0};

	preview_size.width = mPreviewWidth;
	preview_size.height = mPreviewHeight;

	ret = camera_get_preview_rect(&preview_rect.x, &preview_rect.y, 
								&preview_rect.width, &preview_rect.height);
	if(ret) {
		LOGV("coordinate_convert: camera_get_preview_rect failed, return \n");
		return UNKNOWN_ERROR;
	}	
	
	mParameters.getFocusAreas(&area[1], &area[0], &preview_size, &preview_rect,
					kCameraInfo[mCameraId].orientation, is_mirror);
	SET_PARM(CAMERA_PARM_FOCUS_RECT, (int32_t)area);

	if (0 == mCameraId) {			
		SET_PARM(CAMERA_PARM_AF_MODE, mParameters.getFocusMode());
#ifndef CONFIG_CAMERA_788		
		SET_PARM(CAMERA_PARM_FLASH, mParameters.getFlashMode());
#endif		
	}

    SET_PARM(CAMERA_PARAM_SLOWMOTION, mParameters.getSlowmotion());
	SET_PARM(CAMERA_PARM_WB, mParameters.getWhiteBalance());
	SET_PARM(CAMERA_PARM_CAMERA_ID, mParameters.getCameraId());
	SET_PARM(CAMERA_PARM_JPEGCOMP, mParameters.getJpegQuality());
	SET_PARM(CAMERA_PARM_THUMBCOMP, mParameters.getJpegThumbnailQuality());
	SET_PARM(CAMERA_PARM_EFFECT, mParameters.getEffect());
	SET_PARM(CAMERA_PARM_SCENE_MODE, mParameters.getSceneMode());

	mZoomLevel = mParameters.getZoom();
	SET_PARM(CAMERA_PARM_ZOOM, mZoomLevel);
	SET_PARM(CAMERA_PARM_BRIGHTNESS, mParameters.getBrightness());
	SET_PARM(CAMERA_PARM_SHARPNESS, mParameters.getSharpness());
	SET_PARM(CAMERA_PARM_CONTRAST, mParameters.getContrast());
	SET_PARM(CAMERA_PARM_SATURATION, mParameters.getSaturation());
	SET_PARM(CAMERA_PARM_EXPOSURE_COMPENSATION, mParameters.getExposureCompensation());
	SET_PARM(CAMERA_PARM_ANTIBANDING, mParameters.getAntiBanding());
	SET_PARM(CAMERA_PARM_ISO, mParameters.getIso());
	SET_PARM(CAMERA_PARM_DCDV_MODE, mParameters.getRecordingHint());

	int ns_mode = mParameters.getInt("nightshot-mode");
	if (ns_mode < 0) ns_mode = 0;
	SET_PARM(CAMERA_PARM_NIGHTSHOT_MODE, ns_mode);

	int luma_adaptation = mParameters.getInt("luma-adaptation");
	if (luma_adaptation < 0) luma_adaptation = 0;
	SET_PARM(CAMERA_PARM_LUMA_ADAPTATION, luma_adaptation);

	double focal_len = atof(mParameters.get("focal-length")) * 1000;
	SET_PARM(CAMERA_PARM_FOCAL_LENGTH,  (int32_t)focal_len);

	int th_w, th_h, th_q;
	th_w = mParameters.getInt("jpeg-thumbnail-width");
	if (th_w < 0) LOGW("property jpeg-thumbnail-width not specified");

	th_h = mParameters.getInt("jpeg-thumbnail-height");
	if (th_h < 0) LOGW("property jpeg-thumbnail-height not specified");

	th_q = mParameters.getInt("jpeg-thumbnail-quality");
	if (th_q < 0) LOGW("property jpeg-thumbnail-quality not specified");

	if (th_w >= 0 && th_h >= 0 && th_q >= 0) {
		LOGI("setting thumbnail dimensions to %dx%d, quality %d", th_w, th_h, th_q);
		
		int ret = camera_set_thumbnail_properties(th_w, th_h, th_q);
		
		if (ret != CAMERA_SUCCESS) {
			LOGE("camera_set_thumbnail_properties returned %d", ret);
			}
	}

	camera_encode_properties_type encode_properties = {0};
	// Set Default JPEG encoding--this does not cause a callback
	encode_properties.quality = mParameters.getInt("jpeg-quality");
	
	if (encode_properties.quality < 0) {
		LOGW("JPEG-image quality is not specified "
		"or is negative, defaulting to %d",
		encode_properties.quality);
		encode_properties.quality = 100;
	}
	else LOGV("Setting JPEG-image quality to %d",
			encode_properties.quality);
	
	encode_properties.format = CAMERA_JPEG;
	encode_properties.file_size = 0x0;
	camera_set_encode_properties(&encode_properties);

	LOGV("setCameraParameters: X");
	return NO_ERROR;	
}

void SprdCameraHardware::getPictureFormat(int * format)
{
	*format = mPictureFormat;
}

////////////////////////////////////////////////////////////////////////////
// message handler
/////////////////////////////////////////////////////////////////////////////
#define PARSE_LOCATION(what,type,fmt,desc) do {                                           \
                pt->what = 0;                                                              \
                const char *what##_str = mParameters.get("gps-"#what);                    \
                LOGV("%s: GPS PARM %s --> [%s]", __func__, "gps-"#what, what##_str); \
                if (what##_str) {                                                         \
                    type what = 0;                                                        \
                    if (sscanf(what##_str, fmt, &what) == 1)                              \
                        pt->what = what;                                                   \
                    else {                                                                \
                        LOGE("GPS " #what " %s could not"                                 \
                              " be parsed as a " #desc,                                   \
                              what##_str);                                                \
                        result = false;                                          \
                    }                                                                     \
                }                                                                         \
                else {                                                                    \
                    LOGW("%s: GPS " #what " not specified: "               \
                          "defaulting to zero in EXIF header.", __func__);                          \
                    result = false;                                              \
               }                                                                          \
            }while(0)
            
bool SprdCameraHardware::getCameraLocation(camera_position_type *pt)
{
	bool result = true;
	
	PARSE_LOCATION(timestamp, long, "%ld", "long");
	if (0 == pt->timestamp) 
		pt->timestamp = time(NULL);
	
	PARSE_LOCATION(altitude, double, "%lf", "double float");
	PARSE_LOCATION(latitude, double, "%lf", "double float");
	PARSE_LOCATION(longitude, double, "%lf", "double float");
	
	pt->process_method = mParameters.get("gps-processing-method");

	LOGV("%s: setting image location result %d,  ALT %lf LAT %lf LON %lf",
			__func__, result, pt->altitude, pt->latitude, pt->longitude);
			
	return result;
}

bool SprdCameraHardware::displayOneFrame(uint32_t width, uint32_t height, uint32_t phy_addr, char *virtual_addr)
{
	if (!mPreviewWindow || !mGrallocHal || 0 == phy_addr) {
		return false;
	}

	LOGV("%s: size = %dx%d, addr = %d", __func__, width, height, phy_addr);
	
	buffer_handle_t 	*buf_handle = NULL;
	int 				stride = 0;
	void 				*vaddr = NULL;
	int					ret = 0;

	ret = mPreviewWindow->dequeue_buffer(mPreviewWindow, &buf_handle, &stride);
	if (0 != ret) {
		LOGE("%s: failed to dequeue gralloc buffer!", __func__);
		return false;
	}

	ret = mGrallocHal->lock(mGrallocHal, *buf_handle, GRALLOC_USAGE_SW_WRITE_OFTEN,
							0, 0, width, height, &vaddr);
	//if the ret is 0 and the vaddr is NULL, whether the unlock should be called?

	if (0 != ret || NULL == vaddr)
	{
		LOGE("%s: failed to lock gralloc buffer", __func__);
		return false;
	}

	//ret = camera_copy_data_virtual(width, height, phy_addr, (uint32_t)vaddr);
	memcpy(vaddr, virtual_addr, width*height*3/2);
	mGrallocHal->unlock(mGrallocHal, *buf_handle);
	
	if(0 != ret) {
		LOGE("%s: camera_copy_data_virtual() failed.", __func__);
		return false;
	}

	ret = mPreviewWindow->enqueue_buffer(mPreviewWindow, buf_handle);
	if(0 != ret) {
		LOGE("%s: enqueue_buffer() failed.", __func__);
		return false;
	}		
	
	return true;
}

void SprdCameraHardware::receivePreviewFDFrame(camera_frame_type *frame)
{
    Mutex::Autolock cbLock(&mPreviewCbLock);

	if (NULL == frame) {
		LOGE("receivePreviewFDFrame: invalid frame pointer");
		return;
	}	
			
    ssize_t offset = frame->buf_id;
    camera_frame_metadata_t metadata;
    camera_face_t face_info[FACE_DETECT_NUM];
    uint32_t k = 0;

   /* if(1 == mParameters.getInt("smile-snap-mode")){*/
    LOGV("receive face_num %d.",frame->face_num);
    metadata.number_of_faces = frame->face_num;
    if((0 != frame->face_num)&&(frame->face_num<=FACE_DETECT_NUM)) {
       for(k=0 ; k<frame->face_num ; k++) {
            face_info[k].id = k;
            face_info[k].rect[0] = (frame->face_ptr->sx*2000/mPreviewWidth)-1000;
            face_info[k].rect[1] = (frame->face_ptr->sy*2000/mPreviewHeight)-1000;
            face_info[k].rect[2] = (frame->face_ptr->ex*2000/mPreviewWidth)-1000;
            face_info[k].rect[3] = (frame->face_ptr->ey*2000/mPreviewHeight)-1000;
            LOGV("smile level %d.\n",frame->face_ptr->smile_level);

            face_info[k].score = frame->face_ptr->smile_level;
            frame->face_ptr++;
       }
    }
	
    metadata.faces = &face_info[0];
    if(mMsgEnabled&CAMERA_MSG_PREVIEW_METADATA) {
        LOGV("smile capture msg is enabled.");
        if(camera_get_rot_set()) {
            mData_cb(CAMERA_MSG_PREVIEW_METADATA,mPreviewHeapArray[kPreviewBufferCount+offset]->camera_memory,0,&metadata,mUser);
        } else {
            mData_cb(CAMERA_MSG_PREVIEW_METADATA,mPreviewHeapArray[offset]->camera_memory,0,&metadata,mUser);
        }
    } else {
        LOGV("smile capture msg is disabled.");
    }
}

void SprdCameraHardware::receivePreviewFrame(camera_frame_type *frame)
{
	Mutex::Autolock cbLock(&mPreviewCbLock);

	if (NULL == frame) {
		LOGE("receivePreviewFrame: invalid frame pointer");
		return;
	}	
	
	ssize_t offset = frame->buf_id;
	camera_frame_metadata_t metadata;
	camera_face_t face_info[FACE_DETECT_NUM];
	uint32_t k = 0;

	int width, height, frame_size, offset_size;		

	width = frame->dx;/*mPreviewWidth;*/
	height = frame->dy;/*mPreviewHeight;*/
	LOGV("receivePreviewFrame: width=%d, height=%d \n",width, height);
    if (miSPreviewFirstFrame) {
        GET_END_TIME;
        GET_USE_TIME;
        LOGE("Launch Camera Time:%d(ms).",s_use_time);
        miSPreviewFirstFrame = 0;
    }

	if (!displayOneFrame(width, height, frame->buffer_phy_addr, (char *)frame->buf_Virt_Addr))
	{
		LOGE("%s: displayOneFrame failed!", __func__);
	}

#ifdef CONFIG_CAMERA_ISP
	send_img_data(2, mPreviewWidth, mPreviewHeight, (char *)frame->buf_Virt_Addr, frame->dx * frame->dy * 3 /2);
#endif

	if(mData_cb != NULL)
	{
		LOGV("receivePreviewFrame mMsgEnabled: 0x%x",mMsgEnabled);
		if (mMsgEnabled & CAMERA_MSG_PREVIEW_FRAME) {
			if(camera_get_rot_set()) {
				mData_cb(CAMERA_MSG_PREVIEW_FRAME,mPreviewHeapArray[kPreviewBufferCount+offset]->camera_memory,0,NULL,mUser);
			} else {
				mData_cb(CAMERA_MSG_PREVIEW_FRAME,mPreviewHeapArray[offset]->camera_memory,0,NULL,mUser);
			}
		}

		if ((mMsgEnabled & CAMERA_MSG_VIDEO_FRAME) && isRecordingMode()) {
			nsecs_t timestamp = systemTime();/*frame->timestamp;*/
			LOGV("test timestamp = %lld, mIsStoreMetaData: %d.",timestamp, mIsStoreMetaData);
			if(mIsStoreMetaData) {
				uint32_t *data = (uint32_t *)mMetadataHeap->data + offset * METADATA_SIZE / 4;
				*data++ = kMetadataBufferTypeCameraSource;
				*data++ = frame->buffer_phy_addr;
				*data = (uint32_t)frame->buf_Virt_Addr;
				mData_cb_timestamp(timestamp, CAMERA_MSG_VIDEO_FRAME, mMetadataHeap, offset, mUser);
			}
			else {
				//mData_cb_timestamp(timestamp, CAMERA_MSG_VIDEO_FRAME, mPreviewHeap->mBuffers[offset], mUser);
				if(camera_get_rot_set()) {
					mData_cb_timestamp(timestamp, CAMERA_MSG_VIDEO_FRAME, mPreviewHeapArray[kPreviewBufferCount+offset]->camera_memory, 0, mUser);
				} else {
					mData_cb_timestamp(timestamp, CAMERA_MSG_VIDEO_FRAME, mPreviewHeapArray[offset]->camera_memory, 0, mUser);
				}
			}
		//LOGV("receivePreviewFrame: record index: %d, offset: %x, size: %x, frame->buf_Virt_Addr: 0x%x.", offset, off, size, (uint32_t)frame->buf_Virt_Addr);
		}
		else {
			if(CAMERA_SUCCESS != camera_release_frame(offset)){
				LOGE("receivePreviewFrame: fail to camera_release_frame().offset: %d.", offset);
			}
		}
		// When we are doing preview but not recording, we need to
		// release every preview frame immediately so that the next
		// preview frame is delivered.  However, when we are recording
		// (whether or not we are also streaming the preview frames to
		// the screen), we have the user explicitly release a preview
		// frame via method releaseRecordingFrame().  In this way we
		// allow a video encoder which is potentially slower than the
		// preview stream to skip frames.  Note that we call
		// camera_release_frame() in this method because we first
		// need to check to see if mPreviewCallback != NULL, which
		// requires holding mCallbackLock.

	}
	else
		LOGE("receivePreviewFrame: mData_cb is null.");

}

//can not use callback lock?
void SprdCameraHardware::notifyShutter()
{
	LOGV("notifyShutter: E");
	print_time();

	LOGV("notifyShutter mMsgEnabled: 0x%x.", mMsgEnabled);

	if ((CAMERA_ZSL_CONTINUE_SHOT_MODE != mCaptureMode)
		&& (CAMERA_NORMAL_CONTINUE_SHOT_MODE != mCaptureMode)) {
		if (mMsgEnabled & CAMERA_MSG_SHUTTER)
			mNotify_cb(CAMERA_MSG_SHUTTER, 0, 0, mUser);
	} else {
			mNotify_cb(CAMERA_MSG_SHUTTER, 0, 0, mUser);
	}

	print_time();
	LOGV("notifyShutter: X");
}


// Pass the pre-LPM raw picture to raw picture callback.
// This method is called by a libqcamera thread, different from the one on
// which startPreview() or takePicture() are called.
void SprdCameraHardware::receiveRawPicture(camera_frame_type *frame)
{
	LOGV("receiveRawPicture: E");
	
	print_time();

	Mutex::Autolock cbLock(&mCaptureCbLock);

	if (NULL == frame) {
		LOGE("receiveRawPicture: invalid frame pointer");
		return;
	}		

	if(SPRD_INTERNAL_CAPTURE_STOPPING == getCaptureState()) {
		LOGV("receiveRawPicture: warning: capture state = SPRD_INTERNAL_CAPTURE_STOPPING, return \n");
		return;
	}

	void *vaddr = NULL;
	uint32_t phy_addr = 0;
	uint32_t width = mPreviewWidth;
	uint32_t height = mPreviewHeight;	
/* to do it
	if (iSDisplayCaptureFrame()) {
		phy_addr = getRedisplayMem();

		if(0 == phy_addr) {
			LOGE("%s: get review memory failed", __func__);
			goto callbackraw;
		}

		if( 0 != camera_get_data_redisplay(phy_addr, width, height, frame->buffer_phy_addr, 
										frame->buffer_uv_phy_addr, frame->dx, frame->dy)) {
			LOGE("%s: Fail to camera_get_data_redisplay.", __func__);
			FreeReDisplayMem();
			goto callbackraw;
		}

		if (!displayOneFrame(width, height, phy_addr, (char *)frame->buf_Virt_Addr))
		{
			LOGE("%s: displayOneFrame failed", __func__);
		}
		FreeReDisplayMem();
	}*/
callbackraw:
	if (mData_cb!= NULL) {
		// Find the offset within the heap of the current buffer.
		ssize_t offset = (uint32_t)frame->buf_Virt_Addr;
		offset -= (uint32_t)mRawHeap->data;
		ssize_t frame_size = 0;

		if(CAMERA_RGB565 == frame->format)
			frame_size = frame->dx * frame->dy * 2;        // for RGB565
		else if(CAMERA_YCBCR_4_2_2 == frame->format)
			frame_size = frame->dx * frame->dy * 2;        //for YUV422
		else if(CAMERA_YCBCR_4_2_0 == frame->format)
			frame_size = frame->dx * frame->dy * 3 / 2;          //for YUV420
		else
			frame_size = frame->dx * frame->dy * 2;
		
		if (offset + frame_size <= (ssize_t)mRawHeap->phys_size) {
			offset /= frame_size;
			
			LOGV("mMsgEnabled: 0x%x, offset: %d.",mMsgEnabled, (uint32_t)offset);
			
			if (mMsgEnabled & CAMERA_MSG_RAW_IMAGE) {
				mData_cb(CAMERA_MSG_RAW_IMAGE, mRawHeap->camera_memory, offset, NULL, mUser);
			}
			
			if(mMsgEnabled & CAMERA_MSG_RAW_IMAGE_NOTIFY) {
				LOGV("mMsgEnabled & CAMERA_MSG_RAW_IMAGE_NOTIFY");
				mNotify_cb(CAMERA_MSG_RAW_IMAGE_NOTIFY, 0,0,mUser);
			}
		}
		else 
			LOGE("receiveRawPicture: virtual address %p is out of range!", frame->buf_Virt_Addr);
	}
	else
		LOGV("Raw-picture callback was canceled--skipping.");
	
	print_time();
	LOGV("receiveRawPicture: X");
}
    
// Encode the post-LPM raw picture.

// This method is called by a libqcamera thread, different from the one on
// which startPreview() or takePicture() are called.			
void SprdCameraHardware::receivePostLpmRawPicture(camera_frame_type *frame)
{
	LOGV("receivePostLpmRawPicture: E");
	print_time();

	Mutex::Autolock cbLock(&mCaptureCbLock);
	
	if (NULL == frame) {
		LOGE("receivePostLpmRawPicture: invalid frame pointer");
		return;
	}		

	if (mData_cb!= NULL) {
		bool encode_location = true;
		camera_position_type pt = {0};

		encode_location = getCameraLocation(&pt);
		if (encode_location) {			
			if (camera_set_position(&pt, NULL, NULL) != CAMERA_SUCCESS) {
				LOGE("receiveRawPicture: camera_set_position: error");
				// return;  // not a big deal
			}
		}
		else
			LOGV("receiveRawPicture: not setting image location");

		mJpegSize = 0;

		camera_handle_type camera_handle;
		if(CAMERA_SUCCESS != camera_encode_picture(frame, &camera_handle, camera_cb, this)) {
			setCameraState(SPRD_ERROR, STATE_CAPTURE);
			FreePmem(mRawHeap);
			mRawHeap = NULL;
			LOGE("receivePostLpmRawPicture: fail to camera_encode_picture().");
		}
	}
	else {
		LOGV("JPEG callback was cancelled--not encoding image.");
		// We need to keep the raw heap around until the JPEG is fully
		// encoded, because the JPEG encode uses the raw image contained in
		// that heap.
	}
	
	print_time();
	LOGV("receivePostLpmRawPicture: X");
}
		

void SprdCameraHardware::receiveJpegPictureFragment( JPEGENC_CBrtnType *encInfo)
{	
	Mutex::Autolock cbLock(&mCaptureCbLock);
	
	if (NULL == encInfo) {
		LOGE("receiveJpegPictureFragment: invalid enc info pointer");
		return;
	}	
	
    camera_encode_mem_type *enc = (camera_encode_mem_type *)encInfo->outPtr;

    uint8_t *base = (uint8_t *)mJpegHeap->mHeap->base();
    uint32_t size = encInfo->size;
    uint32_t remaining = mJpegHeap->mHeap->virtualSize();
    uint32_t i = 0;
    uint8_t *temp_ptr,*src_ptr;
    remaining -= mJpegSize;
	
	LOGV("receiveJpegPictureFragment E.");
    LOGV("receiveJpegPictureFragment: (status %d size %d remaining %d mJpegSize %d)",
         encInfo->status,
         size, remaining,mJpegSize);

    if (size > remaining) {
		LOGE("receiveJpegPictureFragment: size %d exceeds what "
		 	"remains in JPEG heap (%d), truncating",
		 	size,
		 	remaining);
		size = remaining;
    }

    //camera_handle.mem.encBuf[index].used_len = 0;
	LOGV("receiveJpegPictureFragment : base + mJpegSize: %x, enc->buffer: %x, size: %x", (uint32_t)(base + mJpegSize), (uint32_t)enc->buffer, size) ;

	memcpy(base + mJpegSize, enc->buffer, size);
    mJpegSize += size;

	LOGV("receiveJpegPictureFragment X.");
}
            
void SprdCameraHardware::receiveJpegPosPicture(void)//(camera_frame_type *frame)
{
	LOGV("receiveJpegPosPicture: E");
	print_time();
	
	Mutex::Autolock cbLock(&mCaptureCbLock);	

	if (mData_cb!= NULL) {
		bool encode_location = true;
		camera_position_type pt = {0};

		encode_location = getCameraLocation(&pt);
		if (encode_location) {
			if (camera_set_position(&pt, NULL, NULL) != CAMERA_SUCCESS) {
				LOGE("%s: camera_set_position: error", __func__);
				// return;  // not a big deal
			}
		}
		else
			LOGV("%s: not setting image location", __func__);

		mJpegSize = 0;
	}
	else {
		LOGV("%s: JPEG callback was cancelled--not encoding image.", __func__);
		// We need to keep the raw heap around until the JPEG is fully
		// encoded, because the JPEG encode uses the raw image contained in
		// that heap.
		if (!iSZslMode()) {
			deinitCapture();
		}
	}
	
	print_time();
	LOGV("%s: receiveJpegPosPicture: free mCallbackLock!", __func__);
}				

// This method is called by a libqcamera thread, different from the one on
// which startPreview() or takePicture() are called.
void SprdCameraHardware::receiveJpegPicture(JPEGENC_CBrtnType *encInfo)
{
    GET_END_TIME;
    GET_USE_TIME;
    LOGE("Capture Time:%d(ms).",s_use_time);

	LOGV("receiveJpegPicture: E image (%d bytes out of %d)",
				mJpegSize, mJpegHeap->mBufferSize);
	print_time();
	Mutex::Autolock cbLock(&mCaptureCbLock);

	int index = 0;

	//if (mJpegPictureCallback) {
	if (mData_cb) {
		LOGV("receiveJpegPicture: mData_cb.");
		// The reason we do not allocate into mJpegHeap->mBuffers[offset] is
		// that the JPEG image's size will probably change from one snapshot
		// to the next, so we cannot reuse the MemoryBase object.
		LOGV("receiveJpegPicture: mMsgEnabled: 0x%x.", mMsgEnabled);
		
		// mJpegPictureCallback(buffer, mPictureCallbackCookie);
		if ((CAMERA_ZSL_CONTINUE_SHOT_MODE != mCaptureMode)
			&& (CAMERA_NORMAL_CONTINUE_SHOT_MODE != mCaptureMode)) {
			if (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE){
				camera_memory_t *mem = mGetMemory_cb(-1, mJpegSize, 1, 0);
				memcpy(mem->data, mJpegHeap->mHeap->base(), mJpegSize);
				//mData_cb(CAMERA_MSG_COMPRESSED_IMAGE,buffer, mUser );
				mData_cb(CAMERA_MSG_COMPRESSED_IMAGE,mem, 0, NULL, mUser );
				mem->release(mem);
			}
		} else {
			camera_memory_t *mem = mGetMemory_cb(-1, mJpegSize, 1, 0);
			memcpy(mem->data, mJpegHeap->mHeap->base(), mJpegSize);
			mData_cb(CAMERA_MSG_COMPRESSED_IMAGE,mem, 0, NULL, mUser );
			mem->release(mem);
		}
	}
	else 
		LOGV("JPEG callback was cancelled--not delivering image.");

	// NOTE: the JPEG encoder uses the raw image contained in mRawHeap, so we need
	// to keep the heap around until the encoding is complete.
	LOGV("receiveJpegPicture: free the Raw and Jpeg mem. 0x%x, 0x%x", mRawHeap, mMiscHeap);

	if (!iSZslMode()) {
		if (encInfo->need_free) {
			deinitCapture();
		}
	}
	print_time();
	LOGV("receiveJpegPicture: X callback done.");
}

void SprdCameraHardware::receiveJpegPictureError(void)
{
	LOGV("receiveJpegPictureError.");
	print_time();
	Mutex::Autolock cbLock(&mCaptureCbLock);

	int index = 0;
	if (mData_cb) {
		LOGV("receiveJpegPicture: mData_cb.");
		if (mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE){
			mData_cb(CAMERA_MSG_COMPRESSED_IMAGE,NULL, 0, NULL, mUser );
		}
	} else 
		LOGV("JPEG callback was cancelled--not delivering image.");
	
	// NOTE: the JPEG encoder uses the raw image contained in mRawHeap, so we need
	// to keep the heap around until the encoding is complete.
	
	print_time();
	LOGV("receiveJpegPictureError: X callback done.");
}

void SprdCameraHardware::receiveCameraExitError(void)
{
	Mutex::Autolock cbPreviewLock(&mPreviewCbLock);
	Mutex::Autolock cbCaptureLock(&mCaptureCbLock);	
	
	if((mMsgEnabled & CAMERA_MSG_ERROR) && (mData_cb != NULL)) {
		LOGE("HandleErrorState");
		mNotify_cb(CAMERA_MSG_ERROR, 0,0,mUser);
	}
	
	LOGE("HandleErrorState:don't enable error msg!");
}

void SprdCameraHardware::receiveTakePictureError(void)
{
	Mutex::Autolock cbLock(&mCaptureCbLock);
	
	LOGE("camera cb: invalid state %s for taking a picture!",
		 getCameraStateStr(getCaptureState()));

	if ((mMsgEnabled & CAMERA_MSG_RAW_IMAGE) && (NULL != mData_cb))
		mData_cb(CAMERA_MSG_RAW_IMAGE, NULL, 0 , NULL, mUser);

	if ((mMsgEnabled & CAMERA_MSG_COMPRESSED_IMAGE) && (NULL != mData_cb))
		mData_cb(CAMERA_MSG_COMPRESSED_IMAGE, NULL, 0, NULL, mUser);
}


//transite from 'from' state to 'to' state and signal the waitting thread. if the current state is not 'from', transite to SPRD_ERROR state
//should be called from the callback
SprdCameraHardware::Sprd_camera_state
SprdCameraHardware::transitionState(SprdCameraHardware::Sprd_camera_state from, 
									SprdCameraHardware::Sprd_camera_state to,
									SprdCameraHardware::state_owner owner, bool lock)
{
	volatile SprdCameraHardware::Sprd_camera_state *which_ptr = NULL;
	LOGV("transitionState: owner = %d, lock = %d", owner, lock);
	
	//if (lock) mStateLock.lock();
		
	switch (owner) {
	case STATE_CAMERA:
		which_ptr = &mCameraState.camera_state;
		break;

	case STATE_PREVIEW:
		which_ptr = &mCameraState.preview_state;
		break;

	case STATE_CAPTURE:
		which_ptr = &mCameraState.capture_state;
		break;		

	default:
		LOGV("changeState: error owner");
		break;
	}

	if (NULL != which_ptr) {		
		if (from != *which_ptr) {
			to = SPRD_ERROR;
		}
		
		LOGV("changeState: %s --> %s", getCameraStateStr(from),
								   getCameraStateStr(to));	

		if (*which_ptr != to) {
			*which_ptr = to;
			mStateWait.signal();			
		}
	}	

	//if (lock) mStateLock.unlock();

	return to;
}

void SprdCameraHardware::HandleStartPreview(camera_cb_type cb, 
											 int32_t parm4)
{

	LOGV("HandleStartPreview in: cb = %d, parm4 = 0x%x, state = %s", 
				cb, parm4, getCameraStateStr(getPreviewState()));
	
	switch(cb) {
	case CAMERA_RSP_CB_SUCCESS:
		transitionState(SPRD_INTERNAL_PREVIEW_REQUESTED, 
					SPRD_PREVIEW_IN_PROGRESS, 
					STATE_PREVIEW);
		break;
	
	case CAMERA_EVT_CB_FRAME:
		LOGV("CAMERA_EVT_CB_FRAME");
		switch (getPreviewState()) {
		case SPRD_PREVIEW_IN_PROGRESS:			
	        receivePreviewFrame((camera_frame_type *)parm4);
			break;

		case SPRD_INTERNAL_PREVIEW_STOPPING:
            LOGV("camera cb: discarding preview frame "
                 "while stopping preview");			
			break;

		default:
            LOGV("HandleStartPreview: invalid state");			
			break;
			}			
		break;

	case CAMERA_EVT_CB_FD:
		LOGV("CAMERA_EVT_CB_FD");
		if (isPreviewing()) {
	    	receivePreviewFDFrame((camera_frame_type *)parm4);		
		}
		break;
	
	case CAMERA_EXIT_CB_FAILED:			//Execution failed or rejected
		LOGE("SprdCameraHardware::camera_cb: @CAMERA_EXIT_CB_FAILURE(%d) in state %s.",
				parm4, getCameraStateStr(getPreviewState()));
		transitionState(getPreviewState(), SPRD_ERROR, STATE_PREVIEW);
		receiveCameraExitError();
		break;
		
	default:
		transitionState(getPreviewState(), SPRD_ERROR, STATE_PREVIEW);
		LOGE("unexpected cb %d for CAMERA_FUNC_START_PREVIEW.", cb);		
		break;
	}

	LOGV("HandleStartPreview out, state = %s", getCameraStateStr(getPreviewState()));		
}

void SprdCameraHardware::HandleStopPreview(camera_cb_type cb, 										   
										  int32_t parm4)
{
	LOGV("HandleStopPreview in: cb = %d, parm4 = 0x%x, state = %s", 
				cb, parm4, getCameraStateStr(getPreviewState()));
	
	transitionState(SPRD_INTERNAL_PREVIEW_STOPPING, 
				SPRD_IDLE, 
				STATE_PREVIEW);
	freePreviewMem();

	LOGV("HandleStopPreview out, state = %s", getCameraStateStr(getPreviewState()));		
}

void SprdCameraHardware::HandleTakePicture(camera_cb_type cb, 										   
										 	 int32_t parm4)
{
	LOGV("HandleTakePicture in: cb = %d, parm4 = 0x%x, state = %s", 
				cb, parm4, getCameraStateStr(getCaptureState()));

	switch (cb) {
	case CAMERA_RSP_CB_SUCCESS:		
		LOGV("HandleTakePicture: CAMERA_RSP_CB_SUCCESS");
		transitionState(SPRD_INTERNAL_RAW_REQUESTED, 
					SPRD_WAITING_RAW, 
					STATE_CAPTURE);		
		break;

	case CAMERA_EVT_CB_SNAPSHOT_DONE:
		LOGV("HandleTakePicture: CAMERA_EVT_CB_SNAPSHOT_DONE");
		notifyShutter();
		receiveRawPicture((camera_frame_type *)parm4);
		break;

	case CAMERA_EXIT_CB_DONE:
		LOGV("HandleTakePicture: CAMERA_EXIT_CB_DONE");
		if (SPRD_WAITING_RAW == getCaptureState())
		{
			transitionState(SPRD_WAITING_RAW, 
						((NULL != mData_cb) ? SPRD_WAITING_JPEG : SPRD_IDLE), 
						STATE_CAPTURE);	
	        // It's important that we call receiveRawPicture() before
	        // we transition the state because another thread may be
	        // waiting in cancelPicture(), and then delete this object.
	        // If the order were reversed, we might call
	        // receiveRawPicture on a dead object.		
			receivePostLpmRawPicture((camera_frame_type *)parm4);		
		}
		break;

	case CAMERA_EXIT_CB_FAILED:			//Execution failed or rejected
		LOGE("SprdCameraHardware::camera_cb: @CAMERA_EXIT_CB_FAILURE(%d) in state %s.",
				parm4, getCameraStateStr(getCaptureState()));
		transitionState(getCaptureState(), SPRD_ERROR, STATE_CAPTURE);
		receiveCameraExitError();
		break;		

	default:	
		LOGE("HandleTakePicture: unkown cb = %d", cb);
		transitionState(getCaptureState(), SPRD_ERROR, STATE_CAPTURE);
		receiveTakePictureError();
		break;			
	}

	LOGV("HandleTakePicture out, state = %s", getCameraStateStr(getCaptureState()));		
}

void SprdCameraHardware::HandleCancelPicture(camera_cb_type cb, 										   
										 	 int32_t parm4)
{
	LOGV("HandleCancelPicture in: cb = %d, parm4 = 0x%x, state = %s", 
				cb, parm4, getCameraStateStr(getCaptureState()));

	{
		Mutex::Autolock cbLock(&mCaptureCbLock);		
	}

	transitionState(SPRD_INTERNAL_CAPTURE_STOPPING, 
				SPRD_IDLE, 
				STATE_CAPTURE);	

	
	
	LOGV("HandleCancelPicture out, state = %s", getCameraStateStr(getCaptureState()));	
}

void SprdCameraHardware::HandleEncode(camera_cb_type cb, 										   
								  		int32_t parm4)
{
	LOGV("HandleEncode in: cb = %d, parm4 = 0x%x, state = %s", 
				cb, parm4, getCameraStateStr(getCaptureState()));
	
	switch (cb) {
	case CAMERA_RSP_CB_SUCCESS:
        // We already transitioned the camera state to
        // SPRD_WAITING_JPEG when we called
        // camera_encode_picture().		
		break;
		
	case CAMERA_EXIT_CB_DONE:
		LOGV("HandleEncode: CAMERA_EXIT_CB_DONE");
		if (SPRD_WAITING_JPEG == getCaptureState()) {
	        // Receive the last fragment of the image.
	        receiveJpegPictureFragment((JPEGENC_CBrtnType *)parm4);
			LOGV("CAMERA_EXIT_CB_DONE MID.");
	        receiveJpegPicture((JPEGENC_CBrtnType *)parm4);
#if 1//to do it
			if( ((JPEGENC_CBrtnType *)parm4)->need_free ) {
				transitionState(SPRD_WAITING_JPEG,
						SPRD_IDLE,
						STATE_CAPTURE);
			} else {
				transitionState(SPRD_WAITING_JPEG,
						SPRD_INTERNAL_RAW_REQUESTED,
						STATE_CAPTURE);
			}
#else
			transitionState(SPRD_WAITING_JPEG, 
						SPRD_IDLE, 
						STATE_CAPTURE);
#endif
		}
		break;

	case CAMERA_EXIT_CB_FAILED:		
		LOGV("HandleEncode: CAMERA_EXIT_CB_FAILED");
		transitionState(getCaptureState(), SPRD_ERROR, STATE_CAPTURE);
		receiveCameraExitError();
		break;		

	default:	
		LOGV("HandleEncode: unkown error = %d", cb);
		transitionState(getCaptureState(), SPRD_ERROR, STATE_CAPTURE);
		receiveJpegPictureError();
		break;	
	}

	LOGV("HandleEncode out, state = %s", getCameraStateStr(getCaptureState()));
}

void SprdCameraHardware::HandleFocus(camera_cb_type cb, 										   
								  int32_t parm4)
{
	LOGV("HandleFocus in: cb = %d, parm4 = 0x%x, state = %s", 
				cb, parm4, getCameraStateStr(getPreviewState()));
	
    if (NULL == mNotify_cb) {
		LOGE("HandleFocus: mNotify_cb is NULL");
        return;
    }

    switch (cb) {
    case CAMERA_RSP_CB_SUCCESS:
		LOGV("camera cb: autofocus has started.");
        break;

    case CAMERA_EXIT_CB_DONE:
        LOGV("camera cb: autofocus succeeded.");
        LOGV("camera cb: autofocus mNotify_cb start.");	
		if (mMsgEnabled & CAMERA_MSG_FOCUS)
            mNotify_cb(CAMERA_MSG_FOCUS, 1, 0, mUser);
		else
			LOGE("camera cb: mNotify_cb is null.");
	
   		LOGV("camera cb: autofocus mNotify_cb ok.");	

    case CAMERA_EXIT_CB_ABORT:
		LOGE("camera cb: autofocus aborted");
		break;	

    case CAMERA_EXIT_CB_FAILED:
        LOGE("camera cb: autofocus failed");
        if (mMsgEnabled & CAMERA_MSG_FOCUS)
            mNotify_cb(CAMERA_MSG_FOCUS, 0, 0, mUser);		
		break;
	
    defaut:			
        LOGE("camera cb: unknown cb %d for " 
		"CAMERA_FUNC_START_FOCUS!", cb);
        if (mMsgEnabled & CAMERA_MSG_FOCUS)
            mNotify_cb(CAMERA_MSG_FOCUS, 0, 0, mUser);		
		break;
    }

	LOGV("HandleFocus out, state = %s", getCameraStateStr(getCaptureState()));
}

void SprdCameraHardware::HandleStartCamera(camera_cb_type cb, 										   
								  		int32_t parm4)
{
	LOGV("HandleCameraStart in: cb = %d, parm4 = 0x%x, state = %s", 
				cb, parm4, getCameraStateStr(getCameraState()));	

	transitionState(SPRD_INIT, SPRD_IDLE, STATE_CAMERA);	

	LOGV("HandleCameraStart out, state = %s", getCameraStateStr(getCameraState()));
}

void SprdCameraHardware::HandleStopCamera(camera_cb_type cb, int32_t parm4)
{
	LOGV("HandleStopCamera in: cb = %d, parm4 = 0x%x, state = %s", 
				cb, parm4, getCameraStateStr(getCameraState()));
	
	transitionState(SPRD_INTERNAL_STOPPING, SPRD_INIT, STATE_CAMERA);
	
	LOGV("HandleStopCamera out, state = %s", getCameraStateStr(getCameraState()));		
}

void SprdCameraHardware::camera_cb(camera_cb_type cb,
                                           const void *client_data,
                                           camera_func_type func,
                                           int32_t parm4)
{
	SprdCameraHardware *obj = (SprdCameraHardware *)client_data;
	
    switch(func) {
    // This is the commonest case.
	case CAMERA_FUNC_START_PREVIEW:
	    obj->HandleStartPreview(cb, parm4);		
		break;
		
	case CAMERA_FUNC_STOP_PREVIEW:
	    obj->HandleStopPreview(cb, parm4);
	    break;
		
	case CAMERA_FUNC_RELEASE_PICTURE:
		obj->HandleCancelPicture(cb, parm4);		 
	    break;
		
	case CAMERA_FUNC_TAKE_PICTURE:
		obj->HandleTakePicture(cb, parm4);
	    break;
		
	case CAMERA_FUNC_ENCODE_PICTURE:
	    obj->HandleEncode(cb, parm4);
	    break;
		
	case CAMERA_FUNC_START_FOCUS:
		obj->HandleFocus(cb, parm4);
		break;

	case CAMERA_FUNC_START:
		obj->HandleStartCamera(cb, parm4);
	    break;
		
	case CAMERA_FUNC_STOP:
		obj->HandleStopCamera(cb, parm4);
		break;		
		
    default:
        // transition to SPRD_ERROR ?
        LOGE("Unknown camera-callback status %d", cb);
		break;
	}
}	

////////////////////////////////////////////////////////////////////////////////////////
//memory related
////////////////////////////////////////////////////////////////////////////////////////
SprdCameraHardware::MemPool::MemPool(int buffer_size, int num_buffers,
                                         int frame_size,
                                         int frame_offset,
                                         const char *name) :
    mBufferSize(buffer_size),
    mNumBuffers(num_buffers),
    mFrameSize(frame_size),
    mFrameOffset(frame_offset),
    mBuffers(NULL), mName(name)
{
    // empty
}

void SprdCameraHardware::MemPool::completeInitialization()
{
    // If we do not know how big the frame will be, we wait to allocate
    // the buffers describing the individual frames until we do know their
    // size.

    if (mFrameSize > 0) {
        mBuffers = new sp<MemoryBase>[mNumBuffers];
        for (int i = 0; i < mNumBuffers; i++) {
            mBuffers[i] = new
                MemoryBase(mHeap,
                           i * mBufferSize + mFrameOffset,
                           mFrameSize);
        }
    }
}

SprdCameraHardware::AshmemPool::AshmemPool(int buffer_size, int num_buffers,
                                               int frame_size,
                                               int frame_offset,
                                               const char *name) :
    SprdCameraHardware::MemPool(buffer_size,
                                    num_buffers,
                                    frame_size,
                                    frame_offset,
                                    name)
{
        LOGV("constructing MemPool %s backed by ashmem: "
             "%d frames @ %d bytes, offset %d, "
             "buffer size %d",
             mName,
             num_buffers, frame_size, frame_offset, buffer_size);

        int page_mask = getpagesize() - 1;
        int ashmem_size = buffer_size * num_buffers;
        ashmem_size += page_mask;
        ashmem_size &= ~page_mask;

        mHeap = new MemoryHeapBase(ashmem_size);

        completeInitialization();
}
	
SprdCameraHardware::MemPool::~MemPool()
{
    LOGV("destroying MemPool %s", mName);
    if (mFrameSize > 0)
        delete [] mBuffers;
    mHeap.clear();
    LOGV("destroying MemPool %s completed", mName);
}

status_t SprdCameraHardware::MemPool::dump(int fd, const Vector<String16> &args) const
{
    const size_t SIZE = 256;
    char buffer[SIZE];
    String8 result;

    snprintf(buffer, 255, "SprdCameraHardware::AshmemPool::dump\n");
    result.append(buffer);
    if (mName) {
        snprintf(buffer, 255, "mem pool name (%s)\n", mName);
        result.append(buffer);
    }
    if (mHeap != 0) {
        snprintf(buffer, 255, "heap base(%p), size(%d), flags(%d), device(%s)\n",
                 mHeap->getBase(), mHeap->getSize(),
                 mHeap->getFlags(), mHeap->getDevice());
        result.append(buffer);
    }
    snprintf(buffer, 255, "buffer size (%d), number of buffers (%d),"
             " frame size(%d), and frame offset(%d)\n",
             mBufferSize, mNumBuffers, mFrameSize, mFrameOffset);
    result.append(buffer);
    write(fd, result.string(), result.size());
    return NO_ERROR;
}

/////////////////////////////////////////////////////////////////////////////////

/** Close this device */

static camera_device_t *g_cam_device;

static int HAL_camera_device_close(struct hw_device_t* device)
{
    LOGI("%s", __func__);
    if (device) {
        camera_device_t *cam_device = (camera_device_t *)device;
        delete static_cast<SprdCameraHardware *>(cam_device->priv);
        free(cam_device);
        g_cam_device = 0;
    }
#ifdef CONFIG_CAMERA_ISP
	stopispserver();
	ispvideo_RegCameraFunc(1, NULL);
	ispvideo_RegCameraFunc(2, NULL);
	ispvideo_RegCameraFunc(3, NULL);
	ispvideo_RegCameraFunc(4, NULL);
#endif
    return 0;
}

static inline SprdCameraHardware *obj(struct camera_device *dev)
{
    return reinterpret_cast<SprdCameraHardware *>(dev->priv);
}

/** Set the preview_stream_ops to which preview frames are sent */
static int HAL_camera_device_set_preview_window(struct camera_device *dev,
                                                struct preview_stream_ops *buf)
{
    LOGV("%s", __func__);
    return obj(dev)->setPreviewWindow(buf);
}

/** Set the notification and data callbacks */
static void HAL_camera_device_set_callbacks(struct camera_device *dev,
        camera_notify_callback notify_cb,
        camera_data_callback data_cb,
        camera_data_timestamp_callback data_cb_timestamp,
        camera_request_memory get_memory,
        void* user)
{
    LOGV("%s", __func__);
    obj(dev)->setCallbacks(notify_cb, data_cb, data_cb_timestamp,
                           get_memory,
                           user);
}

/**
 * The following three functions all take a msg_type, which is a bitmask of
 * the messages defined in include/ui/Camera.h
 */

/**
 * Enable a message, or set of messages.
 */
static void HAL_camera_device_enable_msg_type(struct camera_device *dev, int32_t msg_type)
{
    LOGV("%s", __func__);
    obj(dev)->enableMsgType(msg_type);
}

/**
 * Disable a message, or a set of messages.
 *
 * Once received a call to disableMsgType(CAMERA_MSG_VIDEO_FRAME), camera
 * HAL should not rely on its client to call releaseRecordingFrame() to
 * release video recording frames sent out by the cameral HAL before and
 * after the disableMsgType(CAMERA_MSG_VIDEO_FRAME) call. Camera HAL
 * clients must not modify/access any video recording frame after calling
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME).
 */
static void HAL_camera_device_disable_msg_type(struct camera_device *dev, int32_t msg_type)
{
    LOGV("%s", __func__);
    obj(dev)->disableMsgType(msg_type);
}

/**
 * Query whether a message, or a set of messages, is enabled.  Note that
 * this is operates as an AND, if any of the messages queried are off, this
 * will return false.
 */
static int HAL_camera_device_msg_type_enabled(struct camera_device *dev, int32_t msg_type)
{
    LOGV("%s", __func__);
    return obj(dev)->msgTypeEnabled(msg_type);
}

/**
 * Start preview mode.
 */
static int HAL_camera_device_start_preview(struct camera_device *dev)
{
    LOGV("%s", __func__);
    return obj(dev)->startPreview();
}

/**
 * Stop a previously started preview.
 */
static void HAL_camera_device_stop_preview(struct camera_device *dev)
{
    LOGV("%s", __func__);
    obj(dev)->stopPreview();
}

/**
 * Returns true if preview is enabled.
 */
static int HAL_camera_device_preview_enabled(struct camera_device *dev)
{
    LOGV("%s", __func__);
    return obj(dev)->previewEnabled();
}

/**
 * Request the camera HAL to store meta data or real YUV data in the video
 * buffers sent out via CAMERA_MSG_VIDEO_FRAME for a recording session. If
 * it is not called, the default camera HAL behavior is to store real YUV
 * data in the video buffers.
 *
 * This method should be called before startRecording() in order to be
 * effective.
 *
 * If meta data is stored in the video buffers, it is up to the receiver of
 * the video buffers to interpret the contents and to find the actual frame
 * data with the help of the meta data in the buffer. How this is done is
 * outside of the scope of this method.
 *
 * Some camera HALs may not support storing meta data in the video buffers,
 * but all camera HALs should support storing real YUV data in the video
 * buffers. If the camera HAL does not support storing the meta data in the
 * video buffers when it is requested to do do, INVALID_OPERATION must be
 * returned. It is very useful for the camera HAL to pass meta data rather
 * than the actual frame data directly to the video encoder, since the
 * amount of the uncompressed frame data can be very large if video size is
 * large.
 *
 * @param enable if true to instruct the camera HAL to store
 *      meta data in the video buffers; false to instruct
 *      the camera HAL to store real YUV data in the video
 *      buffers.
 *
 * @return OK on success.
 */
static int HAL_camera_device_store_meta_data_in_buffers(struct camera_device *dev, int enable)
{
    LOGV("%s", __func__);
    return obj(dev)->storeMetaDataInBuffers(enable);
}

/**
 * Start record mode. When a record image is available, a
 * CAMERA_MSG_VIDEO_FRAME message is sent with the corresponding
 * frame. Every record frame must be released by a camera HAL client via
 * releaseRecordingFrame() before the client calls
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME). After the client calls
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
 * responsibility to manage the life-cycle of the video recording frames,
 * and the client must not modify/access any video recording frames.
 */
static int HAL_camera_device_start_recording(struct camera_device *dev)
{
    LOGV("%s", __func__);
    return obj(dev)->startRecording();
}

/**
 * Stop a previously started recording.
 */
static void HAL_camera_device_stop_recording(struct camera_device *dev)
{
    LOGV("%s", __func__);
    obj(dev)->stopRecording();
}

/**
 * Returns true if recording is enabled.
 */
static int HAL_camera_device_recording_enabled(struct camera_device *dev)
{
    LOGV("%s", __func__);
    return obj(dev)->recordingEnabled();
}

/**
 * Release a record frame previously returned by CAMERA_MSG_VIDEO_FRAME.
 *
 * It is camera HAL client's responsibility to release video recording
 * frames sent out by the camera HAL before the camera HAL receives a call
 * to disableMsgType(CAMERA_MSG_VIDEO_FRAME). After it receives the call to
 * disableMsgType(CAMERA_MSG_VIDEO_FRAME), it is the camera HAL's
 * responsibility to manage the life-cycle of the video recording frames.
 */
static void HAL_camera_device_release_recording_frame(struct camera_device *dev,
                                const void *opaque)
{
    LOGV("%s", __func__);
    obj(dev)->releaseRecordingFrame(opaque);
}

/**
 * Start auto focus, the notification callback routine is called with
 * CAMERA_MSG_FOCUS once when focusing is complete. autoFocus() will be
 * called again if another auto focus is needed.
 */
static int HAL_camera_device_auto_focus(struct camera_device *dev)
{
    LOGV("%s", __func__);
    return obj(dev)->autoFocus();
}

/**
 * Cancels auto-focus function. If the auto-focus is still in progress,
 * this function will cancel it. Whether the auto-focus is in progress or
 * not, this function will return the focus position to the default.  If
 * the camera does not support auto-focus, this is a no-op.
 */
static int HAL_camera_device_cancel_auto_focus(struct camera_device *dev)
{
    LOGV("%s", __func__);
    return obj(dev)->cancelAutoFocus();
}

/**
 * Take a picture.
 */
static int HAL_camera_device_take_picture(struct camera_device *dev)
{
    LOGV("%s", __func__);
    return obj(dev)->takePicture();
}

/**
 * Cancel a picture that was started with takePicture. Calling this method
 * when no picture is being taken is a no-op.
 */
static int HAL_camera_device_cancel_picture(struct camera_device *dev)
{
    LOGV("%s", __func__);
    return obj(dev)->cancelPicture();
}

/**
 * Set the camera parameters. This returns BAD_VALUE if any parameter is
 * invalid or not supported.
 */
static int HAL_camera_device_set_parameters(struct camera_device *dev,
                                            const char *parms)
{
    LOGV("%s", __func__);
    String8 str(parms);
    SprdCameraParameters p(str);
    return obj(dev)->setParameters(p);
}

/** Return the camera parameters. */
char *HAL_camera_device_get_parameters(struct camera_device *dev)
{
    LOGV("%s", __func__);
    String8 str;
    SprdCameraParameters parms = obj(dev)->getParameters();
    str = parms.flatten();
    return strdup(str.string());
}

void HAL_camera_device_put_parameters(struct camera_device *dev, char *parms)
{
    LOGV("%s", __func__);
    free(parms);
}

/**
 * Send command to camera driver.
 */
static int HAL_camera_device_send_command(struct camera_device *dev,
                    int32_t cmd, int32_t arg1, int32_t arg2)
{
    LOGV("%s", __func__);
    return obj(dev)->sendCommand(cmd, arg1, arg2);
}

/**
 * Release the hardware resources owned by this object.  Note that this is
 * *not* done in the destructor.
 */
static void HAL_camera_device_release(struct camera_device *dev)
{
    LOGV("%s", __func__);
    obj(dev)->release();
}

/**
 * Dump state of the camera hardware
 */
static int HAL_camera_device_dump(struct camera_device *dev, int fd)
{
    LOGV("%s", __func__);
    return obj(dev)->dump(fd);
}

static int HAL_getNumberOfCameras()
{
	return SprdCameraHardware::getNumberOfCameras();
}

static int HAL_getCameraInfo(int cameraId, struct camera_info *cameraInfo)
{
	return SprdCameraHardware::getCameraInfo(cameraId, cameraInfo);
}

#define SET_METHOD(m) m : HAL_camera_device_##m

static camera_device_ops_t camera_device_ops = {
        SET_METHOD(set_preview_window),
        SET_METHOD(set_callbacks),
        SET_METHOD(enable_msg_type),
        SET_METHOD(disable_msg_type),
        SET_METHOD(msg_type_enabled),
        SET_METHOD(start_preview),
        SET_METHOD(stop_preview),
        SET_METHOD(preview_enabled),
        SET_METHOD(store_meta_data_in_buffers),
        SET_METHOD(start_recording),
        SET_METHOD(stop_recording),
        SET_METHOD(recording_enabled),
        SET_METHOD(release_recording_frame),
        SET_METHOD(auto_focus),
        SET_METHOD(cancel_auto_focus),
        SET_METHOD(take_picture),
        SET_METHOD(cancel_picture),
        SET_METHOD(set_parameters),
        SET_METHOD(get_parameters),
        SET_METHOD(put_parameters),
        SET_METHOD(send_command),
        SET_METHOD(release),
        SET_METHOD(dump),
};

#undef SET_METHOD

static int HAL_IspVideoStartPreview(uint32_t param1, uint32_t param2)
{
	int rtn=0x00;
	SprdCameraHardware * fun_ptr = dynamic_cast<SprdCameraHardware *>((SprdCameraHardware *)g_cam_device->priv);
	if (NULL != fun_ptr)
	{
		rtn=fun_ptr->startPreview();
	}
	return rtn;
}

static int HAL_IspVideoStopPreview(uint32_t param1, uint32_t param2)
{
	int rtn=0x00;
	SprdCameraHardware * fun_ptr = dynamic_cast<SprdCameraHardware *>((SprdCameraHardware *)g_cam_device->priv);
	if (NULL != fun_ptr)
	{
		fun_ptr->stopPreview();
	}
	return rtn;
}

static int HAL_IspVideoSetParam(uint32_t width, uint32_t height)
{
	int rtn=0x00;
	SprdCameraHardware * fun_ptr = dynamic_cast<SprdCameraHardware *>((SprdCameraHardware *)g_cam_device->priv);

	if (NULL != fun_ptr)
	{
		LOGE("ISP_TOOL: HAL_IspVideoSetParam width:%d, height:%d", width, height);
		fun_ptr->setCaptureRawMode(1);
		rtn=fun_ptr->setTakePictureSize(width,height);
	}
	return rtn;
}

static int HAL_IspVideoTakePicture(uint32_t param1, uint32_t param2)
{
	int rtn=0x00;
	SprdCameraHardware * fun_ptr = dynamic_cast<SprdCameraHardware *>((SprdCameraHardware *)g_cam_device->priv);
	if (NULL != fun_ptr)
	{
		rtn=fun_ptr->takePicture();
	}
	return rtn;
}

static int HAL_camera_device_open(const struct hw_module_t* module,
                                  const char *id,
                                  struct hw_device_t** device)
{
    LOGV("%s", __func__);
    GET_START_TIME;

    int cameraId = atoi(id);
    if (cameraId < 0 || cameraId >= HAL_getNumberOfCameras()) {
        LOGE("Invalid camera ID %s", id);
        return -EINVAL;
    }

    if (g_cam_device) {
        if (obj(g_cam_device)->getCameraId() == cameraId) {
            LOGV("returning existing camera ID %s", id);
            goto done;
        } else {
            LOGE("Cannot open camera %d. camera %d is already running!",
                    cameraId, obj(g_cam_device)->getCameraId());
            return -ENOSYS;
        }
    }

    g_cam_device = (camera_device_t *)malloc(sizeof(camera_device_t));
    if (!g_cam_device)
        return -ENOMEM;

    g_cam_device->common.tag     = HARDWARE_DEVICE_TAG;
    g_cam_device->common.version = 1;
    g_cam_device->common.module  = const_cast<hw_module_t *>(module);
    g_cam_device->common.close   = HAL_camera_device_close;

    g_cam_device->ops = &camera_device_ops;

    LOGI("%s: open camera %s", __func__, id);

    //g_cam_device->priv = new SprdCameraHardware(cameraId, g_cam_device);
    g_cam_device->priv = new SprdCameraHardware(cameraId);


#ifdef CONFIG_CAMERA_ISP
	startispserver();
	ispvideo_RegCameraFunc(1, HAL_IspVideoStartPreview);
	ispvideo_RegCameraFunc(2, HAL_IspVideoStopPreview);
	ispvideo_RegCameraFunc(3, HAL_IspVideoTakePicture);
	ispvideo_RegCameraFunc(4, HAL_IspVideoSetParam);
#endif


done:
    *device = (hw_device_t *)g_cam_device;

    LOGI("%s: opened camera %s (%p)", __func__, id, *device);
    return 0;
}

static hw_module_methods_t camera_module_methods = {
            open : HAL_camera_device_open
};

extern "C" {
    struct camera_module HAL_MODULE_INFO_SYM = {
      common : {
          tag           : HARDWARE_MODULE_TAG,
          version_major : 1,
          version_minor : 0,
          id            : CAMERA_HARDWARE_MODULE_ID,
          name          : "Sprd camera HAL",
          author        : "Spreadtrum Corporation",
          methods       : &camera_module_methods,
      },
      get_number_of_cameras : HAL_getNumberOfCameras,
      get_camera_info       : HAL_getCameraInfo
    };
}

}

