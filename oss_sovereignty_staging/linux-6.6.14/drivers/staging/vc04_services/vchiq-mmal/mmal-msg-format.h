 
 

#ifndef MMAL_MSG_FORMAT_H
#define MMAL_MSG_FORMAT_H

#include <linux/math.h>

#include "mmal-msg-common.h"

 

struct mmal_audio_format {
	u32 channels;		 
	u32 sample_rate;	 

	u32 bits_per_sample;	 
	u32 block_align;	 
};

struct mmal_video_format {
	u32 width;		 
	u32 height;		 
	struct mmal_rect crop;	 
	struct s32_fract frame_rate;	 
	struct s32_fract par;		 

	 
	u32 color_space;
};

struct mmal_subpicture_format {
	u32 x_offset;
	u32 y_offset;
};

union mmal_es_specific_format {
	struct mmal_audio_format audio;
	struct mmal_video_format video;
	struct mmal_subpicture_format subpicture;
};

 
struct mmal_es_format_local {
	u32 type;	 

	u32 encoding;	 
	u32 encoding_variant;	 

	union mmal_es_specific_format *es;	 

	u32 bitrate;	 
	u32 flags;	 

	u32 extradata_size;	 
	u8  *extradata;		 
};

 
struct mmal_es_format {
	u32 type;	 

	u32 encoding;	 
	u32 encoding_variant;	 

	u32 es;	 

	u32 bitrate;	 
	u32 flags;	 

	u32 extradata_size;	 
	u32 extradata;		 
};

#endif  
