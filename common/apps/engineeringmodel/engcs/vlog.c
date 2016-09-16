#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/time.h>
#include "engopt.h"
#include "kfifo.h"
#include "cutils/properties.h"

#define ENG_CARDLOG_PROPERTY	"persist.sys.cardlog"
#define DATA_BUF_SIZE (4096)
#define WRITE_BUF_SIZE (32768)
#define FIFO_BUF_SIZE (65536)
static char log_data[DATA_BUF_SIZE];
static char write_data[WRITE_BUF_SIZE];
static int vser_fd = 0;

int is_sdcard_exist=1;
int pipe_fd;

extern eng_mutex_t w_mutex = PTHREAD_MUTEX_INITIALIZER;
extern eng_mutex_t vlog_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;//init cond

/* eng_sd_log abandoned
extern eng_mutex_t sd_mutex = PTHREAD_MUTEX_INITIALIZER;
extern pthread_cond_t sd_cond = PTHREAD_COND_INITIALIZER;
*/

/* use kfifo to hold log data*/
char fifo_buffer[FIFO_BUF_SIZE];
struct kfifo log_fifo = {
    .buffer = fifo_buffer,
    .size = FIFO_BUF_SIZE,
    .in = 0,
    .out = 0
};
struct kfifo * p_log_fifo = &log_fifo;

static int s_connect_type = 0;
static char* s_connect_ser_path[]={
	"/dev/ttyS1", //uart
	"/dev/vser", //usb
	NULL
};
static char* s_cp_pipe[]={
	"/dev/vbpipe0", //cp_td : slog_td
	"/dev/slog_w", //cp_w
	NULL
};

int get_vser_fd(void)
{
	return vser_fd;
}

int restart_vser(void)
{
	ENG_LOG("eng_vlog close usb serial:%d\n", vser_fd);
	close(vser_fd);

	vser_fd = open(s_connect_ser_path[s_connect_type],O_WRONLY);
	if(vser_fd < 0) {
		ALOGE("eng_vlog cannot open general serial\n");
		return 0;
	}
	ENG_LOG("eng_vlog reopen usb serial:%d\n", vser_fd);
	return 0;
}

#define MAX_OPEN_TIMES  10
//int main(int argc, char **argv)
void *eng_vlog_fifo_thread(void *x)
{
	int ser_fd;
	int sdcard_fd;
	int card_log_fd = -1;
	int r_cnt, w_cnt;
	int res,n;
	char cardlog_property[8];
	char log_name[40];
	time_t now;
	int wait_cnt = 0;
	struct tm *timenow;
	struct eng_param * param = (struct eng_param *)x;
	int i = 0;

	ENG_LOG("eng_vlog  start\n");

	if(param == NULL){
		ALOGE("eng_vlog invalid input\n");
		return NULL;
	}
	
	ENG_LOG("eng_vlog open vitual pipe...\n");
	/*open vbpipe/spipe*/
	 do{
		pipe_fd = open(s_cp_pipe[param->cp_type], O_RDONLY);
		if(pipe_fd < 0) {
			ALOGE("eng_vlog cannot open %s, times:%d, %s\n", s_cp_pipe[param->cp_type],wait_cnt,strerror(errno));
			
			sleep(5);
		}
	}while(pipe_fd < 0);

	sdcard_fd = open("/dev/block/mmcblk0",O_RDONLY);
	if ( sdcard_fd < 0 ) {
		is_sdcard_exist = 0;
		ENG_LOG("eng_vlog No sd card!!!");
	}else{
		is_sdcard_exist = 1;
	}
	close(sdcard_fd);

	ENG_LOG("eng_vlog put log data from pipe to fifo\n");
	while(1) {
		r_cnt = read(pipe_fd, log_data, DATA_BUF_SIZE);

		if ( is_sdcard_exist ) {
			memset(cardlog_property, 0, sizeof(cardlog_property));
			property_get(ENG_CARDLOG_PROPERTY, cardlog_property, "");
			n = atoi(cardlog_property);

			if ( 1==n ){
				sleep(1);
			       	continue;
				/* sd_log abandoned
				eng_mutex_lock(&sd_mutex);
                		kfifo_put(sdfifo, log_data, r_cnt);
                		ENG_LOG("eng_vlog sdfifo len %d\n", kfifo_len(sdfifo));
                		if ( kfifo_len(sdfifo) > (4096*16) )
                        		pthread_cond_signal(&sd_cond);

                		eng_mutex_unlock(&sd_mutex);*/
			}
		}
		
		if (r_cnt < 0) {
                        ENG_LOG("eng_vlog read no log data : r_cnt=%d, %s\n",  r_cnt, strerror(errno));
                        continue;
                }

		eng_mutex_lock(&vlog_mutex);
                w_cnt = kfifo_put(p_log_fifo, log_data, r_cnt);
		ENG_LOG("eng_vlog log_fifo_len %d\n", kfifo_len(p_log_fifo));
                if ( kfifo_len(p_log_fifo) > 0 )
                        pthread_cond_signal(&cond);
                eng_mutex_unlock(&vlog_mutex);
		
		ENG_LOG("eng_vlog read %d, write &d to log_fifo\n", r_cnt, w_cnt);
	}
out:
	close(pipe_fd);
	return 0;
}



