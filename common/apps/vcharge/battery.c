#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "vcharge.h"

#define BATTERY_NODE_PATH "/sys/class/power_supply/battery/"
#define BATTERY_0_NODE BATTERY_NODE_PATH "battery_0"
#define BATTERY_1_NODE BATTERY_NODE_PATH "battery_1"

enum{
    BATTERY_0 = 0,
    BATTERY_1,
    BAT_NODE_MAX,
};


static int data_buf[BAT_NODE_MAX];
static unsigned int cal_flag = 0;
int parse_data(char * buf, int buf_len)
{
    char * buf_pos = NULL;
    uint32_t adc_0 = 0;
    uint32_t adc_1 = 0;
    uint32_t battery_0 = 0;
    uint32_t battery_1 = 1;

    errno = 0;
    adc_0 = strtol(buf, (char **)NULL, 16);
    if(errno == EINVAL){
        LOGE("input data format error\n");
        return -1;
    }

    buf_pos = strstr(buf, ",");
    if(buf_pos == NULL){
        LOGE("%s: can't find ','1\n", __func__);
        return -1;
    }
    buf_pos += 1;
    adc_1 = strtol(buf_pos, (char **)NULL, 16);

    buf_pos = strstr(buf_pos, ",");
    if(buf_pos == NULL){
        LOGE("%s: can't find ':'2\n", __func__);
        return -1;
    }
    buf_pos += 1;
    battery_0 = strtol(buf_pos, (char **)NULL, 16);

    buf_pos = strstr(buf_pos, ",");
    if(buf_pos == NULL){
        LOGE("%s: can't find ':'3\n", __func__);
        return -1;
    }
    buf_pos += 1;
    battery_1 = strtol(buf_pos, (char **)NULL, 16);

    buf_pos = strstr(buf_pos, ",");
    if(buf_pos == NULL){
        LOGE("%s: can't find cal_flag\n", __func__);
        cal_flag = 0;
    }else{
        buf_pos += 1;
        cal_flag = strtoul(buf_pos, (char **)NULL, 16);
    }

    LOGD("%s: adc_0: 0x%x, adc_1: 0x%x, bat_0: 0x%x, bat_1: 0x%x,cal_flag: 0x%x\n", \
                __func__, adc_0, adc_1, battery_0, battery_1, cal_flag);

    data_buf[BATTERY_0]= battery_0;
    data_buf[BATTERY_1] = battery_1;

    return 0;
}

#define STR_BUF_SIZE 12
#define ADC_CAL_BIT (0x200)
int update_battery_node(void)
{
    char str_buf[STR_BUF_SIZE];
    int fd;
    int n, ret;

    LOGE("update_battery_node\n");

    if(cal_flag & ADC_CAL_BIT)	{
        LOGD("vcharge:use calibrate\n");
    }else{
		LOGD("vcharge:don't calibrate\n");
		return 0;
    }

    if(data_buf[BATTERY_0] != 0){
        n = snprintf(str_buf, STR_BUF_SIZE, "%d", data_buf[BATTERY_0]);
        LOGD("battery_0 :%s\n",str_buf);
        fd = open(BATTERY_0_NODE, O_WRONLY|O_NONBLOCK);
        if(fd < 0){
            LOGE("open %s error with %d\n", BATTERY_0_NODE, fd);
            return -1;
        }
        do{
            ret = write(fd, str_buf, n);
            if(n == ret)
              break;
            LOGE("write %s failed\n", BATTERY_1_NODE);
            sleep(1);
        }while(1);
        close(fd);
    }

    if(data_buf[BATTERY_1] != 0){
        n = snprintf(str_buf, STR_BUF_SIZE, "%d", data_buf[BATTERY_1]);
        LOGD("battery_1 :%s\n",str_buf);
        fd = open(BATTERY_1_NODE, O_WRONLY|O_NONBLOCK);
        if(fd < 0){
            LOGE("open %s error with %d\n", BATTERY_1_NODE, fd);
            return -1;
        }
        do{
            ret = write(fd, str_buf, n);
            if(n == ret)
              break;
            LOGE("write %s failed\n", BATTERY_1_NODE);
            sleep(1);
        }while(1);
        close(fd);
    }
    return 0;    
}
