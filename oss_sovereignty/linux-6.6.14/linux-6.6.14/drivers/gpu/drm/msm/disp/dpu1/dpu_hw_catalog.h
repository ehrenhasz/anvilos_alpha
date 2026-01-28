#ifndef _DPU_HW_CATALOG_H
#define _DPU_HW_CATALOG_H
#include <linux/kernel.h>
#include <linux/bug.h>
#include <linux/bitmap.h>
#include <linux/err.h>
#define MAX_BLOCKS    12
#define DPU_HW_BLK_NAME_LEN	16
#define MAX_IMG_WIDTH 0x3fff
#define MAX_IMG_HEIGHT 0x3fff
#define CRTC_DUAL_MIXERS	2
#define MAX_XIN_COUNT 16
enum {
	DPU_MDP_PANIC_PER_PIPE = 0x1,
	DPU_MDP_10BIT_SUPPORT,
	DPU_MDP_AUDIO_SELECT,
	DPU_MDP_PERIPH_0_REMOVED,
	DPU_MDP_VSYNC_SEL,
	DPU_MDP_MAX
};
enum {
	DPU_SSPP_SCALER_QSEED2 = 0x1,
	DPU_SSPP_SCALER_QSEED3,
	DPU_SSPP_SCALER_QSEED3LITE,
	DPU_SSPP_SCALER_QSEED4,
	DPU_SSPP_SCALER_RGB,
	DPU_SSPP_CSC,
	DPU_SSPP_CSC_10BIT,
	DPU_SSPP_CURSOR,
	DPU_SSPP_QOS,
	DPU_SSPP_QOS_8LVL,
	DPU_SSPP_EXCL_RECT,
	DPU_SSPP_SMART_DMA_V1,
	DPU_SSPP_SMART_DMA_V2,
	DPU_SSPP_TS_PREFILL,
	DPU_SSPP_TS_PREFILL_REC1,
	DPU_SSPP_CDP,
	DPU_SSPP_INLINE_ROTATION,
	DPU_SSPP_MAX
};
enum {
	DPU_MIXER_LAYER = 0x1,
	DPU_MIXER_SOURCESPLIT,
	DPU_MIXER_GC,
	DPU_DIM_LAYER,
	DPU_MIXER_COMBINED_ALPHA,
	DPU_MIXER_MAX
};
enum {
	DPU_DSPP_PCC = 0x1,
	DPU_DSPP_MAX
};
enum {
	DPU_PINGPONG_TE = 0x1,
	DPU_PINGPONG_TE2,
	DPU_PINGPONG_SPLIT,
	DPU_PINGPONG_SLAVE,
	DPU_PINGPONG_DITHER,
	DPU_PINGPONG_DSC,
	DPU_PINGPONG_MAX
};
enum {
	DPU_CTL_SPLIT_DISPLAY = 0x1,
	DPU_CTL_ACTIVE_CFG,
	DPU_CTL_FETCH_ACTIVE,
	DPU_CTL_VM_CFG,
	DPU_CTL_HAS_LAYER_EXT4,
	DPU_CTL_DSPP_SUB_BLOCK_FLUSH,
	DPU_CTL_MAX
};
enum {
	DPU_INTF_INPUT_CTRL = 0x1,
	DPU_INTF_TE,
	DPU_DATA_HCTL_EN,
	DPU_INTF_STATUS_SUPPORTED,
	DPU_INTF_MAX
};
enum {
	DPU_WB_LINE_MODE = 0x1,
	DPU_WB_BLOCK_MODE,
	DPU_WB_UBWC,
	DPU_WB_YUV_CONFIG,
	DPU_WB_PIPE_ALPHA,
	DPU_WB_XY_ROI_OFFSET,
	DPU_WB_QOS,
	DPU_WB_QOS_8LVL,
	DPU_WB_CDP,
	DPU_WB_INPUT_CTRL,
	DPU_WB_CROP,
	DPU_WB_MAX
};
enum {
	DPU_VBIF_QOS_OTLIM = 0x1,
	DPU_VBIF_QOS_REMAP,
	DPU_VBIF_MAX
};
enum {
	DPU_DSC_OUTPUT_CTRL = 0x1,
	DPU_DSC_HW_REV_1_2,
	DPU_DSC_NATIVE_42x_EN,
	DPU_DSC_MAX
};
#define DPU_HW_BLK_INFO \
	char name[DPU_HW_BLK_NAME_LEN]; \
	u32 id; \
	u32 base; \
	u32 len; \
	unsigned long features
