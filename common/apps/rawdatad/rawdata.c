#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/inotify.h>
#include <cutils/properties.h>


#include "crc32.h"
#include "rawdata.h"


static char * inotify_event_array[] = {
	"File was accessed",
	"File was modified",
	"File attributes were changed",
	"writtable file closed",
	"Unwrittable file closed",
	"File was opened",
	"File was moved from X",
	"File was moved to Y",
	"Subfile was created",
	"Subfile was deleted",
	"Self was deleted",
	"Self was moved",
	"",
	"Backing fs was unmounted",
	"Event queued overflowed",
	"File was ignored"
};


static const char* RAW_DATA_PARTITION[RAWDATA_PARTITION_NUM] = {
	"/dev/block/mmcblk0p8",
	"/dev/block/mmcblk0p9"
};

static const char* PRODUCT_INFO[] = {
	"/productinfo/productinfo.bin",
	"/productinfo/factorymode.file",
	"/productinfo/alarm_flag",
	"/productinfo/poweron_timeinmillis"
};

static const char* NO_FEADBACK[] = {
	"/productinfo/productinfo.bin"
};

static const char * monitored_files[] = {
	"/productinfo/"
};

static struct wd_name wd_array[RAWDATA_MONITOR_ITEM];
static unsigned short crc;
static unsigned int   counter;
static unsigned char  rawdata_buffer[RAWDATA_PARTITION_NUM][RAWDATA_BUFFER_SIZE];

static void dump_data(unsigned char *data)
{
	int i;

	DBG("%s: First %d data",__FUNCTION__, RAWDATA_DUMP_SIZE);
	for(i=0; i<RAWDATA_DUMP_SIZE; i++) {
		DBG("data[%d]=0x%x",i, data[i]);
	}

	DBG("%s: Last %d data",__FUNCTION__, RAWDATA_DUMP_SIZE);
	for(i=RAWDATA_DATASIZE-RAWDATA_DUMP_SIZE; i<RAWDATA_DATASIZE; i++) {
		DBG("data[%d]=0x%x",i, data[i]);
	}

	DBG("%s: Extra %d data",__FUNCTION__, RAWDATA_DUMP_SIZE);
	for(i=RAWDATA_DATASIZE; i<RAWDATA_DATASIZE+RAWDATA_DUMP_SIZE; i++) {
		DBG("data[%d]=0x%x",i, data[i]);
	}
}

static int read_mmc_partition(const char *partname, loff_t offset, unsigned char *buf, unsigned int size)
{
	int fd;
	unsigned int rsize;

	DBG("%s: patrname=%s,offset=%lld",__FUNCTION__, partname, offset);

	fd = open(partname, O_RDWR, 0);
	if (fd < 0) {
		DBG("failed to open %s\n", partname);
		return -1;
	}

	if (lseek(fd, offset, SEEK_SET) != offset) {
		DBG("failed to lseek %lld in %s\n", offset, partname);
		close(fd);
		return -1;
	}

	rsize = read(fd, buf, size);
	if (rsize != size) {
		DBG("read error rsize = %d  size = %d\n", rsize, size);
		close(fd);
		return -1;
	}

	close(fd);

	DBG("%s: rsize=%d; size=%d",__FUNCTION__, rsize, size);

	return rsize;
}

static int write_mmc_partition(const char *partname, loff_t offset, unsigned char *buf, unsigned int size)
{
	int fd;
	unsigned int wsize;

	DBG("%s: patrname=%s,offset=%lld",__FUNCTION__, partname, offset);

	fd = open(partname, O_RDWR, 0);
	if (fd < 0) {
		DBG("failed to open %s\n", partname);
		return -1;
	}

	if (lseek(fd, offset, SEEK_SET) != offset) {
		DBG("failed to lseek %lld in %s\n", offset, partname);
		close(fd);
		return -1;
	}

	wsize = write(fd, buf, size);
	if (wsize != size) {
		DBG("write error rsize = %d  size = %d\n", wsize, size);
		close(fd);
		return -1;
	}

	fsync(fd);

	close(fd);

	DBG("%s: wsize=%d; size=%d",__FUNCTION__, wsize, size);

	return wsize;
}

