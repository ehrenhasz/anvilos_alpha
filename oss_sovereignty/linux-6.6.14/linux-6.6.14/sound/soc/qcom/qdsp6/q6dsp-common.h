#ifndef __Q6DSP_COMMON_H__
#define __Q6DSP_COMMON_H__
#include <linux/kernel.h>
#define PCM_MAX_NUM_CHANNEL  8
#define PCM_CHANNEL_NULL 0
#define PCM_CHANNEL_FL    1	 
#define PCM_CHANNEL_FR    2	 
#define PCM_CHANNEL_FC    3	 
#define PCM_CHANNEL_LS   4	 
#define PCM_CHANNEL_RS   5	 
#define PCM_CHANNEL_LFE  6	 
#define PCM_CHANNEL_CS   7	 
#define PCM_CHANNEL_LB   8	 
#define PCM_CHANNEL_RB   9	 
#define PCM_CHANNELS   10	 
int q6dsp_map_channels(u8 ch_map[PCM_MAX_NUM_CHANNEL], int ch);
int q6dsp_get_channel_allocation(int channels);
#endif  
