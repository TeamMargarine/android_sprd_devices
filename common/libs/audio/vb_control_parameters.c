#include <utils/Log.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <sys/wait.h>
#include <sys/mman.h>
#include <tinyalsa/asoundlib.h>
#include "aud_enha.h"

//#ifdef __cplusplus
//extern "c"
//{
//#endif


#define MY_DEBUG

#ifdef MY_DEBUG
#define MY_TRACE    ALOGW
#else
#define MY_TRACE(...)
#endif

#define VBC_CMD_TAG   "VBC"
/*
#define READ_PARAS(type, exp)    if (s_vbpipe_fd > 0 && paras_ptr != NULL) { \
        exp = read(s_vbpipe_fd, paras_ptr, sizeof(type)); \
        }
*/

#define ENG_AUDIO_PGA       "/sys/class/vbc_param_config/vbc_pga_store"

/* vbc control parameters struct here.*/
typedef struct Paras_Mode_Gain
{
    unsigned short  is_mode;
    unsigned short  is_volume;

    unsigned short  mode_index;
    unsigned short  volume_index;

    unsigned short  dac_set;
    unsigned short  adc_set;

    unsigned short  dac_gain;
    unsigned short  adc_gain;

    unsigned short  path_set;
    unsigned short  pa_setting;

    unsigned short  reserved[16];
}paras_mode_gain_t;

typedef struct Switch_ctrl
{
    unsigned int  is_switch; /* switch vbc contrl to dsp.*/
}switch_ctrl_t;

typedef struct Samplerate_ctrl
{
    unsigned short samplerate; /* change samplerate.*/
}set_samplerate_t;

typedef struct Set_Mute
{
    unsigned int  is_mute;
}set_mute_t;

typedef struct Device_ctrl
{
    unsigned short  	is_open; /* if is_open is true, open device; else close device.*/
    unsigned short  	is_headphone;
    unsigned int 	is_downlink_mute;
    unsigned int 	is_uplink_mute;
    paras_mode_gain_t 	paras_mode;
}device_ctrl_t;

typedef struct Open_hal
{
    unsigned int  sim_card;   /*sim card number*/
}open_hal_t;


typedef struct {
    unsigned short adc_pga_gain_l;
    unsigned short adc_pga_gain_r;
    uint32_t fm_pga_gain_l;
    uint32_t fm_pga_gain_r;
    uint32_t dac_pga_gain_l;
    uint32_t dac_pga_gain_r;
    uint32_t devices;
    uint32_t mode;
}pga_gain_nv_t;

/* list vbc cmds */
enum VBC_CMD_E
{
    VBC_CMD_NONE = 0,
/* current mode and volume gain parameters.*/
    VBC_CMD_SET_MODE = 1,
    VBC_CMD_RSP_MODE = 2,
    VBC_CMD_SET_GAIN = 3,
    VBC_CMD_RSP_GAIN = 4,
/* whether switch vb control to dsp parameters.*/
    VBC_CMD_SWITCH_CTRL = 5,
    VBC_CMD_RSP_SWITCH = 6,
/* whether mute or not.*/
    VBC_CMD_SET_MUTE = 7,
    VBC_CMD_RSP_MUTE = 8,
/* open/close device parameters.*/
    VBC_CMD_DEVICE_CTRL = 9,
    VBC_CMD_RSP_DEVICE = 10,

    VBC_CMD_HAL_OPEN = 11,
    VBC_CMD_RSP_OPEN  =12,

    VBC_CMD_HAL_CLOSE = 13,
    VBC_CMD_RSP_CLOSE = 14,

    VBC_CMD_SET_SAMPLERATE = 15,
    VBC_CMD_RSP_SAMPLERATE = 16,

    VBC_CMD_MAX
};
#ifdef AUDIO_SPIPE_TD
#define VBC_ARM_CHANNELID 2
#else
#define VBC_ARM_CHANNELID 1
#endif

typedef struct
{
    char        tag[4];   /* "VBC" */
    unsigned int    cmd_type;
    unsigned int    paras_size; /* the size of Parameters Data, unit: bytes*/
}parameters_head_t;

static unsigned short s_vbc_pipe_count = 0;
enum VBC_CALL_FLOW
{
    VBC_CALL_CLOSE_DEVICE = 0,
    VBC_CALL_OPEN_DEVICE  = 1,
    VBC_CALL_SET_MODE     = 2,
    VBC_CALL_SET_GAIN     = 3,
    VBC_CALL_SET_MUTE     = 4,
    VBC_CALL_DEVICE_CTRL  = 5,
    VBC_CALL_SWITCH_CTRL  = 6,
    VBC_CALL_END          = 7
};

static char *call_control_flag[VBC_CALL_END] = {
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
};

typedef struct
{
	char vbpipe[VBC_PIPE_NAME_MAX_LEN];//vbpipe5/vbpipe6
	int vbchannel_id;
	cp_type_t cp_type;
	int vbpipe_fd;
	int vbpipe_pre_fd;
	int is_exit;
	struct tiny_audio_device *adev;
}vbc_ctrl_thread_para_t;

static pthread_t *s_vbc_ctrl_thread_id =NULL;
static vbc_ctrl_thread_para_t *st_vbc_ctrl_thread_para = NULL;

static uint32_t android_cur_device = 0x0;   // devices value the same as audiosystem.h
static int s_is_active = 0;
static int android_sim_num = 0;
static vbc_ctrl_pipe_para_t s_default_vbc_ctrl_pipe_info =
{
	"/dev/vbpipe6",0,CP_TG 
};

/* Transfer packet by vbpipe, packet format as follows.*/
/************************************************
----------------------------
|Paras Head |  Paras Data  |
----------------------------
************************************************/

/*
 * local functions declaration.
 */
static int  vbc_call_set_flag(unsigned int flag);

static int  vbc_call_get_flag(void);

static int  vbc_call_open_device(struct tiny_audio_device *adev);

static int  vbc_call_init_call_process(struct tiny_audio_device *adev) ;

static int  ReadParas_Head(int fd_pipe,  parameters_head_t *head_ptr);

static int  WriteParas_Head(int fd_pipe,  parameters_head_t *head_ptr);

static int  ReadParas_DeviceCtrl(int fd_pipe, device_ctrl_t *paras_ptr);

static int  ReadParas_ModeGain(int fd_pipe,  paras_mode_gain_t *paras_ptr);

static int  ReadParas_SwitchCtrl(int fd_pipe,  switch_ctrl_t *paras_ptr);

static int  ReadParas_Mute(int fd_pipe,  set_mute_t *paras_ptr);

void *vbc_ctrl_thread_routine(void *args);

/*
 * local functions definition.
 */
static int vbc_call_set_flag(unsigned int flag)
{
    property_set("alsa.audio.vbc.call.flag", call_control_flag[flag]);
#if 0
    char propBuf[10] = {0};

    property_get("alsa.audio.vbc.call.flag", propBuf, "0");
#endif

    return 0;
}

static int vbc_call_get_flag(void)
{
    char propBuf[10] = {0};

    property_get("alsa.audio.vbc.call.flag", propBuf, "0");

    ALOGW("vbc_call_get_flag call_control_flow = %s\n", propBuf);

    return atoi(propBuf);
}

static int vbc_call_init_call_process(struct tiny_audio_device *adev)
{
    char propBuf[15] = {0};

    if (property_get("alsa.audio.vbc.modem.reset", propBuf, "0") &&
        (0 == strcmp(propBuf, "modemreset")))
    {
        ALOGW("modem crash");
        vbc_call_set_flag(VBC_CALL_CLOSE_DEVICE);
        property_set("alsa.audio.vbc.modem.reset", "0");

        return 0;
    }

    switch (vbc_call_get_flag())
    {
        case VBC_CALL_SWITCH_CTRL:
            ALOGW("vbc_call_init_call_process set vbc control to dsp\n");
            mixer_ctl_set_value(adev->private_ctl.vbc_switch, 0, 0);
            //break;
            //comment break
        case VBC_CALL_OPEN_DEVICE:
            vbc_call_open_device(adev);
            break;
        default:
            ALOGW("vbc_call_init_call_process default");
            break;
    }

    return 0;
}

