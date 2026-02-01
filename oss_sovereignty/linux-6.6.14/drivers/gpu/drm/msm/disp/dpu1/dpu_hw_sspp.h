 
 

#ifndef _DPU_HW_SSPP_H
#define _DPU_HW_SSPP_H

#include "dpu_hw_catalog.h"
#include "dpu_hw_mdss.h"
#include "dpu_hw_util.h"
#include "dpu_formats.h"

struct dpu_hw_sspp;

 
#define DPU_SSPP_FLIP_LR		BIT(0)
#define DPU_SSPP_FLIP_UD		BIT(1)
#define DPU_SSPP_SOURCE_ROTATED_90	BIT(2)
#define DPU_SSPP_ROT_90			BIT(3)
#define DPU_SSPP_SOLID_FILL		BIT(4)

 
#define DPU_SSPP_SCALER (BIT(DPU_SSPP_SCALER_RGB) | \
			 BIT(DPU_SSPP_SCALER_QSEED2) | \
			 BIT(DPU_SSPP_SCALER_QSEED3) | \
			 BIT(DPU_SSPP_SCALER_QSEED3LITE) | \
			 BIT(DPU_SSPP_SCALER_QSEED4))

 
#define DPU_SSPP_CSC_ANY (BIT(DPU_SSPP_CSC) | \
			  BIT(DPU_SSPP_CSC_10BIT))

 
enum {
	DPU_SSPP_COMP_0,
	DPU_SSPP_COMP_1_2,
	DPU_SSPP_COMP_2,
	DPU_SSPP_COMP_3,

	DPU_SSPP_COMP_MAX
};

 
enum dpu_sspp_multirect_index {
	DPU_SSPP_RECT_SOLO = 0,
	DPU_SSPP_RECT_0,
	DPU_SSPP_RECT_1,
};

enum dpu_sspp_multirect_mode {
	DPU_SSPP_MULTIRECT_NONE = 0,
	DPU_SSPP_MULTIRECT_PARALLEL,
	DPU_SSPP_MULTIRECT_TIME_MX,
};

enum {
	DPU_FRAME_LINEAR,
	DPU_FRAME_TILE_A4X,
	DPU_FRAME_TILE_A5X,
};

enum dpu_hw_filter {
	DPU_SCALE_FILTER_NEAREST = 0,
	DPU_SCALE_FILTER_BIL,
	DPU_SCALE_FILTER_PCMN,
	DPU_SCALE_FILTER_CA,
	DPU_SCALE_FILTER_MAX
};

enum dpu_hw_filter_alpa {
	DPU_SCALE_ALPHA_PIXEL_REP,
	DPU_SCALE_ALPHA_BIL
};

enum dpu_hw_filter_yuv {
	DPU_SCALE_2D_4X4,
	DPU_SCALE_2D_CIR,
	DPU_SCALE_1D_SEP,
	DPU_SCALE_BIL
};

struct dpu_hw_sharp_cfg {
	u32 strength;
	u32 edge_thr;
	u32 smooth_thr;
	u32 noise_thr;
};

struct dpu_hw_pixel_ext {
	 
	uint8_t enable_pxl_ext;

	int init_phase_x[DPU_MAX_PLANES];
	int phase_step_x[DPU_MAX_PLANES];
	int init_phase_y[DPU_MAX_PLANES];
	int phase_step_y[DPU_MAX_PLANES];

	 
	int num_ext_pxls_left[DPU_MAX_PLANES];
	int num_ext_pxls_right[DPU_MAX_PLANES];
	int num_ext_pxls_top[DPU_MAX_PLANES];
	int num_ext_pxls_btm[DPU_MAX_PLANES];

	 
	int left_ftch[DPU_MAX_PLANES];
	int right_ftch[DPU_MAX_PLANES];
	int top_ftch[DPU_MAX_PLANES];
	int btm_ftch[DPU_MAX_PLANES];

	 
	int left_rpt[DPU_MAX_PLANES];
	int right_rpt[DPU_MAX_PLANES];
	int top_rpt[DPU_MAX_PLANES];
	int btm_rpt[DPU_MAX_PLANES];

	uint32_t roi_w[DPU_MAX_PLANES];
	uint32_t roi_h[DPU_MAX_PLANES];

	 
	enum dpu_hw_filter horz_filter[DPU_MAX_PLANES];
	enum dpu_hw_filter vert_filter[DPU_MAX_PLANES];

};

 
struct dpu_sw_pipe_cfg {
	struct drm_rect src_rect;
	struct drm_rect dst_rect;
};

 
struct dpu_hw_pipe_ts_cfg {
	u64 size;
	u64 time;
};

 
struct dpu_sw_pipe {
	struct dpu_hw_sspp *sspp;
	enum dpu_sspp_multirect_index multirect_index;
	enum dpu_sspp_multirect_mode multirect_mode;
};

 
struct dpu_hw_sspp_ops {
	 
	void (*setup_format)(struct dpu_sw_pipe *pipe,
			     const struct dpu_format *fmt, u32 flags);

	 
	void (*setup_rects)(struct dpu_sw_pipe *pipe,
			    struct dpu_sw_pipe_cfg *cfg);

	 
	void (*setup_pe)(struct dpu_hw_sspp *ctx,
			struct dpu_hw_pixel_ext *pe_ext);

	 
	void (*setup_sourceaddress)(struct dpu_sw_pipe *ctx,
				    struct dpu_hw_fmt_layout *layout);

	 
	void (*setup_csc)(struct dpu_hw_sspp *ctx, const struct dpu_csc_cfg *data);

	 
	void (*setup_solidfill)(struct dpu_sw_pipe *pipe, u32 color);

	 

	void (*setup_multirect)(struct dpu_sw_pipe *pipe);

	 
	void (*setup_sharpening)(struct dpu_hw_sspp *ctx,
			struct dpu_hw_sharp_cfg *cfg);


	 
	void (*setup_qos_lut)(struct dpu_hw_sspp *ctx,
			struct dpu_hw_qos_cfg *cfg);

	 
	void (*setup_qos_ctrl)(struct dpu_hw_sspp *ctx,
			       bool danger_safe_en);

	 
	void (*setup_histogram)(struct dpu_hw_sspp *ctx,
			void *cfg);

	 
	void (*setup_scaler)(struct dpu_hw_sspp *ctx,
		struct dpu_hw_scaler3_cfg *scaler3_cfg,
		const struct dpu_format *format);

	 
	u32 (*get_scaler_ver)(struct dpu_hw_sspp *ctx);

	 
	void (*setup_cdp)(struct dpu_sw_pipe *pipe,
			  const struct dpu_format *fmt,
			  bool enable);
};

 
struct dpu_hw_sspp {
	struct dpu_hw_blk base;
	struct dpu_hw_blk_reg_map hw;
	const struct msm_mdss_data *ubwc;

	 
	enum dpu_sspp idx;
	const struct dpu_sspp_cfg *cap;

	 
	struct dpu_hw_sspp_ops ops;
};

struct dpu_kms;
 
struct dpu_hw_sspp *dpu_hw_sspp_init(const struct dpu_sspp_cfg *cfg,
		void __iomem *addr, const struct msm_mdss_data *mdss_data);

 
void dpu_hw_sspp_destroy(struct dpu_hw_sspp *ctx);

int _dpu_hw_sspp_init_debugfs(struct dpu_hw_sspp *hw_pipe, struct dpu_kms *kms,
			      struct dentry *entry);

#endif  

