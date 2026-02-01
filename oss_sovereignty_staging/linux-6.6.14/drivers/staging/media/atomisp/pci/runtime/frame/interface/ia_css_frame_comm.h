 
 

#ifndef __IA_CSS_FRAME_COMM_H__
#define __IA_CSS_FRAME_COMM_H__

#include "type_support.h"
#include "platform_support.h"
#include "runtime/bufq/interface/ia_css_bufq_comm.h"
#include <system_local.h>	  

 
struct ia_css_frame_sp_plane {
	unsigned int offset;	 
	 
};

struct ia_css_frame_sp_binary_plane {
	unsigned int size;
	struct ia_css_frame_sp_plane data;
};

struct ia_css_frame_sp_yuv_planes {
	struct ia_css_frame_sp_plane y;
	struct ia_css_frame_sp_plane u;
	struct ia_css_frame_sp_plane v;
};

struct ia_css_frame_sp_nv_planes {
	struct ia_css_frame_sp_plane y;
	struct ia_css_frame_sp_plane uv;
};

struct ia_css_frame_sp_rgb_planes {
	struct ia_css_frame_sp_plane r;
	struct ia_css_frame_sp_plane g;
	struct ia_css_frame_sp_plane b;
};

struct ia_css_frame_sp_plane6 {
	struct ia_css_frame_sp_plane r;
	struct ia_css_frame_sp_plane r_at_b;
	struct ia_css_frame_sp_plane gr;
	struct ia_css_frame_sp_plane gb;
	struct ia_css_frame_sp_plane b;
	struct ia_css_frame_sp_plane b_at_r;
};

struct ia_css_sp_resolution {
	u16 width;		 
	u16 height;	 
};

 
struct ia_css_frame_sp_info {
	struct ia_css_sp_resolution res;
	u16 padded_width;		 
	unsigned char format;		 
	unsigned char raw_bit_depth;	 
	unsigned char raw_bayer_order;	 
	unsigned char padding[3];	 
};

struct ia_css_buffer_sp {
	union {
		ia_css_ptr xmem_addr;
		enum sh_css_queue_id queue_id;
	} buf_src;
	enum ia_css_buffer_type buf_type;
};

struct ia_css_frame_sp {
	struct ia_css_frame_sp_info info;
	struct ia_css_buffer_sp buf_attr;
	union {
		struct ia_css_frame_sp_plane raw;
		struct ia_css_frame_sp_plane rgb;
		struct ia_css_frame_sp_rgb_planes planar_rgb;
		struct ia_css_frame_sp_plane yuyv;
		struct ia_css_frame_sp_yuv_planes yuv;
		struct ia_css_frame_sp_nv_planes nv;
		struct ia_css_frame_sp_plane6 plane6;
		struct ia_css_frame_sp_binary_plane binary;
	} planes;
};

void ia_css_frame_info_to_frame_sp_info(
    struct ia_css_frame_sp_info *sp_info,
    const struct ia_css_frame_info *info);

void ia_css_resolution_to_sp_resolution(
    struct ia_css_sp_resolution *sp_info,
    const struct ia_css_resolution *info);

#endif  
