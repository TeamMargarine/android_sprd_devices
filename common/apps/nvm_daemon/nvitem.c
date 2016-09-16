#define LOG_TAG "nvitemd"

#include <cutils/log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <string.h>
#include <mtd/mtd-user.h>
#include <time.h>
#include <math.h>
#include <hardware_legacy/power.h>
#include "crc32b.h"

#define NV_DEBUG

#ifdef NV_DEBUG
#define NV_LOGD(x...) ALOGD( x )
#define NV_LOGE(x...) ALOGE( x )
#else
#define NV_LOGD(x...) do {} while(0)
#define NV_LOGE(x...) do {} while(0)
#endif

/* #define TEST_MODEM    1 */
#define DATA_BUF_LEN    (2048 * 10)
#define min(A,B)	(((A) < (B)) ? (A) : (B))

typedef uint16_t file_number_t;
typedef struct {
        file_number_t type;	/* 0 : fixnv ; 1 : runtimenv ; 2 : productinfo */
        uint16_t req_type;	/* 0 : data ; 1 : sync command */
        uint32_t offset;
        uint32_t size;
} request_header_t;

#define FIXNV_SIZE              (64 * 1024)
#define RUNTIMENV_SIZE		    (256 * 1024)
#define PRODUCTINFO_SIZE	    (3 * 1024)

#ifdef CONFIG_EMMC
/* see g_sprd_emmc_partition_cfg[] in u-boot/nand_fdl/fdl-2/src/fdl_partition.c */ 
#define PARTITION_MODEM		    "/dev/block/mmcblk0p2"
#define PARTITION_DSP		    "/dev/block/mmcblk0p3"
#define PARTITION_FIX_NV1	    "/dev/block/mmcblk0p4"
#define PARTITION_FIX_NV2	    "/dev/block/mmcblk0p5"
#define PARTITION_RUNTIME_NV1	"/dev/block/mmcblk0p6"
#define PARTITION_RUNTIME_NV2	"/dev/block/mmcblk0p7"
#define PARTITION_PROD_INFO1	"/dev/block/mmcblk0p8"
#define PARTITION_PROD_INFO2	"/dev/block/mmcblk0p9"
#define EMMC_MODEM_SIZE		    (8 * 1024 * 1024)
#else
#define PARTITION_MODEM		"modem"
#define PARTITION_DSP		"dsp"
#define F2R1_MODEM_SIZE		(3500 * 1024)
#define F4R2_MODEM_SIZE		(8 * 1024 * 1024)
#endif

#define MODEM_IMAGE_OFFSET  0
#define MODEM_BANK          "guestOS_2_bank"
#define DSP_IMAGE_OFFSET    0
#define DSP_BANK            "dsp_bank"
#define BUFFER_LEN	        (512)
#define REQHEADINFOCOUNT	(3)

unsigned char data[DATA_BUF_LEN];
#ifndef CONFIG_EMMC
int modem_offset=0, dsp_offset=0;
#endif
static int req_head_info_count = 0;

/* nand version */
unsigned short indexvalue = 0;
unsigned char fixnv_buffer[FIXNV_SIZE + 4];
unsigned long fixnv_buffer_offset;
unsigned char phasecheck_buffer[PRODUCTINFO_SIZE + 8];
unsigned long phasecheck_buffer_offset;
static int update_finxn_flag = 0;
/* nand version */

void req_head_info(request_header_t head)
{
	if (head.req_type == 0)
		NV_LOGD("type = %d  req_type = %d  offset = %d  size = %d\n",
				head.type, head.req_type, head.offset, head.size);
	else
		NV_LOGD("req_type = %d\n", head.req_type);
}

unsigned short calc_checksum(unsigned char *dat, unsigned long len)
{
	unsigned long checksum = 0;
	unsigned short *pstart, *pend;

	if (0 == (unsigned long)dat % 2)  {
		pstart = (unsigned short *)dat;
		pend = pstart + len / 2;
		while (pstart < pend) {
			checksum += *pstart;
			pstart ++;
		}
		if (len % 2)
			checksum += *(unsigned char *)pstart;
	} else {
		pstart = (unsigned char *)dat;
		while (len > 1) {
			checksum += ((*pstart) | ((*(pstart + 1)) << 8));
			len -= 2;
			pstart += 2;
		}
		if (len)
			checksum += *pstart;
	}
	checksum = (checksum >> 16) + (checksum & 0xffff);
	checksum += (checksum >> 16);

	return (~checksum);
}


