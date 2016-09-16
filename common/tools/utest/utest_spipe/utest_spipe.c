/*
 * Copyright (C) 2012 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>

#define ERR(x...) fprintf(stderr, x)
#define INFO(x...) fprintf(stdout, x)

#define MS_IN_SEC 1000
#define NS_IN_MS  1000000

#define DEFAULT_DATA_SIZE (100 * 1024 * 1024)
#define BYTES_LIMIT (1024 * 1024)
#define BYTES_DEFAULT 1000

typedef enum{
    CMD_VERIFY_ID = 0,
    CMD_THROUGHPUT_ID = 1,
    CMD_LATENCY_ID = 2,
    CMD_MAX,
}cmd_id;

typedef struct{
    cmd_id ID;
    char* name;
    char* optcfg;
    int (*func)(void*);
}cmd_tag;

typedef struct{
	struct timespec tm_begin;
	struct timespec tm_end;
}timer_tag;

typedef struct{
	cmd_id ID;
	int bytes;
	int count;
	int step;
	int fd;
	int thread_status;
	pthread_t rd_thread;
	pthread_t wr_thread;
        char * dev_path;
	char * txbuf;
	char * rxbuf;
	timer_tag * tm;
}thread_argv;

/*32 bytes*/
static char pattern[] = {
            0x01, 0x10, 0x11,
            0x23, 0x32, 0x33,
            0x45, 0x54, 0x55,
            0x67, 0x76, 0x77,
            0x89, 0x98, 0x99,
            0xab, 0xba, 0xbb,
            0xcd, 0xdc, 0xdd,
            0xef, 0xfe, 0xff,
            0xa1, 0xb2, 0xc3,
            0xd4, 0xe5, 0xf6,
            0xaa, 0xee,
};

static int verify(void *argv);
static int multithreads_run(void *argv);
static int singlethread_run(void *argv);

cmd_tag cmd_container[CMD_MAX] = {
                {CMD_VERIFY_ID, "verify", ":md:b:t:s:", verify},
                {CMD_THROUGHPUT_ID, "throughput", ":md:b:t:s:", singlethread_run},
                {CMD_LATENCY_ID, "latency", ":md:b:t:s:", singlethread_run},
};

/*
 * usage --  print the usage of utest_spipe
 * @return: none
 */
static void usage(void)
{
    INFO("Usage:\n");
    INFO("\tutest_spipe verify | [options] | [-d dev] | [-b unit_size] | [-t total_size] | [-s step]\n");
    INFO("\tutest_spipe throughput | [options] | [-d dev] | [-b unit_size] | [-t total_size] | [-s step]\n");
    INFO("\tutest_spipe latency | [options] | [-d dev] | [-b unit_size] | [-t total_size] | [-s step]\n");
    INFO("OPTIONS\n");
    INFO("\t-m\n");
    INFO("\t\tThroughput or latency test will run in multi-threads.\n");
}

/*
 * clean_spipe --  clean the pipe to ensure no residual data existing
 * @dev_path: path of device node
 * @return: error value, 0 means successful
 */
static int clean_spipe(char* dev_path)
{
    int fd;
    int ret_val;
    char buf[128];
    if(-1 == (fd = open(dev_path, O_RDWR | O_NONBLOCK))){
        return -EINVAL;
    }

//    ret_val = write(fd, "1", 1);

    do{
        ret_val = read(fd, buf, 128);

    }while((0 < ret_val) || (ret_val == -1 && errno == EINTR));
    if(-1 == ret_val){
        close(fd);
        return -EINVAL;
    }

    close(fd);
    return 0;
}

/*
* delta_miliseconds --  calculate the delta value of time
 * @begin: begin time, first input variable
 * @end:   end time, second input variable
 * @return: miliseconds
 */
static unsigned int delta_miliseconds(struct timespec *begin, struct timespec *end)
{
    int carrier;
    long ns;
    unsigned int ms;
    time_t sec;

    if(NULL == begin || NULL == end){
        return 0;
    }

    ns = end->tv_nsec - begin->tv_nsec;

    sec = end->tv_sec - begin->tv_sec;

    return sec * MS_IN_SEC + ns / NS_IN_MS;
}

