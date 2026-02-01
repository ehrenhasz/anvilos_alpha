 
 
#ifndef __RGA_HW_H__
#define __RGA_HW_H__

#define RGA_CMDBUF_SIZE 0x20

 
#define MAX_WIDTH 8192
#define MAX_HEIGHT 8192

#define MIN_WIDTH 34
#define MIN_HEIGHT 34

#define DEFAULT_WIDTH 100
#define DEFAULT_HEIGHT 100

#define RGA_TIMEOUT 500

 
#define RGA_SYS_CTRL 0x0000
#define RGA_CMD_CTRL 0x0004
#define RGA_CMD_BASE 0x0008
#define RGA_INT 0x0010
#define RGA_MMU_CTRL0 0x0014
#define RGA_VERSION_INFO 0x0028

#define RGA_MODE_BASE_REG 0x0100
#define RGA_MODE_MAX_REG 0x017C

#define RGA_MODE_CTRL 0x0100
#define RGA_SRC_INFO 0x0104
#define RGA_SRC_Y_RGB_BASE_ADDR 0x0108
#define RGA_SRC_CB_BASE_ADDR 0x010c
#define RGA_SRC_CR_BASE_ADDR 0x0110
#define RGA_SRC1_RGB_BASE_ADDR 0x0114
#define RGA_SRC_VIR_INFO 0x0118
#define RGA_SRC_ACT_INFO 0x011c
#define RGA_SRC_X_FACTOR 0x0120
#define RGA_SRC_Y_FACTOR 0x0124
#define RGA_SRC_BG_COLOR 0x0128
#define RGA_SRC_FG_COLOR 0x012c
#define RGA_SRC_TR_COLOR0 0x0130
#define RGA_SRC_TR_COLOR1 0x0134

#define RGA_DST_INFO 0x0138
#define RGA_DST_Y_RGB_BASE_ADDR 0x013c
#define RGA_DST_CB_BASE_ADDR 0x0140
#define RGA_DST_CR_BASE_ADDR 0x0144
#define RGA_DST_VIR_INFO 0x0148
#define RGA_DST_ACT_INFO 0x014c

#define RGA_ALPHA_CTRL0 0x0150
#define RGA_ALPHA_CTRL1 0x0154
#define RGA_FADING_CTRL 0x0158
#define RGA_PAT_CON 0x015c
#define RGA_ROP_CON0 0x0160
#define RGA_ROP_CON1 0x0164
#define RGA_MASK_BASE 0x0168

#define RGA_MMU_CTRL1 0x016C
#define RGA_MMU_SRC_BASE 0x0170
#define RGA_MMU_SRC1_BASE 0x0174
#define RGA_MMU_DST_BASE 0x0178

 
#define RGA_MODE_RENDER_BITBLT 0
#define RGA_MODE_RENDER_COLOR_PALETTE 1
#define RGA_MODE_RENDER_RECTANGLE_FILL 2
#define RGA_MODE_RENDER_UPDATE_PALETTE_LUT_RAM 3

#define RGA_MODE_BITBLT_MODE_SRC_TO_DST 0
#define RGA_MODE_BITBLT_MODE_SRC_SRC1_TO_DST 1

#define RGA_MODE_CF_ROP4_SOLID 0
#define RGA_MODE_CF_ROP4_PATTERN 1

#define RGA_COLOR_FMT_ABGR8888 0
#define RGA_COLOR_FMT_XBGR8888 1
#define RGA_COLOR_FMT_RGB888 2
#define RGA_COLOR_FMT_BGR565 4
#define RGA_COLOR_FMT_ABGR1555 5
#define RGA_COLOR_FMT_ABGR4444 6
#define RGA_COLOR_FMT_YUV422SP 8
#define RGA_COLOR_FMT_YUV422P 9
#define RGA_COLOR_FMT_YUV420SP 10
#define RGA_COLOR_FMT_YUV420P 11
 
#define RGA_COLOR_FMT_CP_1BPP 12
#define RGA_COLOR_FMT_CP_2BPP 13
#define RGA_COLOR_FMT_CP_4BPP 14
#define RGA_COLOR_FMT_CP_8BPP 15
#define RGA_COLOR_FMT_MASK 15

#define RGA_COLOR_FMT_IS_YUV(fmt) \
	(((fmt) >= RGA_COLOR_FMT_YUV422SP) && ((fmt) < RGA_COLOR_FMT_CP_1BPP))
#define RGA_COLOR_FMT_IS_RGB(fmt) \
	((fmt) < RGA_COLOR_FMT_YUV422SP)

