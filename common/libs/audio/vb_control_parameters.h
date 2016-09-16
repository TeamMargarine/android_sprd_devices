/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef VBC_CONTROL_PARAMETERS_H
#define VBC_CONTROL_PARAMETERS_H

#define BUF_SIZE 1024

#define VBC_PIPE_NAME_MAX_LEN 16

#define AUDIO_XML_PATH "/system/etc/audio_hw.xml"

#define RO_MODEM_T_ENABLE_PROPERTY     "ro.modem.t.enable"
#define RO_MODEM_W_ENABLE_PROPERTY     "ro.modem.w.enable"


typedef enum {
    CP_W,
    CP_TG,
    CP_MAX
}cp_type_t;

/*support multiple call for multiple modem(cp0/cp1/...):
different modem is corresponding to different pipe and all pipes use the only vbc.
support multiple pipe:
1. change VBC_PIPE_COUNT
2. change the definition of s_vbc_ctrl_pipe_info.
3. change channel_id for different cp .On sharp, 0 for cp0,  1 for cp1,2 for ap
*/

typedef struct
{
    char s_vbc_ctrl_pipe_name[VBC_PIPE_NAME_MAX_LEN];
    int channel_id;
    cp_type_t cp_type;
}vbc_ctrl_pipe_para_t;



typedef struct{
	int num;
	vbc_ctrl_pipe_para_t *vbc_ctrl_pipe_info;
}audio_modem_t;


struct modem_config_parse_state{
	audio_modem_t *modem_info;
	vbc_ctrl_pipe_para_t *vbc_ctrl_pipe_info;
};

#endif
