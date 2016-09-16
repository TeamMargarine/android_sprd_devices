/*
 * Copyright (C) 2010 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
 
//#define LOG_NDEBUG 0
#include <hardware/hardware.h>

#include <fcntl.h>
#include <errno.h>


#include <sys/ioctl.h>
#include <sys/mman.h>

#include <cutils/log.h>
#include <cutils/atomic.h>

#include <hardware/hwcomposer.h>

#include <EGL/egl.h>

#include "gralloc_priv.h"
#include "sprd_fb.h"
#include "scale_rotate.h"

#include "ion_sprd.h"
#include "hwcomposer_sprd.h"
#include <cutils/properties.h>
#include "dump_bmp.h"

#define SPRD_ION_DEV "/dev/ion"

#define OVERLAY_BUF_NUM 2
//extern & define the dump parameters
extern int dump_layer(const char* path ,const char* pSrc , const char* ptype ,  int width , int height , int format ,int64_t randNum ,  int index , int LayerIndex = 0);
static int64_t g_GeometoryChanged_Num = 0;
static bool g_GeometryChanged = false;
static bool bFirstGeometroyChanged = false;
static char* currentOsdOverlayAddr = NULL;
static char* currentVideoOverlayAddr = NULL;
static int videoOverlayFormat = 0;
static int osdOverlayFormat = 0;
inline unsigned int round_up_to_page_size(unsigned int x)
{
    return (x + (PAGE_SIZE-1)) & ~(PAGE_SIZE-1);
}

using namespace android;
/*****************************************************************************/
typedef struct {
	char *pFrontAddr;
	char *pBackAddr;
	int     format;
} FrameBufferInfo;


static int debugenable = 0;
inline int MIN(int x, int y) {
    return ((x < y) ? x : y);
}

inline int MAX(int x, int y) {
    return ((x > y) ? x : y);
}
//get the back frame buffer addr
static int getFrameBufferInfo(struct hwc_context_t * context , FrameBufferInfo* fbInfo)
{
	struct fb_fix_screeninfo finfo;
	uint32_t bytespp = 0;
	if (ioctl(context->fbfd, FBIOGET_FSCREENINFO, &finfo) == -1)
	{
		ALOGE_IF(debugenable , "fail to get FBIOGET_FSCREENINFO finfo");
		return -1;
	}
	struct fb_var_screeninfo info;
	if (ioctl(context->fbfd, FBIOGET_VSCREENINFO, &info) == -1)
	{
		ALOGE_IF(debugenable , "fail to get FBIOGET_VSCREENINFO info");
		return -1;
	}
	switch(info.bits_per_pixel)
	{
        case 16:
            bytespp = 2;
            fbInfo->format = HAL_PIXEL_FORMAT_RGB_565;
            break;
        case 24:
            bytespp = 3;
            fbInfo->format = HAL_PIXEL_FORMAT_RGB_888;
            break;
        case 32:
            bytespp = 4;
            fbInfo->format = HAL_PIXEL_FORMAT_RGBA_8888;
            break;
        default:
		ALOGE_IF(debugenable , "fail to getFrameBufferInfo not support bits per pixel:%d" , info.bits_per_pixel);
		return -1;
	}
	if(info.yoffset == info.yres) //flushing the second buffer.
	{
		fbInfo->pFrontAddr = (char*)((unsigned int)context->fb_virt_addr + info.xres*info.yres*bytespp);
		fbInfo->pBackAddr  = (char *)(context->fb_virt_addr);
		return 0;
	}
	else if(info.yoffset == 0) //flushing the first buffer.
	{
		fbInfo->pFrontAddr = (char *)(context->fb_virt_addr);
		fbInfo->pBackAddr = (char*)((unsigned int)context->fb_virt_addr + info.xres*info.yres*bytespp);
		return 0;
	}
	else
	{
		ALOGE_IF(debugenable , "fail to getFrameBufferInfo");
		return -1;
	}
}
static int getDumpPath(char *pPath)
{
	char value[PROPERTY_VALUE_MAX];
	if(0 == property_get("dump.hwcomposer.path" , value , "0"))
	{
		ALOGE_IF(debugenable , "fail to getDumpPath not set path");
		return -1;
	}
	if(strchr(value , '/') != NULL)
	{
		sprintf(pPath , "%s" , value);
		return 0;
	}
	else
		pPath[0] = 0;
	ALOGE_IF(debugenable , "fail to getDumpPath path format error");
	return -2;
}
static int transform_video_layer(struct hwc_context_t *context, hwc_layer_t * l)
{
	uint32_t current_overlay_phy_addr;
	uint32_t current_overlay_vir_addr;
	int ret = 0;
#ifdef VIDEO_LAYER_USE_RGB
	int dstFormat = HAL_PIXEL_FORMAT_RGBX_8888;
#else
	int dstFormat = HAL_PIXEL_FORMAT_YCbCr_420_SP;
#endif
	videoOverlayFormat = dstFormat;
	const native_handle_t *pNativeHandle = l->handle;
	struct private_handle_t *private_h = (struct private_handle_t *)pNativeHandle;

	context->src_img.y_addr = private_h->phyaddr;


	current_overlay_phy_addr = context->overlay_phy_addr + context->overlay_buf_size*context->overlay_index;
	current_overlay_vir_addr = ((unsigned int)context->overlay_v_addr) + context->overlay_buf_size*context->overlay_index;
#ifdef SCAL_ROT_TMP_BUF
	uint32_t tmp_phy_addr = context->overlay_phy_addr_tmp;
	uint32_t tmp_virt_addr = (uint32_t)context->overlay_v_addr_tmp;
#else
	uint32_t tmp_phy_addr = 0;
	uint32_t tmp_virt_addr = 0;
#endif
	ret = transform_layer(context->src_img.y_addr , private_h->base, context->src_img.format , l->transform,
					context->src_img.w, context->src_img.h, current_overlay_phy_addr , current_overlay_vir_addr ,
					dstFormat , context->fb_rect.w , context->fb_rect.h , &context->src_rect , 
					tmp_phy_addr , tmp_virt_addr);
	currentVideoOverlayAddr = (char*)current_overlay_vir_addr;
	return ret;
}