#define RGA_COLOR_NONE_SWAP 0
#define RGA_COLOR_RB_SWAP 1
#define RGA_COLOR_ALPHA_SWAP 2
#define RGA_COLOR_UV_SWAP 4

#define RGA_SRC_CSC_MODE_BYPASS 0
#define RGA_SRC_CSC_MODE_BT601_R0 1
#define RGA_SRC_CSC_MODE_BT601_R1 2
#define RGA_SRC_CSC_MODE_BT709_R0 3
#define RGA_SRC_CSC_MODE_BT709_R1 4

#define RGA_SRC_ROT_MODE_0_DEGREE 0
#define RGA_SRC_ROT_MODE_90_DEGREE 1
#define RGA_SRC_ROT_MODE_180_DEGREE 2
#define RGA_SRC_ROT_MODE_270_DEGREE 3

#define RGA_SRC_MIRR_MODE_NO 0
#define RGA_SRC_MIRR_MODE_X 1
#define RGA_SRC_MIRR_MODE_Y 2
#define RGA_SRC_MIRR_MODE_X_Y 3

#define RGA_SRC_HSCL_MODE_NO 0
#define RGA_SRC_HSCL_MODE_DOWN 1
#define RGA_SRC_HSCL_MODE_UP 2

#define RGA_SRC_VSCL_MODE_NO 0
#define RGA_SRC_VSCL_MODE_DOWN 1
#define RGA_SRC_VSCL_MODE_UP 2

#define RGA_SRC_TRANS_ENABLE_R 1
#define RGA_SRC_TRANS_ENABLE_G 2
#define RGA_SRC_TRANS_ENABLE_B 4
#define RGA_SRC_TRANS_ENABLE_A 8

#define RGA_SRC_BIC_COE_SELEC_CATROM 0
#define RGA_SRC_BIC_COE_SELEC_MITCHELL 1
#define RGA_SRC_BIC_COE_SELEC_HERMITE 2
#define RGA_SRC_BIC_COE_SELEC_BSPLINE 3

#define RGA_DST_DITHER_MODE_888_TO_666 0
#define RGA_DST_DITHER_MODE_888_TO_565 1
#define RGA_DST_DITHER_MODE_888_TO_555 2
#define RGA_DST_DITHER_MODE_888_TO_444 3

#define RGA_DST_CSC_MODE_BYPASS 0
#define RGA_DST_CSC_MODE_BT601_R0 1
#define RGA_DST_CSC_MODE_BT601_R1 2
#define RGA_DST_CSC_MODE_BT709_R0 3

#define RGA_ALPHA_ROP_MODE_2 0
#define RGA_ALPHA_ROP_MODE_3 1
#define RGA_ALPHA_ROP_MODE_4 2

#define RGA_ALPHA_SELECT_ALPHA 0
#define RGA_ALPHA_SELECT_ROP 1

#define RGA_ALPHA_MASK_BIG_ENDIAN 0
#define RGA_ALPHA_MASK_LITTLE_ENDIAN 1

#define RGA_ALPHA_NORMAL 0
#define RGA_ALPHA_REVERSE 1

#define RGA_ALPHA_BLEND_GLOBAL 0
#define RGA_ALPHA_BLEND_NORMAL 1
#define RGA_ALPHA_BLEND_MULTIPLY 2

#define RGA_ALPHA_CAL_CUT 0
#define RGA_ALPHA_CAL_NORMAL 1

#define RGA_ALPHA_FACTOR_ZERO 0
#define RGA_ALPHA_FACTOR_ONE 1
#define RGA_ALPHA_FACTOR_OTHER 2
#define RGA_ALPHA_FACTOR_OTHER_REVERSE 3
#define RGA_ALPHA_FACTOR_SELF 4

#define RGA_ALPHA_COLOR_NORMAL 0
#define RGA_ALPHA_COLOR_MULTIPLY_CAL 1

 
union rga_mode_ctrl {
	unsigned int val;
	struct {
		 
		unsigned int render:3;
		 
		unsigned int bitblt:1;
		unsigned int cf_rop4_pat:1;
		unsigned int alpha_zero_key:1;
		unsigned int gradient_sat:1;
		 
		unsigned int reserved:25;
	} data;
};

union rga_src_info {
	unsigned int val;
	struct {
		 
		unsigned int format:4;
		 
		unsigned int swap:3;
		unsigned int cp_endian:1;
		 
		unsigned int csc_mode:2;
		unsigned int rot_mode:2;
		unsigned int mir_mode:2;
		unsigned int hscl_mode:2;
		unsigned int vscl_mode:2;
		 
		unsigned int trans_mode:1;
		unsigned int trans_enable:4;
		 
		unsigned int dither_up_en:1;
		unsigned int bic_coe_sel:2;
		 