static int vbc_call_open_device(struct tiny_audio_device *adev)
{
    int ret = 0;

    adev->pcm_modem_dl= pcm_open(s_tinycard, PORT_MODEM, PCM_OUT | PCM_MMAP, &pcm_config_vx);
    if (!pcm_is_ready(adev->pcm_modem_dl)) {
        ALOGE("cannot open pcm_modem_dl : %s", pcm_get_error(adev->pcm_modem_dl));
        pcm_close(adev->pcm_modem_dl);
        ret = -1;
    }
    adev->pcm_modem_ul= pcm_open(s_tinycard, PORT_MODEM, PCM_IN, &pcm_config_vrec_vx);
    if (!pcm_is_ready(adev->pcm_modem_ul)) {
        ALOGE("cannot open pcm_modem_ul : %s", pcm_get_error(adev->pcm_modem_ul));
        pcm_close(adev->pcm_modem_ul);
        pcm_close(adev->pcm_modem_dl);
        ret = -1;
    }

    mixer_ctl_set_value(adev->private_ctl.internal_pa, 0, inter_pa_call);
    adev->call_start = 1;
    ALOGW("START CALL,open pcm device..call_start = %d.\n", adev->call_start);

    return ret;
}

static int  ReadParas_Head(int fd_pipe,  parameters_head_t *head_ptr)
{
    int ret = 0;
    if (fd_pipe > 0 && head_ptr != NULL) {
        ret = read(fd_pipe, head_ptr, sizeof(parameters_head_t));
    }
    return ret;
}

static int  WriteParas_Head(int fd_pipe,  parameters_head_t *head_ptr)
{
    int ret = 0;
    if (fd_pipe > 0 && head_ptr != NULL) {
        ret = write(fd_pipe, head_ptr, sizeof(parameters_head_t));
    }
    return ret;
}

static int  ReadParas_OpenHal(int fd_pipe, open_hal_t *hal_open_param)
{
    int ret = 0;
    if (fd_pipe > 0 && hal_open_param != NULL) {
        ret = read(fd_pipe, hal_open_param, sizeof(open_hal_t));
    }
    return ret;
}


static int  ReadParas_DeviceCtrl(int fd_pipe, device_ctrl_t *paras_ptr)
{
    int ret = 0;
    if (fd_pipe > 0 && paras_ptr != NULL) {
        ret = read(fd_pipe, paras_ptr, sizeof(device_ctrl_t));
    }
    return ret;
}

static int  ReadParas_ModeGain(int fd_pipe,  paras_mode_gain_t *paras_ptr)
{
    int ret = 0;
    if (fd_pipe > 0 && paras_ptr != NULL) {
        ret = read(fd_pipe, paras_ptr, sizeof(paras_mode_gain_t));
    }
    return ret;
}

static int  ReadParas_SwitchCtrl(int fd_pipe,  switch_ctrl_t *paras_ptr)
{
    int ret = 0;
    if (fd_pipe > 0 && paras_ptr != NULL) {
        ret = read(fd_pipe, paras_ptr, sizeof(switch_ctrl_t));
    }
    return ret;
}

static int  ReadParas_SetSamplerate(int fd_pipe,  set_samplerate_t *paras_ptr)
{
    int ret = 0;
    if (fd_pipe > 0 && paras_ptr != NULL) {
        ret = read(fd_pipe, paras_ptr, sizeof(set_samplerate_t));
        if(ret != sizeof(set_samplerate_t))
            ret = -1;
    }
    return ret;
}

static int  ReadParas_Mute(int fd_pipe,  set_mute_t *paras_ptr)
{
    int ret = 0;
    if (fd_pipe > 0 && paras_ptr != NULL) {
        ret = read(fd_pipe, paras_ptr, sizeof(set_mute_t));
    }
    return ret;
}

int Write_Rsp2cp(int fd_pipe, unsigned int cmd)
{
	int ret = 0;
	int count = 0;
	parameters_head_t write_common_head;
    memset(&write_common_head, 0, sizeof(parameters_head_t));
	memcpy(&write_common_head.tag[0], VBC_CMD_TAG, 3);
    write_common_head.cmd_type = cmd+1;
    write_common_head.paras_size = 0;
	if(fd_pipe < 0){
		ALOGE("%s vbpipe has not open...",__func__);
		return -1;
	}
	WriteParas_Head(fd_pipe, &write_common_head);
	MY_TRACE("%s: send  cmd(%d) to cp .",__func__,write_common_head.cmd_type);
	return 0;
}

unsigned short GetCall_Cur_Device()
{
    return android_cur_device;
}
unsigned short GetAudio_vbcpipe_count(void)
{
	return s_vbc_pipe_count;
}

static int32_t GetAudio_mode_number_from_device(struct tiny_audio_device *adev)
{
    int32_t lmode;
    if(((adev->devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) && (adev->devices & AUDIO_DEVICE_OUT_SPEAKER))
			|| ((adev->devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) && (adev->devices & AUDIO_DEVICE_OUT_SPEAKER))){
		lmode = 1;  //headfree
	}else if(adev->devices & AUDIO_DEVICE_OUT_EARPIECE){
		lmode = 2;  //handset
	}else if((adev->devices & AUDIO_DEVICE_OUT_SPEAKER) || (adev->devices & AUDIO_DEVICE_OUT_FM_SPEAKER)){
		lmode = 3;  //handsfree
	}else if((adev->devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) || (adev->devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) 
	        || (adev->devices & AUDIO_DEVICE_OUT_FM_HEADSET) || (adev->devices & AUDIO_DEVICE_IN_WIRED_HEADSET)){
		lmode = 0;  //headset
    }else{
		ALOGW("%s device(0x%x) is not support, set default:handsfree \n",__func__,adev->devices);
        lmode = 3;
	}
    return lmode;
}

static int GetAudio_fd_from_nv()
{
    int fd = -1;
    off_t offset = 0;

    fd = open(ENG_AUDIO_PARA_DEBUG, O_RDONLY);
    if (-1 == fd) {
        ALOGW("%s, file %s open failed:%s\n",__func__,ENG_AUDIO_PARA_DEBUG,strerror(errno));
        fd = open(ENG_AUDIO_PARA,O_RDONLY);
        if(-1 == fd){
            ALOGE("%s, file %s open error:%s\n",__func__,ENG_AUDIO_PARA,strerror(errno));
            return -1;
        }
    }else{
    //check the size of /data/local/tmp/audio_para
        offset = lseek(fd,-1,SEEK_END);
        if((offset+1) != 4*sizeof(AUDIO_TOTAL_T)){
            ALOGE("%s, file %s size (%d) error \n",__func__,ENG_AUDIO_PARA_DEBUG,offset+1);
            close(fd);
            fd = open(ENG_AUDIO_PARA,O_RDONLY);
            if(-1 == fd){
                ALOGE("%s, file %s open error:%s\n",__func__,ENG_AUDIO_PARA,strerror(errno));
                return -1;
            }
        }
    }
    return fd;
}

static int  GetAudio_pga_nv(struct tiny_audio_device *adev, AUDIO_TOTAL_T *aud_params_ptr, pga_gain_nv_t *pga_gain_nv, uint32_t vol_level)
{
    if((NULL == aud_params_ptr) || (NULL == pga_gain_nv)){
        ALOGE("%s aud_params_ptr or pga_gain_nv is NULL",__func__);
        return -1;
    }
    pga_gain_nv->adc_pga_gain_l = aud_params_ptr->audio_nv_arm_mode_info.tAudioNvArmModeStruct.reserve[AUDIO_NV_CAPTURE_GAIN_INDEX];    //43
    pga_gain_nv->adc_pga_gain_r = pga_gain_nv->adc_pga_gain_l;
    
    pga_gain_nv->dac_pga_gain_l = aud_params_ptr->audio_nv_arm_mode_info.tAudioNvArmModeStruct.app_config_info_set.app_config_info[0].arm_volume[vol_level];
    pga_gain_nv->dac_pga_gain_r = pga_gain_nv->dac_pga_gain_l;
    
    pga_gain_nv->fm_pga_gain_l  = (aud_params_ptr->audio_nv_arm_mode_info.tAudioNvArmModeStruct.reserve[AUDIO_NV_FM_GAINL_INDEX]
        | ((aud_params_ptr->audio_nv_arm_mode_info.tAudioNvArmModeStruct.reserve[AUDIO_NV_FM_DGAIN_INDEX]<<16) & 0xffff0000));  //18,19
    pga_gain_nv->fm_pga_gain_r  = pga_gain_nv->fm_pga_gain_l;

    pga_gain_nv->devices = adev->devices;

    ALOGW("%s, dac_pga_gain_l:0x%x adc_pga_gain_l:0x%x fm_pga_gain_l:0x%x fm_pga_gain_r:0x%x device:0x%x vol_level:0x%x ",
        __func__,pga_gain_nv->dac_pga_gain_l,pga_gain_nv->adc_pga_gain_l,pga_gain_nv->fm_pga_gain_l,pga_gain_nv->fm_pga_gain_r,pga_gain_nv->devices,vol_level);
    return 0;
}

