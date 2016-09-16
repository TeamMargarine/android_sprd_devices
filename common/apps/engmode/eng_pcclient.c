#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <termios.h>
#include <sys/ioctl.h>
#include "cutils/sockets.h"
#include "cutils/properties.h"
#include <private/android_filesystem_config.h>
#include "eng_pcclient.h"
#include "eng_diag.h"
#include "engopt.h"
#include "eng_at.h"
#include "eng_sqlite.h"

#define VLOG_PRI  -20
#define USB_CONFIG_VSER_GSER  "mass_storage,adb,vser,gser"
#define USB_CONFIG_GSER6  "mass_storage,adb,gser6"
extern void	disconnect_vbus_charger(void);
// current run mode: TD or W
int g_run_mode = ENG_RUN_TYPE_TD;

// devices' at nodes for PC
char* s_at_ser_path = "/dev/ttyGS0";

// devices' diag nodes for PC
char* s_connect_ser_path[] = {
    "/dev/ttyS1", //uart
    "/dev/vser", //usb
    NULL
};

// devices' nodes for cp
char* s_cp_pipe[] = {
    "/dev/slog_td", //cp_td : slog_td
    "/dev/slog_w", //cp_w
    "/dev/slog_wcn", //cp_btwifi
    NULL
};

static struct eng_param cmdparam = {
    .califlag = 0,
    .engtest = 0,
    .cp_type = CP_TD,
    .connect_type = CONNECT_USB,
    .nativeflag = 0
};

static void set_vlog_priority(void)
{
    int inc = VLOG_PRI;
    int res = 0;

    errno = 0;
    res = nice(inc);
    if (res < 0){
        printf("cannot set vlog priority, res:%d ,%s\n", res,
                strerror(errno));
        return;
    }
    int pri = getpriority(PRIO_PROCESS, getpid());
    printf("now vlog priority is %d\n", pri);
    return;
}

/* Parse one parameter which is before a special char for string.
 * buf:[IN], string data to be parsed.
 * gap:[IN], char, get value before this charater.
 * value:[OUT] parameter value
 * return length of parameter
 */
static int cali_parse_one_para(char * buf, char gap, int* value)
{
    int len = 0;
    char *ch = NULL;
    char str[10] = {0};

    if(buf != NULL && value  != NULL){
        ch = strchr(buf, gap);
        if(ch != NULL){
            len = ch - buf ;
            strncpy(str, buf, len);
            *value = atoi(str);
        }
    }
    return len;
}

void eng_check_factorymode_fornand(void)
{
    int ret;
    int fd;
    int status = eng_sql_string2int_get(ENG_TESTMODE);
    char status_buf[8];
    char config_property[64];
    char modem_enable[5];
    char usb_config[64];

#ifdef USE_BOOT_AT_DIAG
    ENG_LOG("%s: status=%x\n",__func__, status);
    property_get("persist.sys.usb.config", config_property, "");
    property_get("ro.modem.wcn.enable", modem_enable, "");
    if((status==1)||(status == ENG_SQLSTR2INT_ERR)) {
        fd=open(ENG_FACOTRYMODE_FILE, O_RDWR|O_CREAT|O_TRUNC, 0660);
        if(fd > 0)
            close(fd);

        ENG_LOG("%s: modem_enable: %s\n", __FUNCTION__, modem_enable);
        if(!strcmp(modem_enable, "1")) {
            strcpy(usb_config, USB_CONFIG_GSER6);
        }else {
            strcpy(usb_config, USB_CONFIG_VSER_GSER);
        }
        if(strncmp(config_property, usb_config, strlen(usb_config))){
            property_set("sys.usb.config", usb_config);
            property_set("persist.sys.usb.config", usb_config);
        }
    } else if (status == 0) {
        remove(ENG_FACOTRYMODE_FILE);
    } else {
        remove(ENG_FACOTRYMODE_FILE);
    }
#endif

    fd=open(ENG_FACOTRYSYNC_FILE, O_RDWR|O_CREAT|O_TRUNC, 0660);
    if(fd >= 0)
        close(fd);
}

