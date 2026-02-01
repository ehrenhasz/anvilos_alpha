 

#ifndef _GVT_FB_DECODER_H_
#define _GVT_FB_DECODER_H_

#include <linux/types.h>

#include "display/intel_display_limits.h"

struct intel_vgpu;

#define _PLANE_CTL_FORMAT_SHIFT		24
#define _PLANE_CTL_TILED_SHIFT		10
#define _PIPE_V_SRCSZ_SHIFT		0
#define _PIPE_V_SRCSZ_MASK		(0xfff << _PIPE_V_SRCSZ_SHIFT)
#define _PIPE_H_SRCSZ_SHIFT		16
#define _PIPE_H_SRCSZ_MASK		(0x1fff << _PIPE_H_SRCSZ_SHIFT)

#define _PRI_PLANE_FMT_SHIFT		26
#define _PRI_PLANE_STRIDE_MASK		(0x3ff << 6)
#define _PRI_PLANE_X_OFF_SHIFT		0
#define _PRI_PLANE_X_OFF_MASK		(0x1fff << _PRI_PLANE_X_OFF_SHIFT)
#define _PRI_PLANE_Y_OFF_SHIFT		16
#define _PRI_PLANE_Y_OFF_MASK		(0xfff << _PRI_PLANE_Y_OFF_SHIFT)

#define _CURSOR_MODE			0x3f
#define _CURSOR_ALPHA_FORCE_SHIFT	8
#define _CURSOR_ALPHA_FORCE_MASK	(0x3 << _CURSOR_ALPHA_FORCE_SHIFT)
#define _CURSOR_ALPHA_PLANE_SHIFT	10
#define _CURSOR_ALPHA_PLANE_MASK	(0x3 << _CURSOR_ALPHA_PLANE_SHIFT)
#define _CURSOR_POS_X_SHIFT		0
#define _CURSOR_POS_X_MASK		(0x1fff << _CURSOR_POS_X_SHIFT)
#define _CURSOR_SIGN_X_SHIFT		15
#define _CURSOR_SIGN_X_MASK		(1 << _CURSOR_SIGN_X_SHIFT)
#define _CURSOR_POS_Y_SHIFT		16
#define _CURSOR_POS_Y_MASK		(0xfff << _CURSOR_POS_Y_SHIFT)
#define _CURSOR_SIGN_Y_SHIFT		31
#define _CURSOR_SIGN_Y_MASK		(1 << _CURSOR_SIGN_Y_SHIFT)

#define _SPRITE_FMT_SHIFT		25
#define _SPRITE_COLOR_ORDER_SHIFT	20
#define _SPRITE_YUV_ORDER_SHIFT		16
#define _SPRITE_STRIDE_SHIFT		6
#define _SPRITE_STRIDE_MASK		(0x1ff << _SPRITE_STRIDE_SHIFT)
#define _SPRITE_SIZE_WIDTH_SHIFT	0
#define _SPRITE_SIZE_HEIGHT_SHIFT	16
#define _SPRITE_SIZE_WIDTH_MASK		(0x1fff << _SPRITE_SIZE_WIDTH_SHIFT)
#define _SPRITE_SIZE_HEIGHT_MASK	(0xfff << _SPRITE_SIZE_HEIGHT_SHIFT)
#define _SPRITE_POS_X_SHIFT		0
#define _SPRITE_POS_Y_SHIFT		16
#define _SPRITE_POS_X_MASK		(0x1fff << _SPRITE_POS_X_SHIFT)
#define _SPRITE_POS_Y_MASK		(0xfff << _SPRITE_POS_Y_SHIFT)
#define _SPRITE_OFFSET_START_X_SHIFT	0
#define _SPRITE_OFFSET_START_Y_SHIFT	16
#define _SPRITE_OFFSET_START_X_MASK	(0x1fff << _SPRITE_OFFSET_START_X_SHIFT)
#define _SPRITE_OFFSET_START_Y_MASK	(0xfff << _SPRITE_OFFSET_START_Y_SHIFT)

enum GVT_FB_EVENT {
	FB_MODE_SET_START = 1,
	FB_MODE_SET_END,
	FB_DISPLAY_FLIP,
};

enum DDI_PORT {
	DDI_PORT_NONE	= 0,
	DDI_PORT_B	= 1,
	DDI_PORT_C	= 2,
	DDI_PORT_D	= 3,
	DDI_PORT_E	= 4
};

 
struct intel_vgpu_primary_plane_format {
	u8	enabled;	 
	u32	tiled;		 
	u8	bpp;		 
	u32	hw_format;	 
	u32	drm_format;	 
	u32	base;		 
	u64     base_gpa;
	u32	x_offset;	 
	u32	y_offset;	 
	u32	width;		 
	u32	height;		 
	u32	stride;		 
};

struct intel_vgpu_sprite_plane_format {
	u8	enabled;	 
	u8	tiled;		 
	u8	bpp;		 
	u32	hw_format;	 
	u32	drm_format;	 
	u32	base;		 
	u64     base_gpa;
	u32	x_pos;		 
	u32	y_pos;		 
	u32	x_offset;	 
	u32	y_offset;	 
	u32	width;		 
	u32	height;		 
	u32	stride;		 
};

struct intel_vgpu_cursor_plane_format {
	u8	enabled;
	u8	mode;		 
	u8	bpp;		 
	u32	drm_format;	 
	u32	base;		 
	u64     base_gpa;
	u32	x_pos;		 
	u32	y_pos;		 
	u8	x_sign;		 
	u8	y_sign;		 
	u32	width;		 
	u32	height;		 
	u32	x_hot;		 
	u32	y_hot;		 
};

struct intel_vgpu_pipe_format {
	struct intel_vgpu_primary_plane_format	primary;
	struct intel_vgpu_sprite_plane_format	sprite;
	struct intel_vgpu_cursor_plane_format	cursor;
	enum DDI_PORT ddi_port;   
};

struct intel_vgpu_fb_format {
	struct intel_vgpu_pipe_format	pipes[I915_MAX_PIPES];
};

int intel_vgpu_decode_primary_plane(struct intel_vgpu *vgpu,
	struct intel_vgpu_primary_plane_format *plane);
int intel_vgpu_decode_cursor_plane(struct intel_vgpu *vgpu,
	struct intel_vgpu_cursor_plane_format *plane);
int intel_vgpu_decode_sprite_plane(struct intel_vgpu *vgpu,
	struct intel_vgpu_sprite_plane_format *plane);

#endif