static int GetAudio_gain_by_devices(struct tiny_audio_device *adev, pga_gain_nv_t *pga_gain_nv, uint32_t vol_level)
{
    int ret = 0;
    int fd = -1;
    int32_t lmode = 0;
    AUDIO_TOTAL_T * aud_params_ptr = NULL;
    char * dev_name = NULL;
    lmode = GetAudio_mode_number_from_device(adev);
    fd = GetAudio_fd_from_nv();
    if(fd < 0) {
        ALOGE("%s, get audio fd(%d) error ",__func__,fd);
        return -1;
    }
    aud_params_ptr = (AUDIO_TOTAL_T *)mmap(0, 4*sizeof(AUDIO_TOTAL_T),PROT_READ,MAP_SHARED,fd,0);
    if ( NULL == aud_params_ptr ) {
        ALOGE("%s, mmap failed %s",__func__,strerror(errno));
        close(fd);
        return -1;
    }
    //get music gain from nv
    ret = GetAudio_pga_nv(adev, &aud_params_ptr[lmode], pga_gain_nv, vol_level);
    if(ret < 0){
        munmap((void *)aud_params_ptr, 4*sizeof(AUDIO_TOTAL_T));
        close(fd);
        return -1;
    }
    //close fd
    munmap((void *)aud_params_ptr, 4*sizeof(AUDIO_TOTAL_T));
    close(fd);
    return 0;
}

static int SetVoice_gain_by_devices(struct tiny_audio_device *adev, pga_gain_nv_t *pga_gain_nv)
{
    if(NULL == pga_gain_nv){
        ALOGE("%s pga_gain_nv NULL",__func__);
        return -1;
    }
    if(pga_gain_nv->devices & AUDIO_DEVICE_OUT_EARPIECE){
        audio_pga_apply(adev->pga,pga_gain_nv->dac_pga_gain_l,"voice-earpiece");
    }
    if((pga_gain_nv->devices & AUDIO_DEVICE_OUT_SPEAKER) && ((pga_gain_nv->devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) || (pga_gain_nv->devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE))){
        audio_pga_apply(adev->pga,pga_gain_nv->dac_pga_gain_l,"voice-headphone-spk-l");
        audio_pga_apply(adev->pga,pga_gain_nv->dac_pga_gain_r,"voice-headphone-spk-r");
    }else{
        if(pga_gain_nv->devices & AUDIO_DEVICE_OUT_SPEAKER){
            audio_pga_apply(adev->pga,pga_gain_nv->dac_pga_gain_l,"voice-speaker-l");
            audio_pga_apply(adev->pga,pga_gain_nv->dac_pga_gain_r,"voice-speaker-r");
        }
        if((pga_gain_nv->devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) || (pga_gain_nv->devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE)){
            audio_pga_apply(adev->pga,pga_gain_nv->dac_pga_gain_l,"voice-headphone-l");
            audio_pga_apply(adev->pga,pga_gain_nv->dac_pga_gain_r,"voice-headphone-r");
        }
    }
    if((pga_gain_nv->devices & AUDIO_DEVICE_IN_BUILTIN_MIC) || (pga_gain_nv->devices & AUDIO_DEVICE_IN_BACK_MIC) || (pga_gain_nv->devices & AUDIO_DEVICE_IN_WIRED_HEADSET)){
        audio_pga_apply(adev->pga,pga_gain_nv->adc_pga_gain_l,"voice-capture-l");
        audio_pga_apply(adev->pga,pga_gain_nv->adc_pga_gain_r,"voice-capture-r");
    }
    ALOGW("%s out, devices:0x%x ",__func__,pga_gain_nv->devices);
    return 0;
}

static int SetAudio_gain_by_devices(struct tiny_audio_device *adev, pga_gain_nv_t *pga_gain_nv)
{
    if(NULL == pga_gain_nv){
        ALOGE("%s pga_gain_nv NULL",__func__);
        return -1;
    }
    if(pga_gain_nv->devices & AUDIO_DEVICE_OUT_EARPIECE){
        audio_pga_apply(adev->pga,pga_gain_nv->dac_pga_gain_l,"earpiece");
    }
    if((pga_gain_nv->devices & AUDIO_DEVICE_OUT_SPEAKER) && ((pga_gain_nv->devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) || (pga_gain_nv->devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE))){
        audio_pga_apply(adev->pga,pga_gain_nv->dac_pga_gain_l,"headphone-spk-l");
        audio_pga_apply(adev->pga,pga_gain_nv->dac_pga_gain_r,"headphone-spk-r");
    }else{
        if(pga_gain_nv->devices & AUDIO_DEVICE_OUT_SPEAKER){
            audio_pga_apply(adev->pga,pga_gain_nv->dac_pga_gain_l,"speaker-l");
            audio_pga_apply(adev->pga,pga_gain_nv->dac_pga_gain_r,"speaker-r");
        }
        if((pga_gain_nv->devices & AUDIO_DEVICE_OUT_WIRED_HEADSET) || (pga_gain_nv->devices & AUDIO_DEVICE_OUT_WIRED_HEADPHONE)){
    	    audio_pga_apply(adev->pga,pga_gain_nv->dac_pga_gain_l,"headphone-l");
            audio_pga_apply(adev->pga,pga_gain_nv->dac_pga_gain_r,"headphone-r");
        }
    }
    if(pga_gain_nv->devices & AUDIO_DEVICE_OUT_FM_HEADSET){
        audio_pga_apply(adev->pga,pga_gain_nv->fm_pga_gain_l,"linein-hp-l");
        audio_pga_apply(adev->pga,pga_gain_nv->fm_pga_gain_r,"linein-hp-r");
    }else if(pga_gain_nv->devices & AUDIO_DEVICE_OUT_FM_SPEAKER){
        audio_pga_apply(adev->pga,pga_gain_nv->fm_pga_gain_l,"linein-spk-l");
        audio_pga_apply(adev->pga,pga_gain_nv->fm_pga_gain_r,"linein-spk-r");
    }else if((pga_gain_nv->devices & AUDIO_DEVICE_IN_BUILTIN_MIC) || (pga_gain_nv->devices & AUDIO_DEVICE_IN_BACK_MIC) || (pga_gain_nv->devices & AUDIO_DEVICE_IN_WIRED_HEADSET)){
        audio_pga_apply(adev->pga,pga_gain_nv->adc_pga_gain_l,"capture-l");
	    audio_pga_apply(adev->pga,pga_gain_nv->adc_pga_gain_r,"capture-r");
    }
    ALOGW("%s out, devices:0x%x ",__func__,pga_gain_nv->devices);
    return 0;
}

static void SetAudio_gain_route(struct tiny_audio_device *adev, uint32_t vol_level)
{
    int ret = 0;
    pga_gain_nv_t pga_gain_nv;
    memset(&pga_gain_nv,0,sizeof(pga_gain_nv_t));
    ret = GetAudio_gain_by_devices(adev,&pga_gain_nv,vol_level);
    if(ret < 0){
        return;
    }
    ret = SetAudio_gain_by_devices(adev,&pga_gain_nv);
    if(ret < 0){
        return;
    }    
}