static int eng_parse_cmdline(struct eng_param * cmdvalue)
{
    int fd = 0;
    char cmdline[ENG_CMDLINE_LEN] = {0};
    char *str = NULL;
    int mode =  0;
    int freq = 0;
    int device = 0;
    int len = -1;

    if(cmdvalue == NULL)
        return -1;

    fd = open("/proc/cmdline", O_RDONLY);
    if (fd >= 0) {
        if (read(fd, cmdline, sizeof(cmdline)) > 0){
            ALOGD("eng_pcclient: cmdline %s\n",cmdline);
            /*calibration*/
            str = strstr(cmdline, "calibration");
            if ( str  != NULL){
                cmdvalue->califlag = 1;
		   disconnect_vbus_charger();
                /*calibration= mode,freq, device. Example: calibration=8,10096,146*/
                str = strchr(str, '=');
                if(str != NULL){
                    str++;
                    /*get calibration mode*/
                    len = cali_parse_one_para(str, ',', &mode);
                    if(len > 0){
                        str = str + len +1;
                        /*get calibration freq*/
                        len = cali_parse_one_para(str, ',', &freq);
                        /*get calibration device*/
                        str = str + len +1;
                        len = cali_parse_one_para(str, ' ', &device);
                    }
                    switch(mode){
                        case 1:
                        case 5:
                        case 7:
                        case 8:
                            cmdvalue->cp_type = CP_TD;
                            break;
                        case 11:
                        case 12:
                        case 14:
                        case 15:
                            cmdvalue->cp_type = CP_WCDMA;
                            break;
                        default:
                            break;
                    }

                    /*Device[4:6] : device that AP uses;  0: UART 1:USB  2:SPIPE*/
                    cmdvalue->connect_type = (device >> 4) & 0x3;

                    if(device >>7)
                        cmdvalue->nativeflag = 1;
                    else
                        cmdvalue->nativeflag = 0;

                    ALOGD("eng_pcclient: cp_type=%d, connent_type(AP) =%d, is_native=%d\n",
                            cmdvalue->cp_type, cmdvalue->connect_type, cmdvalue->nativeflag );
                }
            }else{
                /*if not in calibration mode, use default */
                cmdvalue->cp_type = CP_TD;
                cmdvalue->connect_type = CONNECT_USB;
            }
            /*engtest*/
            if(strstr(cmdline,"engtest") != NULL)
                cmdvalue->engtest = 1;
        }
        close(fd);
    }

    ENG_LOG("eng_pcclient califlag=%d \n", cmdparam.califlag);

    return 0;
}

int main (int argc, char** argv)
{
    static char atPath[ENG_DEV_PATH_LEN];
    static char diagPath[ENG_DEV_PATH_LEN];
    char cmdline[ENG_CMDLINE_LEN];
    int opt;
    int type;
    int run_type = ENG_RUN_TYPE_TD;
    eng_thread_t t1,t2;

    while ( -1 != (opt = getopt(argc, argv, "t:a:d:"))) {
        switch (opt) {
            case 't':
                type = atoi(optarg);
                switch(type) {
                    case 0:
                        run_type = ENG_RUN_TYPE_WCDMA; // W Mode
                        break;
                    case 1:
                        run_type = ENG_RUN_TYPE_TD; // TD Mode
                        break;
                    case 2:
                        run_type = ENG_RUN_TYPE_BTWIFI; // BT WIFI Mode
                        break;
                    default:
                        ENG_LOG("engpcclient error run type: %d\n", run_type);
                        return 0;
                }
                break;
            case 'a': // AT port path
                strcpy(atPath, optarg);
                s_at_ser_path = atPath;
                break;
            case 'd': // Diag port path
                strcpy(diagPath, optarg);
                s_connect_ser_path[CONNECT_USB] = diagPath;
                break;
            default:
                exit(EXIT_FAILURE);
        }
    }

    ENG_LOG("engpcclient runtype:%d, atPath:%s, diagPath:%s", run_type,
            s_at_ser_path, s_connect_ser_path[CONNECT_USB]);

    // Remember the run type for at channel's choice.
    g_run_mode = run_type;

    // Create the sqlite database for factory mode.
    eng_sqlite_create();

    // Get the status of calibration mode & device type.
    eng_parse_cmdline(&cmdparam);

    if(cmdparam.califlag != 1){
        cmdparam.cp_type = run_type;
    }

    // Check factory mode and switch device mode.
    eng_check_factorymode_fornand();

    set_vlog_priority();

    // Create vlog thread for reading diag data from modem and send it to PC.
    if (0 != eng_thread_create( &t1, eng_vlog_thread, &cmdparam)){
        ENG_LOG("vlog thread start error");
    }

    // Create vdiag thread for reading diag data from PC, some data will be
    // processed by ENG/AP, and some will be pass to modem transparently.
    if (0 != eng_thread_create( &t2, eng_vdiag_thread, &cmdparam)){
        ENG_LOG("vdiag thread start error");
    }

    if(cmdparam.califlag != 1 || cmdparam.nativeflag != 1){
        eng_at_pcmodem(run_type);
    }

    while(1){
        sleep(10000);
    }

    return 0;
}