/*
 * buf_init --  initialize rxbuf & txbuf, and clean spipe
 * @rxbuf: pointer to receive buff
 * @rxbuf_len: length of receive buff
 * @txbuf: pointer to transmit buff
 * @rxbuf: length of transmit buff
 * @return: error value, 0 means successful
 */
static int buf_init(char **rxbuf, unsigned int rxbuf_len, char **txbuf, unsigned int txbuf_len)
{
    if(rxbuf_len <= 0 || txbuf_len <= 0){
        ERR("buf_init: invalid input parameters\n");
        return -EINVAL;
    }

    /*initialize txbuf*/
    *txbuf = malloc(txbuf_len);
    if(NULL == *txbuf){
        ERR("buf_init: allocate memory failed");
        return -ENOMEM;
    }
    memset(*txbuf, 0, txbuf_len);

    /*initialize rxbuf*/
    *rxbuf = malloc(rxbuf_len);
    if(NULL == *rxbuf){
        free(*txbuf);
        ERR("buf_init: allocate memory failed");
        return -ENOMEM;
    }
    memset(*rxbuf, 0, rxbuf_len);

    return 0;
}


/*
 * verify -- verify the data through loopback test, and print the result
 * @argv: the pointer to input parameter
 * @return: error value, 0 means successful
 */
static int verify(void* argv)
{
    struct timespec tm_begin, tm_end;
    char *txbuf, *rxbuf, *temp_ptr, *dev_path;
    int i, j, s, fd, rest, num, ret_val;
    int bytes, count, step;
    unsigned int mask, ms;
    thread_argv *data = argv;

    bytes = data->bytes;
    count = data->count;
    step = data->step;
    dev_path = data->dev_path;

    if(bytes < 0 || count < 0){
        ERR("verify: invalid input parameters\n");
        return -EINVAL;
    }

    mask = sizeof(pattern) - 1;

    /*initialize txbuf & rxbuf*/
    if(0 != (ret_val = buf_init(&rxbuf, bytes + 1, &txbuf, bytes + 1))){
        return ret_val;
    }

    /*clean spipe0*/
    clean_spipe(dev_path);

    /*open spipe0*/
    fd = open(dev_path, O_RDWR);
    if(-1 == fd){
        free(txbuf);
        free(rxbuf);
        ERR("spipe_verify: open spipe failed\n");
        return -EINVAL;
    }
    for(s = 0; s < step; s++){
        for(i = 0; i < count; i++){

            for(j = 0; j < bytes; j++){
                txbuf[j] = pattern[(i * bytes + j) & mask];
            }

            /*throuput to spipe0*/
            rest = bytes;
            temp_ptr = txbuf;
            do{
                num = write(fd, temp_ptr, rest);
                if(num != -1){
                    rest -= num;
                    temp_ptr += num;
                }
            }while((num != -1 && rest > 0) || (num == -1 && errno == EINTR));
            if(num == -1){
                goto FAIL;
            }

            /*loopback from spipe0*/
            rest = bytes;
            temp_ptr = rxbuf;
            do{
                num = read(fd, temp_ptr, rest);
                if(num != -1){
                    rest -= num;
                    temp_ptr += num;
                }
            }while((num != -1 && rest > 0) || (num == -1 && errno == EINTR));
            if(num == -1){
                goto FAIL;
            }

            if(0 == strcmp(rxbuf, txbuf)){
                continue;
            }else{
                INFO("Round %d: wrong data!\n", i);
                INFO("rxbuf : %s\n", rxbuf);
                INFO("txbuf : %s\n", txbuf);
                goto FAIL;
            }
        }

        INFO("Verify step %d finished: Correct data!\n", s + 1);
        INFO("\tTotal data size %.3f MB, count %d, unit size %d Bytes.\n", ((float)(count*bytes)/1024/1024), count, bytes);
    }

    ret_val = 0;
    goto SUCCESS;

FAIL :
    /*ret_val = -EINVAL;*/
SUCCESS :
    free(txbuf);
    free(rxbuf);
    close(fd);
    return ret_val;
}