static void SetCall_ModePara(struct tiny_audio_device *adev,paras_mode_gain_t *mode_gain_paras)
{
    int i = 0;
	unsigned short switch_earpice = 0;
	unsigned short switch_headset = 0;
	unsigned short switch_speaker = 0;
	unsigned short switch_mic0 = 0;
	unsigned short switch_mic1 = 0;
	unsigned short switch_hp_mic = 0;
    unsigned short switch_table[6] = {0};
    uint32_t switch_device[] = {AUDIO_DEVICE_OUT_EARPIECE,AUDIO_DEVICE_OUT_SPEAKER,AUDIO_DEVICE_IN_BUILTIN_MIC,AUDIO_DEVICE_IN_BACK_MIC,AUDIO_DEVICE_IN_WIRED_HEADSET,AUDIO_DEVICE_OUT_WIRED_HEADSET};

	MY_TRACE("%s path_set:0x%x .android_cur_device:0x%x ",__func__,mode_gain_paras->path_set,android_cur_device);
	switch_earpice = (mode_gain_paras->path_set & 0x0040)>>6;
	switch_headset = mode_gain_paras->path_set & 0x0001;
	switch_speaker = (mode_gain_paras->path_set & 0x0008)>>3;
	switch_mic0 = (mode_gain_paras->path_set & 0x0400)>>10;     //AUDIO_DEVICE_IN_BUILTIN_MIC
	switch_mic1 = (mode_gain_paras->path_set & 0x0800)>>11;     //AUDIO_DEVICE_IN_BACK_MIC
	switch_hp_mic = (mode_gain_paras->path_set & 0x1000)>>12;

    switch_table[0] = switch_earpice;
    switch_table[1] = switch_speaker;
    switch_table[2] = switch_mic0;
    switch_table[3] = switch_mic1;
    switch_table[4] = switch_hp_mic;
    switch_table[5] = switch_headset;
//At present, switch of pa cannot handle mulit-device
    android_cur_device = 0;
    if(switch_earpice){
        android_cur_device |= AUDIO_DEVICE_OUT_EARPIECE;
    }
    if(switch_speaker){
        android_cur_device |= AUDIO_DEVICE_OUT_SPEAKER;
    }
    if(switch_headset){
        android_cur_device |= AUDIO_DEVICE_OUT_WIRED_HEADSET;
    }
    if(switch_mic0){
        android_cur_device |= AUDIO_DEVICE_IN_BUILTIN_MIC;
    }
    if(switch_mic1){
        android_cur_device |= AUDIO_DEVICE_IN_BACK_MIC;
    }
    if(switch_hp_mic){
        android_cur_device |= AUDIO_DEVICE_IN_WIRED_HEADSET;
    }

	for(i=0; i<(sizeof(switch_table)/sizeof(unsigned short));i++)
    {
        if(switch_table[i]){
            set_call_route(adev,switch_device[i],1);
        }
    }
    for(i=0; i<(sizeof(switch_table)/sizeof(unsigned short));i++)
    {
        if(!switch_table[i]){
        #ifdef _DSP_CTRL_CODEC      //if dsp control codec, we cann't close headset.
            if(i == 5 || i == 4){
                continue;
            }
        #endif
            set_call_route(adev,switch_device[i],0);
        }
    }
    /*
        we need to wait for codec here before call connected, maybe driver needs to fix this problem.
    */
    if(!adev->call_connected){
        usleep(1000*100);
    }
	ALOGW("%s successfully, device: earpice(%s), headphone(%s), speaker(%s), Main_Mic(%s), Back_Mic(%s), hp_mic(%s) devices(0x%x)"
				,__func__,switch_earpice ? "Open":"Close",switch_headset ? "Open":"Close",switch_speaker ? "Open":"Close",
				switch_mic0 ? "Open":"Close",switch_mic1 ? "Open":"Close",switch_hp_mic ? "Open":"Close",android_cur_device);
}

static void SetCall_VolumePara(struct tiny_audio_device *adev,paras_mode_gain_t *mode_gain_paras)
{
	int ret = 0;
	pga_gain_nv_t pga_gain_nv;
	memset(&pga_gain_nv,0,sizeof(pga_gain_nv_t));
	if(NULL == mode_gain_paras){
		ret = -1;
		ALOGE("%s mode paras is NULL!!",__func__);
		return;
	}
	pga_gain_nv.devices = android_cur_device;
	pga_gain_nv.mode = adev->mode;
	pga_gain_nv.adc_pga_gain_l= mode_gain_paras->adc_gain & 0x00ff;
	pga_gain_nv.adc_pga_gain_r= (mode_gain_paras->adc_gain & 0xff00) >> 8;
	pga_gain_nv.dac_pga_gain_l= mode_gain_paras->dac_gain & 0x000000ff;
	pga_gain_nv.dac_pga_gain_r= (mode_gain_paras->dac_gain & 0x0000ff00) >> 8;

	ret = SetVoice_gain_by_devices(adev,&pga_gain_nv);
    if(ret < 0){
        return;
    }
	ALOGW("%s successfully ,dac_pga_gain_l:0x%x ,dac_pga_gain_r:0x%x ,adc_pga_gain_l:0x%x ,adc_pga_gain_r:0x%x ,devices:0x%x ,mode:%d ",
		__func__,pga_gain_nv.dac_pga_gain_l,pga_gain_nv.dac_pga_gain_r,pga_gain_nv.adc_pga_gain_l,pga_gain_nv.adc_pga_gain_r,pga_gain_nv.devices,adev->mode);
}

int SetParas_OpenHal_Incall(int fd_pipe)	//Get open hal cmd and sim card
{
    int ret = 0;
    open_hal_t hal_open_param;
    parameters_head_t read_common_head;
    memset(&hal_open_param,0,sizeof(open_hal_t));
    memset(&read_common_head, 0, sizeof(parameters_head_t));
    MY_TRACE("%s in...",__func__);

    ret = Write_Rsp2cp(fd_pipe,VBC_CMD_HAL_OPEN);
    if(ret < 0){
        ALOGE("Error, %s Write_Rsp2cp failed(%d).",__func__,ret);
    }
    ret = ReadParas_OpenHal(fd_pipe,&hal_open_param);
    if (ret <= 0) {
        ALOGE("Error, read %s failed(%d).",__func__,ret);
    }
    ret = Write_Rsp2cp(fd_pipe,VBC_CMD_HAL_OPEN);
    if(ret < 0){
        ALOGE("Error, %s Write_Rsp2cp failed(%d).",__func__,ret);
    }
    android_sim_num = hal_open_param.sim_card;
    MY_TRACE("%s successfully,sim card number(%d)",__func__,android_sim_num);
    return ret;
}

int GetParas_DeviceCtrl_Incall(int fd_pipe,device_ctrl_t *device_ctrl_param)	//open,close
{
	int ret = 0;
	MY_TRACE("%s in... ",__func__);
	ret = Write_Rsp2cp(fd_pipe,VBC_CMD_DEVICE_CTRL);
	if(ret < 0){
		ALOGE("Error, %s Write_Rsp2cp failed(%d).",__func__,ret);
	}
	ret = ReadParas_DeviceCtrl(fd_pipe,device_ctrl_param);
	if (ret <= 0) {
		ALOGE("Error, read %s failed(%d).",__func__,ret);
	}
	if((!device_ctrl_param->paras_mode.is_mode) || (!device_ctrl_param->paras_mode.is_volume)){	//check whether is setDevMode
		ret =-1;
		ALOGE("Error: %s,ReadParas_DeviceCtrl wrong cmd_type.",__func__);
		return ret;
	}
	MY_TRACE("%s successfully ,is_open(%d) is_headphone(%d) is_downlink_mute(%d) is_uplink_mute(%d) volume_index(%d) adc_gain(0x%x), path_set(0x%x), dac_gain(0x%x), pa_setting(0x%x) ",__func__,device_ctrl_param->is_open,device_ctrl_param->is_headphone, \
		device_ctrl_param->is_downlink_mute,device_ctrl_param->is_uplink_mute,device_ctrl_param->paras_mode.volume_index,device_ctrl_param->paras_mode.adc_gain,device_ctrl_param->paras_mode.path_set,device_ctrl_param->paras_mode.dac_gain,device_ctrl_param->paras_mode.pa_setting);
	return ret;
}