static int verify_osd_layer(struct hwc_context_t *context, hwc_layer_t * l)
{
	int src_width,src_height;
	const native_handle_t *pNativeHandle = l->handle;
	struct private_handle_t *private_h = (struct private_handle_t *)pNativeHandle;

	//return 0;

#ifndef _ALLOC_OSD_BUF
	if ((l->transform != 0) ||!(private_h->flags & private_handle_t::PRIV_FLAGS_USES_PHY))
		return 0;
#endif
	
	if((l->transform != 0) && ((l->transform & HAL_TRANSFORM_ROT_90) != HAL_TRANSFORM_ROT_90)) {
		return 0;
	} else if(l->transform == 0) {
		if((private_h->width != context->fb_width) || (private_h->height!= context->fb_height))
			return 0;
#ifndef _DMA_COPY_OSD_LAYER
		if (!(private_h->flags & private_handle_t::PRIV_FLAGS_USES_PHY))
			return 0;
#endif
	} else if((private_h->width != context->fb_height) || (private_h->height!= context->fb_width)
		|| !(private_h->flags & private_handle_t::PRIV_FLAGS_USES_PHY)) {
		return 0;
	}
	
	struct sprd_rect src_rect;
	struct sprd_rect fb_rect;
	src_rect.x = MAX(l->sourceCrop.left, 0);
	src_rect.x = MIN(src_rect.x, private_h->width);
	src_rect.y = MAX(l->sourceCrop.top, 0);
	src_rect.y = MIN(src_rect.y, private_h->height);
	src_rect.w = MIN(l->sourceCrop.right - src_rect.x, private_h->width - src_rect.x);
	src_rect.h = MIN(l->sourceCrop.bottom - src_rect.y, private_h->height - src_rect.y);

	fb_rect.x = MAX(l->displayFrame.left, 0);
	fb_rect.x = MIN(fb_rect.x, context->fb_width);
	fb_rect.y = MAX(l->displayFrame.top, 0);
	fb_rect.y = MIN(fb_rect.y, context->fb_height);
	fb_rect.w = MIN(l->displayFrame.right - fb_rect.x, context->fb_width - fb_rect.x);
	fb_rect.h = MIN(l->displayFrame.bottom - fb_rect.y, context->fb_height - fb_rect.y);

	if ((l->transform & HAL_TRANSFORM_ROT_90) == HAL_TRANSFORM_ROT_90){
		src_width = src_rect.h;
		src_height =  src_rect.w;
	}else{
		src_width = src_rect.w;
		src_height = src_rect.h;
	}
	
	if((src_width != fb_rect.w) ||(src_height != fb_rect.h)) {
		return 0;
	}

	//only support full screen now
	if((src_rect.x != 0) || (src_rect.y != 0)) {
		return 0;
	}

 	if((fb_rect.w != context->fb_width) || (fb_rect.h!= context->fb_height)) {
		return 0;
	}
	
	return SPRD_LAYERS_OSD;	
}

