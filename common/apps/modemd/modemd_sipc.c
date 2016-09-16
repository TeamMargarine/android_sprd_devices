#define LOG_TAG 	"MODEMD"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <cutils/properties.h>
#include <utils/Log.h>
#include "modemd.h"

/******************************************************
 *
 ** sipc interface begin
 *
 *****************************************************/
int load_sipc_image(char *fin, int offsetin, char *fout, int offsetout, int size)
{
	int res = -1, fdin, fdout, bufsize, i, rsize, rrsize, wsize;
	char buf[8192];
	int buf_size = sizeof(buf);

	MODEMD_LOGD("Loading %s in bank %s:%d %d", fin, fout, offsetout, size);

	fdin = open(fin, O_RDONLY, 0);
	fdout = open(fout, O_RDWR, 0);
	if (fdin < 0) {
		if (fdout>=0)
			close(fdout);

		MODEMD_LOGE("failed to open %s", fin);
		return -1;
	}
	if (fdout < 0) {
		close(fdin);
		MODEMD_LOGE("failed to open %s", fout);
		return -1;
	}

	if (lseek(fdin, offsetin, SEEK_SET) != offsetin) {
		MODEMD_LOGE("failed to lseek %d in %s", offsetin, fin);
		goto leave;
	}

	if (lseek(fdout, offsetout, SEEK_SET) != offsetout) {
		MODEMD_LOGE("failed to lseek %d in %s", offsetout, fout);
		goto leave;
	}

	for (i = 0; size > 0; i += min(size, buf_size)) {
		rsize = min(size, buf_size);
		rrsize = read(fdin, buf, rsize);
		if (rrsize != rsize) {
			MODEMD_LOGE("failed to read %s", fin);
			goto leave;
		}
		wsize = write(fdout, buf, rsize);
		if (wsize != rsize) {
			MODEMD_LOGE("failed to write %s [wsize = %d  rsize = %d  remain = %d]",
					fout, wsize, rsize, size);
			goto leave;
		}
		size -= rsize;
	}

	res = 0;
leave:
	close(fdin);
	close(fdout);
	return res;
}

int load_sipc_modem_img(int modem, int is_modem_assert)
{
	char modem_partition[128] = {0};
	char dsp_partition[128] = {0};
	char modem_bank[128] = {0};
	char dsp_bank[128] = {0};
	int sipc_modem_size = 0;
	int sipc_dsp_size = 0;
	char alive_info[20]={0};
	int i, ret;

	if(modem == TD_MODEM) {
		sipc_modem_size = TD_MODEM_SIZE;
		sipc_dsp_size = TD_DSP_SIZE;
		strcpy(modem_partition, TD_PARTI_MODEM);
		strcpy(dsp_partition, TD_PARTI_DSP);
		strcpy(modem_bank, TD_MODEM_BANK);
		strcpy(dsp_bank, TD_DSP_BANK);
	} else if(modem == W_MODEM) {
		sipc_modem_size = W_MODEM_SIZE;
		sipc_dsp_size = W_DSP_SIZE;
		strcpy(modem_partition, W_PARTI_MODEM);
		strcpy(dsp_partition, W_PARTI_DSP);
		strcpy(modem_bank, W_MODEM_BANK);
		strcpy(dsp_bank, W_DSP_BANK);
	}

	/* write 1 to stop*/
	if(modem == TD_MODEM) {
		MODEMD_LOGD("write 1 to %s", TD_MODEM_STOP);
		write_proc_file(TD_MODEM_STOP, 0, "1");
	} else if(modem == W_MODEM) {
		MODEMD_LOGD("write 1 to %s", W_MODEM_STOP);
		write_proc_file(W_MODEM_STOP, 0, "1");
	}

	/* load modem */
	MODEMD_LOGD("load modem image from %s to %s, len=%d",
			modem_partition, modem_bank, sipc_modem_size);
	load_sipc_image(modem_partition, 0, modem_bank, 0, sipc_modem_size);

	/* load dsp */
	MODEMD_LOGD("load dsp image from %s to %s, len=%d",
			dsp_partition, dsp_bank, sipc_dsp_size);
	load_sipc_image(dsp_partition, 0, dsp_bank, 0, sipc_dsp_size);

	stop_service(modem, 0);

	/* write 1 to start*/
	if(modem == TD_MODEM) {
		MODEMD_LOGD("write 1 to %s", TD_MODEM_START);
		write_proc_file(TD_MODEM_START, 0, "1");
		strcpy(alive_info, "TD Modem Alive");
	} else if(modem == W_MODEM) {
		MODEMD_LOGD("write 1 to %s", W_MODEM_START);
		write_proc_file(W_MODEM_START, 0, "1");
		strcpy(alive_info, "W Modem Alive");
	} else {
		MODEMD_LOGE("error unkown modem  alive_info");
	}
	MODEMD_LOGD("wait for 20s\n");

	sleep(20);

	if(is_modem_assert) {
		/* info socket clients that modem is reset */
		for(i = 0; i < MAX_CLIENT_NUM; i++) {
			MODEMD_LOGE("client_fd[%d]=%d\n",i, client_fd[i]);
			if(client_fd[i] >= 0) {
				ret = write(client_fd[i], alive_info,strlen(alive_info));
				MODEMD_LOGE("write %d bytes to client_fd[%d]:%d to info modem is alive",
						ret, i, client_fd[i]);
				if(ret < 0) {
					MODEMD_LOGE("reset client_fd[%d]=-1",i);
					close(client_fd[i]);
					client_fd[i] = -1;
				}
			}
		}
	}

	start_service(modem, 0, 1);

	return 0;
}

