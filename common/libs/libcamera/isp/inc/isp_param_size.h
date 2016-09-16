#ifndef _ISP_PARAM_SIZE_H_
#define _ISP_PARAM_SIZE_H_
/*----------------------------------------------------------------------------*
 **				Dependencies					*
 **---------------------------------------------------------------------------*/
#include <sys/types.h>
/**---------------------------------------------------------------------------*
 **				Compiler Flag					*
 **---------------------------------------------------------------------------*/
#ifdef   __cplusplus
extern   "C"
{
#endif

/**---------------------------------------------------------------------------*
**				Micro Define					*
**----------------------------------------------------------------------------*/
#define ISP_SIZE_480x270   (1<<0)
#define ISP_SIZE_640x480   (1<<1)
#define ISP_SIZE_800x600   (1<<2)
#define ISP_SIZE_1280x960  (1<<3)
#define ISP_SIZE_1296x972  (1<<4)
#define ISP_SIZE_1280x1024 (1<<5)
#define ISP_SIZE_1600x1200 (1<<6)
#define ISP_SIZE_1632x1224 (1<<7)
#define ISP_SIZE_1920x1080 (1<<8)
#define ISP_SIZE_2048x1536 (1<<9)
#define ISP_SIZE_2592x1944 (1<<10)
#define ISP_SIZE_3264x2448 (1<<11)
#define ISP_SIZE_END 0xffffffff


struct isp_size_info{
	uint32_t size_id;
	uint32_t width;
	uint32_t height;
};

struct isp_size_info* ISP_ParamGetSizeInfo(void);

/**----------------------------------------------------------------------------*
**				Compiler Flag					*
**----------------------------------------------------------------------------*/
#ifdef   __cplusplus
}
#endif
/**---------------------------------------------------------------------------*/
#endif
// End