		unsigned int reserved:6;
	} data;
};

union rga_src_vir_info {
	unsigned int val;
	struct {
		 
		unsigned int vir_width:15;
		unsigned int reserved:1;
		 
		unsigned int vir_stride:10;
		 
		unsigned int reserved1:6;
	} data;
};

union rga_src_act_info {
	unsigned int val;
	struct {
		 
		unsigned int act_width:13;
		unsigned int reserved:3;
		 
		unsigned int act_height:13;
		unsigned int reserved1:3;
	} data;
};

union rga_src_x_factor {
	unsigned int val;
	struct {
		 
		unsigned int down_scale_factor:16;
		 
		unsigned int up_scale_factor:16;
	} data;
};

union rga_src_y_factor {
	unsigned int val;
	struct {
		 
		unsigned int down_scale_factor:16;
		 
		unsigned int up_scale_factor:16;
	} data;
};

 
union rga_src_cp_gr_color {
	unsigned int val;
	struct {
		 
		unsigned int gradient_x:16;
		 
		unsigned int gradient_y:16;
	} data;
};

union rga_src_transparency_color0 {
	unsigned int val;
	struct {
		 
		unsigned int trans_rmin:8;
		 
		unsigned int trans_gmin:8;
		 
		unsigned int trans_bmin:8;
		 
		unsigned int trans_amin:8;
	} data;
};

union rga_src_transparency_color1 {
	unsigned int val;
	struct {
		 
		unsigned int trans_rmax:8;
		 
		unsigned int trans_gmax:8;
		 
		unsigned int trans_bmax:8;
		 
		unsigned int trans_amax:8;
	} data;
};

union rga_dst_info {
	unsigned int val;
	struct {
		 
		unsigned int format:4;
		 
		unsigned int swap:3;
		 
		unsigned int src1_format:3;
		 
		unsigned int src1_swap:2;
		 
		unsigned int dither_up_en:1;
		unsigned int dither_down_en:1;
		unsigned int dither_down_mode:2;
		 
		unsigned int csc_mode:2;
		unsigned int csc_clip:1;
		 
		unsigned int reserved:13;
	} data;
};

union rga_dst_vir_info {
	unsigned int val;
	struct {
		 
		unsigned int vir_stride:15;
		unsigned int reserved:1;
		 
		unsigned int src1_vir_stride:15;
		unsigned int reserved1:1;
	} data;
};

union rga_dst_act_info {
	unsigned int val;
	struct {
		 
		unsigned int act_width:12;
		unsigned int reserved:4;
		 
		unsigned int act_height:12;
		unsigned int reserved1:4;
	} data;
};

union rga_alpha_ctrl0 {
	unsigned int val;
	struct {
		 
		unsigned int rop_en:1;
		unsigned int rop_select:1;
		unsigned int rop_mode:2;
		 
		unsigned int src_fading_val:8;
		 
		unsigned int dst_fading_val:8;
		unsigned int mask_endian:1;
		 
		unsigned int reserved:11;
	} data;
};

union rga_alpha_ctrl1 {
	unsigned int val;
	struct {
		 
		unsigned int dst_color_m0:1;
		unsigned int src_color_m0:1;
		 
		unsigned int dst_factor_m0:3;
		unsigned int src_factor_m0:3;
		 
		unsigned int dst_alpha_cal_m0:1;
		unsigned int src_alpha_cal_m0:1;
		 
		unsigned int dst_blend_m0:2;
		unsigned int src_blend_m0:2;
		 
		unsigned int dst_alpha_m0:1;
		unsigned int src_alpha_m0:1;
		 
		unsigned int dst_factor_m1:3;
		unsigned int src_factor_m1:3;
		 
		unsigned int dst_alpha_cal_m1:1;
		unsigned int src_alpha_cal_m1:1;
		 
		unsigned int dst_blend_m1:2;
		unsigned int src_blend_m1:2;
		 
		unsigned int dst_alpha_m1:1;
		unsigned int src_alpha_m1:1;
		 
		unsigned int reserved:2;
	} data;
};

union rga_fading_ctrl {
	unsigned int val;
	struct {
		 
		unsigned int fading_offset_r:8;
		 
		unsigned int fading_offset_g:8;
		 
		unsigned int fading_offset_b:8;
		 
		unsigned int fading_en:1;
		unsigned int reserved:7;
	} data;
};

union rga_pat_con {
	unsigned int val;
	struct {
		 
		unsigned int width:8;
		 
		unsigned int height:8;
		 
		unsigned int offset_x:8;
		 
		unsigned int offset_y:8;
	} data;
};

#endif
