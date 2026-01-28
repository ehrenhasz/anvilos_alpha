#ifndef _DPU_HW_UTIL_H
#define _DPU_HW_UTIL_H
#include <linux/io.h>
#include <linux/slab.h>
#include "dpu_hw_mdss.h"
#include "dpu_hw_catalog.h"
#define REG_MASK(n)                     ((BIT(n)) - 1)
#define MISR_FRAME_COUNT                0x1
#define MISR_CTRL_ENABLE                BIT(8)
#define MISR_CTRL_STATUS                BIT(9)
#define MISR_CTRL_STATUS_CLEAR          BIT(10)
#define MISR_CTRL_FREE_RUN_MASK         BIT(31)
struct dpu_hw_blk_reg_map {
	void __iomem *blk_addr;
	u32 log_mask;
};
struct dpu_hw_blk {
};
struct dpu_hw_scaler3_de_cfg {
	u32 enable;
	int16_t sharpen_level1;
	int16_t sharpen_level2;
	uint16_t clip;
	uint16_t limit;
	uint16_t thr_quiet;
	uint16_t thr_dieout;
	uint16_t thr_low;
	uint16_t thr_high;
	uint16_t prec_shift;
	int16_t adjust_a[DPU_MAX_DE_CURVES];
	int16_t adjust_b[DPU_MAX_DE_CURVES];
	int16_t adjust_c[DPU_MAX_DE_CURVES];
};
struct dpu_hw_scaler3_cfg {
	u32 enable;
	u32 dir_en;
	int32_t init_phase_x[DPU_MAX_PLANES];
	int32_t phase_step_x[DPU_MAX_PLANES];
	int32_t init_phase_y[DPU_MAX_PLANES];
	int32_t phase_step_y[DPU_MAX_PLANES];
	u32 preload_x[DPU_MAX_PLANES];
	u32 preload_y[DPU_MAX_PLANES];
	u32 src_width[DPU_MAX_PLANES];
	u32 src_height[DPU_MAX_PLANES];
	u32 dst_width;
	u32 dst_height;
	u32 y_rgb_filter_cfg;
	u32 uv_filter_cfg;
	u32 alpha_filter_cfg;
	u32 blend_cfg;
	u32 lut_flag;
	u32 dir_lut_idx;
	u32 y_rgb_cir_lut_idx;
	u32 uv_cir_lut_idx;
	u32 y_rgb_sep_lut_idx;
	u32 uv_sep_lut_idx;
	u32 *dir_lut;
	size_t dir_len;
	u32 *cir_lut;
	size_t cir_len;
	u32 *sep_lut;
	size_t sep_len;
	struct dpu_hw_scaler3_de_cfg de;
	u32 dir_weight;
};
struct dpu_drm_pix_ext_v1 {
	int32_t num_ext_pxls_lr[DPU_MAX_PLANES];
	int32_t num_ext_pxls_tb[DPU_MAX_PLANES];
	int32_t left_ftch[DPU_MAX_PLANES];
	int32_t right_ftch[DPU_MAX_PLANES];
	int32_t top_ftch[DPU_MAX_PLANES];
	int32_t btm_ftch[DPU_MAX_PLANES];
	int32_t left_rpt[DPU_MAX_PLANES];
	int32_t right_rpt[DPU_MAX_PLANES];
	int32_t top_rpt[DPU_MAX_PLANES];
	int32_t btm_rpt[DPU_MAX_PLANES];
};
struct dpu_drm_de_v1 {
	uint32_t enable;
	int16_t sharpen_level1;
	int16_t sharpen_level2;
	uint16_t clip;
	uint16_t limit;
	uint16_t thr_quiet;
	uint16_t thr_dieout;
	uint16_t thr_low;
	uint16_t thr_high;
	uint16_t prec_shift;
	int16_t adjust_a[DPU_MAX_DE_CURVES];
	int16_t adjust_b[DPU_MAX_DE_CURVES];
	int16_t adjust_c[DPU_MAX_DE_CURVES];
};
struct dpu_drm_scaler_v2 {
	uint32_t enable;
	uint32_t dir_en;
	struct dpu_drm_pix_ext_v1 pe;
	uint32_t horz_decimate;
	uint32_t vert_decimate;
	int32_t init_phase_x[DPU_MAX_PLANES];
	int32_t phase_step_x[DPU_MAX_PLANES];
	int32_t init_phase_y[DPU_MAX_PLANES];
	int32_t phase_step_y[DPU_MAX_PLANES];
	uint32_t preload_x[DPU_MAX_PLANES];
	uint32_t preload_y[DPU_MAX_PLANES];
	uint32_t src_width[DPU_MAX_PLANES];
	uint32_t src_height[DPU_MAX_PLANES];
	uint32_t dst_width;
	uint32_t dst_height;
	uint32_t y_rgb_filter_cfg;
	uint32_t uv_filter_cfg;
	uint32_t alpha_filter_cfg;
	uint32_t blend_cfg;
	uint32_t lut_flag;
	uint32_t dir_lut_idx;
	uint32_t y_rgb_cir_lut_idx;
	uint32_t uv_cir_lut_idx;
	uint32_t y_rgb_sep_lut_idx;
	uint32_t uv_sep_lut_idx;
	struct dpu_drm_de_v1 de;
};
struct dpu_hw_qos_cfg {
	u32 danger_lut;
	u32 safe_lut;
	u64 creq_lut;
	bool danger_safe_en;
};
u32 *dpu_hw_util_get_log_mask_ptr(void);
void dpu_reg_write(struct dpu_hw_blk_reg_map *c,
		u32 reg_off,
		u32 val,
		const char *name);
int dpu_reg_read(struct dpu_hw_blk_reg_map *c, u32 reg_off);
#define DPU_REG_WRITE(c, off, val) dpu_reg_write(c, off, val, #off)
#define DPU_REG_READ(c, off) dpu_reg_read(c, off)
void *dpu_hw_util_get_dir(void);
void dpu_hw_setup_scaler3(struct dpu_hw_blk_reg_map *c,
		struct dpu_hw_scaler3_cfg *scaler3_cfg,
		u32 scaler_offset, u32 scaler_version,
		const struct dpu_format *format);
u32 dpu_hw_get_scaler3_ver(struct dpu_hw_blk_reg_map *c,
		u32 scaler_offset);
void dpu_hw_csc_setup(struct dpu_hw_blk_reg_map  *c,
		u32 csc_reg_off,
		const struct dpu_csc_cfg *data, bool csc10);
void dpu_setup_cdp(struct dpu_hw_blk_reg_map *c, u32 offset,
		   const struct dpu_format *fmt, bool enable);
u64 _dpu_hw_get_qos_lut(const struct dpu_qos_lut_tbl *tbl,
		u32 total_fl);
void _dpu_hw_setup_qos_lut(struct dpu_hw_blk_reg_map *c, u32 offset,
			   bool qos_8lvl,
			   const struct dpu_hw_qos_cfg *cfg);
void dpu_hw_setup_misr(struct dpu_hw_blk_reg_map *c,
		u32 misr_ctrl_offset, u8 input_sel);
int dpu_hw_collect_misr(struct dpu_hw_blk_reg_map *c,
		u32 misr_ctrl_offset,
		u32 misr_signature_offset,
		u32 *misr_value);
#endif  