static int verify_video_layer(struct hwc_context_t *context, hwc_layer_t * l) 
{
	int src_width,src_height;
	int dest_width,dest_height;
	const native_handle_t *pNativeHandle = l->handle;
	struct private_handle_t *private_h = (struct private_handle_t *)pNativeHandle;
	
	if (!((private_h->flags & private_handle_t::PRIV_FLAGS_USES_PHY) && !(private_h->flags & private_handle_t::PRIV_FLAGS_NOT_OVERLAY)))
		return 0;
	
	//video layer
	if (private_h->format != HAL_PIXEL_FORMAT_YCbCr_420_SP &&
	    private_h->format != HAL_PIXEL_FORMAT_YCrCb_420_SP &&
	    private_h->format != HAL_PIXEL_FORMAT_YV12){
		return 0;
	}

	if(l->blending != HWC_BLENDING_NONE)
		return 0;
	if(l->transform == HAL_TRANSFORM_FLIP_V)
		return 0;

	if((l->transform == (HAL_TRANSFORM_ROT_90 | HAL_TRANSFORM_FLIP_H)) || (l->transform == (HAL_TRANSFORM_ROT_90 | HAL_TRANSFORM_FLIP_V)))
		return 0;

	context->src_img.format = private_h->format;
	context->src_img.w = private_h->width;
	context->src_img.h = private_h->height;

	int rot_90_270 = (l->transform&HAL_TRANSFORM_ROT_90) == HAL_TRANSFORM_ROT_90;

    	context->src_rect.x = MAX(l->sourceCrop.left, 0);
	context->src_rect.x = (context->src_rect.x + SRCRECT_X_ALLIGNED) & (~SRCRECT_X_ALLIGNED);//dcam 8 pixel crop
    	context->src_rect.x = MIN(context->src_rect.x, context->src_img.w);
    	context->src_rect.y = MAX(l->sourceCrop.top, 0);
	context->src_rect.y = (context->src_rect.y + SRCRECT_Y_ALLIGNED) & (~SRCRECT_Y_ALLIGNED);//dcam 8 pixel crop
    	context->src_rect.y = MIN(context->src_rect.y, context->src_img.h);

    	context->src_rect.w = MIN(l->sourceCrop.right - l->sourceCrop.left, context->src_img.w - context->src_rect.x);
    	context->src_rect.h = MIN(l->sourceCrop.bottom - l->sourceCrop.top, context->src_img.h - context->src_rect.y);
	if((context->src_rect.w - (context->src_rect.w & (~SRCRECT_WIDTH_ALLIGNED)))> ((SRCRECT_WIDTH_ALLIGNED+1)>>1))
	{
		context->src_rect.w = (context->src_rect.w + SRCRECT_WIDTH_ALLIGNED) & (~SRCRECT_WIDTH_ALLIGNED);//dcam 8 pixel crop
	}
	else
	{
		context->src_rect.w = (context->src_rect.w) & (~SRCRECT_WIDTH_ALLIGNED);//dcam 8 pixel crop
	}
	if((context->src_rect.h - (context->src_rect.h & (~SRCRECT_HEIGHT_ALLIGNED)))> ((SRCRECT_HEIGHT_ALLIGNED+1)>>1))
	{
		context->src_rect.h = (context->src_rect.h + SRCRECT_HEIGHT_ALLIGNED) & (~SRCRECT_HEIGHT_ALLIGNED);//dcam 8 pixel crop
	}
	else
	{
		context->src_rect.h = (context->src_rect.h) & (~SRCRECT_HEIGHT_ALLIGNED);//dcam 8 pixel crop
	}
	context->src_rect.w = MIN(context->src_rect.w, context->src_img.w - context->src_rect.x);
	context->src_rect.h = MIN(context->src_rect.h, context->src_img.h - context->src_rect.y);
	//--------------------------------------------------
    	context->fb_rect.x = MAX(l->displayFrame.left, 0);
    	context->fb_rect.y = MAX(l->displayFrame.top, 0);
	context->fb_rect.x = MIN(context->fb_rect.x, context->fb_width);
	context->fb_rect.y = MIN(context->fb_rect.y, context->fb_height);

    	context->fb_rect.w = MIN(l->displayFrame.right - l->displayFrame.left, context->fb_width - context->fb_rect.x);
    	context->fb_rect.h = MIN(l->displayFrame.bottom - l->displayFrame.top, context->fb_height - context->fb_rect.y);
       if((context->fb_rect.w - (context->fb_rect.w & (~FB_WIDTH_ALLIGNED)))> ((FB_WIDTH_ALLIGNED+1)>>1))
       {
       	context->fb_rect.w = (context->fb_rect.w + FB_WIDTH_ALLIGNED) & (~FB_WIDTH_ALLIGNED);//dcam 8 pixel and lcdc must 4 pixel for yuv420
       }
	 else
       {
       	context->fb_rect.w = (context->fb_rect.w) & (~FB_WIDTH_ALLIGNED);//dcam 8 pixel and lcdc must 4 pixel for yuv420
	 }
	if((context->fb_rect.h - (context->fb_rect.h & (~FB_HEIGHT_ALLIGNED)))> ((FB_HEIGHT_ALLIGNED+1)>>1))
	{
		context->fb_rect.h = (context->fb_rect.h + FB_HEIGHT_ALLIGNED) & (~FB_HEIGHT_ALLIGNED);//dcam 8 pixel and lcdc must 4 pixel for yuv420
	}
	else
	{
       	context->fb_rect.h = (context->fb_rect.h) & (~FB_HEIGHT_ALLIGNED);//dcam 8 pixel and lcdc must 4 pixel for yuv420
	}
	context->fb_rect.w = MIN(context->fb_rect.w, context->fb_width - ((context->fb_rect.x + FB_WIDTH_ALLIGNED) & (~FB_WIDTH_ALLIGNED)));
	context->fb_rect.h = MIN(context->fb_rect.h, context->fb_height - ((context->fb_rect.y + FB_HEIGHT_ALLIGNED) & (~FB_HEIGHT_ALLIGNED)));
	ALOGV("rects {%d,%d,%d,%d}, {%d,%d,%d,%d}", context->src_rect.x,context->src_rect.y,context->src_rect.w,context->src_rect.h,
		context->fb_rect.x, context->fb_rect.y, context->fb_rect.w, context->fb_rect.h);

	if(context->src_rect.w < 4 || context->src_rect.h < 4 ||
		context->fb_rect.w < 4 || context->fb_rect.h < 4 ||
		context->fb_rect.w > 960 || context->fb_rect.h > 960){//dcam scaling > 960 should use slice mode
		return 0;
	}

	dest_width = context->fb_rect.w;
	dest_height = context->fb_rect.h;
 	if ((l->transform&HAL_TRANSFORM_ROT_90) == HAL_TRANSFORM_ROT_90){
 		src_width = context->src_rect.h;
 		src_height = context->src_rect.w;
 	}else{
 		src_width = context->src_rect.w;
		src_height = context->src_rect.h;
	}
	if(4*src_width < dest_width || src_width > 4*dest_width ||
		4*src_height < dest_height || src_height > 4*dest_height){//dcam support 1/4-4 scaling
		return 0;
	}

	return SPRD_LAYERS_IMG;	
}



