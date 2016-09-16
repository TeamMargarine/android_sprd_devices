#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include "engopt.h"
#include "eng_audio.h"
#include "eng_diag.h"
#include <ctype.h>
#include "cutils/properties.h"
#include "private/android_filesystem_config.h"
#include <termios.h>

#define DATA_BUF_SIZE (4096*2)
#define MAX_OPEN_TIMES  10
#define DATA_EXT_DIAG_SIZE (4096*2)
#define USB_LOOP_BACK_CMD "AT+SPUSBLOOP="

static char log_data[DATA_BUF_SIZE];
static char ext_data_buf[DATA_EXT_DIAG_SIZE];
static int ext_buf_len;

//referrenced by eng_diag.c
//int audio_fd;
AUDIO_TOTAL_T audio_total[4];

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

static int s_speed_arr[] = {B921600,B115200, B38400, B19200, B9600, B4800, B2400, B1200, B300,
    B921600,B115200, B38400, B19200, B9600, B4800, B2400, B1200, B300, };
static int s_name_arr[] = {921600,115200,38400,  19200, 9600,  4800,  2400,  1200,  300,
    921600, 115200,38400,  19200,  9600, 4800, 2400, 1200,  300, };

static void print_log_data(int cnt)
{
    int i;

    if (cnt > DATA_BUF_SIZE/2)
        cnt = DATA_BUF_SIZE/2;

    ENG_LOG("eng_vdiag vser receive:\n");
    for(i = 0; i < cnt; i++) {
        if (isalnum(log_data[i])){
            ENG_LOG("eng_vdiag %c ", log_data[i]);
        }else{
            ENG_LOG("eng_vdiag %2x ", log_data[i]);
        }
    }
    ENG_LOG("\n");
}

static int usb_loop_mode_check(char* data, int len)
{
    char rsp[64];
    MSG_HEAD_T* diag_head;

    if(data && (!strncasecmp(data + sizeof(MSG_HEAD_T) + 1, USB_LOOP_BACK_CMD,
                    strlen(USB_LOOP_BACK_CMD)))){
        rsp[0] = 0x7e;
        diag_head = (MSG_HEAD_T*)(rsp + 1);
        diag_head->len = sizeof(MSG_HEAD_T)+strlen("\r\nOK\r\n");
        diag_head->seq_num = 0;
        diag_head->type = 0x9c;
        diag_head->subtype = 0x00;

        memcpy(rsp+sizeof(MSG_HEAD_T)+1,"\r\nOK\r\n",strlen("\r\nOK\r\n"));
        rsp[diag_head->len + 1] = 0x7e;
        write(get_vser_fd(),rsp,diag_head->len + 2);

        return 1;
    }

    return 0;
}

void init_user_diag_buf(void)
{
    memset(ext_data_buf,0,DATA_EXT_DIAG_SIZE);
    ext_buf_len = 0;
}

int get_user_diag_buf(char* buf,int len)
{
    int i;
    int is_find = 0;

    for(i = 0; i< len; i++){
        ENG_LOG("eng_vdiag %s: %x\n",__FUNCTION__, buf[i]);

        if (buf[i] == 0x7e && ext_buf_len ==0){ //start
            ext_data_buf[ext_buf_len++] = buf[i];
        }else if (ext_buf_len > 0 && ext_buf_len < DATA_EXT_DIAG_SIZE){
            if (buf[i] == 0x7d) {//ppp shift char, the following char should plus 0x20
                ENG_LOG("eng_vdiag %s: skip shift char:%x\n",__FUNCTION__, buf[i]);
                ext_data_buf[ext_buf_len] = buf[++i]+0x20;
            } else {
                ext_data_buf[ext_buf_len]=buf[i];
            }
            ext_buf_len++;
            if ( buf[i] == 0x7e ){
                is_find = 1;
                break;
            }
        }
    }

    if ( is_find ) {
        for(i = 0; i < ext_buf_len; i++) {
            ENG_LOG("eng_vdiag 0x%x, ",ext_data_buf[i]);
        }
    }
    return is_find;
}


