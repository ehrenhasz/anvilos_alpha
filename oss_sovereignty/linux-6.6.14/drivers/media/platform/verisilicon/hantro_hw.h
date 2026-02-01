 
 

#ifndef HANTRO_HW_H_
#define HANTRO_HW_H_

#include <linux/interrupt.h>
#include <linux/v4l2-controls.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-vp9.h>
#include <media/videobuf2-core.h>

#include "rockchip_av1_entropymode.h"
#include "rockchip_av1_filmgrain.h"

#define DEC_8190_ALIGN_MASK	0x07U

#define MB_DIM			16
#define TILE_MB_DIM		4
#define MB_WIDTH(w)		DIV_ROUND_UP(w, MB_DIM)
#define MB_HEIGHT(h)		DIV_ROUND_UP(h, MB_DIM)

#define FMT_MIN_WIDTH		48
#define FMT_MIN_HEIGHT		48
#define FMT_HD_WIDTH		1280
#define FMT_HD_HEIGHT		720
#define FMT_FHD_WIDTH		1920
#define FMT_FHD_HEIGHT		1088
#define FMT_UHD_WIDTH		3840
#define FMT_UHD_HEIGHT		2160
#define FMT_4K_WIDTH		4096
#define FMT_4K_HEIGHT		2304

#define NUM_REF_PICTURES	(V4L2_HEVC_DPB_ENTRIES_NUM_MAX + 1)

#define AV1_MAX_FRAME_BUF_COUNT	(V4L2_AV1_TOTAL_REFS_PER_FRAME + 1)

struct hantro_dev;
struct hantro_ctx;
struct hantro_buf;
struct hantro_variant;

 
struct hantro_aux_buf {
	void *cpu;
	dma_addr_t dma;
	size_t size;
	unsigned long attrs;
};

 
#define HANTRO_H264_DPB_SIZE		16

 
struct hantro_h264_dec_ctrls {
	const struct v4l2_ctrl_h264_decode_params *decode;
	const struct v4l2_ctrl_h264_scaling_matrix *scaling;
	const struct v4l2_ctrl_h264_sps *sps;
	const struct v4l2_ctrl_h264_pps *pps;
};

 
struct hantro_h264_dec_reflists {
	struct v4l2_h264_reference p[V4L2_H264_REF_LIST_LEN];
	struct v4l2_h264_reference b0[V4L2_H264_REF_LIST_LEN];
	struct v4l2_h264_reference b1[V4L2_H264_REF_LIST_LEN];
};

 
struct hantro_h264_dec_hw_ctx {
	struct hantro_aux_buf priv;
	struct v4l2_h264_dpb_entry dpb[HANTRO_H264_DPB_SIZE];
	struct hantro_h264_dec_reflists reflists;
	struct hantro_h264_dec_ctrls ctrls;
	u32 dpb_longterm;
	u32 dpb_valid;
	s32 cur_poc;
};

 
struct hantro_hevc_dec_ctrls {
	const struct v4l2_ctrl_hevc_decode_params *decode_params;
	const struct v4l2_ctrl_hevc_scaling_matrix *scaling;
	const struct v4l2_ctrl_hevc_sps *sps;
	const struct v4l2_ctrl_hevc_pps *pps;
	u32 hevc_hdr_skip_length;
};

 
struct hantro_hevc_dec_hw_ctx {
	struct hantro_aux_buf tile_sizes;
	struct hantro_aux_buf tile_filter;
	struct hantro_aux_buf tile_sao;
	struct hantro_aux_buf tile_bsd;
	struct hantro_aux_buf ref_bufs[NUM_REF_PICTURES];
	struct hantro_aux_buf scaling_lists;
	s32 ref_bufs_poc[NUM_REF_PICTURES];
	u32 ref_bufs_used;
	struct hantro_hevc_dec_ctrls ctrls;
	unsigned int num_tile_cols_allocated;
};

 
struct hantro_mpeg2_dec_hw_ctx {
	struct hantro_aux_buf qtable;
};

 
struct hantro_vp8_dec_hw_ctx {
	struct hantro_aux_buf segment_map;
	struct hantro_aux_buf prob_tbl;
};

 
struct hantro_vp9_frame_info {
	u32 valid : 1;
	u32 frame_context_idx : 2;
	u32 reference_mode : 2;
	u32 tx_mode : 3;
	u32 interpolation_filter : 3;
	u32 flags;
	u64 timestamp;
};

#define MAX_SB_COLS	64
#define MAX_SB_ROWS	34

 
struct hantro_vp9_dec_hw_ctx {
	struct hantro_aux_buf tile_edge;
	struct hantro_aux_buf segment_map;
	struct hantro_aux_buf misc;
	struct v4l2_vp9_frame_symbol_counts cnts;
	struct v4l2_vp9_frame_context probability_tables;
	struct v4l2_vp9_frame_context frame_context[4];
	struct hantro_vp9_frame_info cur;
	struct hantro_vp9_frame_info last;