/* return >=0 : the data is correct
               -1    : crc failed, the data is incorrect
*/
static int rawdata_check_block(int partition, int block)
{
	int ret=-1;
	unsigned int i;
	unsigned char *ptr=rawdata_buffer[partition];
	unsigned int crc_old=0, crc_calc=0;

	DBG("%s: partition=%d, block=%d",__FUNCTION__, partition, block);

	memset(ptr, 0xff, RAWDATA_BUFFER_SIZE);

	ret=read_mmc_partition(RAW_DATA_PARTITION[partition],
						block*RAWDATA_BUFFER_SIZE,
						ptr,
						RAWDATA_BUFFER_SIZE);
	if(ret==-1) {
		DBG("%s: read %s error, [%s]",__FUNCTION__, RAW_DATA_PARTITION[partition], strerror(errno));
		return ret;
	}

	dump_data(ptr);

	if(block == RAWDATA_BLOCK_PHASECHECK) {
		ret = 0;
		crc_old = (unsigned int)((ptr[RAWDATA_DATASIZE])<<24)|
			      (unsigned int)((ptr[RAWDATA_DATASIZE+1])<<16)|
			      (unsigned int)((ptr[RAWDATA_DATASIZE+2])<<8)|
			      (unsigned int)(ptr[RAWDATA_DATASIZE+3]);
		crc_calc = crc32b(0xFFFFFFFF,ptr,RAWDATA_DATASIZE);

	} else {
		ret = (unsigned int)((ptr[RAWDATA_DATASIZE])<<24)|
		      (unsigned int)((ptr[RAWDATA_DATASIZE+1])<<16)|
		      (unsigned int)((ptr[RAWDATA_DATASIZE+2])<<8)|
		      (unsigned int)(ptr[RAWDATA_DATASIZE+3]);
		crc_old = (unsigned int)((ptr[RAWDATA_CRC_OFFSET])<<24)|
			      (unsigned int)((ptr[RAWDATA_CRC_OFFSET+1])<<16)|
			      (unsigned int)((ptr[RAWDATA_CRC_OFFSET+2])<<8)|
			      (unsigned int)(ptr[RAWDATA_CRC_OFFSET+3]);
		crc_calc = crc32b(0xFFFFFFFF,ptr,RAWDATA_CRC_OFFSET);
	}

	DBG("%s: ret=0x%x, crc_old=0x%x, crc_calc=0x%x",__FUNCTION__, ret, crc_old, crc_calc);
	if(crc_old != crc_calc) {
		ret = -1;
	}

	return ret;
}


static void wirte_productinfo(int partition, int block)
{
	FILE *fp;
	fp = fopen(PRODUCT_INFO[block],"w+");

	if (!fp) {
		DBG("%s: open %s fail [%s]",__FUNCTION__, PRODUCT_INFO[block], strerror(errno));
		return;
	}

	fwrite(rawdata_buffer[partition],RAWDATA_DATASIZE,1,fp);

	fclose(fp);

	if(block == 0){
		if(chmod(PRODUCT_INFO[block],0774)<0)
			DBG("chmod productinfo.bin error");
	}
	DBG("%s: write %s successfully",__FUNCTION__, PRODUCT_INFO[block]);
}

static void init_productinfo()
{
	unsigned int i,j, partition, error_partition;
	int valid1, valid2;

	DBG("%s, BLOCK_NUM=%d",__FUNCTION__,NUM_ELEMS(PRODUCT_INFO));

	for (i=0; i<NUM_ELEMS(PRODUCT_INFO); i++) {
		DBG("==================%s Start==================",__FUNCTION__);
		DBG("%s: handle block[%d]",__FUNCTION__, i);

		//read data from "/dev/block/mmcblk0p8"
		valid1 = rawdata_check_block(RAWDATA_PARTITION0,i);
		if(valid1 == -1) {
			DBG("%s: Read %s from %s ERROR",__FUNCTION__, \
				PRODUCT_INFO[i], RAW_DATA_PARTITION[RAWDATA_PARTITION0]);
		}

		//read data from "/dev/block/mmcblk0p9"
		valid2 = rawdata_check_block(RAWDATA_PARTITION1,i);
		if(valid2 == -1) {
			DBG("%s: Read %s from %s ERROR",__FUNCTION__, \
				PRODUCT_INFO[i], RAW_DATA_PARTITION[RAWDATA_PARTITION1]);
		}

		DBG("%s: %s valid1=%d; valid2=%d",__FUNCTION__,PRODUCT_INFO[i],valid1,valid2);

		if (valid1==-1 && valid2==-1) {
			DBG("%s: Get %s CRC error",__FUNCTION__, PRODUCT_INFO[i]);
			DBG("%s: Won't create file %s",__FUNCTION__, PRODUCT_INFO[i]);
		} else {
			//select rawdata1 or rawdata2
			partition  = (valid1>valid2 ? 0:1);
			DBG("%s: Use %s in %s",__FUNCTION__, \
				PRODUCT_INFO[i],RAW_DATA_PARTITION[partition]);
			//verify the accurate buffer, write to productinfo
			wirte_productinfo(partition,i);

			//write right data to error block
			if(valid1!=valid2) {
				error_partition = 1 - partition;
				DBG("%s: Write %s from %s to %s",__FUNCTION__, \
					PRODUCT_INFO[i], \
					RAW_DATA_PARTITION[partition], \
					RAW_DATA_PARTITION[error_partition]);

				write_mmc_partition(RAW_DATA_PARTITION[error_partition], \
										i * RAWDATA_BUFFER_SIZE, \
										rawdata_buffer[partition], \
										RAWDATA_BUFFER_SIZE);
			}
		}
		DBG("==================%s End==================",__FUNCTION__);
	}
}

