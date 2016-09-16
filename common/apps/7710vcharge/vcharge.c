#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <linux/netlink.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>
#include "vcharge.h"
#include <utils/Log.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>

#define _BUF_SIZE 100
//#define VBPIPE_PATH "/dev/vbpipe3"
#define CHR_DEV_NAME "/dev/battery_capcity_dev"
char buf[_BUF_SIZE] = {0};
int uevent_fd = -1;
void *charge_record_capacity_thread(void *cookie)
{
        unsigned char test=0;
        int fp0 = open("/data/battery_capcity",O_RDWR | O_CREAT, S_IRUSR|S_IWUSR);
        if (fp0 == NULL)
        {
                return -1;
        }
        int fd = open(CHR_DEV_NAME,O_RDWR);
        if (fd == NULL)
        {
                return -1;
        }
        read(fp0,&test,1);
        lseek(fp0,0,SEEK_SET);
        write(fd,&test,1);
        while(1)
        {
                read(fd,&test,1);
                write(fp0,&test,1);
                fsync(fp0);
                lseek(fp0,0,SEEK_SET);
        }
        close(fp0);
        close(fd);


        return 0;
}

int
main(int argc, char **argv) {
    int ret = -1;
    //int vbpipe_fd = -1;
    int pre_usb_online = -1;
    int usb_online, ac_online, need_notify=0, need_start=0;
    int pre_ac_online = -1;
    /* open cmux channel */
    LOGD("start\n");
    while(ret != 1){
        ret = uevent_init();
        if(ret != 1){
            sleep(1);
            LOGE("uevent init error\n");
        }
    }

reopen_channel:
	/*vbpipe_fd = -1;
    while(vbpipe_fd < 0){
        vbpipe_fd = open(VBPIPE_PATH, O_RDWR);
        if(vbpipe_fd < 0){
            LOGE("open %s error\n", VBPIPE_PATH);
            sleep(1);
        }
    }
    LOGD("open vcharge channel success vbpipe_fd=%d\n",vbpipe_fd);
	*/
    pre_usb_online = read_usb();
    pre_ac_online = read_ac();
    //LOGD("inital status usb: %d ac: %d\n", pre_usb_online, pre_ac_online);
    if(pre_usb_online || pre_ac_online){
        //ret = vcharge_write(vbpipe_fd, CMD_START, NULL);
		get_nv();
        LOGD("vcharge start\n");
        if(ret <= 0){
            LOGE("inital write start error [%d][%s]\n",errno, strerror(errno));
            goto err_handle;
        }
    }
    //ret = battery_calibration(vbpipe_fd);
    //if(ret < 0){
    //    LOGE("update adc calibration failed\n");
    //    goto err_handle;
    //}
        int ret_thread = 0;
        pthread_t t_1;
    	ret_thread = pthread_create(&t_1, NULL, charge_record_capacity_thread, NULL);
        if(ret_thread){
                return -1;
        }
    while(1){
        ret = uevent_next_event(buf, _BUF_SIZE);
        if(ret <= 0)
          continue;
        usb_online = read_usb();
        ac_online = read_ac();
        if(usb_online != pre_usb_online){
            pre_usb_online = usb_online;
            need_notify = 1;
            if(usb_online)
              need_start = 1;
            else
              need_start = 0;
        }
        if(ac_online != pre_ac_online){
            pre_ac_online = ac_online;
            need_notify = 1;
            if(ac_online)
              need_start = 1;
            else 
              need_start = 0;
        }
        if(need_notify){
            need_notify = 0;
			if(need_start){
				//ret = vcharge_write(vbpipe_fd, CMD_START, NULL);
				get_nv();
                LOGD("vcharge start\n");
			}
			else{
				//ret = vcharge_write(vbpipe_fd, CMD_STOP, NULL);
				write_nv();
                LOGD("vcharge stop\n");
			}
        }
        if(ret < 0){
            LOGE("write start command error\n");
            break;
        }
    }

err_handle:
    //close(vbpipe_fd);
    sleep(10);
    goto reopen_channel;

    /* never come to here */
    LOGD("exit\n");
    return EXIT_SUCCESS;
}