/*
 * singlethread_run -- running throughput or latency test in a single thread, then prints the result
 * @argv: pointer to input paramerter
 * return: error value, 0 means successful
 */
static int singlethread_run(void *argv)
{
    struct timespec tm_begin, tm_end;
    char *txbuf, *rxbuf, *temp_ptr, *dev_path;
    int i, j, s, fd, rest, num, ret_val;
    int bytes, count, step;
    unsigned int mask, ms;
    thread_argv *data = argv;

    bytes = data->bytes;
    count = data->count;
    step = data->step;
    dev_path = data->dev_path;

    if(bytes < 0 || count < 0){
        ERR("throughput: invalid input parameters\n");
        return -EINVAL;
    }

    mask = sizeof(pattern) - 1;

    /*initialize txbuf & rxbuf*/
    if(0 != (ret_val = buf_init(&rxbuf, bytes + 1, &txbuf, bytes + 1))){
        return ret_val;
    }
    mask = sizeof(pattern) - 1;
    for(j = 0; j < bytes; j++){
        txbuf[j] = pattern[j & mask];
    }

    /*clean spipe0*/
    clean_spipe(dev_path);
    /*open spipe0*/
    fd = open(dev_path, O_RDWR);
    if(-1 == fd){
        free(txbuf);
        free(rxbuf);
        ERR("throughput: open spipe failed\n");
        return -EINVAL;
    }

    for(s = 0; s < step; s++){
        /*timer start*/
        if(-1 == clock_gettime(CLOCK_MONOTONIC, &tm_begin)){
            goto FAIL;
        }

        for(i = 0; i < count; i++){
            /*throuput to spipe0*/
            rest = bytes;
            temp_ptr = txbuf;
            do{
                num = write(fd, temp_ptr, rest);
                if(num != -1){
                    rest -= num;
                    temp_ptr += num;
                }
            }while((num != -1 && rest > 0) || (num == -1 && errno == EINTR));
            if(num == -1){
                goto FAIL;
            }
            /*loopback from spipe0*/
            rest = bytes;
            temp_ptr = rxbuf;
            do{
                num = read(fd, temp_ptr, rest);
                    if(num != -1){
                        rest -= num;
                        temp_ptr += num;
                    }
            }while((num != -1 && rest > 0) || (num == -1 && errno == EINTR));
            if(num == -1){
                goto FAIL;
            }
        }

        /*timer end*/
        if(-1 == clock_gettime(CLOCK_MONOTONIC, &tm_end)){
            goto FAIL;
        }

        /*calculate interval*/
        ms = delta_miliseconds(&tm_begin, &tm_end);

        /*print result*/
        if (data->ID == CMD_THROUGHPUT_ID){
            INFO("Throughput step %d finished.\n", s+1);
            INFO("\tTotal data size %.3f MB, total time cost %u ms, unit size %d Bytes, speed %.3f MB/s.\n",
                ((float)(data->count*data->bytes)/1024/1024), ms, data->bytes,
                (float)(data->count*data->bytes)*MS_IN_SEC/1024/1024/ms);
        }
        else if(data->ID == CMD_LATENCY_ID){
            INFO("Latency step %d finished.\n", s+1);
            INFO("\tTotal count %d, total time cost %u ms, unit size %d Bytes, average time cost %.3f ms per unit.\n",
                data->count, ms, data->bytes, (float)ms/data->count);
        }
    }

    ret_val = 0;
    goto SUCCESS;

FAIL :
    ret_val = -EINVAL;
SUCCESS :
    free(txbuf);
    free(rxbuf);
    close(fd);
    return ret_val;
}


/*
 * read_thread_entry -- the entry of a thread, that reads data from specified dev node
 * @argv: pointer to input paramerter
 * return: error value, 0 means successful
 */