int GetParas_Route_Incall(int fd_pipe,paras_mode_gain_t *mode_gain_paras)	//set_volume & set_route
{
	int ret = 0;
	parameters_head_t read_common_head;
	memset(&read_common_head, 0, sizeof(parameters_head_t));
	MY_TRACE("%s in...",__func__);

	ret = Write_Rsp2cp(fd_pipe,VBC_CMD_SET_MODE);
	if(ret < 0){
		ALOGE("Error, %s Write_Rsp2cp failed(%d).",__func__,ret);
	}
    ret = ReadParas_ModeGain(fd_pipe,mode_gain_paras);
    if (ret <= 0) {
        ALOGE("Error, read %s failed(%d).",__func__,ret);
        return ret;
    }
	if((!mode_gain_paras->is_mode)){	//check whether is setDevMode
		ret =-1;
		ALOGE("Error: %s ReadParas_ModeGain wrong cmd_type.",__func__);
		return ret;
	}
	MY_TRACE("%s successfully,volume_index(%d) adc_gain(0x%x), path_set(0x%x), dac_gain(0x%x), pa_setting(0x%x)",__func__,mode_gain_paras->volume_index,mode_gain_paras->adc_gain, \
		mode_gain_paras->path_set, mode_gain_paras->dac_gain,mode_gain_paras->pa_setting);
	return ret;
}

int GetParas_Volume_Incall(int fd_pipe,paras_mode_gain_t *mode_gain_paras)	//set_volume & set_route
{
	int ret = 0;
	parameters_head_t read_common_head;
	memset(&read_common_head, 0, sizeof(parameters_head_t));
	MY_TRACE("%s in...",__func__);

	ret = Write_Rsp2cp(fd_pipe,VBC_CMD_SET_GAIN);
	if(ret < 0){
		ALOGE("Error, %s Write_Rsp2cp failed(%d).",__func__,ret);
	}
    ret = ReadParas_ModeGain(fd_pipe,mode_gain_paras);
    if (ret <= 0) {
        ALOGE("Error, read %s failed(%d).",__func__,ret);
    }
	if((!mode_gain_paras->is_volume)){	//check whether is setDevMode
		ret =-1;
		ALOGE("Error: %s ReadParas_ModeGain wrong cmd_type.",__func__);
		return ret;
	}
	MY_TRACE("%s successfully,volume_index(%d) adc_gain(0x%x), path_set(0x%x), dac_gain(0x%x), pa_setting(0x%x)",__func__,mode_gain_paras->volume_index,mode_gain_paras->adc_gain, \
		mode_gain_paras->path_set, mode_gain_paras->dac_gain, mode_gain_paras->pa_setting);
	return ret;
}

int GetParas_Switch_Incall(int fd_pipe,switch_ctrl_t *swtich_ctrl_paras)	/* switch vbc contrl to dsp.*/
{
	int ret = 0;
	parameters_head_t read_common_head;
	memset(&read_common_head, 0, sizeof(parameters_head_t));
	MY_TRACE("%s in...",__func__);

	ret = Write_Rsp2cp(fd_pipe,VBC_CMD_SWITCH_CTRL);
	if(ret < 0){
		ALOGE("Error, %s Write_Rsp2cp failed(%d)",__func__,ret);
	}
	ret = ReadParas_SwitchCtrl(fd_pipe,swtich_ctrl_paras);
	if (ret <= 0) {
	    ALOGE("Error, read ReadParas_SwitchCtrl failed(%d)",ret);
	}
	MY_TRACE("%s successfully ,is_switch(%d) ",__func__,swtich_ctrl_paras->is_switch);
	return ret;
}

int GetParas_Samplerate_Incall(int fd_pipe,set_samplerate_t *set_samplerate_paras)	/* set samplerate*/
{
    int ret = 0;
    parameters_head_t read_common_head;
    memset(&read_common_head, 0, sizeof(parameters_head_t));
    MY_TRACE("%s in...",__func__);

    ret = Write_Rsp2cp(fd_pipe,VBC_CMD_SET_SAMPLERATE);
    if(ret < 0){
        ALOGE("Error, %s Write_Rsp2cp failed(%d)",__func__,ret);
    }
    ret = ReadParas_SetSamplerate(fd_pipe,set_samplerate_paras);
    if (ret <= 0) {
        ALOGE("Error, read ReadParas_SetSamplerate ret(%d) failed(%s)",ret,strerror(errno));
    }
    if(set_samplerate_paras->samplerate <= 0){
        ALOGW("Error, get wrong samplerate(%d),set default ",set_samplerate_paras->samplerate);
        set_samplerate_paras->samplerate = VX_NB_SAMPLING_RATE; //8k
    }
    MY_TRACE("%s successfully ,samplerate(%d) ",__func__,set_samplerate_paras->samplerate);
    return ret;
}


int SetParas_Route_Incall(int fd_pipe,struct tiny_audio_device *adev)
{
	int ret = 0;
	unsigned short switch_earpice = 0;
	unsigned short switch_headset = 0;
	unsigned short switch_speaker = 0;
	unsigned short switch_mic0 = 0;
	unsigned short switch_mic1 = 0;
	unsigned short switch_hp_mic = 0;
	paras_mode_gain_t mode_gain_paras;
	memset(&mode_gain_paras,0,sizeof(paras_mode_gain_t));
	MY_TRACE("%s in.....",__func__);
	ret = GetParas_Route_Incall(fd_pipe,&mode_gain_paras);
	if(ret < 0){
		return ret;
	}
	SetCall_ModePara(adev,&mode_gain_paras);
	SetCall_VolumePara(adev,&mode_gain_paras);
	Write_Rsp2cp(fd_pipe,VBC_CMD_SET_MODE);
	MY_TRACE("%s send rsp to cp...",__func__);
	return ret;
}

int SetParas_Volume_Incall(int fd_pipe,struct tiny_audio_device *adev)
{
	int ret = 0;
	paras_mode_gain_t mode_gain_paras;
	memset(&mode_gain_paras,0,sizeof(paras_mode_gain_t));
	MY_TRACE("%s in.....",__func__);
	ret = GetParas_Volume_Incall(fd_pipe,&mode_gain_paras);
	if(ret < 0){
		return ret;
	}
	SetCall_VolumePara(adev,&mode_gain_paras);
	Write_Rsp2cp(fd_pipe,VBC_CMD_SET_GAIN);
	MY_TRACE("%s send rsp to cp...",__func__);
	return ret;
}

int SetParas_Switch_Incall(int fd_pipe,int vbchannel_id,struct tiny_audio_device *adev)
{
	int ret = 0;
	parameters_head_t write_common_head;
	switch_ctrl_t swtich_ctrl_paras;
	memset(&swtich_ctrl_paras,0,sizeof(swtich_ctrl_paras));
	MY_TRACE("%s in...",__func__);
	ret = GetParas_Switch_Incall(fd_pipe,&swtich_ctrl_paras);
	if(ret < 0){
		return ret;
	}

	mixer_ctl_set_value(adev->private_ctl.vbc_switch, 0, vbchannel_id);
	ALOGW("%s, vbchannel_id : %d , VBC %s dsp...",__func__,vbchannel_id,(swtich_ctrl_paras.is_switch)?"Switch control to":"Get control back from");

	Write_Rsp2cp(fd_pipe,VBC_CMD_SWITCH_CTRL);
	MY_TRACE("%s send rsp to cp...",__func__);
	return ret;
}

int SetParas_DeviceCtrl_Incall(int fd_pipe,struct tiny_audio_device *adev)
{
    int ret = 0;
    device_ctrl_t device_ctrl_paras;
    struct mixer_ctl *ctl;
    memset(&device_ctrl_paras,0,sizeof(device_ctrl_t));
    MY_TRACE("%s in.....",__func__);

    /*
     * because of codec,we should set headphone on first if codec is controlled by dsp
     */
#ifdef _DSP_CTRL_CODEC
    set_call_route(adev, AUDIO_DEVICE_OUT_WIRED_HEADSET, 1);
#endif


    ret =GetParas_DeviceCtrl_Incall(fd_pipe,&device_ctrl_paras);
    if(ret < 0){
        return ret;
    }

    //set arm mode paras
    if(device_ctrl_paras.is_open){
        SetCall_ModePara(adev,&device_ctrl_paras.paras_mode);
        SetCall_VolumePara(adev,&device_ctrl_paras.paras_mode);

        if( android_cur_device & AUDIO_DEVICE_OUT_SPEAKER){
            ctl = mixer_get_ctl_by_name(adev->mixer, "Speaker Function");
            ret = mixer_ctl_set_value(ctl, 0, 0);
        }

        if( (android_cur_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) ||
                (android_cur_device & AUDIO_DEVICE_OUT_WIRED_HEADSET)){
            ctl = mixer_get_ctl_by_name(adev->mixer, "HeadPhone Mute");
            ret = mixer_ctl_set_value(ctl, 0, 1);
        }

        if( android_cur_device & AUDIO_DEVICE_OUT_EARPIECE){
            ctl = mixer_get_ctl_by_name(adev->mixer, "Earpiece Function");
            ret = mixer_ctl_set_value(ctl, 0, 0);
        }
    }else{
        ALOGW("%s close device...",__func__);
    }
    Write_Rsp2cp(fd_pipe,VBC_CMD_DEVICE_CTRL);
    MY_TRACE("%s send rsp to cp...",__func__);
    return ret;
}