	unsigned int bsd_ctrl_offset;
	unsigned int segment_map_size;
	unsigned int ctx_counters_offset;
	unsigned int tile_info_offset;

	unsigned short tile_r_info[MAX_SB_ROWS];
	unsigned short tile_c_info[MAX_SB_COLS];
	unsigned int last_tile_r;
	unsigned int last_tile_c;
	unsigned int last_sbs_r;
	unsigned int last_sbs_c;

	unsigned int active_segment;
	u8 feature_enabled[8];
	s16 feature_data[8][4];
};

 
struct hantro_av1_dec_ctrls {
	const struct v4l2_ctrl_av1_sequence *sequence;
	const struct v4l2_ctrl_av1_tile_group_entry *tile_group_entry;
	const struct v4l2_ctrl_av1_frame *frame;
	const struct v4l2_ctrl_av1_film_grain *film_grain;
};

struct hantro_av1_frame_ref {
	int width;
	int height;
	int mi_cols;
	int mi_rows;
	u64 timestamp;
	enum v4l2_av1_frame_type frame_type;
	bool used;
	u32 order_hint;
	u32 order_hints[V4L2_AV1_TOTAL_REFS_PER_FRAME];
	struct vb2_v4l2_buffer *vb2_ref;
};

 
struct hantro_av1_dec_hw_ctx {
	struct hantro_aux_buf db_data_col;
	struct hantro_aux_buf db_ctrl_col;
	struct hantro_aux_buf cdef_col;
	struct hantro_aux_buf sr_col;
	struct hantro_aux_buf lr_col;
	struct hantro_aux_buf global_model;
	struct hantro_aux_buf tile_info;
	struct hantro_aux_buf segment;
	struct hantro_aux_buf film_grain;
	struct hantro_aux_buf prob_tbl;
	struct hantro_aux_buf prob_tbl_out;
	struct hantro_aux_buf tile_buf;
	struct hantro_av1_dec_ctrls ctrls;
	struct hantro_av1_frame_ref frame_refs[AV1_MAX_FRAME_BUF_COUNT];
	u32 ref_frame_sign_bias[V4L2_AV1_TOTAL_REFS_PER_FRAME];
	unsigned int num_tile_cols_allocated;
	struct av1cdfs *cdfs;
	struct mvcdfs  *cdfs_ndvc;
	struct av1cdfs default_cdfs;
	struct mvcdfs  default_cdfs_ndvc;
	struct av1cdfs cdfs_last[NUM_REF_FRAMES];
	struct mvcdfs  cdfs_last_ndvc[NUM_REF_FRAMES];
	int current_frame_index;
};
 
struct hantro_postproc_ctx {
	struct hantro_aux_buf dec_q[VB2_MAX_FRAME];
};

 
struct hantro_postproc_ops {
	void (*enable)(struct hantro_ctx *ctx);
	void (*disable)(struct hantro_ctx *ctx);
	int (*enum_framesizes)(struct hantro_ctx *ctx, struct v4l2_frmsizeenum *fsize);
};

 
struct hantro_codec_ops {
	int (*init)(struct hantro_ctx *ctx);
	void (*exit)(struct hantro_ctx *ctx);
	int (*run)(struct hantro_ctx *ctx);
	void (*done)(struct hantro_ctx *ctx);
	void (*reset)(struct hantro_ctx *ctx);
};

 
enum hantro_enc_fmt {
	ROCKCHIP_VPU_ENC_FMT_YUV420P = 0,
	ROCKCHIP_VPU_ENC_FMT_YUV420SP = 1,
	ROCKCHIP_VPU_ENC_FMT_YUYV422 = 2,
	ROCKCHIP_VPU_ENC_FMT_UYVY422 = 3,
};

extern const struct hantro_variant imx8mm_vpu_g1_variant;
extern const struct hantro_variant imx8mq_vpu_g1_variant;
extern const struct hantro_variant imx8mq_vpu_g2_variant;
extern const struct hantro_variant imx8mq_vpu_variant;
extern const struct hantro_variant px30_vpu_variant;
extern const struct hantro_variant rk3036_vpu_variant;
extern const struct hantro_variant rk3066_vpu_variant;
extern const struct hantro_variant rk3288_vpu_variant;
extern const struct hantro_variant rk3328_vpu_variant;
extern const struct hantro_variant rk3399_vpu_variant;
extern const struct hantro_variant rk3568_vepu_variant;
extern const struct hantro_variant rk3568_vpu_variant;
extern const struct hantro_variant rk3588_vpu981_variant;
extern const struct hantro_variant sama5d4_vdec_variant;
extern const struct hantro_variant sunxi_vpu_variant;

extern const struct hantro_postproc_ops hantro_g1_postproc_ops;
extern const struct hantro_postproc_ops hantro_g2_postproc_ops;
extern const struct hantro_postproc_ops rockchip_vpu981_postproc_ops;

extern const u32 hantro_vp8_dec_mc_filter[8][6];

