#ifndef SOUND_SOC_SKL_SSP_CLK_H
#define SOUND_SOC_SKL_SSP_CLK_H
#define SKL_MAX_SSP		6
#define SKL_MAX_CLK_SRC		3
#define SKL_MAX_SSP_CLK_TYPES	3  
#define SKL_MAX_CLK_CNT		(SKL_MAX_SSP * SKL_MAX_SSP_CLK_TYPES)
#define SKL_MAX_CLK_RATES	10
#define SKL_SCLK_OFS		SKL_MAX_SSP
#define SKL_SCLKFS_OFS		(SKL_SCLK_OFS + SKL_MAX_SSP)
enum skl_clk_type {
	SKL_MCLK,
	SKL_SCLK,
	SKL_SCLK_FS,
};
enum skl_clk_src_type {
	SKL_XTAL,
	SKL_CARDINAL,
	SKL_PLL,
};
struct skl_clk_parent_src {
	u8 clk_id;
	const char *name;
	unsigned long rate;
	const char *parent_name;
};
struct skl_tlv_hdr {
	u32 type;
	u32 size;
};
struct skl_dmactrl_mclk_cfg {
	struct skl_tlv_hdr hdr;
	u32 clk_warm_up:16;
	u32 mclk:1;
	u32 warm_up_over:1;
	u32 rsvd0:14;
	u32 clk_stop_delay:16;
	u32 keep_running:1;
	u32 clk_stop_over:1;
	u32 rsvd1:14;
};
struct skl_dmactrl_sclkfs_cfg {
	struct skl_tlv_hdr hdr;
	u32 sampling_frequency;
	u32 bit_depth;
	u32 channel_map;
	u32 channel_config;
	u32 interleaving_style;
	u32 number_of_channels : 8;
	u32 valid_bit_depth : 8;
	u32 sample_type : 8;
	u32 reserved : 8;
};
union skl_clk_ctrl_ipc {
	struct skl_dmactrl_mclk_cfg mclk;
	struct skl_dmactrl_sclkfs_cfg sclk_fs;
};
struct skl_clk_rate_cfg_table {
	unsigned long rate;
	union skl_clk_ctrl_ipc dma_ctl_ipc;
	void *config;
};
struct skl_ssp_clk {
	const char *name;
	const char *parent_name;
	struct skl_clk_rate_cfg_table rate_cfg[SKL_MAX_CLK_RATES];
};
struct skl_clk_pdata {
	struct skl_clk_parent_src *parent_clks;
	int num_clks;
	struct skl_ssp_clk *ssp_clks;
	void *pvt_data;
};
#endif  