/* loop detect sipc modem state */
void* detect_sipc_modem(void *param)
{
	char assert_dev[30] = {0};
	char watchdog_dev[30] = {0};
	int i, ret, assert_fd, watchdog_fd, max_fd, fd = -1;
	fd_set rfds;
	int is_reset, modem = -1;
	char buf[128], prop[5];
	int numRead;
	int is_assert = 0;

	if(param != NULL)
		modem = *((int *)param);

	if(modem == TD_MODEM) {
		property_get(TD_ASSERT_PRO, assert_dev, TD_ASSERT_DEV);
		snprintf(watchdog_dev, sizeof(watchdog_dev), "%s", TD_WATCHDOG_DEV);
	} else if(modem == W_MODEM) {
		property_get(W_ASSERT_PRO, assert_dev, W_ASSERT_DEV);
		snprintf(watchdog_dev, sizeof(watchdog_dev), "%s", W_WATCHDOG_DEV);
	} else
		MODEMD_LOGE("%s: input wrong modem type!", __func__);

	assert_fd = open(assert_dev, O_RDWR);
	MODEMD_LOGD("%s: open assert dev: %s, fd = %d", __func__, assert_dev, assert_fd);
	if (assert_fd < 0) {
		MODEMD_LOGE("open %s failed, error: %s", assert_dev, strerror(errno));
		exit(-1);
	}

	watchdog_fd = open(watchdog_dev, O_RDONLY);
	MODEMD_LOGD("%s: open watchdog dev: %s, fd = %d", __func__, watchdog_dev, watchdog_fd);
	if (watchdog_fd < 0) {
		MODEMD_LOGE("open %s failed, error: %s", watchdog_dev, strerror(errno));
		exit(-1);
	}

	max_fd = watchdog_fd > assert_fd ? watchdog_fd : assert_fd;

	FD_ZERO(&rfds);
	FD_SET(assert_fd, &rfds);
	FD_SET(watchdog_fd, &rfds);

	for (;;) {
		MODEMD_LOGD("%s: wait for modem assert/hangup event ...", __func__);
		do {
			ret = select(max_fd + 1, &rfds, NULL, NULL, 0);
		} while(ret == -1 && errno == EINTR);
		if (ret > 0) {
			if (FD_ISSET(assert_fd, &rfds)) {
				fd = assert_fd;
			} else if (FD_ISSET(watchdog_fd, &rfds)) {
				fd = watchdog_fd;
			}
			if(fd <= 0) {
				MODEMD_LOGE("none of assert and watchdog fd is readalbe");
				sleep(1);
				continue;
			}
			memset(buf, 0, sizeof(buf));
			numRead = read(fd, buf, sizeof(buf));
			if (numRead <= 0) {
				MODEMD_LOGE("read %d return %d, errno = %s", fd , numRead, strerror(errno));
				sleep(1);
				continue;
			}

			MODEMD_LOGD("buf=%s", buf);
			if(strstr(buf, "Modem Reset")) {
				MODEMD_LOGD("modem reset happen, reload modem...");
				load_sipc_modem_img(modem, is_assert);
				is_assert = 0;
			} else if(strstr(buf, "Modem Assert") || strstr(buf, "wdtirq")) {
				if(strstr(buf, "wdtirq")) {
					if(modem == TD_MODEM) {
						MODEMD_LOGD("td modem hangup");
						strcpy(buf, "TD Modem Hang");
						numRead = sizeof("TD Modem Hang");
					} else if(modem == W_MODEM) {
						MODEMD_LOGD("w modem hangup");
						strcpy(buf, "W Modem Hang");
						numRead = sizeof("W Modem Hang");
					}
				} else {
					MODEMD_LOGD("modem assert happen");
				}
				is_assert = 1;

				/* info socket clients that modem is assert or hangup */
				for(i = 0; i < MAX_CLIENT_NUM; i++) {
					MODEMD_LOGE("client_fd[%d]=%d\n",i, client_fd[i]);
					if(client_fd[i] >= 0) {
						ret = write(client_fd[i], buf, numRead);
						MODEMD_LOGE("write %d bytes to client_fd[%d]:%d to info modem is assert",
							numRead, i, client_fd[i]);
						if(ret < 0) {
							MODEMD_LOGE("reset client_fd[%d]=-1",i);
							close(client_fd[i]);
							client_fd[i] = -1;
						}
					}
				}

				/* reset or not according to property */
				memset(prop, 0, sizeof(prop));
				property_get(MODEMRESET_PROPERTY, prop, "0");
				is_reset = atoi(prop);
				if(is_reset) {
					MODEMD_LOGD("modem reset is enabled, reload modem...");
					load_sipc_modem_img(modem, is_assert);
					is_assert = 0;
				} else {
					MODEMD_LOGD("modem reset is not enabled , will not reset");
				}
			}
		}
	}
}
/******************************************************
 *
 ** sipc interface end
 *
 *****************************************************/