//--------------------chip independent
static int init_fb_parameter(struct hwc_context_t *context)
{
    char const * const device_template[] = {
        "/dev/graphics/fb%u",
        "/dev/fb%u",
        NULL
    };

    int fd = -1;
    int i = 0;
    char name[64];

    while ((fd == -1) && device_template[i]) {
        snprintf(name, 64, device_template[i], 0);
        fd = open(name, O_RDWR, 0);
        i++;
    }

    if (fd < 0) {
        ALOGE("fail to open fb");
        return -errno;
    }

    struct fb_fix_screeninfo finfo;
    if (ioctl(fd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        ALOGE("fail to get fb finfo");
	close(fd);
        return -errno;
    }

    struct fb_var_screeninfo info;
    if (ioctl(fd, FBIOGET_VSCREENINFO, &info) == -1) {
        ALOGE("fail to get fb nfo");
	close(fd);
        return -errno;
    }
    size_t fbSize = round_up_to_page_size(finfo.line_length * info.yres_virtual);
    void* vaddr = mmap(0, fbSize, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (vaddr == MAP_FAILED) {
        AERR( "Error mapping the framebuffer (%s)", strerror(errno) );
	close(fd);
        return -errno;
    }
    context->fbfd = fd;
    context->fb_width = info.xres;
    context->fb_height = info.yres;
    context->fb_virt_addr = vaddr;
    ALOGI("fb_width = %d,fb_height = %d , finfo.line_length:%d ,  info.yres_virtual:%d",context->fb_width,context->fb_height , finfo.line_length,  info.yres_virtual);
    return 0;
}

void dump_yuv(uint8_t* pBuffer,uint32_t aInBufSize)
{
     FILE *fp = fopen("/data/video.data","ab");
     fwrite(pBuffer,1,aInBufSize,fp);
     fclose(fp);
}

static int set_osd_layer(struct hwc_context_t *context, hwc_layer_t * l)
{
	const native_handle_t *pNativeHandle = l->handle;
	struct private_handle_t *private_h = (struct private_handle_t *)pNativeHandle;	
	uint32_t current_overlay_addr;

	context->osd_sync_display = 0;
	if (private_h->flags & private_handle_t::PRIV_FLAGS_USES_PHY) {
		if (0 == l->transform) {
#ifdef _SUPPORT_SYNC_DISP
			ALOGI_IF(debugenable , "osd display directly");
			context->osd_sync_display = 1;
			current_overlay_addr  = private_h->phyaddr;
			currentOsdOverlayAddr = (char*)private_h->base;
#else
			ALOGI_IF(debugenable , "osd display with rot copy");
			context->osd_sync_display = 0;
			current_overlay_addr = context->overlay_phy_addr2 + context->overlay_buf_size2*context->overlay_index2;
			currentOsdOverlayAddr =  (char*)(context->overlay_v_addr2 + context->overlay_buf_size2*context->overlay_index2);
			int ret = camera_rotation_copy_data(context->fb_width, context->fb_height,  private_h->phyaddr, current_overlay_addr);
			if(-1 == ret)
				ALOGE("do osd rotaion copy  fail");
			context->overlay_index2 = (context->overlay_index2 + 1)%OVERLAY_BUF_NUM;
#endif
		} else {
			ALOGI_IF(debugenable , "osd display with rot");
			current_overlay_addr = context->overlay_phy_addr2 + context->overlay_buf_size2*context->overlay_index2;
			currentOsdOverlayAddr =  (char*) (context->overlay_v_addr2 + context->overlay_buf_size2*context->overlay_index2);
			int degree;
			if (HAL_TRANSFORM_ROT_90 == l->transform)
				degree = 90;
			else if (HAL_TRANSFORM_ROT_270 == l->transform)
				degree = 270;
			else
				degree = 180;				
			int ret = camera_rotation(HW_ROTATION_DATA_RGB888, degree, context->fb_height,context->fb_width,  private_h->phyaddr, current_overlay_addr);
			if(-1 == ret)
				ALOGE("do osd rotaion fail");
			context->overlay_index2 = (context->overlay_index2 + 1)%OVERLAY_BUF_NUM;
		}
	} else {
		ALOGI_IF(debugenable , "osd display with dma copy");
		current_overlay_addr = context->overlay_phy_addr2 + context->overlay_buf_size2*context->overlay_index2;
		currentOsdOverlayAddr =  (char*)(context->overlay_v_addr2 + context->overlay_buf_size2*context->overlay_index2);
		camera_rotation_copy_data_from_virtual(context->fb_width, context->fb_height, private_h->base, current_overlay_addr);
		context->overlay_index2 = (context->overlay_index2 + 1)%OVERLAY_BUF_NUM;
	}
	
	struct overlay_setting ov_setting;
	context->osd_overlay_phy_addr = current_overlay_addr;
	
	ov_setting.layer_index = SPRD_LAYERS_OSD;
	ov_setting.data_type = SPRD_DATA_FORMAT_RGB888;
	ov_setting.y_endian = SPRD_DATA_ENDIAN_B0B1B2B3;
	ov_setting.uv_endian = SPRD_DATA_ENDIAN_B0B1B2B3;
	ov_setting.rb_switch = 1;
	ov_setting.rect.x = 0;
	ov_setting.rect.y = 0;
	ov_setting.rect.w = context->fb_width;
	ov_setting.rect.h = context->fb_height;
	ov_setting.buffer = (unsigned char*)current_overlay_addr;
	ALOGI_IF(debugenable , "osd overlay parameter datatype = %d,x=%d,y=%d,w=%d,h=%d,buffer = %x",ov_setting.data_type,ov_setting.rect.x,ov_setting.rect.y,ov_setting.rect.w,ov_setting.rect.h,ov_setting.buffer);
	if (ioctl(context->fbfd, SPRD_FB_SET_OVERLAY, &ov_setting) == -1)
	{
		ALOGE("fail osd SPRD_FB_SET_OVERLAY");
	}
	osdOverlayFormat = HAL_PIXEL_FORMAT_RGBA_8888;
	return 0;	
}


static int set_video_layer(struct hwc_context_t *context, hwc_layer_t * l)
{
	uint32_t current_overlay_addr;
	int ret;

	current_overlay_addr = context->overlay_phy_addr + context->overlay_buf_size*context->overlay_index;

	ret = transform_video_layer(context, l);

	if(-1 == ret){
		ALOGE("transform_video_layer fail");
	}else{
		//ALOGI("do scale and rot success");
		//dump_yuv((uint8_t* )context->overlay_v_addr + context->overlay_buf_size*context->overlay_index, context->fb_rect.w*context->fb_rect.h*3/2);
		//src: current_overlay_addr,SCALE_DATA_YUV420,context->fb_rect.w,context->fb_rect.h
		//dst: context->fb_rect
		//set video layer
		struct overlay_setting ov_setting;
		ov_setting.layer_index = SPRD_LAYERS_IMG;
#ifdef VIDEO_LAYER_USE_RGB
		ov_setting.data_type = SPRD_DATA_FORMAT_RGB888;
		ov_setting.y_endian = SPRD_DATA_ENDIAN_B0B1B2B3;
		ov_setting.uv_endian = SPRD_DATA_ENDIAN_B0B1B2B3;
		ov_setting.rb_switch = 1;
#else
		ov_setting.data_type = SPRD_DATA_FORMAT_YUV420;
		ov_setting.y_endian = SPRD_DATA_ENDIAN_B3B2B1B0;
		ov_setting.uv_endian = SPRD_DATA_ENDIAN_B3B2B1B0;
		ov_setting.rb_switch = 0;
#endif
		ov_setting.rect.x = context->fb_rect.x;
		ov_setting.rect.y = context->fb_rect.y;
		ov_setting.rect.w = context->fb_rect.w;
		ov_setting.rect.h = context->fb_rect.h;
		ov_setting.buffer = (unsigned char*)current_overlay_addr;
		ALOGI_IF(debugenable , "video overlay parameter datatype = %d,x=%d,y=%d,w=%d,h=%d,buffer = %x",ov_setting.data_type,ov_setting.rect.x,ov_setting.rect.y,ov_setting.rect.w,ov_setting.rect.h,ov_setting.buffer);

		if (ioctl(context->fbfd, SPRD_FB_SET_OVERLAY, &ov_setting) == -1)
		{
			ALOGE("fail video SPRD_FB_SET_OVERLAY");
			ioctl(context->fbfd, SPRD_FB_SET_OVERLAY, &ov_setting);//Fix ME later
		}
		context->overlay_index = (context->overlay_index + 1)%OVERLAY_BUF_NUM;
	}

	 return 0;
}

static int is_overlay_supportted(struct hwc_context_t *context, hwc_layer_t * l)
{
	const native_handle_t *pNativeHandle = l->handle;
	struct private_handle_t *private_h = (struct private_handle_t *)pNativeHandle;

	ALOGI_IF(debugenable , "private_h %x,%d,%d,%x",private_h->format,private_h->width,private_h->height,private_h->phyaddr);

       if(private_h->format == HAL_PIXEL_FORMAT_RGBA_8888) {
		return verify_osd_layer(context,  l);
	} else {
		return verify_video_layer(context,  l);
	}
}


static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static struct hw_module_methods_t hwc_module_methods = {
    open: hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    common: {
        tag: HARDWARE_MODULE_TAG,
        version_major: 1,
        version_minor: 0,
        id: HWC_HARDWARE_MODULE_ID,
        name: "SPRD hwcomposer module",
        author: "The Android Open Source Project",
        methods: &hwc_module_methods,
    }
};

/*****************************************************************************/

static void dump_layer(hwc_layer_t const* l) {
    ALOGI_IF(debugenable , "\ttype=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, {%d,%d,%d,%d}, {%d,%d,%d,%d}",
            l->compositionType, l->flags, l->handle, l->transform, l->blending,
            l->sourceCrop.left,
            l->sourceCrop.top,
            l->sourceCrop.right,
            l->sourceCrop.bottom,
            l->displayFrame.left,
            l->displayFrame.top,
            l->displayFrame.right,
            l->displayFrame.bottom);
}


static int hwc_prepare(hwc_composer_device_t *dev, hwc_layer_list_t* list) {
	//is the list ordered in z order???
	struct hwc_context_t *ctx = (struct hwc_context_t *)dev;
	hwc_layer_t * overlay_video = NULL;
	hwc_layer_t * overlay_osd = NULL;
	char value[PROPERTY_VALUE_MAX];

	if(!list)
		return 0;
	//reset dump index and recalculate random number when geometry changed
	if(list->flags & HWC_GEOMETRY_CHANGED)
	{
		if(bFirstGeometroyChanged == false)
		{
			bFirstGeometroyChanged =  true;
			g_GeometoryChanged_Num = 0;
		}
		else
		{
			g_GeometoryChanged_Num++;
		}
		g_GeometryChanged = true;
	}
	else
	{
		g_GeometryChanged = false;
	}
	/*****************check dump flag and dump layers if true*************************/
	if(0 != property_get("dump.hwcomposer.flag" , value , "0"))
	{
		int flag = atoi(value);
		static int index = 0;
		char dumpPath[MAX_DUMP_PATH_LENGTH];
		if(HWCOMPOSER_DUMP_ORIGINAL_LAYERS & flag)
		{
			getDumpPath(dumpPath);
			if(g_GeometryChanged)
			{
				index = 0;
			}
			for (size_t i=0 ; i<list->numHwLayers ; i++) {
				hwc_layer_t * layer_t = &(list->hwLayers[i]);
				struct private_handle_t *private_h = (struct private_handle_t *)layer_t->handle;
				if(private_h == NULL)
				{
					continue;
				}
				dump_layer(dumpPath , (char*)private_h->base , "Layer" , private_h->width , private_h->height , private_h->format , g_GeometoryChanged_Num , index , i);
			}
			index++;
		}
	}
	/***************************************dump end****************************/
	ALOGI_IF(debugenable,"hwc_prepare %d b", list->numHwLayers);
	ctx->fb_layer_count = 0;
	ctx->osd_overlay_flag = 0;
	ctx->video_overlay_flag = 0;

       for (size_t i=0 ; i<list->numHwLayers ; i++) {
		dump_layer(&list->hwLayers[i]);
		list->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
		if((list->hwLayers[i].flags & HWC_SKIP_LAYER)|| !list->hwLayers[i].handle) {
			ctx->fb_layer_count++;
			ALOGI_IF(debugenable , "skip_layer %p , flags:%d , GeometryNum:%lld",list->hwLayers[i].handle , list->hwLayers[i].flags , g_GeometoryChanged_Num);
			continue;
		}
		int  support_overlay = is_overlay_supportted((struct hwc_context_t *)dev, &list->hwLayers[i]);
		if((support_overlay == SPRD_LAYERS_IMG) && (ctx->video_overlay_flag == 0)) {
			ctx->video_overlay_flag = 1;
			list->hwLayers[i].compositionType = HWC_OVERLAY;
			if(i !=0)
			    list->hwLayers[i].hints = HWC_HINT_CLEAR_FB;
			else
			    list->hwLayers[i].hints &= ~HWC_HINT_CLEAR_FB;
			overlay_video = &list->hwLayers[i];
			ALOGI_IF(debugenable , "find video overlay %d",list->hwLayers[i].handle);
		} else if((support_overlay == SPRD_LAYERS_OSD) && (ctx->osd_overlay_flag == 0) && (i == 1)) {
			if ( list->numHwLayers >= 3) {
				ctx->fb_layer_count++;
			} else {
				ctx->osd_overlay_flag = 1;
				list->hwLayers[i].compositionType = HWC_OVERLAY;
				//list->hwLayers[i].hints = HWC_HINT_CLEAR_FB;//must set it???
				overlay_osd = &list->hwLayers[i];
				ALOGI_IF(debugenable , "find osd overlay %d",list->hwLayers[i].handle);
			}
		}else {
			ctx->fb_layer_count++;
		}
	}

	if(ctx->osd_overlay_flag && !ctx->video_overlay_flag) {
		overlay_osd->compositionType = HWC_FRAMEBUFFER;
		ctx->osd_overlay_flag = 0;
		ctx->fb_layer_count++;
		ALOGI_IF(debugenable , "no video layer, abandon osd overlay");
	}

	if ((ctx->pre_fb_layer_count != ctx->fb_layer_count) && overlay_video) {
		/*no blending use gpu, can be optimized*/
		//overlay_video->compositionType = HWC_FRAMEBUFFER;
		//ctx->video_overlay_flag = 0;
		//ctx->fb_layer_count++;
		//ALOGI("----------------------clear fb");
		//overlay_video->hints = HWC_HINT_CLEAR_FB;
	}

	//if(1) {
	//	if (ctx->procs && ctx->procs->invalidate) {
	//		ALOGI("procs->invalidate");
       //		ctx->procs->invalidate(ctx->procs);
	//	}
	//}
	ctx->pre_fb_layer_count = ctx->fb_layer_count;

	ALOGI_IF(debugenable , "fb_layer_count %d",ctx->fb_layer_count);
	ALOGI_IF(debugenable , "hwc_prepare %d e", list->numHwLayers);

	return 0;
}


#ifdef _PROC_OSD_WITH_THREAD
static void* osd_thread_proc(void* data)
{
	ALOGI("osd_thread_proc in");
	struct hwc_context_t *dev = (struct hwc_context_t *)data;
	while (1) {
		sem_wait(&dev->cmd_sem);
		if (dev->osd_proc_cmd) {
			set_osd_layer(dev, (hwc_layer_t *)dev->osd_proc_cmd);
			sem_post(&dev->done_sem);
		} else {
			break;
		}
	}
	ALOGI("osd_thread_proc out");
	return NULL;
}
#endif

static int hwc_set(hwc_composer_device_t *dev,
        hwc_display_t dpy,
        hwc_surface_t sur,
        hwc_layer_list_t* list)
{
    struct hwc_context_t *ctx = (struct hwc_context_t *)dev;
    char value[PROPERTY_VALUE_MAX];
    char dumpPath[MAX_DUMP_PATH_LENGTH];
    property_get("debug.hwcomposer.info" , value , "0");
    if(atoi(value) == 1)
        debugenable = 1;
    else
        debugenable = 0;
    if (dpy == NULL && sur == NULL && list == NULL) {
        // release our resources, the screen is turning off
        // in our case, there is nothing to do.
        return 0;
    }
    if (list == NULL) {
	ALOGW("hwc_set list == NULL");
	EGLBoolean sucess = eglSwapBuffers((EGLDisplay)dpy, (EGLSurface)sur);
	if (!sucess) {
		return HWC_EGL_ERROR;
	}
	return 0;
    }
    //ALOGI("hwc_set %d b", list->numHwLayers);
    for (size_t i=0 ; i<list->numHwLayers ; i++) {
        //dump_layer(&list->hwLayers[i]);
    }


    //ALOGI("hwc_set %d e", list->numHwLayers);

	//ALOGI("set overlay layer begin-----------------");
	hwc_layer_t * video_layer = NULL;
	hwc_layer_t * osd_layer = NULL;
	for (size_t i=0 ; i<list->numHwLayers ; i++) {
		const native_handle_t *pNativeHandle = list->hwLayers[i].handle;
		struct private_handle_t *private_h = (struct private_handle_t *)pNativeHandle;
		if (private_h) {
		//ALOGI("layer virtual addr %x -------------------------", private_h->base);
		}
		if (HWC_OVERLAY == list->hwLayers[i].compositionType) {
			const native_handle_t *pNativeHandle = list->hwLayers[i].handle;
			struct private_handle_t *private_h = (struct private_handle_t *)pNativeHandle;
			if (private_h->format == HAL_PIXEL_FORMAT_RGBA_8888) {
				//set_osd_layer((struct hwc_context_t *)dev, &list->hwLayers[i]);
				osd_layer = &list->hwLayers[i];
			} else {
				//set_video_layer((struct hwc_context_t *)dev, &list->hwLayers[i]);
				video_layer = &list->hwLayers[i];
			}
		}
	}
#ifdef _PROC_OSD_WITH_THREAD
	if (osd_layer) {
		ALOGI_IF(debugenable , "send command to osd proc thread");
		ctx->osd_proc_cmd = osd_layer;
		sem_post(&ctx->cmd_sem);
	}
#endif
	if (video_layer) {
		set_video_layer((struct hwc_context_t *)dev, video_layer);
	}

	if (osd_layer) {
#ifdef _PROC_OSD_WITH_THREAD
		sem_wait(&ctx->done_sem);
#else
		set_osd_layer((struct hwc_context_t *)dev, osd_layer);
#endif
	}

	//ALOGI("set overlay layer end-----------------");

	if ((ctx->fb_layer_count == 0) && (ctx->video_overlay_flag || ctx->osd_overlay_flag)) {
		int layer_indexs = 0;
		struct overlay_display_setting display_setting;
		display_setting.display_mode = SPRD_DISPLAY_OVERLAY_ASYNC;
		if (ctx->video_overlay_flag)
			layer_indexs |= SPRD_LAYERS_IMG;
		if (ctx->osd_overlay_flag) {
			layer_indexs |= SPRD_LAYERS_OSD;
			if (ctx->osd_sync_display)
				display_setting.display_mode = SPRD_DISPLAY_OVERLAY_SYNC;
		}
		display_setting.layer_index = layer_indexs;
		display_setting.rect.x = 0;
		display_setting.rect.y = 0;
		display_setting.rect.w = ctx->fb_width;
		display_setting.rect.h = ctx->fb_height;
		/*************************dump overlay if need******************************/
		if(0 != property_get("dump.hwcomposer.flag" , value , "0"))
		{
			int flag = atoi(value);
			if((HWCOMPOSER_DUMP_VIDEO_OVERLAY_FLAG & flag) && (ctx->video_overlay_flag))
			{
				static int index = 0;
				getDumpPath(dumpPath);
				if(g_GeometryChanged)
				{
					index = 0;
				}
				dump_layer(dumpPath ,currentVideoOverlayAddr , "OverlayVideo" , ctx->fb_rect.w , ctx->fb_rect.h , videoOverlayFormat , g_GeometoryChanged_Num , index);
				index++;
			}
			if((HWCOMPOSER_DUMP_OSD_OVERLAY_FLAG & flag) && (ctx->osd_overlay_flag))
			{
				static int index = 0;
				getDumpPath(dumpPath);
				if(g_GeometryChanged)
				{
					index = 0;
				}
				dump_layer(dumpPath ,currentOsdOverlayAddr , "OverlayOsd" , ctx->fb_width , ctx->fb_height , osdOverlayFormat , g_GeometoryChanged_Num , index);
				index++;
			}
		}
		/*************************dump end**************************************/
		ALOGI_IF(debugenable,"SPRD_FB_DISPLAY_OVERLAY %d", layer_indexs);
		ioctl(ctx->fbfd, SPRD_FB_DISPLAY_OVERLAY, &display_setting);
	} else {
		if ((ctx->video_overlay_flag||ctx->osd_overlay_flag)) {
			ALOGI_IF(debugenable , "eglSwapBuffers video=%d, osd=%d", ctx->video_overlay_flag, ctx->osd_overlay_flag );
			/*************************dump overlay if need******************************/
			if(0 != property_get("dump.hwcomposer.flag" , value , "0"))
			{
				int flag = atoi(value);
				if(ctx->video_overlay_flag)
				{
					if(HWCOMPOSER_DUMP_VIDEO_OVERLAY_FLAG & flag)
					{
						static int index = 0;
						getDumpPath(dumpPath);
						if(g_GeometryChanged)
						{
							index = 0;
						}
						dump_layer(dumpPath ,currentVideoOverlayAddr , "OverlayVideo" , ctx->fb_rect.w , ctx->fb_rect.h , videoOverlayFormat , g_GeometoryChanged_Num , index);
						index++;
					}
				}
			}
			/*************************dump end**************************************/
		}
		ALOGI_IF(debugenable , "eglSwapBuffers");
		EGLBoolean sucess = eglSwapBuffers((EGLDisplay)dpy, (EGLSurface)sur);
		/*************************dump framebuffer if need******************************/
		if(0 != property_get("dump.hwcomposer.flag" , value , "0"))
		{
			int flag = atoi(value);
			if(HWCOMPOSER_DUMP_FRAMEBUFFER_FLAG & flag)
			{
				static int index = 0;
				FrameBufferInfo fbInfo;
				int format;
				getDumpPath(dumpPath);
				if(g_GeometryChanged)
				{
					index = 0;
				}
				if(0 == getFrameBufferInfo(ctx , &fbInfo))
				{
					dump_layer(dumpPath ,fbInfo.pFrontAddr, "FrameBuffer" , ctx->fb_width , ctx->fb_height , fbInfo.format , g_GeometoryChanged_Num , index);
				}
				index++;
			}
		}
		/*************************dump end**************************************/
		if (!sucess) {
			return HWC_EGL_ERROR;
		}
	}
	return 0;
}


static int hwc_eventControl(struct hwc_composer_device* dev, int event, int enabled)
{
    struct hwc_context_t *hwc_dev = (struct hwc_context_t *) dev;

    switch(event)
    {
    case HWC_EVENT_VSYNC:
        if (hwc_dev->mVSyncThread != 0)
            hwc_dev->mVSyncThread->setEnabled(enabled);
        break;
    default:
        break;
    }
    return 0;
}
static hwc_methods_t hwc_device_methods = {
    eventControl: hwc_eventControl
};


static int hwc_device_close(struct hw_device_t *dev)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    if (ctx) {

	if (ctx->fbfd) {
	    close(ctx->fbfd);
	}
	if (ctx->ion_heap != NULL) {
	    ctx->ion_heap.clear();
	}

#ifdef _ALLOC_OSD_BUF	
	if (ctx->ion_heap2 != NULL) {
	    ctx->ion_heap2.clear();
	}
#endif

#ifdef SCAL_ROT_TMP_BUF
	if (ctx->ion_heap_tmp != NULL) {
	    ctx->ion_heap_tmp.clear();
	}	
#endif
#ifdef _PROC_OSD_WITH_THREAD
	ctx->osd_proc_cmd = NULL;
	sem_post(&ctx->cmd_sem);
 	void *dummy;
    	pthread_join(ctx->osd_proc_thread, &dummy);
	sem_destroy(&ctx->cmd_sem);
	sem_destroy(&ctx->done_sem);
#endif
	hwc_eventControl(&ctx->device, HWC_EVENT_VSYNC, 0);
	if (ctx->mVSyncThread != NULL) {
		ctx->mVSyncThread->requestExitAndWait();
	}
	free(ctx);
    }
    return 0;
}

static void hwc_registerProcs(struct hwc_composer_device* dev,
                                    hwc_procs_t const* procs)
{
    struct hwc_context_t *hwc_dev = (struct hwc_context_t *) dev;

    hwc_dev->procs = (typeof(hwc_dev->procs)) procs;
}

/*****************************************************************************/

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = -EINVAL;
    if (!strcmp(name, HWC_HARDWARE_COMPOSER)) {
        struct hwc_context_t *dev;
        dev = (hwc_context_t*)malloc(sizeof(*dev));

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = HWC_DEVICE_API_VERSION_0_3;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = hwc_device_close;

        dev->device.prepare = hwc_prepare;
        dev->device.set = hwc_set;
        dev->device.registerProcs = hwc_registerProcs;
        dev->device.methods = &hwc_device_methods;

#ifdef _PROC_OSD_WITH_THREAD
	sem_init(&dev->cmd_sem, 0, 0);
	sem_init(&dev->done_sem, 0, 0);

    	pthread_attr_t attr;
    	pthread_attr_init(&attr);
    	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    	pthread_create(&dev->osd_proc_thread, &attr, osd_thread_proc, dev);
    	pthread_attr_destroy(&attr);
#endif
	
        *device = &dev->device.common;

        status = init_fb_parameter(dev);

	if (0 == status) {
#ifdef VIDEO_LAYER_USE_RGB
		dev->overlay_buf_size = round_up_to_page_size(dev->fb_width*dev->fb_height*4);
#else
		dev->overlay_buf_size = round_up_to_page_size(dev->fb_width*dev->fb_height*3/2);
#endif
		dev->ion_heap = new MemoryHeapIon(SPRD_ION_DEV, dev->overlay_buf_size*OVERLAY_BUF_NUM, MemoryHeapBase::NO_CACHING, (1 << ION_HEAP_CARVEOUT_ID1));
     		int fd = dev->ion_heap->getHeapID();
		if (fd >= 0) {
	 		int ret,phy_addr, buffer_size;
	 		ret = dev->ion_heap->get_phy_addr_from_ion(&phy_addr, &buffer_size);
			if(ret) {
				status = -EINVAL;
				ALOGE("get_phy_addr_from_ion failed");
			}
			dev->overlay_phy_addr = phy_addr;
			dev->overlay_v_addr = dev->ion_heap->base();
			dev->overlay_index = 0;
			ALOGI("overlay_phy_addr = %x",dev->overlay_phy_addr);
		} else {
			status = -EINVAL;
			ALOGE("alloc overlay buffer failed");
		}
#ifdef _ALLOC_OSD_BUF
		dev->overlay_buf_size2 = round_up_to_page_size(dev->fb_width*dev->fb_height*4);
		dev->ion_heap2 = new MemoryHeapIon(SPRD_ION_DEV, dev->overlay_buf_size2*OVERLAY_BUF_NUM, MemoryHeapBase::NO_CACHING, (1 << ION_HEAP_CARVEOUT_ID1));
     		int fd2 = dev->ion_heap2->getHeapID();
		if (fd2 >= 0) {
	 		int ret,phy_addr, buffer_size;
	 		ret = dev->ion_heap2->get_phy_addr_from_ion(&phy_addr, &buffer_size);
			if(ret) {
				status = -EINVAL;
				ALOGE("osd get_phy_addr_from_ion failed");
			}
			dev->overlay_phy_addr2 = phy_addr;
			dev->overlay_v_addr2 = dev->ion_heap2->base();
			dev->overlay_index2 = 0;
			ALOGI("osd overlay_phy_addr = %x",dev->overlay_phy_addr2);
		} else {
			status = -EINVAL;
			ALOGE("osd alloc overlay buffer failed");
		}
#endif

#ifdef SCAL_ROT_TMP_BUF
		dev->overlay_buf_size_tmp = round_up_to_page_size(dev->fb_width*dev->fb_height*3/2);
		dev->ion_heap_tmp = new MemoryHeapIon(SPRD_ION_DEV, dev->overlay_buf_size_tmp, MemoryHeapBase::NO_CACHING, (1 << ION_HEAP_CARVEOUT_ID1));
     		int fd_tmp = dev->ion_heap_tmp->getHeapID();
		if (fd_tmp >= 0) {
	 		int ret,phy_addr, buffer_size;
	 		ret = dev->ion_heap_tmp->get_phy_addr_from_ion(&phy_addr, &buffer_size);
			if(ret) {
				status = -EINVAL;
				ALOGE("tmp  get_phy_addr_from_ion failed");
			}
			dev->overlay_phy_addr_tmp = phy_addr;
			dev->overlay_v_addr_tmp = dev->ion_heap_tmp->base();
			ALOGI("tmp  overlay_phy_addr = %x",dev->overlay_phy_addr_tmp);
		} else {
			status = -EINVAL;
			ALOGE("tmp  alloc overlay buffer failed");
		}
#endif
	}
	dev->mVSyncThread = new VSyncThread(dev);
    }
    return status;
}
