#ifndef _VCHARGE__H_
#define _VCHARGE__H_
#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <utils/Log.h>
#ifdef LOG_TAG
#undef LOG_TAG
#endif
#define LOG_TAG "VCharge"
#ifndef LOGD
#define LOGD(...) ((void)ALOG(LOG_DEBUG, LOG_TAG, __VA_ARGS__))
#endif
#ifndef LOGE
#define LOGE(...) ((void)ALOG(LOG_ERROR, LOG_TAG, __VA_ARGS__))
#endif

#define CMD_MAGIC "vcharge"
#define CMD_START (1)
#define CMD_STOP (2)
#define CMD_GET_ADC (3)

struct vcharge_cmd{
    char magic[8];
    int cmd_type;
    int data[3];
};
int parse_data(char * buf, int buf_len);
int update_battery_node(void);
int vcharge_write(int fd, int32_t cmd_type, int32_t * data);
int battery_calibration(int vbpipe_fd);
int read_usb(void);
int read_ac(void);
void get_nv(void);
void write_nv(void);
int uevent_init(void);
int uevent_next_event(char* buffer, int buffer_length);

#ifdef __cplusplus
}
#endif

#endif