void *read_thread_entry(void *argv)
{
    int i, j, s;
    int rdn, left, offset, rval;
    unsigned int ms;
    thread_argv *data = (thread_argv *)argv;

    for (s = 0; s < data->step; s++){
        for (i = 0; i < data->count && data->thread_status; i++){
            left = data->bytes;
            offset = 0;
            do{
                rdn = read(data->fd, data->rxbuf + offset, left);
                if(rdn > 0){
                    left -= rdn;
                    offset += rdn;
                }
            }while((left > 0 && rdn != -1) || (rdn == -1 && errno == EINTR));
            if(rdn == -1){
                goto FAIL;
            }
        }

        /*timer end*/
        if(-1 == clock_gettime(CLOCK_MONOTONIC, &data->tm[s].tm_end)){
            goto FAIL;
        }
        /*calculate interval*/
        ms = delta_miliseconds(&data->tm[s].tm_begin, &data->tm[s].tm_end);

        /*print result*/
        if (data->ID == CMD_THROUGHPUT_ID){
            INFO("Throughput step %d finished.\n", s+1);
            INFO("\tTotal data size %.3f MB, total time cost %u ms, unit size %d Bytes, speed %.3f MB/s.\n",
                ((float)(data->count*data->bytes)/1024/1024), ms, data->bytes,
                (float)(data->count*data->bytes)*MS_IN_SEC/1024/1024/ms);
        }
        else if(data->ID == CMD_LATENCY_ID){
            INFO("Latency step %d finished.\n", s+1);
            INFO("\tTotal count %d, total time cost %u ms, unit size %d Bytes, average time cost %.3f ms per unit.\n",
                data->count, ms, data->bytes, (float)ms/data->count);
        }
    }
    if(data->thread_status == 1){
        rval = 0;
        goto SUCCESS;
    }
FAIL:
    rval = -EINVAL;
    data->thread_status = 0;
SUCCESS:
    return ((void*)(rval));
}

/*
 * write_thread_entry -- the entry of a thread, that writes data to specified dev node
 * @argv: pointer to input paramerter
 * return: error value, 0 means successful
 */
void *write_thread_entry(void * argv)
{
    int i, j, s;
    int wrn, left, offset, rval;
    unsigned int ms;
    thread_argv *data = (thread_argv*)argv;

    for (s = 0; s < data->step; s++){
        /*timer end*/
        if(-1 == clock_gettime(CLOCK_MONOTONIC, &data->tm[s].tm_begin)){
            goto FAIL;
        }
        for (i = 0; i < data->count && data->thread_status; i++){
            left = data->bytes;
            offset = 0;

            do{
                wrn = write(data->fd, data->txbuf + offset, left);
                if(wrn > 0){
                left -= wrn;
                offset += wrn;
                }
            }while((left > 0 && wrn != -1) || (wrn == -1 && errno == EINTR));
            if(-1 == wrn){
                goto FAIL;
            }
        }
    }
    if(data->thread_status == 1 ){
        rval = 0;
        goto SUCCESS;
    }
FAIL:
    rval = -EINVAL;
    data->thread_status = 0;
SUCCESS:
    return ((void*)(rval));
}


/*
 * multithreads_run -- running throughput or latency test in multi-threads, then print the result
 * @argv: the pointer to input parameter
 * @return: error value, 0 means successful
 */
