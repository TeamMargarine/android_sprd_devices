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
#include "isp_cali_interface.h"
#include "sensor_drv_u.h"

int32_t ISP_Cali_GetLNCTab(struct sensor_lnc_map_addr *lnc_map_addr, uint32_t x, uint32_t y )
{
#if 0
	SENSOR_EXP_INFO_T *sensor_exp_info = Sensor_GetInfo();
	struct sensor_raw_fix_info* fix_ptr = sensor_exp_info->raw_info_ptr->fix_ptr;
	struct sensor_lnc_map lnc_map =  fix_ptr->lnc;

	lnc_map_addr->grid = lnc_map.map[x][y].grid;
	lnc_map_addr->param_addr = lnc_map.map[x][y].param_addr;
	lnc_map_addr->len = lnc_map.map[x][y].len;
#endif
	return ISP_CALI_RTN_SUCCESS;
}

#define SENSOR_LNC_TAB_FILE_PATH "/mnt/internal/"
int32_t ISP_Cali_SaveLNCTab(struct isp_addr_t lnc_tab, uint32_t size, uint32_t x, uint32_t y)
{
	#if 0
	SENSOR_EXP_INFO_T *sensor_exp_info = Sensor_GetInfo();
	char file_name[256] = {0x00};
	char tmp_buf[128] = {0x00};
	uint32_t len = 0;
	FILE* fp = 0;

	strcpy(file_name, SENSOR_LNC_TAB_FILE_PATH);
	sprintf(tmp_buf, "lnc_%d%d.dat", x, y);
	strcat(file_name, tmp_buf);

	fp = fopen(file_name, "wb");
	if (0 ==fp) {
		ISP_CALI_LOG("ISP_Cali_SaveLNCTab: open file failed\n");
		return ISP_CALI_RTN_ERROR;
	}

	fwrite((uint8_t*)lnc_tab, 1, size*2, fp);
	fclose(fp);
	#endif

	return ISP_CALI_RTN_SUCCESS;

}

void ISP_Cali_GetLensTabSize(struct isp_size_t img_size, uint32_t grid, uint32_t *tab_size)
{
	uint8_t lnc_grid = grid;
	uint16_t width = img_size.width;
	uint16_t height = img_size.height;
	uint16_t grid_x = 0x00;
	uint16_t grid_y = 0x00;

	grid_x = (((width>>1)-1) % lnc_grid) ? (((width>>1)-1)/lnc_grid + 2) : (((width>>1)-1)/lnc_grid + 1);
	grid_y = (((height>>1)-1) % lnc_grid) ? (((height>>1)-1)/lnc_grid + 2) : (((height>>1)-1)/lnc_grid + 1);

	*tab_size = (grid_x * grid_y)*8;
}

int32_t ISP_Cali_GetLensTabs(struct isp_addr_t img_addr, uint32_t grid, struct isp_size_t img_size, uint32_t* lens_tab, uint32_t x, uint32_t y)
{
	uint32_t rtn = 0;
	uint32_t length = 0;
	uint8_t *lnc_tab_buf = 0;
	struct isp_addr_t tg_lnc = {0x00};
	struct isp_addr_t mg_lnc = {0x00};
	struct isp_addr_t mr_lnc = {0x00};
	struct isp_addr_t final_lnc = {0x00};
	SENSOR_EXP_INFO_T *sensor_exp_info = Sensor_GetInfo();
	#if 0
	uint32_t img_pttn = sensor_exp_info->image_pattern;
	struct sensor_raw_fix_info* fix_ptr = sensor_exp_info->raw_info_ptr->fix_ptr;
	struct sensor_lnc_map lnc_map =  fix_ptr->lnc;
	#else
	uint32_t img_pttn;
	struct sensor_raw_fix_info* fix_ptr ;
	struct sensor_lnc_map lnc_map ;
	 img_pttn = sensor_exp_info->image_pattern;
	fix_ptr = sensor_exp_info->raw_info_ptr->fix_ptr;
	lnc_map =  fix_ptr->lnc;
	#endif

	ISP_Cali_GetLNCTabSize(img_size, grid, &length);
	lnc_tab_buf  = (uint8_t*)malloc(length);
	if(!lnc_tab_buf) {
		ISP_CALI_LOG("ISP_Cali_GetLensTabs: malloc failed\n");
		return -1;

	}
	memset((void*)lnc_tab_buf, 0x00, length);
	rtn = ISP_Cali_LNCTaleCalc(img_addr, img_pttn,  img_size, grid, (uint16_t*)lnc_tab_buf);
	if (rtn) {
		ISP_CALI_LOG("ISP_Cali_GetLensTabs: len tab cal failed\n");
		return -1;
	}

	tg_lnc.y_addr = (uint32_t)lnc_map.map[x][y].param_addr;
	mg_lnc.y_addr = (uint32_t)lnc_map.map[x][y].param_addr;
	mr_lnc.y_addr = (uint32_t)lnc_tab_buf;
	final_lnc.y_addr = (uint32_t)lens_tab;

	rtn = ISP_Cali_Get_Advanced_LensShading(&tg_lnc,
					&mr_lnc,
					&mg_lnc,
					&final_lnc,
					length);

	if (0 != rtn) {
		ISP_CALI_LOG("ISP_Cali_GetLensTabs: Get_Advanced_LensShading failed\n");
		return -1;
	}

	return 0;
}
int32_t ISP_Cali_RawRGBStat(struct isp_addr_t *img_addr,
								struct isp_rect_t *rect,
								struct isp_size_t *img_size,
								struct isp_bayer_ptn_stat_t *stat_param)
{
	int32_t rtn = 0;
	SENSOR_EXP_INFO_T *sensor_exp_info = Sensor_GetInfo();
	uint32_t img_pttn = sensor_exp_info->image_pattern;