int check_audio_para_file_size(char *config_file)
{
    int fileSize = 0;
    int tmpFd;

    ENG_LOG("%s: enter",__FUNCTION__);
    tmpFd = open(config_file, O_RDONLY);
    if (tmpFd < 0) {
        ENG_LOG("%s: open error",__FUNCTION__);
        return -1;
    }
    fileSize = lseek(tmpFd, 0, SEEK_END);
    if (fileSize <= 0) {
        ENG_LOG("%s: file size error",__FUNCTION__);
        close(tmpFd);
        return -1;
    }
    close(tmpFd);
    ENG_LOG("%s: check OK",__FUNCTION__);
    return 0;
}

int ensure_audio_para_file_exists(char *config_file)
{
    char buf[2048];
    int srcfd, destfd;
    struct stat sb;
    int nread;
    int ret;

    ENG_LOG("%s: enter",__FUNCTION__);
    ret = access(config_file, R_OK|W_OK);
    if ((ret == 0) || (errno == EACCES)) {
        if ((ret != 0) &&
                (chmod(config_file, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP) != 0)) {
            ALOGE("eng_vdiag Cannot set RW to \"%s\": %s", config_file, strerror(errno));
            return -1;
        }
        if (0 == check_audio_para_file_size(config_file)) {
            ENG_LOG("%s: ensure OK",__FUNCTION__);
            return 0;
        }
    } else if (errno != ENOENT) {
        ALOGE("eng_vdiag Cannot access \"%s\": %s", config_file, strerror(errno));
        return -1;
    }

    srcfd = open((char *)(ENG_AUDIO_PARA), O_RDONLY);
    if (srcfd < 0) {
        ALOGE("eng_vdiag Cannot open \"%s\": %s", (char *)(ENG_AUDIO_PARA), strerror(errno));
        return -1;
    }

    destfd = open(config_file, O_CREAT|O_RDWR, 0660);
    if (destfd < 0) {
        close(srcfd);
        ALOGE("eng_vdiag Cannot create \"%s\": %s", config_file, strerror(errno));
        return -1;
    }

    ENG_LOG("%s: start copy",__FUNCTION__);
    while ((nread = read(srcfd, buf, sizeof(buf))) != 0) {
        if (nread < 0) {
            ALOGE("eng_vdiag Error reading \"%s\": %s",(char *)(ENG_AUDIO_PARA) , strerror(errno));
            close(srcfd);
            close(destfd);
            unlink(config_file);
            return -1;
        }
        write(destfd, buf, nread);
    }

    close(destfd);
    close(srcfd);

    /* chmod is needed because open() didn't set permisions properly */
    if (chmod(config_file, 0660) < 0) {
        ALOGE("eng_vdiag Error changing permissions of %s to 0660: %s",
                config_file, strerror(errno));
        unlink(config_file);
        return -1;
    }

    if (chown(config_file, AID_SYSTEM, AID_SYSTEM) < 0) {
        ALOGE("eng_vdiag Error changing group ownership of %s to %d: %s",
                config_file, AID_SYSTEM, strerror(errno));
        unlink(config_file);
        return -1;
    }
    ENG_LOG("%s: ensure done",__FUNCTION__);
    return 0;
}


static void set_raw_data_speed(int fd, int speed)
{
    unsigned long i = 0;
    int   status = 0;
    struct termios Opt;

    tcflush(fd,TCIOFLUSH);
    tcgetattr(fd, &Opt);
    for ( i= 0;  i  < sizeof(s_speed_arr) / sizeof(int);  i++){
        if  (speed == s_name_arr[i])  {
            /*set raw data mode */
            Opt.c_iflag &= ~(IGNBRK | BRKINT | PARMRK | ISTRIP | INLCR | IGNCR | ICRNL | IXON);
            Opt.c_oflag &= ~OPOST;
            Opt.c_lflag &= ~(ECHO | ECHONL | ICANON | ISIG | IEXTEN);
            Opt.c_cflag &= ~(CSIZE | PARENB);
            Opt.c_cflag |= CS8;
            Opt.c_iflag = ~(ICANON|ECHO|ECHOE|ISIG);
            Opt.c_oflag = ~OPOST;
            cfmakeraw(&Opt);
            /* set baudrate*/
            cfsetispeed(&Opt, s_speed_arr[i]);
            cfsetospeed(&Opt, s_speed_arr[i]);
            status = tcsetattr(fd, TCSANOW, &Opt);
            if  (status != 0)
                perror("tcsetattr fd1");
            break;
        }
    }
}

