 
 

#ifndef	__ATOMISP_COMMON_H__
#define	__ATOMISP_COMMON_H__

#include "../../include/linux/atomisp.h"

#include <linux/v4l2-mediabus.h>

#include <media/videobuf2-v4l2.h>

#include "atomisp_compat.h"

#include "ia_css.h"

extern int dbg_level;
extern int dbg_func;
extern int mipicsi_flag;
extern int pad_w;
extern int pad_h;

 
#define ISP2400_MIN_PAD_W		12
#define ISP2400_MIN_PAD_H		12

#define CSS_DTRACE_VERBOSITY_LEVEL	5	 
#define CSS_DTRACE_VERBOSITY_TIMEOUT	9	 
#define MRFLD_MAX_ZOOM_FACTOR	1024

 
#define ATOMISP_CSS_ISP_PIPE_VERSION_2_7    1

struct atomisp_format_bridge {
	unsigned int pixelformat;
	unsigned int depth;
	u32 mbus_code;
	enum ia_css_frame_format sh_fmt;
	unsigned char description[32];	 
	bool planar;
};

struct atomisp_fmt {
	u32 pixelformat;
	u32 depth;
	u32 bytesperline;
	u32 framesize;
	u32 imagesize;
	u32 width;
	u32 height;
	u32 bayer_order;
};

#endif