#define DPU_HW_SUBBLK_INFO \
	char name[DPU_HW_BLK_NAME_LEN]; \
	u32 id; \
	u32 base; \
	u32 len
struct dpu_scaler_blk {
	DPU_HW_SUBBLK_INFO;
	u32 version;
};
struct dpu_csc_blk {
	DPU_HW_SUBBLK_INFO;
};
struct dpu_pp_blk {
	DPU_HW_SUBBLK_INFO;
	u32 version;
};
struct dpu_dsc_blk {
	DPU_HW_SUBBLK_INFO;
};
enum dpu_qos_lut_usage {
	DPU_QOS_LUT_USAGE_LINEAR,
	DPU_QOS_LUT_USAGE_MACROTILE,
	DPU_QOS_LUT_USAGE_NRT,
	DPU_QOS_LUT_USAGE_MAX,
};
struct dpu_qos_lut_entry {
	u32 fl;
	u64 lut;
};
struct dpu_qos_lut_tbl {
	u32 nentry;
	const struct dpu_qos_lut_entry *entries;
};
struct dpu_rotation_cfg {
	u32 rot_maxheight;
	size_t rot_num_formats;
	const u32 *rot_format_list;
};
struct dpu_caps {
	u32 max_mixer_width;
	u32 max_mixer_blendstages;
	u32 qseed_type;
	bool has_src_split;
	bool has_dim_layer;
	bool has_idle_pc;
	bool has_3d_merge;
	u32 max_linewidth;
	u32 pixel_ram_size;
	u32 max_hdeci_exp;
	u32 max_vdeci_exp;
};
struct dpu_sspp_sub_blks {
	u32 maxdwnscale;
	u32 maxupscale;
	u32 smart_dma_priority;
	u32 max_per_pipe_bw;
	u32 qseed_ver;
	struct dpu_scaler_blk scaler_blk;
	struct dpu_pp_blk csc_blk;
	const u32 *format_list;
	u32 num_formats;
	const u32 *virt_format_list;
	u32 virt_num_formats;
	const struct dpu_rotation_cfg *rotation_cfg;
};
struct dpu_lm_sub_blks {
	u32 maxwidth;
	u32 maxblendstages;
	u32 blendstage_base[MAX_BLOCKS];
};
struct dpu_dspp_sub_blks {
	struct dpu_pp_blk pcc;
};
struct dpu_pingpong_sub_blks {
	struct dpu_pp_blk te;
	struct dpu_pp_blk te2;
	struct dpu_pp_blk dither;
};
struct dpu_dsc_sub_blks {
	struct dpu_dsc_blk enc;
	struct dpu_dsc_blk ctl;
};
enum dpu_clk_ctrl_type {
	DPU_CLK_CTRL_NONE,
	DPU_CLK_CTRL_VIG0,
	DPU_CLK_CTRL_VIG1,
	DPU_CLK_CTRL_VIG2,
	DPU_CLK_CTRL_VIG3,
	DPU_CLK_CTRL_VIG4,
	DPU_CLK_CTRL_RGB0,
	DPU_CLK_CTRL_RGB1,
	DPU_CLK_CTRL_RGB2,
	DPU_CLK_CTRL_RGB3,
	DPU_CLK_CTRL_DMA0,
	DPU_CLK_CTRL_DMA1,
	DPU_CLK_CTRL_DMA2,
	DPU_CLK_CTRL_DMA3,
	DPU_CLK_CTRL_DMA4,
	DPU_CLK_CTRL_DMA5,
	DPU_CLK_CTRL_CURSOR0,
	DPU_CLK_CTRL_CURSOR1,
	DPU_CLK_CTRL_INLINE_ROT0_SSPP,
	DPU_CLK_CTRL_REG_DMA,
	DPU_CLK_CTRL_WB2,
	DPU_CLK_CTRL_MAX,
};
struct dpu_clk_ctrl_reg {
	u32 reg_off;
	u32 bit_off;
};
struct dpu_mdp_cfg {
	DPU_HW_BLK_INFO;
	struct dpu_clk_ctrl_reg clk_ctrls[DPU_CLK_CTRL_MAX];
};
struct dpu_ctl_cfg {
	DPU_HW_BLK_INFO;
	s32 intr_start;
};
struct dpu_sspp_cfg {
	DPU_HW_BLK_INFO;
	const struct dpu_sspp_sub_blks *sblk;
	u32 xin_id;
	enum dpu_clk_ctrl_type clk_ctrl;
	u32 type;
};
struct dpu_lm_cfg {
	DPU_HW_BLK_INFO;
	const struct dpu_lm_sub_blks *sblk;
	u32 pingpong;
	u32 dspp;
	unsigned long lm_pair;
};
struct dpu_dspp_cfg  {
	DPU_HW_BLK_INFO;
	const struct dpu_dspp_sub_blks *sblk;
};
struct dpu_pingpong_cfg  {
	DPU_HW_BLK_INFO;
	u32 merge_3d;
	s32 intr_done;
	s32 intr_rdptr;
	const struct dpu_pingpong_sub_blks *sblk;
};
struct dpu_merge_3d_cfg  {
	DPU_HW_BLK_INFO;
	const struct dpu_merge_3d_sub_blks *sblk;
};
struct dpu_dsc_cfg {
	DPU_HW_BLK_INFO;
	const struct dpu_dsc_sub_blks *sblk;
};
struct dpu_intf_cfg  {
	DPU_HW_BLK_INFO;
	u32 type;    
	u32 controller_id;
	u32 prog_fetch_lines_worst_case;
	s32 intr_underrun;
	s32 intr_vsync;
	s32 intr_tear_rd_ptr;
};
struct dpu_wb_cfg {
	DPU_HW_BLK_INFO;
	u8 vbif_idx;
	u32 maxlinewidth;
	u32 xin_id;
	s32 intr_wb_done;
	const u32 *format_list;
	u32 num_formats;
	enum dpu_clk_ctrl_type clk_ctrl;
};
struct dpu_vbif_dynamic_ot_cfg {
	u64 pps;
	u32 ot_limit;
};
struct dpu_vbif_dynamic_ot_tbl {
	u32 count;
	const struct dpu_vbif_dynamic_ot_cfg *cfg;
};
struct dpu_vbif_qos_tbl {
	u32 npriority_lvl;
	const u32 *priority_lvl;
};
struct dpu_vbif_cfg {
	DPU_HW_BLK_INFO;
	u32 default_ot_rd_limit;
	u32 default_ot_wr_limit;
	u32 xin_halt_timeout;
	u32 qos_rp_remap_size;
	struct dpu_vbif_dynamic_ot_tbl dynamic_ot_rd_tbl;
	struct dpu_vbif_dynamic_ot_tbl dynamic_ot_wr_tbl;
	struct dpu_vbif_qos_tbl qos_rt_tbl;
	struct dpu_vbif_qos_tbl qos_nrt_tbl;
	u32 memtype_count;
	u32 memtype[MAX_XIN_COUNT];
};
enum {
	DPU_PERF_CDP_USAGE_RT,
	DPU_PERF_CDP_USAGE_NRT,
	DPU_PERF_CDP_USAGE_MAX
};
struct dpu_perf_cdp_cfg {
	bool rd_enable;
	bool wr_enable;
};
struct dpu_mdss_version {
	u8 core_major_ver;
	u8 core_minor_ver;
};
struct dpu_perf_cfg {
	u32 max_bw_low;
	u32 max_bw_high;
	u32 min_core_ib;
	u32 min_llcc_ib;
	u32 min_dram_ib;
	u32 undersized_prefill_lines;
	u32 xtra_prefill_lines;
	u32 dest_scale_prefill_lines;
	u32 macrotile_prefill_lines;
	u32 yuv_nv12_prefill_lines;
	u32 linear_prefill_lines;
	u32 downscaling_prefill_lines;
	u32 amortizable_threshold;
	u32 min_prefill_lines;
	u32 clk_inefficiency_factor;
	u32 bw_inefficiency_factor;
	u32 safe_lut_tbl[DPU_QOS_LUT_USAGE_MAX];
	u32 danger_lut_tbl[DPU_QOS_LUT_USAGE_MAX];
	struct dpu_qos_lut_tbl qos_lut_tbl[DPU_QOS_LUT_USAGE_MAX];
	struct dpu_perf_cdp_cfg cdp_cfg[DPU_PERF_CDP_USAGE_MAX];
};
struct dpu_mdss_cfg {
	const struct dpu_mdss_version *mdss_ver;
	const struct dpu_caps *caps;
	const struct dpu_mdp_cfg *mdp;
	u32 ctl_count;
	const struct dpu_ctl_cfg *ctl;
	u32 sspp_count;
	const struct dpu_sspp_cfg *sspp;
	u32 mixer_count;
	const struct dpu_lm_cfg *mixer;
	u32 pingpong_count;
	const struct dpu_pingpong_cfg *pingpong;
	u32 merge_3d_count;
	const struct dpu_merge_3d_cfg *merge_3d;
	u32 dsc_count;
	const struct dpu_dsc_cfg *dsc;
	u32 intf_count;
	const struct dpu_intf_cfg *intf;
	u32 vbif_count;
	const struct dpu_vbif_cfg *vbif;
	u32 wb_count;
	const struct dpu_wb_cfg *wb;
	u32 ad_count;
	u32 dspp_count;
	const struct dpu_dspp_cfg *dspp;
	const struct dpu_perf_cfg *perf;
	const struct dpu_format_extended *dma_formats;
	const struct dpu_format_extended *cursor_formats;
	const struct dpu_format_extended *vig_formats;
};
extern const struct dpu_mdss_cfg dpu_msm8998_cfg;
extern const struct dpu_mdss_cfg dpu_sdm845_cfg;
extern const struct dpu_mdss_cfg dpu_sm8150_cfg;
extern const struct dpu_mdss_cfg dpu_sc8180x_cfg;
extern const struct dpu_mdss_cfg dpu_sm8250_cfg;
extern const struct dpu_mdss_cfg dpu_sc7180_cfg;
extern const struct dpu_mdss_cfg dpu_sm6115_cfg;
extern const struct dpu_mdss_cfg dpu_sm6125_cfg;
extern const struct dpu_mdss_cfg dpu_sm6350_cfg;
extern const struct dpu_mdss_cfg dpu_qcm2290_cfg;
extern const struct dpu_mdss_cfg dpu_sm6375_cfg;
extern const struct dpu_mdss_cfg dpu_sm8350_cfg;
extern const struct dpu_mdss_cfg dpu_sc7280_cfg;
extern const struct dpu_mdss_cfg dpu_sc8280xp_cfg;
extern const struct dpu_mdss_cfg dpu_sm8450_cfg;
extern const struct dpu_mdss_cfg dpu_sm8550_cfg;
#endif  