int SetParas_Samplerate_Incall(int fd_pipe,struct tiny_audio_device *adev)
{
    int ret = 0;
    struct mixer_ctl *ctl;
    set_samplerate_t device_set_samplerate;
    memset(&device_set_samplerate,0,sizeof(set_samplerate_t));
    MY_TRACE("%s in.....",__func__);

    ret = GetParas_Samplerate_Incall(fd_pipe,&device_set_samplerate);
    if(ret < 0){
        return ret;
    }
    if( 10 == device_set_samplerate.samplerate ){
        if( android_cur_device & AUDIO_DEVICE_OUT_SPEAKER){
            ctl = mixer_get_ctl_by_name(adev->mixer, "Speaker Function");
            mixer_ctl_set_value(ctl, 0, 1);
        }
        if( android_cur_device & AUDIO_DEVICE_OUT_EARPIECE){
            ctl = mixer_get_ctl_by_name(adev->mixer, "Earpiece Function");
            mixer_ctl_set_value(ctl, 0, 1);
        }
        if( (android_cur_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) ||
                (android_cur_device & AUDIO_DEVICE_OUT_WIRED_HEADSET)){
            ctl = mixer_get_ctl_by_name(adev->mixer, "HeadPhone Mute");
            mixer_ctl_set_value(ctl, 0, 0);
        }
    }
    else {
    }
    ret = Write_Rsp2cp(fd_pipe,VBC_CMD_SET_SAMPLERATE);
    if(ret < 0){
        ALOGE("Error, %s Write_Rsp2cp1 failed(%d).",__func__,ret);
    }
    MY_TRACE("%s send rsp to cp...",__func__);
    return ret;
}


void vbc_ctrl_init(struct tiny_audio_device *adev)
{
    char prop_t[5] = {0};
    char prop_w[5] = {0};
    bool t_enable = false;
    bool w_enalbe = false;
    int i=0;
    bool result=false;
    vbc_ctrl_thread_para_t* vbc_ctrl_index = NULL;
 	if(property_get(RO_MODEM_T_ENABLE_PROPERTY, prop_t, "") && 0 == strcmp(prop_t, "1") )
	{	
		MY_TRACE("%s:ro.modem.t.enable",__func__);
		t_enable = true;
		s_vbc_pipe_count++;
	}
	if(property_get(RO_MODEM_W_ENABLE_PROPERTY, prop_w, "") && 0 == strcmp(prop_w, "1"))
	{
		MY_TRACE("%s:ro.modem.w.enable",__func__);
		w_enalbe = true;
		s_vbc_pipe_count++;
	}

	if(s_vbc_pipe_count)	
	{
		st_vbc_ctrl_thread_para = malloc(s_vbc_pipe_count *
									sizeof(vbc_ctrl_thread_para_t));
		if(!st_vbc_ctrl_thread_para)
		{
			MY_TRACE("error, vbc_ctrl_init malloc para failed,%d",s_vbc_pipe_count);
		}
		s_vbc_ctrl_thread_id = malloc(s_vbc_pipe_count *
									sizeof(pthread_t));
		if(!s_vbc_ctrl_thread_id)
		{
			MY_TRACE("error, vbc_ctrl_init malloc id failed,%d",s_vbc_pipe_count);
		}
			//initialize vbc pipe information
		vbc_ctrl_index = st_vbc_ctrl_thread_para;
		for(i=0;i<adev->cp->num;i++)
		{
			if((t_enable && (adev->cp->vbc_ctrl_pipe_info+i)->cp_type == CP_TG) ||
			     (w_enalbe && (adev->cp->vbc_ctrl_pipe_info+i)->cp_type == CP_W))
			{
	                memcpy(vbc_ctrl_index->vbpipe, (adev->cp->vbc_ctrl_pipe_info+i)->s_vbc_ctrl_pipe_name, VBC_PIPE_NAME_MAX_LEN);
					vbc_ctrl_index->vbchannel_id = (adev->cp->vbc_ctrl_pipe_info+i)->channel_id;
					vbc_ctrl_index->cp_type = (adev->cp->vbc_ctrl_pipe_info+i)->cp_type;
					vbc_ctrl_index->adev = adev;
					vbc_ctrl_index->vbpipe_fd = -1;
					vbc_ctrl_index->vbpipe_pre_fd = -1;
					vbc_ctrl_index->is_exit = 0;
					vbc_ctrl_index++;
					result = true;
					
			}
		}
	}

	if(!result)	
	{
		MY_TRACE("warning: no ro.modem.x.enable,apply default modem profile");
		if(st_vbc_ctrl_thread_para)
		{
			free(st_vbc_ctrl_thread_para);
		}
		if(s_vbc_ctrl_thread_id)
		{
			free(s_vbc_ctrl_thread_id);
		}
		s_vbc_pipe_count = 1;
		st_vbc_ctrl_thread_para = malloc(sizeof(vbc_ctrl_thread_para_t));
		if(!st_vbc_ctrl_thread_para)
		{
			MY_TRACE("error, vbc_ctrl_init malloc para failed,%d",s_vbc_pipe_count);
		}
		s_vbc_ctrl_thread_id = malloc(sizeof(pthread_t));
		if(!s_vbc_ctrl_thread_id)
		{
			MY_TRACE("error, vbc_ctrl_init malloc id failed,%d",s_vbc_pipe_count);
		}
		//initialize vbc pipe information
		memcpy(st_vbc_ctrl_thread_para->vbpipe, s_default_vbc_ctrl_pipe_info.s_vbc_ctrl_pipe_name, VBC_PIPE_NAME_MAX_LEN);
		st_vbc_ctrl_thread_para->vbchannel_id = s_default_vbc_ctrl_pipe_info.channel_id;
		st_vbc_ctrl_thread_para->cp_type = s_default_vbc_ctrl_pipe_info.cp_type;
		st_vbc_ctrl_thread_para->adev = adev;
		st_vbc_ctrl_thread_para->vbpipe_fd = -1;
		st_vbc_ctrl_thread_para->vbpipe_pre_fd = -1;
		st_vbc_ctrl_thread_para->is_exit = 0;			
	}

	MY_TRACE("%s:enable modem :%d",__func__,s_vbc_pipe_count);
}


int vbc_ctrl_open(struct tiny_audio_device *adev)
{
    if (s_is_active) return (-1);
    int rc,i=0,j=0;

    MY_TRACE("%s IN.",__func__);
    s_is_active = 1;
    vbc_ctrl_init(adev);
    while(i<s_vbc_pipe_count)
    {
        rc = pthread_create((pthread_t *)(s_vbc_ctrl_thread_id+i), NULL,
                vbc_ctrl_thread_routine, (void *)(st_vbc_ctrl_thread_para+i));
        if (rc) {
            ALOGE("error, pthread_create failed, rc=%d, i:%d", rc, i);
            while(j<i)
            {
                //need to delete the threads.
                //pthread_cancel (s_vbc_ctrl_thread_id[j]);
                j++;
            }
            s_is_active = 0;
            return (-1);
        }
        i++;
     }

    adev->cur_vbpipe_fd = -1;
    return (0);
}

