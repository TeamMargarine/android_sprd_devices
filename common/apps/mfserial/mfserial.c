/*
 * this file is written based on eng_pcclient.c
 */

#define LOG_TAG "MFSERIAL"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <termios.h>
#include <limits.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/time.h>
#include <cutils/properties.h>
#include <utils/Log.h>

/*#define ENG_TRACING*/
#ifdef ENG_TRACING
#define ALOGD(fmt...)  \
do{	\
	char log[512] = {0}; \
	char cmd[576] = {0};\
	sprintf(log, ##fmt); \
	sprintf(cmd, "echo \"%s: %s\n\" > /dev/kmsg", LOG_TAG, log);\
	system(cmd); \
 }while(0)

#define ENG_LOG ALOGD
#else
#define ENG_LOG(format, ...)
#endif

#define ENG_BUFFER_SIZE		256
#define DEV_NAME_SIZE		128

#define PROP_TTYDEV  "persist.ttydev"

/* default parameters */
static char * uart_devname = "/dev/ttyS1";
static char * at_mux1_devname = "/dev/ts0710mux9";
#if 0
static char * at_mux2_devname = "/dev/ts0710mux0"; /* channel unsol. command from */
static int at_mux2_fd;
#endif
static int uart_fd;
static int at_mux1_fd;
static int have_phoneserver = 1;
static pthread_mutex_t uart_mutex;


void usage(void)
{
	fprintf(stderr,
		"\n"
		"Usage: mfserial at_devname uart_devname have_phoneserver\n"
		"  Or:  mfserial\n\n"
		"It works either in\n"
		"  - AT mode (default): bridging between AT and UART ports. or\n"
		"  - Serial console mode: console on the dedicated UART port (115200n8n1).\n"
		"To enter serial console mode, please enter \"sh\" followed by an enter.\n"
		"To go back to AT mode, please enter \"exit\" followed by an enter, or just\n"
		"ctrl-c.\n"
		"The timeout value for AT ACK is 40s. An unrecognized AT cmd will not get\n"
		"any ack from the modem until timeout, \"ERROR\" will be returned then.\n\n"
		"  at_devname        the AT device name, e.g. mux1 (for /dev/mux1)\n"
		"  uart_devname      the UART device name, e.g. ttyS1 (for /dev/ttyS1)\n"
		"  have_phoneserver  0 if there's no phoneserver, 1 otherwise\n"
		"  if there is no arguments, ts0710mux9/ttyS1/1 will be used by default\n\n"
		"Example:\n"
		"  mfserial ts0710mux9 ttyS1 1\n"
		"\n");
}

/*
 * open at/uart devices
 */
static int open_devices(void)
{
	struct termios termio;
	static char ttydev[12];
	char path[32];

	/* uart device */
	uart_fd = open(uart_devname, O_RDWR);
	if(uart_fd < 0){
		ALOGE("%s: open %s failed [%s]\n", __func__, uart_devname, strerror(errno));
		return -1;
	}

	/* set uart parameters: 115200n8n1 */
	tcgetattr(uart_fd, &termio);
	cfsetispeed(&termio, (speed_t)B115200);
	cfsetospeed(&termio, (speed_t)B115200);
	termio.c_cflag &= ~PARENB;
	termio.c_cflag &= ~CSIZE;
	termio.c_cflag |= CS8;
	termio.c_cflag &= ~CSTOPB;
	tcsetattr(uart_fd, TCSAFLUSH, &termio);

	/* use uart as stdio */
	dup2(uart_fd, 0);
	dup2(uart_fd, 1);
	dup2(uart_fd, 2);

	if(!have_phoneserver) {
		property_get(PROP_TTYDEV, ttydev, "ttyNK3");
		sprintf(path, "/dev/%s", ttydev);
		/* get nkconsole ready */
		if(open(path, O_RDWR) < 0)
			ALOGD("%s: open %s failed [%s]\n",
					__func__, path, strerror(errno));

		/* get mux ready */
		/*
		if(open("/dev/ts0710mux0", O_RDWR) < 0)
			ALOGD("%s: open /dev/ts0710mux0 failed [%s]\n",
					__func__, strerror(errno));
		*/

	} else {
		/* FIXME: we need to wait for phoneserver to finish some work */
		 sleep(5);
	}

	/* at device */
#if 0
	ALOGD("open mux0 device.");
	at_mux2_fd = open(at_mux2_devname, O_RDONLY);
	if(at_mux2_fd < 0){
		ALOGD("%s: open %s failed [%s]\n",
				__func__, at_mux2_devname, strerror(errno));
		return -1;
	}
#endif
	ALOGD("open mux9 device.");
	at_mux1_fd = open(at_mux1_devname, O_RDWR/* | O_NONBLOCK*/);
	if(at_mux1_fd < 0){
		ALOGD("%s: open %s failed [%s]\n",
				__func__, at_mux1_devname, strerror(errno));
		return -1;
	}

	return 0;
}


void close_devices(void)
{
	ALOGD("close all devices");

	close(uart_fd);
	close(at_mux1_fd);
#if 0
	close(at_mux2_fd);
#endif
}

int is_calibration_mode(void)
{
	char cmdline[1024];
	int fd;

	fd = open("/proc/cmdline", O_RDONLY);
	if (fd >= 0) {
		int n = read(fd, cmdline, 1023);
		if (n < 0) n = 0;

		if (n > 0 && cmdline[n-1] == '\n') n--;

		cmdline[n] = 0;
		close(fd);
	} else {
		ALOGE("Failed to open /proc/cmdline!\n");
		exit(-1);
	}

	return (strstr(cmdline, "calibration=") != 0);
}

void disable_printk(void)
{
	system("echo 0 > /proc/sys/kernel/printk");
}

void enable_printk(void)
{
	system("echo 8 > /proc/sys/kernel/printk");
}

int is_valid(char *buf)
{
	return ((strlen(buf) > 1)
		&& ((buf[0]|0x20) == 'a')
		&& ((buf[1]|0x20) == 't'));
}

int chomp_chr(char *str, int len, char c)
{
        int sz;
        char *pcurr, *pnext;

        sz = len;
        pcurr = str;
        while((pnext = strchr(pcurr, c)) != NULL) {
                sz--;
                memmove(pnext, pnext +1, sz - (str - pnext));
                pcurr = pnext;
                str[sz] = '\0';
        }

        return sz;
}

int check_response_final(char *resp)
{
	const char *final[] = {
		"OK",
		"CONNECT",
		"ERROR",
		"+CMS ERROR:",
		"+CME ERROR:",
		"NO CARRIER",
		"NO ANSWER",
		"NO DIALTONE"
	};

	int num_final = sizeof(final)/sizeof(final[0]);
	int i = 0;

	if (resp == NULL) return 0;

	for (; i < num_final; i++) {
		if (strstr(resp, final[i]))
			return 1;
	}

	return 0;
}

int send_data_to_uart(char *data, int size)
{
	int ret = -1;

	if (uart_fd < 0) {
		ALOGD("%s: uart is not open!", __func__);
		return ret;
	}

	pthread_mutex_lock(&uart_mutex);

	ret = write(uart_fd, data, size);

	pthread_mutex_unlock(&uart_mutex);

	return ret;
}

void mux_reader_thread(const char*caller, int fd, void *pa)
{
	char databuf[ENG_BUFFER_SIZE];
	int len;
	int  cnt = 0;
	int  *runing = (int*)pa;

	ALOGD("Thread mux_reader_thread start: %s\n", caller);

	while(*runing) {
		cnt = 10;
		while(cnt--){
			memset(databuf, 0, ENG_BUFFER_SIZE);

			len = read(fd, databuf, ENG_BUFFER_SIZE);
			ENG_LOG("%s: got %d chars from modem: %s\n", caller, len, databuf);
			if (len < 0)
				ENG_LOG("%s: waiting for modem...[%d]\n", caller, cnt);
			else {
				int i = 0;

				databuf[len] = '\0';
				len = chomp_chr(databuf, len, '\r');

				/* debug statement */
				for (i = 0; i < len; i++) ENG_LOG("| %#04x |", databuf[i]);

				ENG_LOG("%s: write to uart port\n", caller);

				if (cnt >= 0) {
					send_data_to_uart(databuf, len);
				} else {
					strcpy(databuf, caller);
					strcat(databuf, ": ERROR\r\n");
					send_data_to_uart(databuf, strlen(databuf) +1);
				}

				break;
			}

			sleep(2);
		}

	}

}


void *at_resp_reader_thread(void *pa)
{
	mux_reader_thread("AT resp reader", at_mux1_fd, pa);

	return NULL;
}
#if 0
void *unsol_resp_reader_thread(void *pa)
{
	mux_reader_thread("Unsol resp reader", at_mux2_fd, pa);

	return NULL;
}
#endif
void cleanup_to_exit(void)
{
	pthread_mutex_destroy(&uart_mutex);

	close_devices();

	enable_printk();
}

int main(int argc, char **argv)
{
	char databuf[ENG_BUFFER_SIZE];
	int status, ret;
	pid_t pid;
	pthread_t tid1, tid2;

	ALOGD("mfserail started!\n");

	/* should not touch UART in calibration mode */
	if (is_calibration_mode()) {
		ALOGD("Calibration mode, hang forever!\n");
		pause();
		return -1;
	}

	if (argc != 4 && argc !=1) {
		usage();
		return -1;
	}

	if (argc == 4) {
		char tmp[DEV_NAME_SIZE];
		snprintf(tmp, sizeof(tmp), "/dev/%s", argv[1]);
		at_mux1_devname = strdup(tmp);
		snprintf(tmp, sizeof(tmp), "/dev/%s", argv[2]);
		uart_devname = strdup(tmp);
		have_phoneserver = strtol(argv[3], NULL, 10);
	}

	ALOGD("at_mux1_devname: %s, uart_devname: %s, have_phoneserver: %d\n",
			at_mux1_devname, uart_devname, have_phoneserver);

	if (open_devices()) {
		 return -1;
	}

	/*
	 * we set it here in case loglevel is not 0 at boot time.
	 * although a little bit late, but still better than doing nothing.
	 */
	disable_printk();

	pthread_mutex_init(&uart_mutex, NULL);

	status = 1;

#if 0
	if (pthread_create(&tid1, NULL, (void*)unsol_resp_reader_thread, &status) != 0) {
		ALOGD("unsol resp reading thread create error! ");
		return -1;
	}
#endif
	if (pthread_create(&tid2, NULL, (void*)at_resp_reader_thread, &status) != 0) {
		ALOGD("at resp reading thread create error! ");
		status = 0;
#if 0
		pthread_join(tid1, NULL);
#endif
		cleanup_to_exit();

		return -1;
	}


	pid = fork();
	if (pid) {
		waitpid(pid, NULL, 0);
		ALOGD("mfserail exited!\n");

		status = 0;
#if 0
		pthread_join(tid1, NULL);
#endif
		pthread_join(tid2, NULL);

		cleanup_to_exit();

		exit(0);
	}

	if (setsid() != getpid()){
		ALOGD("%s: setsid failed [%s]\n", __func__, strerror(errno));
	}

	while(1){
		int len;
		ENG_LOG("read from uart\n");
		memset(databuf, 0, ENG_BUFFER_SIZE);

		pthread_mutex_lock(&uart_mutex);
		len = read(uart_fd, databuf, ENG_BUFFER_SIZE);
		pthread_mutex_unlock(&uart_mutex);

		ENG_LOG(" -> got %d chars: %s\n", len, databuf);
		ENG_LOG("    last chars is: %x\n", databuf[len-1]);

		if (!strncmp (databuf, "sh", 2)) {
			ENG_LOG("start shell\n");
			enable_printk();
			system("/system/bin/sh");
			disable_printk();
			ENG_LOG("back from shell\n");
		} else if(is_valid(databuf)) {
			int cnt = 20;
			char chomp;
			/* discard last 0x0D or 0x0A in the string  */
			while (len > 0) {
				chomp = databuf[len-1];
				if (chomp != 0x0a && chomp != 0x0d)
					break;
				else
					len--;
			}
			/* increase the length to fill '\r' */
			len += 1;

			databuf[len-1]='\r';
			ENG_LOG("    change last chars to: %x\n", databuf[len-1]);
			ENG_LOG("write to at port\n");
			len = write(at_mux1_fd, databuf, len);
			sleep(1);

			ENG_LOG(" -> wrote %d chars\n", len);

			#if 0
			ENG_LOG("read from at port\n");
			while(cnt--){
				memset(databuf, 0, ENG_BUFFER_SIZE);

				len = read(at_mux1_fd, databuf, ENG_BUFFER_SIZE);
				ENG_LOG(" -> got %d chars from modem: %s\n", len, databuf);
				if (len < 0)
					ENG_LOG("waiting for modem...[%d]\n", cnt);
				else {
					int i;

					databuf[len] = '\0';
					len = chomp_chr(databuf, len, '\r');

					/* debug statement */
					for (i = 0; i < len; i++) ENG_LOG("| %#04x |", databuf[i]);

					ENG_LOG("write to uart port\n");
					if (cnt >= 0) {
						send_data_to_uart(databuf, len);
					} else {
						send_data_to_uart("ERROR\n\r", 8);
					}

					/* determine if the response is end */
					if (check_response_final(databuf)) break;
				}
			}
			#endif
		}
	}
	return 0;
}

