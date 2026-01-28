#ifndef _DPU_HW_INTF_H
#define _DPU_HW_INTF_H
#include "dpu_hw_catalog.h"
#include "dpu_hw_mdss.h"
#include "dpu_hw_util.h"
struct dpu_hw_intf;
struct dpu_hw_intf_timing_params {
	u32 width;		 
	u32 height;		 
	u32 xres;		 
	u32 yres;		 
	u32 h_back_porch;
	u32 h_front_porch;
	u32 v_back_porch;
	u32 v_front_porch;
	u32 hsync_pulse_width;
	u32 vsync_pulse_width;
	u32 hsync_polarity;
	u32 vsync_polarity;
	u32 border_clr;
	u32 underflow_clr;
	u32 hsync_skew;
	bool wide_bus_en;
};
struct dpu_hw_intf_prog_fetch {
	u8 enable;
	u32 fetch_start;
};
struct dpu_hw_intf_status {
	u8 is_en;		 
	u8 is_prog_fetch_en;	 
	u32 frame_count;	 
	u32 line_count;		 
};
struct dpu_hw_intf_cmd_mode_cfg {
	u8 data_compress;	 
};
struct dpu_hw_intf_ops {
	void (*setup_timing_gen)(struct dpu_hw_intf *intf,
			const struct dpu_hw_intf_timing_params *p,
			const struct dpu_format *fmt);
	void (*setup_prg_fetch)(struct dpu_hw_intf *intf,
			const struct dpu_hw_intf_prog_fetch *fetch);
	void (*enable_timing)(struct dpu_hw_intf *intf,
			u8 enable);
	void (*get_status)(struct dpu_hw_intf *intf,
			struct dpu_hw_intf_status *status);
	u32 (*get_line_count)(struct dpu_hw_intf *intf);
	void (*bind_pingpong_blk)(struct dpu_hw_intf *intf,
			const enum dpu_pingpong pp);
	void (*setup_misr)(struct dpu_hw_intf *intf);
	int (*collect_misr)(struct dpu_hw_intf *intf, u32 *misr_value);
	int (*enable_tearcheck)(struct dpu_hw_intf *intf, struct dpu_hw_tear_check *cfg);
	int (*disable_tearcheck)(struct dpu_hw_intf *intf);
	int (*connect_external_te)(struct dpu_hw_intf *intf, bool enable_external_te);
	void (*vsync_sel)(struct dpu_hw_intf *intf, u32 vsync_source);
	void (*disable_autorefresh)(struct dpu_hw_intf *intf, uint32_t encoder_id, u16 vdisplay);
	void (*program_intf_cmd_cfg)(struct dpu_hw_intf *intf,
				     struct dpu_hw_intf_cmd_mode_cfg *cmd_mode_cfg);
};
struct dpu_hw_intf {
	struct dpu_hw_blk_reg_map hw;
	enum dpu_intf idx;
	const struct dpu_intf_cfg *cap;
	struct dpu_hw_intf_ops ops;
};
struct dpu_hw_intf *dpu_hw_intf_init(const struct dpu_intf_cfg *cfg,
		void __iomem *addr, const struct dpu_mdss_version *mdss_rev);
void dpu_hw_intf_destroy(struct dpu_hw_intf *intf);
#endif  
