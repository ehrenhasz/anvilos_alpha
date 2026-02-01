 
 

#ifndef _DPU_HW_VBIF_H
#define _DPU_HW_VBIF_H

#include "dpu_hw_catalog.h"
#include "dpu_hw_mdss.h"
#include "dpu_hw_util.h"

struct dpu_hw_vbif;

 
struct dpu_hw_vbif_ops {
	 
	void (*set_limit_conf)(struct dpu_hw_vbif *vbif,
			u32 xin_id, bool rd, u32 limit);

	 
	u32 (*get_limit_conf)(struct dpu_hw_vbif *vbif,
			u32 xin_id, bool rd);

	 
	void (*set_halt_ctrl)(struct dpu_hw_vbif *vbif,
			u32 xin_id, bool enable);

	 
	bool (*get_halt_ctrl)(struct dpu_hw_vbif *vbif,
			u32 xin_id);

	 
	void (*set_qos_remap)(struct dpu_hw_vbif *vbif,
			u32 xin_id, u32 level, u32 remap_level);

	 
	void (*set_mem_type)(struct dpu_hw_vbif *vbif,
			u32 xin_id, u32 value);

	 
	void (*clear_errors)(struct dpu_hw_vbif *vbif,
		u32 *pnd_errors, u32 *src_errors);

	 
	void (*set_write_gather_en)(struct dpu_hw_vbif *vbif, u32 xin_id);
};

struct dpu_hw_vbif {
	 
	struct dpu_hw_blk_reg_map hw;

	 
	enum dpu_vbif idx;
	const struct dpu_vbif_cfg *cap;

	 
	struct dpu_hw_vbif_ops ops;
};

 
struct dpu_hw_vbif *dpu_hw_vbif_init(const struct dpu_vbif_cfg *cfg,
		void __iomem *addr);

void dpu_hw_vbif_destroy(struct dpu_hw_vbif *vbif);

#endif  