void *eng_vdiag_thread(void *x)
{
    int pipe_fd;
    int ser_fd;
    int r_cnt, w_cnt;
    int res, ret=0;
    int audio_fd = -1;
    int wait_cnt = 0;
    struct eng_param * param = (struct eng_param *)x;

    if(param == NULL){
        ALOGE("eng_vdiag invalid input\n");
        return NULL;
    }
    if(param->califlag)
        initialize_ctrl_file();
    /*open usb/usart*/
    ser_fd = open( s_connect_ser_path[param->connect_type], O_RDONLY);
    if(ser_fd >= 0){
        if(param->connect_type == CONNECT_UART){
            set_raw_data_speed(ser_fd, 115200);
        }
    }else{
        ALOGE("eng_vdiag cannot open general serial\n");
        return NULL;
    }

    /*open vpipe/spipe*/
    do{
        pipe_fd = open(s_cp_pipe[param->cp_type], O_WRONLY);
        if(pipe_fd < 0) {
            ENG_LOG("eng_vdiag cannot open %s, times:%d\n", s_cp_pipe[param->cp_type], wait_cnt);
            if(wait_cnt++ >= MAX_OPEN_TIMES){
                close(ser_fd);
                ALOGE("eng_vdiag cannot open vpipe(spipe)\n");
                return NULL;
            }
            sleep(5);
        }
    }while(pipe_fd < 0);

    memset(&audio_total, 0, sizeof(audio_total));
    if(0 == ensure_audio_para_file_exists((char *)(ENG_AUDIO_PARA_DEBUG))){
        audio_fd = open(ENG_AUDIO_PARA_DEBUG,O_RDWR);
    }
    if (audio_fd >= 0) {
        read(audio_fd, &audio_total, sizeof(audio_total));
        close(audio_fd);
    }
    ENG_LOG("put diag data from serial to pipe\n");

    init_user_diag_buf();

    while(1) {
        r_cnt = read(ser_fd, log_data, DATA_BUF_SIZE/2);
        if (r_cnt == DATA_BUF_SIZE/2) {
            r_cnt += read(ser_fd, log_data+r_cnt, DATA_BUF_SIZE/2);
        }
        if (r_cnt < 0) {
            ENG_LOG("eng_vdiag read log data error  from serial: %s\n", strerror(errno));
            close(ser_fd);
            ENG_LOG("eng_vdiag reopen serial\n");
            ser_fd = open(s_connect_ser_path[param->connect_type], O_RDONLY);
            if(ser_fd < 0) {
                close(pipe_fd);
                ALOGE("eng_vdiag cannot open vendor serial\n");
                return NULL;
            }
            continue;
        }
        ret=0;

        // check if usb loop back mode
        if(usb_loop_mode_check(log_data,r_cnt))
            continue;

        if (get_user_diag_buf(log_data,r_cnt)){
            ret = eng_diag(ext_data_buf,ext_buf_len);
            init_user_diag_buf();
        }

        if(ret == 1)
            continue;

        ENG_LOG("eng_vdiag DIAGLOG:: read length =%d\n", r_cnt);
        //print_log_data(r_cnt);
        do{
            w_cnt = write(pipe_fd, log_data, r_cnt);
            if (w_cnt < 0) {
                ENG_LOG("eng_vdiag no log data write:%d ,%s\n", w_cnt, strerror(errno));
                continue;
            }else{
                r_cnt -= w_cnt;
            }
        }while(r_cnt >0);
        ENG_LOG("eng_vdiag read from diag %d, write to pipe%d\n", r_cnt, w_cnt);
    }
out:
    close(pipe_fd);
    close(ser_fd);
    return 0;
}
