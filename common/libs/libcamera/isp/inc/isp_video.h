#ifndef _ISP_VIDEO_H
#define _ISP_VIDEO_H

#include <stdio.h>

int ispvideo_RegCameraFunc(uint32_t cmd, int(*func)(uint32_t, uint32_t));
void send_img_data(uint32_t format, uint32_t width, uint32_t height, char *imgptr, int imagelen);
void send_capture_data(uint32_t format, uint32_t width, uint32_t height, char *ch0_ptr, int ch0_len,char *ch1_ptr, int ch1_len,char *ch2_ptr, int ch2_len);
void startispserver();
void stopispserver();
void validispserver(int32_t valid);
#endif