static void read_productinfo(unsigned int block, unsigned char *pdata)
{
	FILE *fp;

	DBG("%s: block=%d",__FUNCTION__, block);

	if(block >= NUM_ELEMS(PRODUCT_INFO)) {
		DBG("%s: Wrong block number",__FUNCTION__);
		return;
	}

	fp = fopen(PRODUCT_INFO[block],"r");

	if (!fp) {
		DBG("%s: open %s fail [%s]",__FUNCTION__, PRODUCT_INFO[block],strerror(errno));
		return;
	}

	fseek(fp,0,SEEK_END);
	int size=ftell(fp);
	fseek(fp,0,SEEK_SET);
	DBG("%s: size = %d\n", __FUNCTION__,size);

	memset(pdata, 0xff, RAWDATA_BUFFER_SIZE);
	fread(pdata,size,1,fp);
	DBG("%s: data: %s",__FUNCTION__, pdata);

	fclose(fp);
}

static void write_partition(int block, unsigned char *ptr)
{
	unsigned int crc=0, counter;
	unsigned char buf[RAWDATA_COUNTER_SIZE];
	unsigned char crc1,crc2,crc3,crc4;

	DBG("%s: block=%d",__FUNCTION__, block);

	//get counter
	read_mmc_partition(RAW_DATA_PARTITION[RAWDATA_PARTITION0],block*RAWDATA_BUFFER_SIZE+RAWDATA_DATASIZE,buf,RAWDATA_COUNTER_SIZE);

	counter = (unsigned int)(buf[0]<<24)|
	      	  (unsigned int)(buf[1]<<16)|
	      	  (unsigned int)(buf[2]<<8)|
	      	  (unsigned int)buf[3];

	DBG("%s: counter = 0x%x, 0x%x, 0x%x, 0x%x",__FUNCTION__, buf[0],buf[1],buf[2],buf[3]);

	//increase counter
	if(counter > RAWDATA_COUNTER_THRES)
		counter = 0;
	else
		counter ++;

	//set counter
	ptr[RAWDATA_DATASIZE]   = (counter>>24)&0xFF;
	ptr[RAWDATA_DATASIZE+1] = (counter>>16)&0xFF;
	ptr[RAWDATA_DATASIZE+2] = (counter>>8)&0xFF;
	ptr[RAWDATA_DATASIZE+3] =  counter&0xFF;


	//set crc
	crc = crc32b(0xFFFFFFFF,ptr,RAWDATA_CRC_OFFSET);
	crc1=ptr[RAWDATA_CRC_OFFSET]   = (crc>>24)&0xFF;
	crc2=ptr[RAWDATA_CRC_OFFSET+1] = (crc>>16)&0xFF;
	crc3=ptr[RAWDATA_CRC_OFFSET+2] = (crc>>8)&0xFF;
	crc4=ptr[RAWDATA_CRC_OFFSET+3] =  crc&0xFF;

	DBG("%s: counter=0x%x, crc=0x%x, crc1=0x%x,crc2=0x%x,crc3=0x%x,crc4=0x%x",__FUNCTION__, \
		counter, crc,crc1,crc2,crc3,crc4);

	write_mmc_partition(RAW_DATA_PARTITION[RAWDATA_PARTITION0], block * RAWDATA_BUFFER_SIZE,
		ptr, RAWDATA_BUFFER_SIZE);

	write_mmc_partition(RAW_DATA_PARTITION[RAWDATA_PARTITION1], block * RAWDATA_BUFFER_SIZE,
		ptr, RAWDATA_BUFFER_SIZE);

	DBG("%s: write finish",__FUNCTION__);

}