int update_fixnvfile(const char *src, unsigned long size)
{
	int srcfd;
	unsigned long ret, length;
	unsigned long index, crc;
	unsigned short sum = 0, *dataaddr;
	unsigned char *flag;

	sum = calc_checksum(fixnv_buffer, size - 4);
	dataaddr = (unsigned short *)(fixnv_buffer + size - 4);
	*dataaddr = sum;
	dataaddr = (unsigned short *)(fixnv_buffer + size - 2);
	*dataaddr = (indexvalue + 1) % 100; /* 0 -- 99 */

	flag = (unsigned char *)(fixnv_buffer + size); *flag = 0x5a;
	flag ++; *flag = 0x5a;
	flag ++; *flag = 0x5a;
	flag ++; *flag = 0x5a;

	if (req_head_info_count < REQHEADINFOCOUNT) {
		req_head_info_count++;
		NV_LOGD("sum = 0x%04x  indexvalue = %d\n", sum, indexvalue);
	}

#ifdef CONFIG_EMMC
	srcfd = open(src, O_RDWR, 0);
#else
	srcfd = open(src, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
#endif
	if (srcfd < 0) {
		NV_LOGE("create %s error\n", src);
		return -1;
	}

	ret = write(srcfd, fixnv_buffer, size + 4);
	if (ret != (size + 4)) {
		NV_LOGE("write %s error size = %ld  ret = %ld\n", src, (size + 4), ret);
	} else {
		if (req_head_info_count < REQHEADINFOCOUNT) {
			req_head_info_count++;
			NV_LOGD("write %s right size = %ld  ret = %ld\n", src, (size + 4), ret);
		}
	}
	fsync(srcfd);
	close(srcfd);

	return 1;
}

int update_backupfile(const char *dst, unsigned long size)
{
	int dstfd;
	unsigned long ret, length;

	if (req_head_info_count < REQHEADINFOCOUNT) {
		req_head_info_count++;
		NV_LOGD("update bkupfixnv = %s  size = %ld\n", dst, size);
	}

#ifdef CONFIG_EMMC
	dstfd = open(dst, O_RDWR, 0);
#else
	dstfd = open(dst, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
#endif
	if (dstfd < 0) {
		NV_LOGE("create %s error\n", dst);
		return -1;
	}

	ret = write(dstfd, fixnv_buffer, size + 4);
	if (ret != (size + 4)) {
		NV_LOGE("write %s error size = %ld  ret = %ld\n", dst, (size + 4), ret);
	} else {
		if (req_head_info_count < REQHEADINFOCOUNT) {
			req_head_info_count++;
			NV_LOGD("write %s right size = %ld  ret = %ld\n", dst, (size + 4), ret);
		}
	}
	fsync(dstfd);
	close(dstfd);

	return 1;
}

#ifdef CONFIG_EMMC
int update_productinfofile_emmc(const char *src, const char *dst, unsigned long size)
{
	int srcfd, dstfd;
	unsigned long ret, length;
	unsigned long index, crc;
	unsigned short sum = 0, *dataaddr;

	if (strcmp(src, dst) == 0)
		return -1;

	/* 5a is defined by raw data */
	phasecheck_buffer[size] = 0x5a;
	phasecheck_buffer[size + 1] = 0x5a;
	phasecheck_buffer[size + 2] = 0x5a;
	phasecheck_buffer[size + 3] = 0x5a;

	/* crc32 */
	crc = crc32b(0xffffffff, phasecheck_buffer, size + 4);
	phasecheck_buffer[size + 4] = (crc & (0xff << 24)) >> 24;
	phasecheck_buffer[size + 5] = (crc & (0xff << 16)) >> 16;
	phasecheck_buffer[size + 6] = (crc & (0xff << 8)) >> 8;
	phasecheck_buffer[size + 7] = crc & 0xff;

	srcfd = open(src, O_RDWR, 0);
	if (srcfd < 0) {
		NV_LOGE("create %s error\n", src);
		return -1;
	}

	ret = write(srcfd, phasecheck_buffer, size + 8);
	fsync(srcfd);
	if (ret != (size + 8)) {
		NV_LOGE("write %s error size = %ld  ret = %ld\n", src, (size + 8), ret);
	} else {
		if (req_head_info_count < REQHEADINFOCOUNT) {
			req_head_info_count++;
			NV_LOGD("write %s right size = %ld  ret = %ld\n", src, (size + 8), ret);
		}
	}
	close(srcfd);

	dstfd = open(dst, O_RDWR, 0);
	if (dstfd < 0) {
		NV_LOGE("create %s error\n", dst);
		return -1;
	}

	ret = write(dstfd, phasecheck_buffer, size + 8);
	fsync(dstfd);
	if (ret != (size + 4)) {
		NV_LOGE("write %s error size = %ld  ret = %ld\n", dst, (size + 4), ret);
	} else {
		if (req_head_info_count < REQHEADINFOCOUNT) {
			req_head_info_count++;
			NV_LOGE("write %s right size = %ld  ret = %ld\n", dst, (size + 4), ret);
		}
	}
	close(dstfd);
	return 1;
}
#else
int update_productinfofile(const char *src, const char *dst, unsigned long size)
{
	int srcfd, dstfd;
	unsigned long ret, length;
	unsigned long index, crc;
	unsigned short sum = 0, *dataaddr;

	if (strcmp(src, dst) == 0)
		return -1;

	sum = calc_checksum(phasecheck_buffer, size);
	dataaddr = (unsigned short *)(phasecheck_buffer + size);
	*dataaddr = sum;
	dataaddr = (unsigned short *)(phasecheck_buffer + size + 2);
	*dataaddr = (indexvalue + 1) % 100; /* 0 -- 99 */

	if (req_head_info_count < REQHEADINFOCOUNT) {
		req_head_info_count++;
		NV_LOGD("sum = 0x%04x  indexvalue = %d  new = %d\n", sum, indexvalue, *dataaddr);
	}

	srcfd = open(src, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);;
	if (srcfd < 0) {
		NV_LOGE("create %s error\n", src);
		return -1;
	}

	ret = write(srcfd, phasecheck_buffer, size + 4);
	fsync(srcfd);
	if (ret != (size + 4)) {
		NV_LOGE("write %s error size = %d  ret = %d\n", src, (size + 4), ret);
	} else {
		if (req_head_info_count < REQHEADINFOCOUNT) {
			req_head_info_count++;
			NV_LOGD("write %s right size = %d  ret = %d\n", src, (size + 4), ret);
		}
	}
	close(srcfd);

	dstfd = open(dst, O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
	if (dstfd < 0) {
		NV_LOGE("create %s error\n", dst);
		return -1;
	}

	ret = write(dstfd, phasecheck_buffer, size + 4);
	fsync(dstfd);
	if (ret != (size + 4)) {
		NV_LOGE("write %s error size = %d  ret = %d\n", dst, (size + 4), ret);
	} else {
		if (req_head_info_count < REQHEADINFOCOUNT) {
			req_head_info_count++;
			NV_LOGD("write %s right size = %d  ret = %d\n", dst, (size + 4), ret);
		}
	}

	close(dstfd);
	return 1;
}
#endif

int main(int argc, char **argv)
{
	int pipe_fd = 0, fix_nvitem_fd = 0, runtime_nvitem_fd = 0, runtime_nvitem_bkfd = 0, product_info_fd = 0, fd = 0, ret = 0;
	int r_cnt, w_cnt, res, total, length;
	request_header_t req_header;
	unsigned long crc;
	unsigned short *indexaddress;

	ret = nice(-16);
	if ((-1 == ret) && (errno == EACCES))
		NV_LOGE("set priority fail\n");
reopen_vbpipe:
	req_head_info_count = 0;
	NV_LOGD("reopen vbpipe\n");

	pipe_fd = open("/dev/vbpipe1", O_RDWR);
	if (pipe_fd < 0) {
		NV_LOGE("cannot open vbpipe1\n");
		exit(1);
	}

	while (1) {
		r_cnt = read(pipe_fd, &req_header, sizeof(request_header_t));
		acquire_wake_lock(PARTIAL_WAKE_LOCK, "NvItemdLock");
		if (req_head_info_count < REQHEADINFOCOUNT) {
			req_head_info_count++;
			NV_LOGD("1 r_cnt = %d\n", r_cnt);
		}
		/* write_proc_file("/proc/nk/stop", 0,  "2");     --->   echo 2 > /proc/nk/stop */
		/* write_proc_file("/proc/nk/restart", 0,  "2");  --->   echo 2 > /proc/nk/restart */
		if (r_cnt == 0) {
			/* check if the system is shutdown , if yes just reload the binary image */
			NV_LOGE("1r vbpipe1 return 0, modem assert finished");
			/* when read returns 0 : it means vbpipe has received
			 * sysconf so we should  close the file descriptor
			 * and re-open the pipe.
			 */
			close(pipe_fd);
			NV_LOGD("goto reopen vbpipe\n");
			goto reopen_vbpipe;
		}

		if (r_cnt < 0) {
			NV_LOGE("1 no nv data : %d\n", r_cnt);
			release_wake_lock("NvItemdLock");
			sleep(1);
			continue;
		}

		if (req_head_info_count < REQHEADINFOCOUNT) {
			req_head_info_count++;
			req_head_info(req_header);
		}

		if (req_header.req_type == 1) {
			/* 1 : sync command */
			/* mocor update file : fixnv --> runtimenv --> backupfixnv */
			if (update_finxn_flag == 1)
#ifdef CONFIG_EMMC
				update_backupfile(PARTITION_FIX_NV2, FIXNV_SIZE);
#else
				update_backupfile("/backupfixnv/fixnv.bin", FIXNV_SIZE);
#endif

			update_finxn_flag = 0;

			/* 1 : sync command */
			NV_LOGD("sync start\n");

			sync();

			NV_LOGD("sync end\n");

			req_header.type = 0;
			NV_LOGD("write back sync start\n");

			w_cnt = write(pipe_fd, &req_header, sizeof(request_header_t));
			NV_LOGD("write back sync end w_cnt = %d\n", w_cnt);
			if (w_cnt < 0) {
				NV_LOGE("failed to write sync command\n");
			}
			release_wake_lock("NvItemdLock");
			continue;
		}

		if (req_header.type == 0) {
			if ((req_header.offset + req_header.size) > FIXNV_SIZE) {
				NV_LOGE("Fixnv is too big\n");
			}

			memset(fixnv_buffer, 0xff, FIXNV_SIZE + 4);
#ifdef CONFIG_EMMC
			fix_nvitem_fd = open(PARTITION_FIX_NV1, O_RDWR, 0);
#else
			fix_nvitem_fd = open("/fixnv/fixnv.bin", O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
#endif
			if (fix_nvitem_fd < 0) {
				NV_LOGE("cannot open fixnv mtd device\n");
				close(pipe_fd);
				release_wake_lock("NvItemdLock");
				exit(1);
			}

			fd = fix_nvitem_fd;
			length = FIXNV_SIZE;
			read(fd, fixnv_buffer, length + 4);
			close(fd);

			indexaddress = (unsigned short *)(fixnv_buffer + length - 2);
			indexvalue = *indexaddress;
			fixnv_buffer_offset = req_header.offset;
		} else if (req_header.type == 1) {
			if ((req_header.offset + req_header.size) > RUNTIMENV_SIZE) {
				NV_LOGE("Runtimenv is too big\n");
			}

#ifdef CONFIG_EMMC
			runtime_nvitem_fd = open(PARTITION_RUNTIME_NV1, O_RDWR, 0);
#else
			runtime_nvitem_fd = open("/runtimenv/runtimenv.bin", O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
#endif
			if (runtime_nvitem_fd < 0) {
				NV_LOGE("cannot open runtimenv mtd device\n");
				close(pipe_fd);
				release_wake_lock("NvItemdLock");
				exit(1);
			}

			fd = runtime_nvitem_fd;
			length = RUNTIMENV_SIZE;

			res = lseek(fd, req_header.offset, SEEK_SET);
			if (res < 0) {
				NV_LOGE("lseek runtime_nvitem_fd errno\n");
			}

#ifdef CONFIG_EMMC
			runtime_nvitem_bkfd = open(PARTITION_RUNTIME_NV2, O_RDWR, 0);
#else
			runtime_nvitem_bkfd = open("/runtimenv/runtimenvbkup.bin", O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
#endif
			if (runtime_nvitem_bkfd < 0) {
				NV_LOGE("cannot open runtimenv mtd backip device\n");
				close(pipe_fd);
				close(fd);
				release_wake_lock ("NvItemdLock");
				exit(1);
			}
			res = lseek(runtime_nvitem_bkfd, req_header.offset, SEEK_SET);
			if (res < 0) {
				NV_LOGE("lseek runtime_nvitem_bkfd errno\n");
			}
		} else if (req_header.type == 2) {
			/* phase check */
			if ((req_header.offset + req_header.size) > PRODUCTINFO_SIZE) {
				NV_LOGE("Productinfo is too big\n");
			}

			memset(phasecheck_buffer, 0xff, PRODUCTINFO_SIZE + 8);
#ifdef CONFIG_EMMC
			product_info_fd = open(PARTITION_PROD_INFO1, O_RDWR, 0);
#else
			product_info_fd = open("/productinfo/productinfo.bin", O_RDWR | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
#endif
			if (product_info_fd < 0) {
				NV_LOGE("cannot open productinfo mtd device\n");
				close(pipe_fd);
				release_wake_lock("NvItemdLock");
				exit(1);
			}

			fd = product_info_fd;
			length = PRODUCTINFO_SIZE;
#ifdef CONFIG_EMMC
			read(fd, phasecheck_buffer, length + 8);
#else
			read(fd, phasecheck_buffer, length + 4);
			indexaddress = (unsigned short *)(phasecheck_buffer + length + 2);
			indexvalue = *indexaddress;
#endif
			close(fd);
			phasecheck_buffer_offset = req_header.offset;
		} else {
			NV_LOGE("file type error\n");
			release_wake_lock("NvItemdLock");
			continue;
		}

		total = req_header.size;
		while (total > 0) {
			memset(data, 0xff, DATA_BUF_LEN);
			if (total > DATA_BUF_LEN)
				r_cnt = read(pipe_fd, data, DATA_BUF_LEN);
			else
				r_cnt = read(pipe_fd, data, total);

			if (req_head_info_count < REQHEADINFOCOUNT) {
				req_head_info_count++;
				NV_LOGD("2 r_cnt = %d  size = %d\n", r_cnt, total);
			}

			if (r_cnt == 0) {
				if (length == RUNTIMENV_SIZE) {
					close(fd);
					close(runtime_nvitem_bkfd);
				}
				NV_LOGE("2r vbpipe1 return 0, modem assert finished");
				close(pipe_fd);
				NV_LOGD("goto reopen vbpipe\n");
				release_wake_lock("NvItemdLock");
				goto reopen_vbpipe;
			}

			if (r_cnt < 0) {
				NV_LOGE("2 no nv data : %d\n", r_cnt);
				continue;
			}

			/*if (r_cnt > 1024)
			  array_value(length, data, 1024);
			  else
			  array_value(length, data, r_cnt);*/

			if (length == RUNTIMENV_SIZE) {
				w_cnt = write(fd, data, r_cnt);
				if (w_cnt < 0) {
					NV_LOGE("failed to write runrimenv write\n");
					continue;
				}
				//////////////
				w_cnt = write(runtime_nvitem_bkfd, data, r_cnt);
				if (w_cnt < 0) {
					NV_LOGE("failed to write runrimebkupnv write\n");
					continue;
				}
				//////////////
			} else if (length == FIXNV_SIZE) {
				memcpy(fixnv_buffer + fixnv_buffer_offset, data, r_cnt);
				fixnv_buffer_offset += r_cnt;
			} else if (length == PRODUCTINFO_SIZE) {
				memcpy(phasecheck_buffer + phasecheck_buffer_offset, data, r_cnt);
				phasecheck_buffer_offset += r_cnt;
			}

			total -= r_cnt;
		} /* while (total > 0) */
		if (length == RUNTIMENV_SIZE) {
			fsync(fd);
			close(fd);
			fsync(runtime_nvitem_bkfd);
			close(runtime_nvitem_bkfd);

			if (req_head_info_count < REQHEADINFOCOUNT) {
				req_head_info_count++;
				NV_LOGD("write runrimenv and runrimebkupnv finished\n");
			}
		} else if (length == PRODUCTINFO_SIZE) {
#ifdef CONFIG_EMMC
			update_productinfofile_emmc(PARTITION_PROD_INFO1, PARTITION_PROD_INFO2, length);
#else
			update_productinfofile("/productinfo/productinfo.bin", "/productinfo/productinfobkup.bin", length);
#endif
		} else if (length == FIXNV_SIZE) {
			update_finxn_flag = 1;
#ifdef CONFIG_EMMC
			update_fixnvfile(PARTITION_FIX_NV1, length);
#else
			update_fixnvfile("/fixnv/fixnv.bin", length);
#endif
		}

		release_wake_lock("NvItemdLock");
	} /* while(1) */

	NV_LOGD("close pipe_fd fixnv runtimenv\n");
	close(pipe_fd);

	return 0;
}

