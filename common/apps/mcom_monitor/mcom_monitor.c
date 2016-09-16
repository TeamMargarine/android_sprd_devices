#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/time.h>

#define LOG_TAG 	"SPRDMCOM"
#include <utils/Log.h>
#define MCOM_LOG  ALOGD


#define DATA_BUF_SIZE (4096)
static char log_data[DATA_BUF_SIZE+1];

#define MAX_WAIT_COUNT  10
int main(int argc, char **argv)
{
	int mcom_fd;
	int r_cnt = 0;
    int count = 0;

    sleep(50); 
	MCOM_LOG("Start to open mcom monitor\n");
    
    do
    {
        mcom_fd = open("/dev/mcom",O_RDONLY);
        if(mcom_fd >= 0)
        {
            MCOM_LOG("mcom open successful!\r\n");
            break;
        }
        else
        {
           MCOM_LOG("mcom open error: %d!\r\n", mcom_fd); 
        }

        sleep(10);

	}while(count++ < MAX_WAIT_COUNT);
    
    if(count >= MAX_WAIT_COUNT)
    {
       MCOM_LOG("mcom open error, exit\r\n"); 
        exit(1);
    }


	MCOM_LOG("Start to read mcom data!\r\n");
	while(1) 
    {
		r_cnt = read(mcom_fd, log_data, DATA_BUF_SIZE);

		if (r_cnt < 0) 
        {
			MCOM_LOG("mcom read error :%d\n", r_cnt);
			continue;
		}
        
        if(r_cnt > DATA_BUF_SIZE)
        {
            r_cnt = DATA_BUF_SIZE;
        }

        log_data[r_cnt] = 0;

		MCOM_LOG("%s\r\n", log_data);
	}
out:
	close(mcom_fd);
	return 0;
}