void hantro_watchdog(struct work_struct *work);
void hantro_run(struct hantro_ctx *ctx);
void hantro_irq_done(struct hantro_dev *vpu,
		     enum vb2_buffer_state result);
void hantro_start_prepare_run(struct hantro_ctx *ctx);
void hantro_end_prepare_run(struct hantro_ctx *ctx);

irqreturn_t hantro_g1_irq(int irq, void *dev_id);
void hantro_g1_reset(struct hantro_ctx *ctx);

int hantro_h1_jpeg_enc_run(struct hantro_ctx *ctx);
int rockchip_vpu2_jpeg_enc_run(struct hantro_ctx *ctx);
void hantro_h1_jpeg_enc_done(struct hantro_ctx *ctx);
void rockchip_vpu2_jpeg_enc_done(struct hantro_ctx *ctx);

dma_addr_t hantro_h264_get_ref_buf(struct hantro_ctx *ctx,
				   unsigned int dpb_idx);
u16 hantro_h264_get_ref_nbr(struct hantro_ctx *ctx,
			    unsigned int dpb_idx);
int hantro_h264_dec_prepare_run(struct hantro_ctx *ctx);
int rockchip_vpu2_h264_dec_run(struct hantro_ctx *ctx);
int hantro_g1_h264_dec_run(struct hantro_ctx *ctx);
int hantro_h264_dec_init(struct hantro_ctx *ctx);
void hantro_h264_dec_exit(struct hantro_ctx *ctx);

int hantro_hevc_dec_init(struct hantro_ctx *ctx);
void hantro_hevc_dec_exit(struct hantro_ctx *ctx);
int hantro_g2_hevc_dec_run(struct hantro_ctx *ctx);
int hantro_hevc_dec_prepare_run(struct hantro_ctx *ctx);
void hantro_hevc_ref_init(struct hantro_ctx *ctx);
dma_addr_t hantro_hevc_get_ref_buf(struct hantro_ctx *ctx, s32 poc);
int hantro_hevc_add_ref_buf(struct hantro_ctx *ctx, int poc, dma_addr_t addr);

int rockchip_vpu981_av1_dec_init(struct hantro_ctx *ctx);
void rockchip_vpu981_av1_dec_exit(struct hantro_ctx *ctx);
int rockchip_vpu981_av1_dec_run(struct hantro_ctx *ctx);
void rockchip_vpu981_av1_dec_done(struct hantro_ctx *ctx);

static inline unsigned short hantro_vp9_num_sbs(unsigned short dimension)
{
	return (dimension + 63) / 64;
}

static inline size_t
hantro_vp9_mv_size(unsigned int width, unsigned int height)
{
	int num_ctbs;

	 
	num_ctbs = hantro_vp9_num_sbs(width) * hantro_vp9_num_sbs(height);
	return (num_ctbs * 64) * 16;
}

static inline size_t
hantro_h264_mv_size(unsigned int width, unsigned int height)
{
	 
	return 64 * MB_WIDTH(width) * MB_WIDTH(height) + 32;
}

static inline size_t
hantro_hevc_mv_size(unsigned int width, unsigned int height)
{
	 
	return width * height / 16;
}

static inline unsigned short hantro_av1_num_sbs(unsigned short dimension)
{
	return DIV_ROUND_UP(dimension, 64);
}

static inline size_t
hantro_av1_mv_size(unsigned int width, unsigned int height)
{
	size_t num_sbs = hantro_av1_num_sbs(width) * hantro_av1_num_sbs(height);

	return ALIGN(num_sbs * 384, 16) * 2 + 512;
}

int hantro_g1_mpeg2_dec_run(struct hantro_ctx *ctx);
int rockchip_vpu2_mpeg2_dec_run(struct hantro_ctx *ctx);
void hantro_mpeg2_dec_copy_qtable(u8 *qtable,
				  const struct v4l2_ctrl_mpeg2_quantisation *ctrl);
int hantro_mpeg2_dec_init(struct hantro_ctx *ctx);
void hantro_mpeg2_dec_exit(struct hantro_ctx *ctx);

int hantro_g1_vp8_dec_run(struct hantro_ctx *ctx);
int rockchip_vpu2_vp8_dec_run(struct hantro_ctx *ctx);
int hantro_vp8_dec_init(struct hantro_ctx *ctx);
void hantro_vp8_dec_exit(struct hantro_ctx *ctx);
void hantro_vp8_prob_update(struct hantro_ctx *ctx,
			    const struct v4l2_ctrl_vp8_frame *hdr);

int hantro_g2_vp9_dec_run(struct hantro_ctx *ctx);
void hantro_g2_vp9_dec_done(struct hantro_ctx *ctx);
int hantro_vp9_dec_init(struct hantro_ctx *ctx);
void hantro_vp9_dec_exit(struct hantro_ctx *ctx);
void hantro_g2_check_idle(struct hantro_dev *vpu);
irqreturn_t hantro_g2_irq(int irq, void *dev_id);

#endif  
