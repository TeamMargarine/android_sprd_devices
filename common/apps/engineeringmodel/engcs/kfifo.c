#include "kfifo.h"
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

struct kfifo *kfifo_init(unsigned char *buffer, unsigned int size)
{
    struct kfifo *fifo;

    fifo = malloc(sizeof(struct kfifo));
    if (!fifo)
        return (void*)(-ENOMEM);

    fifo->buffer = buffer;
    fifo->size = size;
    fifo->in = fifo->out = 0;

    return fifo;
}

struct kfifo *kfifo_alloc(unsigned int size)
{
    unsigned char *buffer;
    struct kfifo *ret;

    buffer = malloc(size);
    if (!buffer)
        return (void *)(-ENOMEM);

    ret = kfifo_init(buffer, size);

    if ((unsigned long)ret<=0)
    {
        free(buffer);
    }

    return ret;
}

void kfifo_free(struct kfifo *fifo)
{
    free(fifo->buffer);
    free(fifo);
}

unsigned int __kfifo_put(struct kfifo *fifo, unsigned char *buffer, unsigned int len)
{
  unsigned int l; 
  len = min(len, fifo->size - fifo->in + fifo->out); 
  /* first put the data starting from fifo->in to buffer end */ 
  l = min(len, fifo->size - (fifo->in & (fifo->size - 1))); 
  memcpy(fifo->buffer + (fifo->in & (fifo->size - 1)), buffer, l); 
  /* then put the rest (if any) at the beginning of the buffer */ 
  memcpy(fifo->buffer, buffer + l, len - l); 
  fifo->in += len; 
  return len; 
}

unsigned int __kfifo_get(struct kfifo *fifo,
             unsigned char *buffer, unsigned int len)
{
  unsigned int l; 
  len = min(len, fifo->in - fifo->out); 
  /* first get the data from fifo->out until the end of the buffer */ 
  l = min(len, fifo->size - (fifo->out & (fifo->size - 1))); 
  memcpy(buffer, fifo->buffer + (fifo->out & (fifo->size - 1)), l); 
  /* then get the rest (if any) from the beginning of the buffer */ 
  memcpy(buffer + l, fifo->buffer, len - l); 
  fifo->out += len; 
  return len; 
}