int uevent_init()
{
    struct sockaddr_nl addr;
    int sz = 64*1024;
    int s;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 0xffffffff;

    s = socket(PF_NETLINK, SOCK_DGRAM, NETLINK_KOBJECT_UEVENT);
    if(s < 0)
      return 0;

    setsockopt(s, SOL_SOCKET, SO_RCVBUFFORCE, &sz, sizeof(sz));

    if(bind(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
        close(s);
        return 0;
    }

    LOGD("func: %s line: %d get %d\n", __func__, __LINE__, s);
    uevent_fd = s;
    return (uevent_fd > 0);
}

int uevent_next_event(char* buffer, int buffer_length)
{
    while (1) {
        struct pollfd fds;
        int nr;

        fds.fd = uevent_fd;
        fds.events = POLLIN;
        fds.revents = 0;
        nr = poll(&fds, 1, -1);

        if(nr > 0 && fds.revents == POLLIN) {
            int count = recv(uevent_fd, buffer, buffer_length, 0);
            if (count > 0) {
                return count;
            } 
        }
    }

    /* won't get here */
    return 0;
}

int read_usb(void)
{
    int fd = -1;
    char buf[3];
    int ret = -1;
    fd = open("/sys/class/power_supply/usb/online", O_RDONLY);
    if(fd < 0){
        LOGE("%s: usb online open error\n", __func__);
        return 0;
    }
    ret = read(fd, buf, 2U);
    if(ret >= 0)
      buf[ret] = '\0';
    close(fd);
    if(ret >= 1){
        if(buf[0] == '1')
          return 1;
        else if (buf[0] == '0')
          return 0;
        else
            LOGE("%s: dont know what is it\n", __func__);
    }
    return 0;
}

int read_ac(void)
{
    int fd = -1;
    char buf[3];
    int ret = -1;
    fd = open("/sys/class/power_supply/ac/online", O_RDONLY);
    if(fd < 0){
        LOGE("%s: ac online open error\n", __func__);
        return 0;
    }
    ret = read(fd, buf, 2U);
    if(ret >= 0)
      buf[ret] = '\0';
    close(fd);
    if(ret >= 1){
        if(buf[0] == '1')
          return 1;
        else if (buf[0] == '0')
          return 0;
        else
            LOGE("%s: dont know what is it\n", __func__);
    }
    return 0;
}
/*
int vcharge_write(int fd, int32_t cmd_type, int32_t * data)
{
    struct vcharge_cmd cmd_data;
    int ret = 0;
    memset(&cmd_data, 0, sizeof(struct vcharge_cmd));
    snprintf((void *)&cmd_data.magic, sizeof(cmd_data.magic), "%s", CMD_MAGIC);

    cmd_data.cmd_type = cmd_type;
    if(data){
        cmd_data.data[0] = *(data);
        cmd_data.data[1] = *(data + 1);
        cmd_data.data[2] = *(data + 2);
    }

    ret = write(fd, &cmd_data, sizeof(struct vcharge_cmd));
    return ret;
}
int vcharge_read(int fd, char * data_buf, int32_t cnt)
{
    int ret = 0;

    ret = read(fd, data_buf, cnt);
    return ret;
}
*/
#define NV_FILE "/data/.battery_nv"
#define BATTERY_NV_NODE "/sys/class/power_supply/battery/hw_switch_point"
void get_nv(void)
{
	int file_fd;
	int sys_fd;
	char nv_value[10] = {0};
	int ret;
	file_fd = open(NV_FILE, O_RDONLY | O_CREAT, S_IRUSR|S_IWUSR);
	if(file_fd == -1){
		LOGE("open file: %s error %d\n", NV_FILE, file_fd);
		return;
	}

	ret = read(file_fd, nv_value, sizeof(nv_value));

	if(ret == 0){
		LOGD("file: %s empty\n", NV_FILE);
		close(file_fd);
		return;
	}

	sys_fd = open(BATTERY_NV_NODE, O_WRONLY);
	if(sys_fd == -1){
		LOGE("file: %s open error\n", BATTERY_NV_NODE);
		close(file_fd);
		return;
	}
	ret = write(sys_fd, nv_value, strlen(nv_value));
	if(ret != (int)strlen(nv_value)){
		LOGE("write %s failed\n", BATTERY_NV_NODE);
	}
	LOGD("write %s to %s \n", nv_value, BATTERY_NV_NODE);
	close(file_fd);
	close(sys_fd);
	return;
}

void write_nv(void)
{
	int file_fd;
	int sys_fd;
	char nv_value[10] = {0};
	int ret;

	sys_fd = open(BATTERY_NV_NODE, O_RDONLY);
	if(sys_fd == -1){
		LOGE("file: %s open error\n", BATTERY_NV_NODE);
		return;
	}
	ret = read(sys_fd, nv_value, sizeof(nv_value));
	if(ret == 0){
		LOGE("read %s failed\n", BATTERY_NV_NODE);
		close(sys_fd);
		return;
	}
	file_fd = open(NV_FILE, O_WRONLY);
	if(file_fd == -1){
		LOGE("open %s failed\n", NV_FILE);
		close(sys_fd);
		return;
	}
	ret = write(file_fd, nv_value, strlen(nv_value));
	if(ret != (int)strlen(nv_value)){
		LOGE("write %s failed\n", NV_FILE);
	}
	LOGD("write %s to %s \n", nv_value, NV_FILE);
	close(sys_fd);
	close(file_fd);
}
/*
int battery_calibration(int vbpipe_fd)
{
    int ret = -1;
    ret = vcharge_write(vbpipe_fd, CMD_GET_ADC, NULL);
    if(ret < 0){
        LOGE("write adc calibration cmd error\n");
        return -1;
    }

    ret = -1;
    ret = vcharge_read(vbpipe_fd, buf, _BUF_SIZE - 1);
    LOGD("adc cal got %s", buf);
    if(ret < 0){
        LOGE("read adc calibration value error\n");
        return -1;
    }

    buf[ret] = '\n';
    ret = parse_data(buf, ret);
    if(ret < 0){
        LOGE("parse adc calibration value error\n");
        return -1;
    }

    do{
        ret = update_battery_node();
        if(ret >= 0){
            break;
        }
        LOGE("write adc calibration value to node error\n");
        sleep(1);
    }while(1);
    LOGD("update adc calibration value success\n");
    return 0;
}*/
