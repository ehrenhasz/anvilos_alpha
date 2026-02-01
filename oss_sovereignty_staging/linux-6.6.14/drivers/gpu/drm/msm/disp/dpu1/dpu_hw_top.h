 
 

#ifndef _DPU_HW_TOP_H
#define _DPU_HW_TOP_H

#include "dpu_hw_catalog.h"
#include "dpu_hw_mdss.h"
#include "dpu_hw_util.h"

struct dpu_hw_mdp;

 
struct traffic_shaper_cfg {
	bool en;
	bool rd_client;
	u32 client_id;
	u32 bpc_denom;
	u64 bpc_numer;
};

 
struct split_pipe_cfg {
	bool en;
	enum dpu_intf_mode mode;
	enum dpu_intf intf;
	bool split_flush_en;
};

 
struct dpu_danger_safe_status {
	u8 mdp;
	u8 sspp[SSPP_MAX];
};

 
struct dpu_vsync_source_cfg {
	u32 pp_count;
	u32 frame_rate;
	u32 ppnumber[PINGPONG_MAX];
	u32 vsync_source;
};

 
struct dpu_hw_mdp_ops {
	 
	void (*setup_split_pipe)(struct dpu_hw_mdp *mdp,
			struct split_pipe_cfg *p);

	 
	void (*setup_traffic_shaper)(struct dpu_hw_mdp *mdp,
			struct traffic_shaper_cfg *cfg);

	 
	bool (*setup_clk_force_ctrl)(struct dpu_hw_mdp *mdp,
			enum dpu_clk_ctrl_type clk_ctrl, bool enable);

	 
	void (*get_danger_status)(struct dpu_hw_mdp *mdp,
			struct dpu_danger_safe_status *status);

	 
	void (*setup_vsync_source)(struct dpu_hw_mdp *mdp,
				struct dpu_vsync_source_cfg *cfg);

	 
	void (*get_safe_status)(struct dpu_hw_mdp *mdp,
			struct dpu_danger_safe_status *status);

	 
	void (*intf_audio_select)(struct dpu_hw_mdp *mdp);
};

struct dpu_hw_mdp {
	struct dpu_hw_blk base;
	struct dpu_hw_blk_reg_map hw;

	 
	const struct dpu_mdp_cfg *caps;

	 
	struct dpu_hw_mdp_ops ops;
};

 
struct dpu_hw_mdp *dpu_hw_mdptop_init(const struct dpu_mdp_cfg *cfg,
		void __iomem *addr,
		const struct dpu_mdss_cfg *m);

void dpu_hw_mdp_destroy(struct dpu_hw_mdp *mdp);

#endif  