static int rawdata_nofeadback_block(const char *name)
{
	unsigned int i,ret = 0;

	for(i=0; i<NUM_ELEMS(NO_FEADBACK); i++) {
		DBG("%s: name=%s, nofeadback=%s",__FUNCTION__, name, NO_FEADBACK[i]);
		if(strstr(NO_FEADBACK[i],name)!=NULL) {
			ret = 1;
			break;
		}
	}

	DBG("%s: ret=%d",__FUNCTION__, ret);
	return ret;
}

int main(int argc, char **argv)
{
	int fd,wd;
	char buffer[MAX_BUF_SIZE]={0};
	struct inotify_event * event;
	int len, tmp_len,i, index;
	char strbuf[32];

	DBG("Start RAWDATAD");

	init_productinfo();
	while(1)
	{
		fd = inotify_init();

		if (fd < 0) {
			DBG("Fail to initialize inotify.");
			//exit(EXIT_FAILURE);
			continue;
		}

		//to improve, 1 should be computed
		for (i=0; i<NUM_ELEMS(monitored_files); i++) {
			wd_array[i].name = monitored_files[i];
			wd = inotify_add_watch(fd, wd_array[i].name, IN_ALL_EVENTS);
			if (wd < 0) {
				DBG("Can't add watch for %s.", wd_array[i].name);
				while(i)
				{
					inotify_rm_watch(fd, wd_array[--i].name);
				}
				close(fd);
				break;
				//exit(EXIT_FAILURE);
			}
			DBG("Add %s to watch", wd_array[i].name);
			wd_array[i].wd = wd;
		}
		if (wd < 0)
			continue;
		property_set(RAWDATA_PROPERTY, RAWDATA_READY);

		while((len=read(fd, buffer, MAX_BUF_SIZE)) > 0) {

			index = 0;
			DBG("Some event happens, len = %d.", len);

			while (index < len) {
				event = (struct inotify_event *)(buffer+index);

				DBG("Event mask: 0x%08x, name: %s", event->mask, event->name);
				for (i=0; i<EVENT_NUM; i++) {
					if (inotify_event_array[i][0] == '\0')
						continue;
					if (event->mask & (1<<i)) {
						DBG("Event: %s", inotify_event_array[i]);
					}
				}

				memset(strbuf, 0, sizeof(strbuf));

				if (event->mask & IN_ISDIR) {
					strcpy(strbuf, "Direcotory");
				}
				else {
					strcpy(strbuf, "File");
				}

				//IN_CREATE
				if (event->mask & IN_CREATE) {
					DBG("Create FILE %s",event->name);
				}

				//IN_MODIFY
				if (event->mask & IN_MODIFY) {
					for(i=0; i<NUM_ELEMS(PRODUCT_INFO); i++) {
						DBG("event->name=%s, %s", event->name,PRODUCT_INFO[i]);
						if(strstr(PRODUCT_INFO[i],event->name)!=NULL) {
							read_productinfo(i, rawdata_buffer[0]);
							if(rawdata_nofeadback_block(event->name)==0)
								write_partition(i, rawdata_buffer[0]);
						}
					}

				}

				//IN_DELETE IN_MOVE
				if (event->mask & IN_DELETE || event->mask & IN_MOVE) {
					//DBG("File %s was deleted or moved",wd_array[i].name);
					for(i=0; i<NUM_ELEMS(PRODUCT_INFO); i++) {
						DBG("event->name=%s, %s", event->name,PRODUCT_INFO[i]);
						if(strstr(PRODUCT_INFO[i],event->name)!=NULL) {
							memset(rawdata_buffer[0], 0xff, RAWDATA_BUFFER_SIZE);
							if(rawdata_nofeadback_block(event->name)==0)
								write_partition(i, rawdata_buffer[0]);
						}
					}
				}


				DBG("Object type: %s", strbuf);
				for (i=0; i<NUM_ELEMS(PRODUCT_INFO); i++) {
					if (event->wd != wd_array[i].wd)
						continue;
					DBG("Object name: %s", wd_array[i].name);
					break;
				}

				DBG("index=%d; event->len=%d",index, event->len);
				index += sizeof(struct inotify_event) + event->len;
			}

			memset(buffer, 0, sizeof(buffer));
			DBG("#############################################");

		}
		DBG("**************zhaiyfSome event happens, len = %d.", len);
		for (i=0; i<NUM_ELEMS(monitored_files); i++) {
			inotify_rm_watch(fd, wd_array[i].name);
		}
		close(fd);

	}
	return 0;
}

