/*
 * hardware/sprd/hsdroid/libcamera/sprdcamerahardwareinterface.h
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
#ifndef ANDROID_HARDWARE_SPRD_CAMERA_HARDWARE_H
#define ANDROID_HARDWARE_SPRD_CAMERA_HARDWARE_H

#include <binder/MemoryHeapIon.h>
#include <utils/threads.h>
extern "C" {
    #include <linux/android_pmem.h>
}
#include <sys/types.h>

#include "SprdOEMCamera.h"
#include <utils/threads.h>
#include <utils/RefBase.h>
#include <binder/MemoryBase.h>
#include <binder/MemoryHeapBase.h>
#include <hardware/camera.h>
#include <hardware/gralloc.h>
#include <camera/CameraParameters.h>
#include "SprdCameraParameters.h"
#include "SprdOEMCamera.h"

namespace android {

typedef void (*preview_callback)(sp<MemoryBase>, void * );
typedef void (*recording_callback)(sp<MemoryBase>, void *);
typedef void (*shutter_callback)(void *);
typedef void (*raw_callback)(sp<MemoryBase> , void *);
typedef void (*jpeg_callback)(sp<MemoryBase>, void *);
typedef void (*autofocus_callback)(bool, void *);

typedef struct sprd_camera_memory {
	camera_memory_t *camera_memory;
	MemoryHeapIon *ion_heap;
	uint32_t phys_addr, phys_size;
	void *handle;
	void *data;
}sprd_camera_memory_t;


class SprdCameraHardware : public virtual RefBase {
public:
	SprdCameraHardware(int cameraId);
	virtual                      ~SprdCameraHardware();	
	inline int                   getCameraId() const; 
	virtual void                 release();
	virtual status_t             startPreview();
	virtual void                 stopPreview();
	virtual bool                 previewEnabled();
	virtual status_t             setPreviewWindow(preview_stream_ops *w);
	virtual status_t             startRecording();
	virtual void                 stopRecording();
	virtual void                 releaseRecordingFrame(const void *opaque);
	virtual bool                 recordingEnabled();
	virtual status_t             takePicture();
	virtual status_t             cancelPicture();
	virtual status_t             setTakePictureSize(uint32_t width, uint32_t height);
	virtual status_t             autoFocus();
	virtual status_t             cancelAutoFocus();
	virtual status_t             setParameters(const SprdCameraParameters& params);
	virtual SprdCameraParameters getParameters() const;
	virtual void                 setCallbacks(camera_notify_callback notify_cb,
						camera_data_callback data_cb,
						camera_data_timestamp_callback data_cb_timestamp,
						camera_request_memory get_memory,
						void *user);
	virtual void                 enableMsgType(int32_t msgType);
	virtual void                 disableMsgType(int32_t msgType);
	virtual bool                 msgTypeEnabled(int32_t msgType);
	virtual status_t             sendCommand(int32_t cmd, int32_t arg1, int32_t arg2);
	virtual status_t             storeMetaDataInBuffers(bool enable);
	virtual status_t             dump(int fd) const;
	void                            setCaptureRawMode(bool mode);

public:
	static int                   getPropertyAtv();
	static int                   getNumberOfCameras();
	static int                   getCameraInfo(int cameraId, struct camera_info *cameraInfo);
	static const CameraInfo      kCameraInfo[];
	static const CameraInfo      kCameraInfo3[];
	
private:
	inline void                  print_time();
	
    // This class represents a heap which maintains several contiguous
    // buffers.  The heap may be backed by pmem (when pmem_pool contains
    // the name of a /dev/pmem* file), or by ashmem (when pmem_pool == NULL).
	struct MemPool : public RefBase {
		MemPool(int buffer_size, int num_buffers, int frame_size,
					int frame_offset, const char *name);
		virtual ~MemPool() = 0;
		void     completeInitialization();
		bool     initialized() const { 
			if (mHeap != NULL) {
				if(MAP_FAILED != mHeap->base())
					return true;
				else
					return false;	
			} else {
				return false;
			}
		}
		virtual status_t dump(int fd, const Vector<String16>& args) const;
		int mBufferSize;
		int mNumBuffers;
		int mFrameSize;
		int mFrameOffset;
		sp<MemoryHeapBase> mHeap;
		sp<MemoryBase> *mBuffers;
		const char *mName;
	};

	struct AshmemPool : public MemPool {
		AshmemPool(int buffer_size, int num_buffers, int frame_size,
						int frame_offset, const char *name);
	};
	
	void                  FreeCameraMem(void);
	sprd_camera_memory_t* GetPmem(int buf_size, int num_bufs);
	void                  FreePmem(sprd_camera_memory_t* camera_memory);
	void                  setFdmem(uint32_t size);	
	void                  FreeFdmem(void);
	bool                  allocatePreviewMem();
	void                  freePreviewMem();
	bool                  allocateCaptureMem(bool initJpegHeap);
	void                  freeCaptureMem();	
	uint32_t              getRedisplayMem();
	void                  FreeReDisplayMem();
	static void           camera_cb(camera_cb_type cb,
					const void *client_data,
					camera_func_type func,
					int32_t parm4);
	void                  notifyShutter();
	void                  receiveJpegPictureFragment(JPEGENC_CBrtnType *encInfo);
	void                  receiveJpegPosPicture(void);
	void                  receivePostLpmRawPicture(camera_frame_type *frame);
	void                  receiveRawPicture(camera_frame_type *frame);
	void                  receiveJpegPicture(JPEGENC_CBrtnType *encInfo);
	void                  receivePreviewFrame(camera_frame_type *frame);
	void                  receivePreviewFDFrame(camera_frame_type *frame);	
	void                  receiveCameraExitError(void);
	void                  receiveTakePictureError(void);
	void                  receiveJpegPictureError(void);
	void                  HandleStopCamera(camera_cb_type cb, int32_t parm4);
	void                  HandleStartCamera(camera_cb_type cb, int32_t parm4);
	void                  HandleStartPreview(camera_cb_type cb, int32_t parm4);
	void                  HandleStopPreview(camera_cb_type cb, int32_t parm4);
	void                  HandleTakePicture(camera_cb_type cb, int32_t parm4);
	void                  HandleEncode(camera_cb_type cb,  int32_t parm4);
	void                  HandleFocus(camera_cb_type cb, int32_t parm4);
	void                  HandleCancelPicture(camera_cb_type cb, int32_t parm4);

	enum Sprd_camera_state {
		SPRD_INIT,
		SPRD_IDLE,     
		SPRD_ERROR,
		SPRD_PREVIEW_IN_PROGRESS,
		SPRD_WAITING_RAW,
		SPRD_WAITING_JPEG,

		// internal states 
		SPRD_INTERNAL_PREVIEW_STOPPING,
		SPRD_INTERNAL_CAPTURE_STOPPING,
		SPRD_INTERNAL_PREVIEW_REQUESTED,
		SPRD_INTERNAL_RAW_REQUESTED,
		SPRD_INTERNAL_STOPPING,

	};

	enum state_owner {
		STATE_CAMERA,
		STATE_PREVIEW,
		STATE_CAPTURE
	};

	typedef struct _camera_state	{
		Sprd_camera_state 	camera_state;
		Sprd_camera_state 	preview_state;
		Sprd_camera_state 	capture_state;
	} camera_state;

	const char* const getCameraStateStr(Sprd_camera_state s);    
	Sprd_camera_state transitionState(Sprd_camera_state from,
						Sprd_camera_state to,
						state_owner owner,
						bool lock = true);	
	void                            setCameraState(Sprd_camera_state state, 
								state_owner owner = STATE_CAMERA);
	inline Sprd_camera_state        getCameraState();
	inline Sprd_camera_state        getPreviewState();
	inline Sprd_camera_state        getCaptureState();
	inline bool                     isCameraError();
	inline bool                     isCameraInit();
	inline bool                     isCameraIdle();
	inline bool                     isPreviewing();
	inline bool                     isCapturing();	
	bool                            WaitForPreviewStart();
	bool                            WaitForPreviewStop();
	bool                            WaitForCaptureStart();
	bool                            WaitForCaptureDone();
	bool                            WaitForCameraStart();
	bool                            WaitForCameraStop();		
	bool                            isRecordingMode();
	void                            setRecordingMode(bool enable);	
	bool                            startCameraIfNecessary();
	void                            getPictureFormat(int *format);
	takepicture_mode                getCaptureMode();
	bool                            getCameraLocation(camera_position_type *pt);
	status_t                        startPreviewInternal(bool isRecordingMode);                              
	void                            stopPreviewInternal();
	status_t                        cancelPictureInternal();
	bool                            initPreview();
	void                            deinitPreview();
	bool                            initCapture(bool initJpegHeap);
	void                            deinitCapture();
	status_t                        initDefaultParameters();
	status_t                        setCameraParameters();
	bool                            setCameraDimensions();	
	void                            setCameraPreviewMode();
	void                            changeEmcFreq(char flag);
	void                            setPreviewFreq();
	void                            restoreFreq();
	bool                            displayOneFrame(uint32_t width, 
							uint32_t height,
							uint32_t phy_addr, char *frame_addr);
	bool                            iSDisplayCaptureFrame();
	bool                            iSZslMode();
	/* These constants reflect the number of buffers that libqcamera requires
	for preview and raw, and need to be updated when libqcamera
	changes.
	*/
	static const int                kPreviewBufferCount    = 8;
	static const int                kPreviewRotBufferCount = 4;
	static const int                kRawBufferCount        = 1;
	static const int                kJpegBufferCount       = 1;
	static const int                kRawFrameHeaderSize    = 0x0;	
	Mutex                           mLock; // API lock -- all public methods
	Mutex                           mCallbackLock;
	Mutex                           mPreviewCbLock;
	Mutex                           mCaptureCbLock;
	Mutex                           mStateLock;
	Condition                       mStateWait;	

	uint32_t                        mPreviewHeapSize;
	uint32_t                        mPreviewHeapNum;
	sprd_camera_memory_t*           *mPreviewHeapArray;
	uint32_t                        mPreviewHeapArray_phy[kPreviewBufferCount+kPreviewRotBufferCount];
	void*                           mPreviewHeapArray_vir[kPreviewBufferCount+kPreviewRotBufferCount];

	sprd_camera_memory_t            *mRawHeap;
	uint32_t                        mRawHeapSize;
	sprd_camera_memory_t            *mMiscHeap;
	uint32_t                        mMiscHeapSize;
	sp<AshmemPool>                  mJpegHeap;
	uint32_t                        mJpegHeapSize;
	uint32_t                        mFDAddr;
	camera_memory_t                 *mMetadataHeap;
	sprd_camera_memory_t            *mReDisplayHeap;
	//TODO: put the picture dimensions in the CameraParameters object;
	SprdCameraParameters            mParameters;	
	int                             mPreviewHeight;
	int                             mPreviewWidth;
	int                             mRawHeight;
	int                             mRawWidth;
	int                             mPreviewFormat;//0:YUV422;1:YUV420;2:RGB
	int                             mPictureFormat;//0:YUV422;1:YUV420;2:RGB;3:JPEG  
	int                             mPreviewStartFlag;

	bool                            mRecordingMode;
	uint32_t                        mZoomLevel;
	/* mJpegSize keeps track of the size of the accumulated JPEG.  We clear it
	when we are about to take a picture, so at any time it contains either
	zero, or the size of the last JPEG picture taken.
	*/
	uint32_t                        mJpegSize;
	camera_notify_callback          mNotify_cb;
	camera_data_callback            mData_cb;
	camera_data_timestamp_callback  mData_cb_timestamp;
	camera_request_memory           mGetMemory_cb;
	void                            *mUser;
	preview_stream_ops              *mPreviewWindow;
	static gralloc_module_t const   *mGrallocHal;
	int32_t                         mMsgEnabled;
	bool                            mIsStoreMetaData;
	bool                            mIsFreqChanged;
	int32_t                         mCameraId;
	volatile camera_state           mCameraState;
	int                             miSPreviewFirstFrame;
	takepicture_mode                mCaptureMode;
	bool                             mCaptureRawMode;
};

}; // namespace android

#endif //ANDROID_HARDWARE_SPRD_CAMERA_HARDWARE_H


