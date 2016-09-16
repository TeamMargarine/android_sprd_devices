/*
 * Copyright (C) 2008 The Android Open Source Project
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
#include <linux/ion.h>
#include <binder/MemoryHeapIon.h>
#include <camera/Camera.h>
#include <semaphore.h>
#include "SprdOEMCamera.h"
#include "isp_cali_interface.h"
#include "sensor_drv_u.h"

using namespace android;

#define ERR(x...) fprintf(stderr, ##x)
#define INFO(x...) fprintf(stdout, ##x)

#define UTEST_PARAM_NUM 11

enum utest_sensor_id {
	UTEST_SENSOR_MAIN = 0,
	UTEST_SENSOR_SUB,
	UTEST_SENSOR_ID_MAX
};

enum utest_calibration_cmd_id {
	UTEST_CALIBRATION_AWB= 0,
	UTEST_CALIBRATION_LSC,
	UTEST_CALIBRATION_FLASHLIGHT,
	UTEST_CALIBRATION_MAX
};

struct utest_cmr_context {
	uint32_t sensor_id;
	uint32_t cmd;
	uint32_t capture_raw_vir_addr;
	uint32_t capture_width;
	uint32_t capture_height;
	char *save_directory;

	sp<MemoryHeapIon> cap_pmem_hp;
	uint32_t cap_pmemory_size;
	int cap_physical_addr ;
	unsigned char* cap_virtual_addr;
	sp<MemoryHeapIon> misc_pmem_hp;
	uint32_t misc_pmemory_size;
	int misc_physical_addr;
	unsigned char* misc_virtual_addr;

	sem_t sem_cap_done;
};

static char calibration_awb_file[] = "/data/sensor_%s_awb.dat";
static char calibration_flashlight_file[] = "/data/sensor_%s_flashlight.dat";
static char calibration_lsc_file[] = "/data/sensor_%s_lnc_%d_%d_%d.dat";

static struct utest_cmr_context utest_cmr_cxt;
static struct utest_cmr_context *g_utest_cmr_cxt_ptr = &utest_cmr_cxt;

static void utest_dcam_usage()
{
	INFO("utest_dcam_usage:\n");
	INFO("utest_camera -cmd cali_cmd -id sensor_id -w capture_width -h capture_height -d output_directory \n");
	INFO("-cmd	: calibration command\n");
	INFO("-id	: select sensor id(0: main sensor / 1: sub sensor)\n");
	INFO("-w	: captured picture size width\n");
	INFO("-h	: captured picture size width\n");
	INFO("-d	: directory for saving the captured picture\n");
	INFO("-help	: show this help message\n");
}

static int utest_dcam_param_set(int argc, char **argv)
{
	int i = 0;
	struct utest_cmr_context *cmr_cxt_ptr = g_utest_cmr_cxt_ptr;

	if (argc < UTEST_PARAM_NUM) {
		utest_dcam_usage();
		return -1;
	}

	memset(cmr_cxt_ptr, 0, sizeof(utest_cmr_context));

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-cmd") == 0 && (i < argc-1)) {
			cmr_cxt_ptr->cmd = atoi(argv[++i]);
			if (UTEST_CALIBRATION_MAX <= cmr_cxt_ptr->cmd) {
				ERR("the calibration command is invalid.\n");
				return -1;
			}
		}else if (strcmp(argv[i], "-id") == 0 && (i < argc-1)) {
			cmr_cxt_ptr->sensor_id = atoi(argv[++i]);
			if (UTEST_SENSOR_SUB < cmr_cxt_ptr->sensor_id) {
				ERR("the sensor id must be 0: main_sesor 1: sub_sensor.\n");
				return -1;
			}
		} else if (strcmp(argv[i], "-w") == 0 && (i < argc-1)) {
			cmr_cxt_ptr->capture_width = atoi(argv[++i]);
			if((0 >= cmr_cxt_ptr->capture_width) ||
				(cmr_cxt_ptr->capture_width % 2)) {
				ERR("the width of captured picture is invalid.\n");
				return -1;
			}
		} else if (strcmp(argv[i], "-h") == 0 && (i < argc-1)) {
			cmr_cxt_ptr->capture_height = atoi(argv[++i]);
			if((0 >= cmr_cxt_ptr->capture_height) ||
				(cmr_cxt_ptr->capture_height % 2)) {
				ERR("the height of captured picture is invalid.\n");
				return -1;
			}
		} else if (strcmp(argv[i], "-d") == 0 && (i < argc-1)) {
			cmr_cxt_ptr->save_directory = argv[++i];
		} else if (strcmp(argv[i], "-help") == 0) {
			utest_dcam_usage();
			return -1;
		} else {
			utest_dcam_usage();
			return -1;
		}
	}

	return 0;
}

static void utest_dcam_memory_release(void)
{
	struct utest_cmr_context *cmr_cxt_ptr = g_utest_cmr_cxt_ptr;

	if (cmr_cxt_ptr->cap_physical_addr)
		cmr_cxt_ptr->cap_pmem_hp.clear();

	if (cmr_cxt_ptr->misc_physical_addr)
		cmr_cxt_ptr->misc_pmem_hp.clear();
}

static int utest_dcam_memory_alloc(void)
{
	uint32_t local_width, local_height;
	uint32_t mem_size0, mem_size1;
	uint32_t buffer_size = 0;
	struct utest_cmr_context *cmr_cxt_ptr = g_utest_cmr_cxt_ptr;

	if (camera_capture_max_img_size(&local_width, &local_height))
		return -1;

	if (camera_capture_get_buffer_size(cmr_cxt_ptr->sensor_id, local_width,
		local_height, &mem_size0, &mem_size1))
		return -1;

	buffer_size = camera_get_size_align_page(mem_size0);
	cmr_cxt_ptr->cap_pmem_hp = new MemoryHeapIon("/dev/ion", buffer_size,
		MemoryHeapBase::NO_CACHING, ION_HEAP_CARVEOUT_MASK);
	if (cmr_cxt_ptr->cap_pmem_hp->getHeapID() < 0) {
		ERR("failed to alloc capture pmem buffer.\n");
		return -1;
	}
	cmr_cxt_ptr->cap_pmem_hp->get_phy_addr_from_ion((int *)(&cmr_cxt_ptr->cap_physical_addr),
		(int *)(&cmr_cxt_ptr->cap_pmemory_size));
	cmr_cxt_ptr->cap_virtual_addr = (unsigned char*)cmr_cxt_ptr->cap_pmem_hp->base();
	if (!cmr_cxt_ptr->cap_physical_addr) {
		ERR("failed to alloc capture pmem buffer:addr is null.\n");
		return -1;
	}

	if (mem_size1) {
		buffer_size = camera_get_size_align_page(mem_size1);
		cmr_cxt_ptr->misc_pmem_hp = new MemoryHeapIon("/dev/ion", buffer_size,
			MemoryHeapBase::NO_CACHING, ION_HEAP_CARVEOUT_MASK);
		if (cmr_cxt_ptr->misc_pmem_hp->getHeapID() < 0) {
			ERR("failed to alloc misc pmem buffer.\n");
			return -1;
		}
		cmr_cxt_ptr->misc_pmem_hp->get_phy_addr_from_ion((int *)(&cmr_cxt_ptr->misc_physical_addr),
			(int *)(&cmr_cxt_ptr->misc_pmemory_size));
		cmr_cxt_ptr->misc_virtual_addr = (unsigned char*)cmr_cxt_ptr->misc_pmem_hp->base();
		if (!cmr_cxt_ptr->misc_physical_addr) {
			ERR("failed to alloc misc  pmem buffer:addr is null.\n");
			utest_dcam_memory_release();
			return -1;
		}
	}

	if(0 != mem_size1) {
		if (camera_set_capture_mem(0,
			(uint32_t)cmr_cxt_ptr->cap_physical_addr,
			(uint32_t)cmr_cxt_ptr->cap_virtual_addr,
			(uint32_t)cmr_cxt_ptr->cap_pmemory_size,
			(uint32_t)cmr_cxt_ptr->misc_physical_addr,
			(uint32_t)cmr_cxt_ptr->misc_virtual_addr,
			(uint32_t)cmr_cxt_ptr->misc_pmemory_size)) {
				utest_dcam_memory_release();
				return -1;
		}
	} else {
		if (camera_set_capture_mem(0,
			(uint32_t)cmr_cxt_ptr->cap_physical_addr,
			(uint32_t)cmr_cxt_ptr->cap_virtual_addr,
			(uint32_t)cmr_cxt_ptr->cap_pmemory_size,
			0,
			0,
			0)) {
				utest_dcam_memory_release();
				return -1;
		}
	}

	return 0;
}

static void utest_dcam_close(void)
{
	utest_dcam_memory_release();
	camera_stop(NULL, NULL);
}

static int32_t utest_dcam_awb(void)
{
	struct utest_cmr_context *cmr_cxt_ptr = g_utest_cmr_cxt_ptr;
	SENSOR_EXP_INFO_T *sensor_ptr = (SENSOR_EXP_INFO_T*)Sensor_GetInfo();
	struct isp_addr_t img_addr = {0, 0, 0};
	struct isp_rect_t rect = {0, 0, 0,0};
	struct isp_size_t img_size = {0, 0};
	struct isp_bayer_ptn_stat_t stat_param;
	struct isp_addr_t dst_addr = {0, 0, 0};
	uint32_t *dst_temp_addr = NULL;
	char file_name[128] = {0};
	FILE *fp = NULL;
	int32_t rtn = 0;

	memset(&stat_param, 0, sizeof(isp_bayer_ptn_stat_t));
	img_addr.y_addr = cmr_cxt_ptr->capture_raw_vir_addr;
	rect.x = 0;
	rect.y = 0;
	rect.width = cmr_cxt_ptr->capture_width;
	rect.height = cmr_cxt_ptr->capture_height;
	img_size.width = cmr_cxt_ptr->capture_width;
	img_size.height = cmr_cxt_ptr->capture_height;
#if 0
	dst_temp_addr = (uint32_t *)malloc(cmr_cxt_ptr->capture_width * cmr_cxt_ptr->capture_height * 2);

	if(dst_temp_addr) {

		dst_addr.y_addr = (uint32_t)dst_temp_addr;
		if (ISP_Cali_UnCompressedPacket(img_addr, dst_addr,img_size, 1)) {
			ERR("utest_dcam_awb: failed to unCompresse.\n");
			rtn = -1;
			goto awb_exit;
		}
	} else {
		ERR("utest_dcam_awb: failed to alloc buffer.\n");
		return -1;
	}
#else
dst_addr.y_addr = img_addr.y_addr;
dst_addr.uv_addr = img_addr.uv_addr;
dst_addr.v_addr = img_addr.v_addr;
#endif

	if (ISP_Cali_RawRGBStat(&dst_addr, &rect, &img_size, &stat_param)) {
		ERR("utest_dcam_awb: failed to isp_cali_raw_rgb_status.\n");
		rtn = -1;
		goto awb_exit;
	}

	sprintf(file_name, calibration_awb_file, sensor_ptr->name, cmr_cxt_ptr->capture_width,
			cmr_cxt_ptr->capture_height, 0);
	fp = fopen(file_name, "wb");
	if (fp >= 0) {
		fwrite((void *)(&stat_param), 1, sizeof(stat_param), fp);
		fclose(fp);
	} else {
		ERR("utest_dcam_awb: failed to open calibration_awb_file.\n");
		rtn = -1;
		goto awb_exit;
	}

	if (cmr_cxt_ptr->save_directory) {
		sprintf(file_name, "%s_%dX%d.raw", cmr_cxt_ptr->save_directory, cmr_cxt_ptr->capture_width,
				cmr_cxt_ptr->capture_height);
		fp = fopen(file_name, "wb");
		if (fp >= 0) {
			fwrite((void *)cmr_cxt_ptr->capture_raw_vir_addr , 1, cmr_cxt_ptr->capture_width * 
			cmr_cxt_ptr->capture_height * 2, fp);
			fclose(fp);
		} else {
			ERR("utest_dcam_awb: failed to open save_directory.\n");
			rtn = -1;
			goto awb_exit;
		}
	}

awb_exit:
	free(dst_temp_addr);
	return rtn;
}


static int32_t utest_dcam_flashlight(void)
{
	struct utest_cmr_context *cmr_cxt_ptr = g_utest_cmr_cxt_ptr;
	SENSOR_EXP_INFO_T *sensor_ptr = (SENSOR_EXP_INFO_T*)Sensor_GetInfo();
	struct isp_addr_t img_addr = {0, 0, 0};
	struct isp_rect_t rect = {0, 0, 0,0};
	struct isp_size_t img_size = {0, 0};
	struct isp_bayer_ptn_stat_t stat_param;
	struct isp_addr_t dst_addr = {0, 0, 0};
	uint32_t *dst_temp_addr = NULL;
	char file_name[128] = {0};
	FILE *fp = NULL;
	int32_t rtn = 0;

	memset(&stat_param, 0, sizeof(isp_bayer_ptn_stat_t));
	img_addr.y_addr = cmr_cxt_ptr->capture_raw_vir_addr;
	rect.x = 0;
	rect.y = 0;
	rect.width = cmr_cxt_ptr->capture_width;
	rect.height = cmr_cxt_ptr->capture_height;
	img_size.width = cmr_cxt_ptr->capture_width;
	img_size.height = cmr_cxt_ptr->capture_height;

#if 0
	dst_temp_addr = (uint32_t *)malloc(cmr_cxt_ptr->capture_width * cmr_cxt_ptr->capture_height * 2);

	if(dst_temp_addr) {
		dst_addr.y_addr = (uint32_t)dst_temp_addr;
		if (ISP_Cali_UnCompressedPacket(img_addr, dst_addr,img_size, 1)) {
			ERR("utest_dcam_flashlight: failed to unCompresse.\n");
			rtn = -1;
			goto awb_exit;
		}

	} else {
		ERR("utest_dcam_flashlight: failed to alloc buffer.\n");
		return -1;
	}
#else
dst_addr.y_addr = img_addr.y_addr;
dst_addr.uv_addr = img_addr.uv_addr;
dst_addr.v_addr = img_addr.v_addr;
#endif

	if (ISP_Cali_RawRGBStat(&dst_addr, &rect, &img_size, &stat_param)) {
		ERR("utest_dcam_flashlight: failed to isp_cali_raw_rgb_status.\n");
		rtn = -1;
		goto awb_exit;
	}

	sprintf(file_name, calibration_flashlight_file, sensor_ptr->name, cmr_cxt_ptr->capture_width,
			cmr_cxt_ptr->capture_height, 0);
	fp = fopen(file_name, "wb");
	if (fp >= 0) {
		fwrite((void *)(&stat_param), 1, sizeof(stat_param), fp);
		fclose(fp);
	} else {
		ERR("utest_dcam_flashlight: failed to open calibration_awb_file.\n");
		rtn = -1;
		goto awb_exit;
	}

	if (cmr_cxt_ptr->save_directory) {
		sprintf(file_name, "%s_%dX%d.mipi_raw", cmr_cxt_ptr->save_directory, cmr_cxt_ptr->capture_width,
				cmr_cxt_ptr->capture_height);
		fp = fopen(file_name, "wb");
		if (fp >= 0) {
			fwrite((void *)cmr_cxt_ptr->capture_raw_vir_addr , 1, cmr_cxt_ptr->capture_width * 
			cmr_cxt_ptr->capture_height * 2, fp);
			fclose(fp);
		} else {
			ERR("utest_dcam_flashlight: failed to open save_directory.\n");
			rtn = -1;
			goto awb_exit;
		}
	}

awb_exit:
	if (dst_temp_addr) {
		free(dst_temp_addr);
	}
	return rtn;
}



static int32_t utest_dcam_lsc(void)
{
	struct utest_cmr_context *cmr_cxt_ptr = g_utest_cmr_cxt_ptr;
	SENSOR_EXP_INFO_T *sensor_ptr = (SENSOR_EXP_INFO_T*)Sensor_GetInfo();
	struct isp_addr_t img_addr = {0, 0, 0};
	struct isp_rect_t rect = {0, 0, 0, 0};
	struct isp_size_t img_size = {0, 0};
	struct isp_addr_t dst_addr = {0, 0, 0};
	uint32_t *dst_temp_addr = NULL;
	uint32_t *len_tab_addr = NULL;
	uint32_t len_tab_size = 0;
	int32_t rtn = 0;
	char file_name[128] = {0};
	FILE *fp = NULL;

	img_addr.y_addr = cmr_cxt_ptr->capture_raw_vir_addr;
	rect.x = 0;
	rect.y = 0;
	rect.width = cmr_cxt_ptr->capture_width;
	rect.height = cmr_cxt_ptr->capture_height;
	img_size.width = cmr_cxt_ptr->capture_width;
	img_size.height = cmr_cxt_ptr->capture_height;

	ISP_Cali_GetLensTabSize(img_size, 32, &len_tab_size);

	len_tab_addr = (uint32_t *)malloc(len_tab_size);
	if (NULL == len_tab_addr) {
		rtn = -1;
		ERR("utest_dcam_lsc: failed to alloc buffer.\n");
		goto lsc_exit;
	}
#if 0
	dst_temp_addr = (uint32_t *)malloc(cmr_cxt_ptr->capture_width * cmr_cxt_ptr->capture_height * 2);

	if (NULL == len_tab_addr || NULL == dst_temp_addr) {
		rtn = -1;
		ERR("utest_dcam_lsc: failed to alloc buffer.\n");
		goto lsc_exit;
	}

	dst_addr.y_addr = (uint32_t)dst_temp_addr;

	if (ISP_Cali_UnCompressedPacket(img_addr, dst_addr,img_size, 1)) {
		rtn = -1;
		ERR("utest_dcam_lsc: failed to unCompresse.\n");
		goto lsc_exit;
	}
#else
	dst_addr.y_addr = img_addr.y_addr;
	dst_addr.uv_addr = img_addr.uv_addr;
	dst_addr.v_addr = img_addr.v_addr;
#endif

	if (ISP_Cali_GetLensTabs(dst_addr, 32, img_size, len_tab_addr, 0, 0)) {
		rtn = -1;
		ERR("utest_dcam_lsc: fail to isp_cali_getlenstab.\n");
		goto lsc_exit;
	}


	sprintf(file_name, calibration_lsc_file,sensor_ptr->name, cmr_cxt_ptr->capture_width,
			cmr_cxt_ptr->capture_height, 0);
	fp = fopen(file_name, "wb");
	if (fp >= 0) {
		fwrite(len_tab_addr, 1, len_tab_size, fp);
		fclose(fp);
	} else {
		ERR("utest_dcam_lsc: failed to open calibration_lsc_file.\n");
		rtn = -1;
		goto lsc_exit;
	}

	if (cmr_cxt_ptr->save_directory) {
		sprintf(file_name, "%s_%dX%d.raw", cmr_cxt_ptr->save_directory, cmr_cxt_ptr->capture_width,
				cmr_cxt_ptr->capture_height);
		fp = fopen(file_name, "wb");
		if (fp >= 0) {
			fwrite((void *)cmr_cxt_ptr->capture_raw_vir_addr , 1, cmr_cxt_ptr->capture_width * 
			cmr_cxt_ptr->capture_height * 2, fp);
			fclose(fp);
		} else {
			ERR("utest_dcam_lsc: failed to open save_directory.\n");
			rtn = -1;
			goto lsc_exit;
		}
	}

lsc_exit:
	if (len_tab_addr)
		free(len_tab_addr);

	if (dst_temp_addr)
		free(dst_temp_addr);

	return rtn;
}

static void utest_dcam_cap_cb(camera_cb_type cb,
			const void *client_data,
			camera_func_type func,
			int32_t parm4)
{
	if (CAMERA_FUNC_TAKE_PICTURE == func) {

		struct utest_cmr_context *cmr_cxt_ptr = g_utest_cmr_cxt_ptr;

		switch(cb) {
		case CAMERA_RSP_CB_SUCCESS:
			break;
		case CAMERA_EVT_CB_SNAPSHOT_DONE:
			break;
		case CAMERA_EXIT_CB_DONE:
			{
				camera_frame_type *frame = (camera_frame_type *)parm4;
				cmr_cxt_ptr->capture_raw_vir_addr = (uint32_t)frame->buf_Virt_Addr;
				cmr_cxt_ptr->capture_width = frame->captured_dx;
				cmr_cxt_ptr->capture_height= frame->captured_dy;
				sem_post(&(cmr_cxt_ptr->sem_cap_done));
			}
			break;
		default:
			break;
		}
	}
}

int main(int argc, char **argv)
{
	int32_t rtn = 0;
	struct timespec ts;
	struct utest_cmr_context *cmr_cxt_ptr = g_utest_cmr_cxt_ptr;

	if (utest_dcam_param_set(argc, argv))
		return -1;

	if (sem_init(&(cmr_cxt_ptr->sem_cap_done), 0, 0)) 
		return -1;

	if (CAMERA_SUCCESS != camera_init(cmr_cxt_ptr->sensor_id))
		return -1;

	camera_set_dimensions(cmr_cxt_ptr->capture_width,
					cmr_cxt_ptr->capture_height,
					cmr_cxt_ptr->capture_width,
					cmr_cxt_ptr->capture_height,
					NULL,
					NULL);

	if (utest_dcam_memory_alloc()) {
		rtn = -1;
		goto cali_exit;
	}

	if (CAMERA_SUCCESS != camera_take_picture_raw(utest_dcam_cap_cb,
		NULL, CAMERA_RAW_MODE)) {
		rtn = -1;
		goto cali_exit;
	}

	if (clock_gettime(CLOCK_REALTIME, &ts)) {
		rtn = -1;
		goto cali_exit;
	}
	ts.tv_sec += 3;
	if (sem_timedwait(&(cmr_cxt_ptr->sem_cap_done), &ts)) {
		rtn = -1;
		goto cali_exit;
	} else {
		switch (cmr_cxt_ptr->cmd) {
		case UTEST_CALIBRATION_AWB:
			utest_dcam_awb();
			break;
		case UTEST_CALIBRATION_LSC:
			utest_dcam_lsc();
			break;
		case UTEST_CALIBRATION_FLASHLIGHT:
			utest_dcam_flashlight();
			break;
		default:
			rtn = -1;
			break;
		}
	}

cali_exit:
	utest_dcam_close();
	return rtn;
}