int vbc_ctrl_close()
{
    if (!s_is_active) return (-1);
    MY_TRACE("%s IN.",__func__);
   
    int i=0;
    s_is_active = 0;
    /* close vbpipe.*/
	while(i<s_vbc_pipe_count)
	{
		close((st_vbc_ctrl_thread_para+i)->vbpipe_fd);
		(st_vbc_ctrl_thread_para+i)->vbpipe_fd = -1;
		(st_vbc_ctrl_thread_para+i)->vbpipe_pre_fd = -1;
		(st_vbc_ctrl_thread_para+i)->is_exit = 1;
		i++;
	}

	free(s_vbc_ctrl_thread_id);
	free(st_vbc_ctrl_thread_para);
	
    /* terminate thread.*/
    //pthread_cancel (s_vbc_ctrl_thread);    
    return (0);
}

void *vbc_ctrl_thread_routine(void *arg)
{
    int ret = 0;
    vbc_ctrl_thread_para_t	*para = NULL;
    struct tiny_audio_device *adev;
    parameters_head_t read_common_head;
    parameters_head_t write_common_head;
    para = (vbc_ctrl_thread_para_t *)arg;
    adev = (struct tiny_audio_device *)(para->adev);

	struct mixer_ctl *ctl;

    memset(&read_common_head, 0, sizeof(parameters_head_t));
    memset(&write_common_head, 0, sizeof(parameters_head_t));
    
    memcpy(&write_common_head.tag[0], VBC_CMD_TAG, 3);
    write_common_head.cmd_type = VBC_CMD_NONE;
    write_common_head.paras_size = 0;
    MY_TRACE("vbc_ctrl_thread_routine in pipe_name:%s.", para->vbpipe);
    
RESTART:
    if (para->is_exit) goto EXIT;
    /* open vbpipe to build connection.*/
    if (para->vbpipe_fd == -1) {
        para->vbpipe_fd = open(para->vbpipe, O_RDWR);//open("/dev/vbpipe6", O_RDWR);
        if (para->vbpipe_fd < 0) {
            if(adev->cur_vbpipe_fd!=-1)/*other pipe is using.*/
            {
                MY_TRACE("VBC_CMD_HAL_RESTART other pipe is working, wait a while and then try to open pipe_name:%s.", para->vbpipe);
            }
            else
            {
                MY_TRACE("VBC_CMD_HAL_RESTART try vbc_lock, pipe_name:%s.", para->vbpipe);
                pthread_mutex_lock(&adev->vbc_lock);
                MY_TRACE("VBC_CMD_HAL_RESTART get vbc_lock, pipe_name:%s.", para->vbpipe);
                ALOGE("VBC_CMD_HAL_RESTART Error: vbpipe_name(%s), vbpipe_fd(%d) open failed, %s ", para->vbpipe, para->vbpipe_fd,strerror(errno));   //cp crash
                if(adev->call_start &&
                        (adev->cur_vbpipe_fd==para->vbpipe_pre_fd/*only current vbpipe need to close*/)){                  //cp crash during call
                    mixer_ctl_set_value(adev->private_ctl.vbc_switch, 0, VBC_ARM_CHANNELID);  //switch to arm
                    pthread_mutex_lock(&adev->lock);
                    force_all_standby(adev);
                    pcm_close(adev->pcm_modem_ul);
                    pcm_close(adev->pcm_modem_dl);
                    adev->call_start = 0;
                    adev->call_connected = 0;
                    adev->cur_vbpipe_fd = -1;
                    para->vbpipe_pre_fd = -1;
                    pthread_mutex_unlock(&adev->lock);
                }
                MY_TRACE("VBC_CMD_HAL_RESTART release vbc_lock, pipe_name:%s.", para->vbpipe);
                pthread_mutex_unlock(&adev->vbc_lock);
            }
            sleep(1);
            goto RESTART;
        } else {
            para->vbpipe_pre_fd = para->vbpipe_fd;

            vbc_call_init_call_process(adev);

            ALOGW("vbpipe_name(%s) vbpipe_fd(%d) open successfully.", para->vbpipe, para->vbpipe_fd);
        }
    } else {
        ALOGW("vbpipe_name(%s) warning: vbpipe_fd(%d) NOT closed.", para->vbpipe, para->vbpipe_fd);
    }

    /* loop to read parameters from vbpipe.*/
    while(!para->is_exit)
    {
        ALOGW("%s, looping now...", para->vbpipe);
        /* read parameters common head of the packet.*/
        ret = ReadParas_Head(para->vbpipe_fd, &read_common_head);
        MY_TRACE("VBC_CMD_HAL_get_cmd try vbc_lock, pipe_name:%s, ret:%d.", para->vbpipe, ret);
        pthread_mutex_lock(&adev->vbc_lock);
        MY_TRACE("VBC_CMD_HAL_get_cmd get vbc_lock, pipe_name:%s.", para->vbpipe);
        if(ret < 0) {
            ALOGE("Error, %s read head failed(%s), need to read again ",__func__,strerror(errno));
                        MY_TRACE("VBC_CMD_HAL_get_cmd release vbc_lock, pipe_name:%s.", para->vbpipe);
            pthread_mutex_unlock(&adev->vbc_lock);
            continue;
        }else if (ret == 0) {   //cp something wrong
            ALOGE("Error, %s read head failed(%s), need to reopen vbpipe, pipe_name:%s",__func__,strerror(errno), para->vbpipe);
            if(adev->call_start){                  //cp crash during call
                  if(adev->cur_vbpipe_fd!=para->vbpipe_fd)
                { /*other cp are using vbc, app/ril need to confirm the control flow: in one time just only one modem can use vbc.*/
                    ALOGE("other cp(pipe_fd:%d) is using vbc,  this cp(pipe_name:%s) error, need to reopen.",
                        adev->cur_vbpipe_fd, para->vbpipe);
                }
                else
                {
                    mixer_ctl_set_value(adev->private_ctl.vbc_switch, 0, VBC_ARM_CHANNELID);  //switch to arm
                    pthread_mutex_lock(&adev->lock);
                    force_all_standby(adev);
                    pcm_close(adev->pcm_modem_ul);
                    pcm_close(adev->pcm_modem_dl);
                    adev->call_start = 0;
                    adev->call_connected = 0;
                    adev->cur_vbpipe_fd = -1;
                    pthread_mutex_unlock(&adev->lock);
                }
            }
            sleep(1);
            close(para->vbpipe_fd);
            para->vbpipe_fd = -1;
            MY_TRACE("VBC_CMD_HAL_get_cmd release vbc_lock.");
            pthread_mutex_unlock(&adev->vbc_lock);
            goto RESTART;
        }

        ALOGW("%s In Call, Get CMD(%d) from cp(pipe:%s, pipe_fd:%d, cur_pipe_fd:%d), paras_size:%d devices:0x%x mode:%d",
            adev->call_start ? "":"NOT", read_common_head.cmd_type,
            para->vbpipe, para->vbpipe_fd, adev->cur_vbpipe_fd,
            read_common_head.paras_size,adev->devices,adev->mode);
         pthread_mutex_lock(&adev->lock);
         if((adev->cur_vbpipe_fd!=-1) && (adev->cur_vbpipe_fd!=para->vbpipe_fd))
         {
            ALOGE("other cp(pipe_fd:%d) is using vbc,  this cp(pipe_name:%s) need to wait...",
                        adev->cur_vbpipe_fd, para->vbpipe);
            //if need to reply to cp for this req or not?
            pthread_mutex_unlock(&adev->lock);
            MY_TRACE("VBC_CMD_HAL_get_cmd release vbc_lock, cmd:%d.", read_common_head.cmd_type);
            pthread_mutex_unlock(&adev->vbc_lock);
            continue;
         }
         pthread_mutex_unlock(&adev->lock);
         if (!memcmp(&read_common_head.tag[0], VBC_CMD_TAG, 3)) {
         switch (read_common_head.cmd_type)
         {
            case VBC_CMD_HAL_OPEN:
            {
                MY_TRACE("VBC_CMD_HAL_OPEN IN.");
                pthread_mutex_lock(&adev->lock);
                ALOGW("VBC_CMD_HAL_OPEN, got adev->lock");
                force_all_standby(adev);    /*should standby because MODE_IN_CALL is later than call_start*/
                ALOGW("VBC_CMD_HAL_OPEN standby ok");
                if (vbc_call_open_device(adev) != 0)
                {
                    para->is_exit = 1;
                }
                ALOGW("VBC_CMD_HAL_OPEN open device ok");
                SetParas_OpenHal_Incall(para->vbpipe_fd);   //get sim card number
                adev->cur_vbpipe_fd = para->vbpipe_fd;
		adev->cp_type = para->cp_type;
                vbc_call_set_flag(VBC_CALL_OPEN_DEVICE);
                pthread_mutex_unlock(&adev->lock);
                MY_TRACE("VBC_CMD_HAL_OPEN OUT, cur_vbpipe_id:%d, vbpipe_name:%s.", adev->cur_vbpipe_fd, para->vbpipe);
            }
            break;
            case VBC_CMD_HAL_CLOSE:
            {
                MY_TRACE("VBC_CMD_HAL_CLOSE IN.");
                adev->call_prestop = 1;
                write_common_head.cmd_type = VBC_CMD_RSP_CLOSE;     //ask cp to read vaudio data, "call_prestop" will stop to write pcm data again.
                WriteParas_Head(para->vbpipe_fd, &write_common_head);
                mixer_ctl_set_value(adev->private_ctl.vbc_switch, 0, VBC_ARM_CHANNELID);  //switch vbc to arm
                mixer_ctl_set_value(adev->private_ctl.internal_pa, 0, inter_pa_music);
                if(adev->call_start){  //if mediaserver crashed, audio will reopen, "call_start" value is 0, should bypass all the settings.
                    ALOGW("VBC_CMD_HAL_CLOSE, try lock");
                    pthread_mutex_lock(&adev->lock);
                    ALOGW("VBC_CMD_HAL_CLOSE, got lock");
                    force_all_standby(adev);
                    pcm_close(adev->pcm_modem_ul);
                    pcm_close(adev->pcm_modem_dl);
                    adev->call_start = 0;
                    adev->call_connected = 0;
                    adev->cur_vbpipe_fd = -1;
                    ALOGW("END CALL,close pcm device & switch to arm...");
                    pthread_mutex_unlock(&adev->lock);
                }else{
                    ALOGW("VBC_CMD_HAL_CLOSE, call thread restart, we should stop call!!!");
                }
                ReadParas_Head(para->vbpipe_fd,&write_common_head);
                Write_Rsp2cp(para->vbpipe_fd,VBC_CMD_HAL_CLOSE);
                adev->call_prestop = 0;
                ctl = mixer_get_ctl_by_name(adev->mixer, "HeadPhone Mute");
                ret = mixer_ctl_set_value(ctl, 0, 0);
                vbc_call_set_flag(VBC_CALL_CLOSE_DEVICE);
                MY_TRACE("VBC_CMD_HAL_CLOSE OUT.");
            }
            break;
            case VBC_CMD_SET_MODE:
            {
                MY_TRACE("VBC_CMD_SET_MODE IN.");
                int i=0;
                while(i<s_vbc_pipe_count)
                {
                    if((st_vbc_ctrl_thread_para+i)->vbpipe_fd!=-1)
                    {
                        ret = SetParas_Route_Incall((st_vbc_ctrl_thread_para+i)->vbpipe_fd,adev);
                        if(ret < 0){
                            MY_TRACE("VBC_CMD_SET_MODE SetParas_Route_Incall error. pipe:%s, s_is_exit:%d ",
                                (st_vbc_ctrl_thread_para+i)->vbpipe, (st_vbc_ctrl_thread_para+i)->is_exit);
                            (st_vbc_ctrl_thread_para+i)->is_exit = 1;
                        }
                    }
                    i++;
                }
                MY_TRACE("VBC_CMD_SET_MODE OUT.");
            }
            break;
            case VBC_CMD_SET_GAIN:
            {
                MY_TRACE("VBC_CMD_SET_GAIN IN.");
                int i=0;
                while(i<s_vbc_pipe_count)
                {
                    if((st_vbc_ctrl_thread_para+i)->vbpipe_fd!=-1)
                    {
                        ret = SetParas_Volume_Incall((st_vbc_ctrl_thread_para+i)->vbpipe_fd,adev);
                        if(ret < 0){
                            MY_TRACE("VBC_CMD_SET_GAIN SetParas_Route_Incall error. pipe:%s, s_is_exit:%d ",
                                (st_vbc_ctrl_thread_para+i)->vbpipe, (st_vbc_ctrl_thread_para+i)->is_exit);
                            (st_vbc_ctrl_thread_para+i)->is_exit = 1;
                        }
                    }
                    i++;
                }
                MY_TRACE("VBC_CMD_SET_GAIN OUT.");
            }
            break;
            case VBC_CMD_SWITCH_CTRL:
            {
                MY_TRACE("VBC_CMD_SWITCH_CTRL IN.");
                ret = SetParas_Switch_Incall(para->vbpipe_fd,para->vbchannel_id,adev);
                if(ret < 0){
                    MY_TRACE("VBC_CMD_SWITCH_CTRL SetParas_Switch_Incall error.s_is_exit:%d ",para->is_exit);
                    para->is_exit = 1;
                }
                pthread_mutex_lock(&adev->lock);
                adev->call_connected = 1;
                pthread_mutex_unlock(&adev->lock);
                vbc_call_set_flag(VBC_CALL_SWITCH_CTRL);
                MY_TRACE("VBC_CMD_SWITCH_CTRL OUT.");
            }
            break;
            case VBC_CMD_SET_MUTE:
            {
                MY_TRACE("VBC_CMD_SET_MUTE IN.");
                int ret;
                set_mute_t mute_status;
                ret = read(para->vbpipe_fd, &mute_status, sizeof(set_mute_t));

                if( android_cur_device & AUDIO_DEVICE_OUT_SPEAKER){
                    ctl = mixer_get_ctl_by_name(adev->mixer, "Speaker Function");
                    ret = mixer_ctl_set_value(ctl, 0, 0);
                }
                if( (android_cur_device & AUDIO_DEVICE_OUT_WIRED_HEADPHONE) ||
                        (android_cur_device & AUDIO_DEVICE_OUT_WIRED_HEADSET)){
                    ctl = mixer_get_ctl_by_name(adev->mixer, "HeadPhone Mute");
                    ret = mixer_ctl_set_value(ctl, 0, 1);
                }
                if( android_cur_device & AUDIO_DEVICE_OUT_EARPIECE){
                    ctl = mixer_get_ctl_by_name(adev->mixer, "Earpiece Function");
                    ret = mixer_ctl_set_value(ctl, 0, 0);
                }
                write_common_head.cmd_type = VBC_CMD_RSP_MUTE;     //ask cp to read vaudio data, "call_prestop" will stop to write pcm data again.
                WriteParas_Head(para->vbpipe_fd, &write_common_head);

                MY_TRACE("VBC_CMD_SET_MUTE OUT.");
            }
            break;
            case VBC_CMD_DEVICE_CTRL:
            {
                MY_TRACE("VBC_CMD_DEVICE_CTRL IN.");
                ret = SetParas_DeviceCtrl_Incall(para->vbpipe_fd,adev);
                if(ret < 0){
                    MY_TRACE("VBC_CMD_DEVICE_CTRL SetParas_DeviceCtrl_Incall error.s_is_exit:%d ",para->is_exit);
                    para->is_exit = 1;
                }
                MY_TRACE("VBC_CMD_DEVICE_CTRL OUT.");
            }
            break;
            case VBC_CMD_SET_SAMPLERATE:
                {
                    MY_TRACE("VBC_CMD_SET_SAMPLERATE IN.");
                    ret = SetParas_Samplerate_Incall(para->vbpipe_fd,adev);
                    if(ret < 0){
                        MY_TRACE("VBC_CMD_SET_SAMPLERATE SetParas_Samplerate_Incall error.s_is_exit:%d ",para->is_exit);
                        para->is_exit = 1;
                    }
                    MY_TRACE("VBC_CMD_SET_SAMPLERATE OUT.");
                }
                break;
            default:
                ALOGE("Error: %s wrong cmd_type(%d)",__func__,read_common_head.cmd_type);
            break;
            }
        } else {
            ALOGE("Error, (0x%x)NOT match VBC_CMD_TAG, wrong packet.", *((int*)read_common_head.tag));
        }
        MY_TRACE("VBC_CMD_HAL_get_cmd release vbc_lock.");
        pthread_mutex_unlock(&adev->vbc_lock);
    }

EXIT:
    ALOGW("vbc_ctrl_thread exit, pipe:%s!!!", para->vbpipe);
    return 0;
}

