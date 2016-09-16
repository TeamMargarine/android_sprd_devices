#ifndef _Linux_KFIFO_H
#define _Linux_KFIFO_H

#define min(x,y) ({ \
typeof(x) _x = (x); \
typeof(y) _y = (y); \
(void) (&_x == &_y);   \
_x < _y ? _x : _y; })


struct kfifo {
    unsigned char *buffer;    /* the buffer holding the data */
    unsigned int size;    /* the size of the allocated buffer */
    unsigned int in;    /* data is added at offset (in % size) */
    unsigned int out;    /* data is extracted from off. (out % size) */
};

struct kfifo *kfifo_init(unsigned char *buffer, unsigned int size);
struct kfifo *kfifo_alloc(unsigned int size);
void kfifo_free(struct kfifo *fifo);
unsigned int __kfifo_put(struct kfifo *fifo, unsigned char *buffer, unsigned int len);
unsigned int __kfifo_get(struct kfifo *fifo, unsigned char *buffer, unsigned int len);

static inline void __kfifo_reset(struct kfifo *fifo)
{
    fifo->in = fifo->out = 0;
}

static inline void kfifo_reset(struct kfifo *fifo)
{

    __kfifo_reset(fifo);

}

static inline unsigned int kfifo_put(struct kfifo *fifo,
                     unsigned char *buffer, unsigned int len)
{
    unsigned int ret;

    ret = __kfifo_put(fifo, buffer, len);

    return ret;
}

static inline unsigned int kfifo_get(struct kfifo *fifo,
                     unsigned char *buffer, unsigned int len)
{
    unsigned int ret;

    ret = __kfifo_get(fifo, buffer, len);

    if (fifo->in == fifo->out)
        fifo->in = fifo->out = 0;


    return ret;
}

static inline unsigned int __kfifo_len(struct kfifo *fifo)
{
    return fifo->in - fifo->out;
}

static inline unsigned int kfifo_len(struct kfifo *fifo)
{
    unsigned int ret;

    ret = __kfifo_len(fifo);

    return ret;
}

#endif
