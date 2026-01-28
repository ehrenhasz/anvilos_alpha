#ifndef __IA_CSS_FRAME_PUBLIC_H
#define __IA_CSS_FRAME_PUBLIC_H
#include <media/videobuf2-v4l2.h>
#include <type_support.h>
#include "ia_css_err.h"
#include "ia_css_types.h"
#include "ia_css_frame_format.h"
#include "ia_css_buffer.h"
enum ia_css_bayer_order {
	IA_CSS_BAYER_ORDER_GRBG,  
	IA_CSS_BAYER_ORDER_RGGB,  
	IA_CSS_BAYER_ORDER_BGGR,  
	IA_CSS_BAYER_ORDER_GBRG,  
};
#define IA_CSS_BAYER_ORDER_NUM (IA_CSS_BAYER_ORDER_GBRG + 1)
struct ia_css_frame_plane {
	unsigned int height;  
	unsigned int width;   
	unsigned int stride;  
	unsigned int offset;  
};
struct ia_css_frame_binary_plane {
	unsigned int		  size;  
	struct ia_css_frame_plane data;  
};
struct ia_css_frame_yuv_planes {
	struct ia_css_frame_plane y;  
	struct ia_css_frame_plane u;  
	struct ia_css_frame_plane v;  
};
struct ia_css_frame_nv_planes {
	struct ia_css_frame_plane y;   
	struct ia_css_frame_plane uv;  
};
struct ia_css_frame_rgb_planes {
	struct ia_css_frame_plane r;  
	struct ia_css_frame_plane g;  
	struct ia_css_frame_plane b;  
};
struct ia_css_frame_plane6_planes {
	struct ia_css_frame_plane r;	   
	struct ia_css_frame_plane r_at_b;  
	struct ia_css_frame_plane gr;	   
	struct ia_css_frame_plane gb;	   
	struct ia_css_frame_plane b;	   
	struct ia_css_frame_plane b_at_r;  
};
struct ia_css_crop_info {
	unsigned int start_column;
	unsigned int start_line;
};
struct ia_css_frame_info {
	struct ia_css_resolution res;  
	unsigned int padded_width;  
	enum ia_css_frame_format format;  
	unsigned int raw_bit_depth;  
	enum ia_css_bayer_order raw_bayer_order;  
	struct ia_css_crop_info crop_info;
};
#define IA_CSS_BINARY_DEFAULT_FRAME_INFO { \
	.format			= IA_CSS_FRAME_FORMAT_NUM,  \
	.raw_bayer_order	= IA_CSS_BAYER_ORDER_NUM, \
}
enum ia_css_frame_delay {
	IA_CSS_FRAME_DELAY_0,  
	IA_CSS_FRAME_DELAY_1,  
	IA_CSS_FRAME_DELAY_2   
};
enum ia_css_frame_flash_state {
	IA_CSS_FRAME_FLASH_STATE_NONE,
	IA_CSS_FRAME_FLASH_STATE_PARTIAL,
	IA_CSS_FRAME_FLASH_STATE_FULL
};
struct ia_css_frame {
	struct vb2_v4l2_buffer vb;
	struct list_head queue;
	struct ia_css_frame_info frame_info;  
	ia_css_ptr   data;	        
	unsigned int data_bytes;        
	int dynamic_queue_id;
	enum ia_css_buffer_type buf_type;
	enum ia_css_frame_flash_state flash_state;
	unsigned int exp_id;
	u32 isp_config_id;  
	bool valid;  
	union {
		unsigned int	_initialisation_dummy;
		struct ia_css_frame_plane raw;
		struct ia_css_frame_plane rgb;
		struct ia_css_frame_rgb_planes planar_rgb;
		struct ia_css_frame_plane yuyv;
		struct ia_css_frame_yuv_planes yuv;
		struct ia_css_frame_nv_planes nv;
		struct ia_css_frame_plane6_planes plane6;
		struct ia_css_frame_binary_plane binary;
	} planes;  
};
#define vb_to_frame(vb2) \
	container_of(to_vb2_v4l2_buffer(vb2), struct ia_css_frame, vb)
#define DEFAULT_FRAME { \
	.frame_info		= IA_CSS_BINARY_DEFAULT_FRAME_INFO, \
	.dynamic_queue_id	= SH_CSS_INVALID_QUEUE_ID, \
	.buf_type		= IA_CSS_BUFFER_TYPE_INVALID, \
	.flash_state		= IA_CSS_FRAME_FLASH_STATE_NONE, \
}
int
ia_css_frame_allocate(struct ia_css_frame **frame,
		      unsigned int width,
		      unsigned int height,
		      enum ia_css_frame_format format,
		      unsigned int stride,
		      unsigned int raw_bit_depth);
int ia_css_frame_init_from_info(struct ia_css_frame *frame,
				const struct ia_css_frame_info *info);
int
ia_css_frame_allocate_from_info(struct ia_css_frame **frame,
				const struct ia_css_frame_info *info);
void
ia_css_frame_free(struct ia_css_frame *frame);
static inline const struct ia_css_frame_info *
ia_css_frame_get_info(const struct ia_css_frame *frame)
{
	return frame ? &frame->frame_info : NULL;
}
#endif  