void *eng_vlog_thread(void *x)
{
        int ser_fd;
        int sdcard_fd;
        int card_log_fd = -1;
        int r_cnt, w_cnt;
        int res,n;
        char cardlog_property[8];
        char log_name[40];
        time_t now;
        int wait_cnt = 0;
        struct tm *timenow;
        struct eng_param * param = (struct eng_param *)x;
        int i = 0;

        ENG_LOG("eng_vlog  start\n");

        if(param == NULL){
                ALOGE("eng_vlog invalid input\n");
                return NULL;
        }
        /*open usb/uart*/
        ENG_LOG("eng_vlog  open  serial...\n");

        s_connect_type = param->connect_type;
        ser_fd = open(s_connect_ser_path[s_connect_type], O_WRONLY);
        if(ser_fd < 0) {
                ALOGE("eng_vlog cannot open general serial:%s\n",strerror(errno));
                return NULL;
        }
        vser_fd = ser_fd;


        ENG_LOG("eng_vlog put log data from pipe to serial\n");
        while(1) {
		ENG_LOG("eng_vlog start write log_fifo to serial\n");	
		
		eng_mutex_lock(&vlog_mutex);
                if ( !(kfifo_len(p_log_fifo) > 0) )
                        pthread_cond_wait(&cond, &vlog_mutex);
		eng_mutex_unlock(&vlog_mutex);
		
		r_cnt = kfifo_get(p_log_fifo, write_data, WRITE_BUF_SIZE);
                eng_mutex_lock(&w_mutex);

		if ( r_cnt == 0 ) {
                        ENG_LOG("eng_vlog read no log datas\n");
                        continue;
                }

                w_cnt = write(ser_fd, write_data, r_cnt);
                eng_mutex_unlock(&w_mutex);

                if (w_cnt < 0) {
                        ENG_LOG("eng_vlog no log data write:%d ,%s\n", w_cnt, strerror(errno));
                        close(ser_fd);

                        ser_fd = open(s_connect_ser_path[s_connect_type], O_WRONLY);
                        if(ser_fd < 0) {
                                ALOGE("eng_vlog cannot open general serial\n");
                                close(pipe_fd);
                                return NULL;
                        }
                        vser_fd = ser_fd;
                        ENG_LOG("eng_vlog reopen usb serial:%d\n", ser_fd);
                }
              ENG_LOG("eng_vlog read %d from log_fifio, write %d to vser\n", w_cnt);
        }
out:
        close(ser_fd);
        return 0;
}