	rtn = ISP_Cali_Get_RawRGB_Stat(img_addr,
									rect,
									img_size,
									img_pttn,
									stat_param);

	return rtn;
}

int32_t ISP_Cali_UnCompressedPacket(struct isp_addr_t src_addr, struct isp_addr_t dst_addr, struct isp_size_t img_size, uint32_t edn_type)
{
	uint32_t indx = 0;
	uint32_t length = 0;
	uint8_t *tmp_buf = 0;
	uint8_t val0, val1, val2, val3, val4;
	uint16_t value = 0;
	uint16_t *dst_img = 0;
	uint8_t *img_ptr0, *img_ptr1;

	if ((0 ==src_addr.y_addr)||(0 == dst_addr.y_addr)) {
		ISP_CALI_LOG("ISP_Cali_UnCompressedPacket: img addrs are NULL\n");

		return -1;
	}

	length = img_size.width * img_size.width *2;
	tmp_buf = (uint8_t*)malloc(length);
	if (0 ==tmp_buf) {
		ISP_CALI_LOG("ISP_Cali_UnCompressedPacket: malloc failed\n");
		return -1;
	}
	memset((void*)tmp_buf, 0x00, length);

	length = img_size.width * img_size.height *5 /4;
	switch (edn_type) {
		case 0:
		memcpy((void*)tmp_buf, (void*)src_addr.y_addr, length);
		break;

		case 1:
		{
			img_ptr0 = (uint8_t*)src_addr.y_addr;
			img_ptr1 = (uint8_t*)tmp_buf;
			while(indx < length) {
				val0 = *img_ptr0++;
				val1 = *img_ptr0++;
				val2 = *img_ptr0++;
				val3 = *img_ptr0++;

				*img_ptr1++ = val3;
				*img_ptr1++ = val2;
				*img_ptr1++ = val1;
				*img_ptr1++ = val0;
				indx += 4;
			}
		}
		break;

		default:
		break;

	}

	length = img_size.width * img_size.height;
	indx = 0;
	img_ptr0 = (uint8_t*)tmp_buf;
	dst_img = (uint16_t*)dst_addr.y_addr;
	while (indx < length) {
		val0 = *img_ptr0++;
		val1 = *img_ptr0++;
		val2 = *img_ptr0++;
		val3 = *img_ptr0++;
		val4 = *img_ptr0++;

		value = (val0<<2)|(val4&0x03);
		*dst_img++ = value;
		value = (val1<<2)|((val4>>2)&0x03);
		*dst_img++ = value;
		value = (val2<<2)|((val4>>4)&0x03);
		*dst_img++ = value;
		value = (val3<<2)|((val4>>6)&0x03);
		*dst_img++ = value;

		indx += 4;

	}

	if (tmp_buf) {
		free(tmp_buf);
	}
	return 0;
}