static int multithreads_run(void *argv)
{
    //struct timespec tm_begin, tm_end;
    char *txbuf, *rxbuf, *temp_ptr;
    int i, j, s, fd, rest, num, ret_val;
    int rdval, wrval;
    int step, count, bytes;
    char *dev_path;
    timer_tag *tm;
    unsigned int mask, ms;
    thread_argv *data = argv;

    step = data->step;
    count = data->count;
    bytes = data->bytes;
    dev_path = data->dev_path;

    if(bytes < 0 || count < 0){
        ERR("throughput: invalid input parameters\n");
        return -EINVAL;
    }

    mask = sizeof(pattern) - 1;

    /*initialize txbuf & rxbuf*/
    if(0 != (ret_val = buf_init(&rxbuf, bytes + 1, &txbuf, bytes + 1))){
        return ret_val;
    }
    mask = sizeof(pattern) - 1;
    for(j = 0; j < bytes; j++){
        txbuf[j] = pattern[j & mask];
    }

    /*clean spipe0*/
    clean_spipe(dev_path);
    /*open spipe0*/
    fd = open(dev_path, O_RDWR);
    if(-1 == fd){
        free(txbuf);
        free(rxbuf);
        ERR("throughput: open spipe failed\n");
        return -EINVAL;
    }

    tm = calloc(sizeof(timer_tag), step);
    if(tm == NULL){
        free(txbuf);
        free(rxbuf);
        close(fd);
        ERR("throughput: calloc mem failed\n");
        return -ENOMEM;
    }

    data->tm = tm;
    data->fd = fd;
    data->txbuf = txbuf;
    data->rxbuf = rxbuf;
    data->thread_status = 1;

    if(pthread_create(&data->rd_thread, NULL, read_thread_entry, (void*)(data))){
        free(txbuf);
        free(rxbuf);
        free(tm);
        close(fd);
	data->thread_status = 0;
        ERR("pthread_create rd_thread fail\n");
        return -EINVAL;
    }

    if(pthread_create(&data->wr_thread, NULL, write_thread_entry, (void*)(data))){
        data->thread_status = 0;
	pthread_join(data->rd_thread, NULL);
	free(txbuf);
        free(rxbuf);
        free(tm);
        close(fd);
        ERR("pthread_create wr_thread fail\n");
        return -EINVAL;
    }

    pthread_join(data->rd_thread, (void*)(&rdval));
    pthread_join(data->wr_thread, (void*)(&wrval));
    if(rdval != 0 || wrval != 0){
        ret_val = -EINVAL;
    } else {
        ret_val = 0;
    }

    free(txbuf);
    free(rxbuf);
    free(tm);
    close(fd);
    return ret_val;
}

/*
 * do_parse_cmd --  parse command line
 * @id: command id, first input variable
 * @argc: string number, second input variable
 * @argv: string array, third input variable
 * @return: error value, 0 means successful
 */
static int do_parse_cmd(cmd_id id, int argc, char ** argv)
{
    int opt;
    int bytes = BYTES_DEFAULT;
    int data_size = DEFAULT_DATA_SIZE;
    int count;
    int step = 1;
    int is_multithreads = 0;
    char *dev_path = "/dev/spipe_td0";
    char *optcfg = cmd_container[id].optcfg;
    thread_argv data;

    while ((opt = getopt(argc, argv, optcfg)) != -1){
        switch(opt){
            case 'm':
                is_multithreads = 1;
                break;
            case 'd':
                dev_path = optarg;
                break;
            case 'b':
                bytes = atoi(optarg);
                break;
            case 't':
                data_size = atoi(optarg);
                break;
            case 's':
                step = atoi(optarg);
                break;
            default:
                return -EINVAL;
        }
    }

    if(optind < argc){
        return -EINVAL;
    }

    if(0 >= bytes || BYTES_LIMIT <= bytes){
        bytes = BYTES_DEFAULT;
    }
    count = data_size / bytes;

    if(step <= 0){
        step = 1;
    }

    if(is_multithreads && id != CMD_VERIFY_ID){
        cmd_container[id].func = multithreads_run;
        INFO("Running in multi-threads.\n");
    }else{
        INFO("Running in a single thread.\n");
    }

    data.step = step;
    data.count = count;
    data.bytes = bytes;
    data.dev_path = dev_path;
    data.ID = id;

    return cmd_container[id].func((void*)(&data));
}

int main(int argc, char ** argv)
{
    char *cmd;
    int ret = -EINVAL;
    int i;

    if(argc < 2){
        usage();
        return ret;
    }

    cmd = argv[1];
    argc--;
    argv++;

    for(i = CMD_VERIFY_ID; i < CMD_MAX; i++){
        if(0 == strcmp(cmd_container[i].name, cmd)){
            ret = do_parse_cmd(i, argc, argv);
            break;
        }
    }

    if(0 != ret){
        usage();
    }

    return 0;
}
